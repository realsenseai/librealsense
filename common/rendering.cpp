// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rendering.h"
#include "labeled-point-cloud-utilities.h"

#include <cmath>

namespace rs2
{
    namespace
    {
        // Occupancy cells are one signed byte each: -1 unknown, 0 free, 100 occupied.
        // Two carriers exist. A MAP1 frame is self-describing -- the OCCG sub-header that
        // follows the 20-byte common header holds the geometry -- while the legacy stream
        // reports it out-of-band in UVC metadata. Framing is being retired: MAP1 presence
        // must not be used to decide anything about the PRODUCT, only about the payload.
        struct occupancy_layout
        {
            bool valid = false;
            bool map1 = false;      // true if this specific payload happened to be MAP1-framed
            bool transpose = true;  // display orientation -- see resolve_display_transpose()
            int cols = 0;
            int rows = 0;
            const uint8_t * cells = nullptr;
            size_t cell_count = 0;
            float cell_size_cm = 0.f;     // 0 = unknown
        };

        // D585S and D585_LEGACY publish the occupancy grid with lateral on the width axis
        // (their original, pre-NAV2 convention). Every other D5xx Mapping product publishes
        // X forward on the width axis (REP-103). This used to be inferred from MAP1 presence,
        // which happened to correlate with the product -- but framing is a transport detail,
        // not a product identifier, and pure-payload frames carry no such signal at all. Ask
        // the product directly instead.
        bool resolve_display_transpose( const rs2::frame & frame )
        {
            auto sens = rs2::sensor_from_frame( frame );
            if( sens && sens->supports( RS2_CAMERA_INFO_PRODUCT_ID ) )
            {
                const std::string pid = sens->get_info( RS2_CAMERA_INFO_PRODUCT_ID );
                if( pid == "0B6B" || pid == "0B6A" )   // D585S / D585_LEGACY
                    return false;
            }
            return true;
        }

        occupancy_layout resolve_occupancy_layout( const rs2::frame & frame, const void * data )
        {
            occupancy_layout out;
            out.transpose = resolve_display_transpose( frame );

            static const uint32_t MAP1_MAGIC = 0x3150414DU;   // "MAP1"
            static const size_t MAP1_HEADER_LEN = 20;
            static const size_t MAP1_OCCG_SUBHEADER_LEN = 32;
            static const uint8_t MAP1_DATA_TYPE_OCCG = 2;

            const uint8_t * cells = static_cast< const uint8_t * >( data );
            const size_t frame_bytes = static_cast< size_t >( frame.get_data_size() );

            if( data && frame_bytes >= MAP1_HEADER_LEN + MAP1_OCCG_SUBHEADER_LEN )
            {
                uint32_t magic = 0;
                std::memcpy( &magic, cells, sizeof( magic ) );
                if( magic == MAP1_MAGIC && cells[6] == MAP1_DATA_TYPE_OCCG )
                {
                    // OCCG subheader layout per hkr_fw's map1.py (_OCCG_HDR = "<HHHHiiIQI"):
                    // width, height, res_mm, cell_stride, origin_x_mm, origin_y_mm,
                    // source_frame_id, timestamp_us, cell_count.
                    uint16_t w = 0, h = 0, cell_size_mm = 0;
                    std::memcpy( &w, cells + MAP1_HEADER_LEN + 0, sizeof( w ) );
                    std::memcpy( &h, cells + MAP1_HEADER_LEN + 2, sizeof( h ) );
                    std::memcpy( &cell_size_mm, cells + MAP1_HEADER_LEN + 4, sizeof( cell_size_mm ) );
                    out.cols = w;
                    out.rows = h;
                    out.cells = cells + MAP1_HEADER_LEN + MAP1_OCCG_SUBHEADER_LEN;
                    out.map1 = true;
                    out.cell_size_cm = cell_size_mm / 10.f;
                }
            }

            if( ! out.map1 )
            {
                // Never throw from here: this runs once per frame inside the render loop, so a
                // frame we cannot describe must be skipped, not turned into an exception storm.
                if( ! frame.supports_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS )
                    || ! frame.supports_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_COLUMNS ) )
                    return out;

