// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rendering.h"

namespace rs2
{
    namespace
    {
        // Occupancy cells are one signed byte each: -1 unknown, 0 free, 100 occupied.
        // Two carriers exist. A MAP1 frame is self-describing -- the OCCG sub-header that
        // follows the 20-byte common header holds the geometry -- while the legacy stream
        // reports it out-of-band in UVC metadata.
        struct occupancy_layout
        {
            bool valid = false;
            bool map1 = false;
            int cols = 0;
            int rows = 0;
            const uint8_t * cells = nullptr;
            size_t cell_count = 0;
        };

        occupancy_layout resolve_occupancy_layout( const rs2::frame & frame, const void * data )
        {
            occupancy_layout out;

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
                    uint16_t w = 0, h = 0;
                    std::memcpy( &w, cells + MAP1_HEADER_LEN + 0, sizeof( w ) );
                    std::memcpy( &h, cells + MAP1_HEADER_LEN + 2, sizeof( h ) );
                    out.cols = w;
                    out.rows = h;
                    out.cells = cells + MAP1_HEADER_LEN + MAP1_OCCG_SUBHEADER_LEN;
                    out.map1 = true;
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
            }

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

        // Decodes cells into luminance bytes, applying the MAP1 axis transpose when needed.
        void decode_occupancy_cells( const occupancy_layout & layout, std::vector< uint8_t > & vec, int & tex_cols, int & tex_rows )
        {
            const auto & level_lut = occupancy_level_lut();
            vec.resize( layout.cell_count );
            tex_cols = layout.cols;
            tex_rows = layout.rows;

            if( layout.map1 )
            {
                // MAP1 stores the grid in ROS map order: the width axis is +X, forward from the
                // camera, and the height axis is +Y, lateral. Drawn straight, that puts forward
                // along the screen's horizontal. Present it as a top-down map instead -- forward
                // up the screen, +Y to the left -- which is how the same grid reads in RViz.
                // The legacy stream already arrives with lateral on the width axis, so it is
                // uploaded as-is.
                tex_cols = layout.rows;   // lateral
                tex_rows = layout.cols;   // forward
                for( int r = 0; r < tex_rows; ++r )
                {
                    const int cx = layout.cols - 1 - r;                  // far row first
                    for( int c = 0; c < tex_cols; ++c )
                    {
                        const int cy = layout.rows - 1 - c;              // +Y on the left
                        vec[static_cast< size_t >( r ) * tex_cols + c]
                            = level_lut[layout.cells[static_cast< size_t >( cy ) * layout.cols + cx]];
                    }
                }
            }
            else
            {
                for( size_t i = 0; i < layout.cell_count; ++i )
                    vec[i] = level_lut[layout.cells[i]];
            }
        }
    }  // namespace

    void texture_buffer::upload_occupancy_frame( const rs2::frame & frame, const void * data )
    {
        const occupancy_layout layout = resolve_occupancy_layout( frame, data );
        if( ! layout.valid )
            return;

        std::vector< uint8_t > vec;
        int tex_cols, tex_rows;
        decode_occupancy_cells( layout, vec, tex_cols, tex_rows );

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
}  // namespace rs2