                out.cols = static_cast< int >( frame.get_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_COLUMNS ) );
                out.rows = static_cast< int >( frame.get_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS ) );
                out.cells = cells;
                if( frame.supports_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_CELL_SIZE ) )
                    out.cell_size_cm
                        = static_cast< float >( frame.get_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_CELL_SIZE ) );
            }

            // Sanity bound, both carriers: reject an implausible cell size (real occupancy
            // cells are a few cm across) rather than scale a 2D overlay by a wild value.
            if( out.cell_size_cm < 0.5f || out.cell_size_cm > 50.f )
                out.cell_size_cm = 0.f;

            if( out.cols <= 0 || out.rows <= 0 || ! data )
                return occupancy_layout{};

            out.cell_count = static_cast< size_t >( out.cols ) * out.rows;
            const size_t available = frame_bytes - static_cast< size_t >( out.cells - cells );
            if( available < out.cell_count )
                return occupancy_layout{};

            out.valid = true;
            return out;
        }

        // Three states, so three luminance levels -- free must stay distinguishable from
        // never-observed, which is the whole point of the -1 value. ROS map convention:
        // free = white, occupied = black, unknown = mid-grey. LUT over all 256 byte values
        // instead of a per-cell branch: this runs once per cell, per frame.
        const std::array< uint8_t, 256 > & occupancy_level_lut()
        {
            static const std::array< uint8_t, 256 > lut = []() {
                std::array< uint8_t, 256 > l{};
                for( int raw = 0; raw < 256; ++raw )
                {
                    const int8_t v = static_cast< int8_t >( raw );
                    l[raw] = ( v < 0 ) ? 0x80           // unknown
                           : ( v == 0 ) ? 0xFF          // free
                                        : 0x00;         // occupied
                }
                return l;
            }();
            return lut;
        }

        // Decodes cells into luminance bytes, applying the display-orientation transpose when
        // needed. raw_vec mirrors vec but keeps the original signed value (-1/0/100), for
        // hover-readout.
        void decode_occupancy_cells( const occupancy_layout & layout, std::vector< uint8_t > & vec,
                                      std::vector< int8_t > & raw_vec, int & tex_cols, int & tex_rows )
        {
            const auto & level_lut = occupancy_level_lut();
            vec.resize( layout.cell_count );
            raw_vec.resize( layout.cell_count );
            tex_cols = layout.cols;
            tex_rows = layout.rows;

            if( layout.transpose )
            {
                // This product stores the grid in ROS map order: the width axis is +X, forward
                // from the camera, and the height axis is +Y, lateral. Drawn straight, that puts
                // forward along the screen's horizontal. Present it as a top-down map instead --
                // forward up the screen, +Y to the left -- which is how the same grid reads in
                // RViz. D585S/D585_LEGACY already arrive with lateral on the width axis, so they
                // are uploaded as-is (see resolve_display_transpose()).
                tex_cols = layout.rows;   // lateral
                tex_rows = layout.cols;   // forward
                for( int r = 0; r < tex_rows; ++r )
                {
                    const int cx = layout.cols - 1 - r;                  // far row first
                    for( int c = 0; c < tex_cols; ++c )
                    {
                        const int cy = layout.rows - 1 - c;              // +Y on the left
                        const uint8_t raw = layout.cells[static_cast< size_t >( cy ) * layout.cols + cx];
                        const size_t idx = static_cast< size_t >( r ) * tex_cols + c;
                        vec[idx] = level_lut[raw];
                        raw_vec[idx] = static_cast< int8_t >( raw );
                    }
                }
            }
            else
            {
                for( size_t i = 0; i < layout.cell_count; ++i )
                {
                    vec[i] = level_lut[layout.cells[i]];
                    raw_vec[i] = static_cast< int8_t >( layout.cells[i] );
                }
            }
        }
    }  // namespace

    void texture_buffer::upload_occupancy_frame( const rs2::frame & frame, const void * data )
    {
        const occupancy_layout layout = resolve_occupancy_layout( frame, data );
        if( ! layout.valid )
            return;

        std::vector< uint8_t > vec;
        int tex_cols = 0, tex_rows = 0;
        decode_occupancy_cells( layout, vec, last_occupancy_raw, tex_cols, tex_rows );

        last_occupancy_geometry.valid = true;
        last_occupancy_geometry.tex_cols = tex_cols;
        last_occupancy_geometry.tex_rows = tex_rows;
        last_occupancy_geometry.cell_size_cm = layout.cell_size_cm;

        // Default alignment is 4 byte on windows, store it and work with 1 as our grid columns are not a multiple of 4
        GLint unpackAlignment;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);

        // Change alignment to 1
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Render
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex_cols, tex_rows, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, vec.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // Restore default alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
    }

    void texture_buffer::upload_label_colors( const labeled_points & lpc )
    {
        static const auto lut = []()
        {
            std::array< std::array< uint8_t, 3 >, 256 > res;
            for( auto & c : res )
                c = { 255, 255, 255 };
            for( auto & lc : labeled_point_cloud_utilities::get_label_to_color3f() )
                res[uint8_t( lc.first )] = { uint8_t( std::min( lc.second.x, 1.f ) * 255.f + .5f ),
                                             uint8_t( std::min( lc.second.y, 1.f ) * 255.f + .5f ),
                                             uint8_t( std::min( lc.second.z, 1.f ) * 255.f + .5f ) };
            return res;
        }();

        const int w = int( lpc.get_width() ), h = int( lpc.get_height() );
        const uint8_t * labels = lpc.get_labels();
        if( ! labels || w <= 0 || h <= 0 || lpc.size() < size_t( w ) * h )
            return;

        // Points without depth are drawn black, as the 3D view skips them: FW labels them OVERHEAD
        // (their vertex is 404, but labeling sees z = INT16_MAX mm)
        const rs2::vertex * vertices = lpc.get_vertices();
        auto is_invalid = []( const rs2::vertex & v )
        {
            const float bad = labeled_point_cloud_utilities::invalid_value;
            return ! std::isfinite( v.x ) || ! std::isfinite( v.y ) || ! std::isfinite( v.z ) || v.x == bad || v.y == bad
                || v.z == bad
                || v.x * v.x + v.y * v.y + v.z * v.z
                       >= labeled_point_cloud_utilities::saturated_depth * labeled_point_cloud_utilities::saturated_depth;
        };
        std::vector< uint8_t > rgb( size_t( w ) * h * 3, 0 );
        for( size_t i = 0; i < size_t( w ) * h; ++i )
            if( ! vertices || ! is_invalid( vertices[i] ) )
                std::memcpy( &rgb[i * 3], lut[labels[i]].data(), 3 );

        GLint unpack_alignment;
        glGetIntegerv( GL_UNPACK_ALIGNMENT, &unpack_alignment );
        glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data() );
        glPixelStorei( GL_UNPACK_ALIGNMENT, unpack_alignment );

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    }
}  // namespace rs2
