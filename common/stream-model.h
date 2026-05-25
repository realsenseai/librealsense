// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "rect.h"
#include "rendering.h"
#include <imgui.h>
#include "reflectivity/reflectivity.h"
#include <rsutils/number/stabilized-value.h>
#include <graph-model.h>

namespace rs2
{
    class subdevice_model;
    class viewer_model;

    void draw_rect(const rect& r, int line_width = 1, bool draw_cross = false);

    struct frame_metadata
    {
        std::array<std::pair<bool, rs2_metadata_type>, RS2_FRAME_METADATA_COUNT> md_attributes{};
    };

    struct attribute
    {
        std::string name;
        std::string value;
        std::string description;
    };

    bool draw_combo_box(const std::string& id, const std::vector<std::string>& device_names, int& new_index);

    class stream_model
    {
    public:
        stream_model();
        std::shared_ptr<texture_buffer> upload_frame(frame&& f);
        bool is_stream_visible() const;
        void update_ae_roi_rect(const rect& stream_rect, const mouse_info& mouse, std::string& error_message);
        void show_frame(const rect& stream_rect, const mouse_info& g, std::string& error_message);
        rect get_normalized_zoom(const rect& stream_rect, const mouse_info& g, bool is_middle_clicked, float zoom_val);

        bool is_stream_alive();
        void show_stream_footer(ImFont* font, const rect &stream_rect, const mouse_info& mouse, const std::map<int, stream_model> &streams, viewer_model& viewer);
        void show_stream_header(ImFont* font, const rect& stream_rect, viewer_model& viewer);
        float show_stream_imu( ImFont * font,
                               const rect & stream_rect,
                               const rs2_vector & axis,
                               const mouse_info & mouse,
                               char const * units,
                               char const * title = nullptr,
                               float y_offset = 0.f );
        void show_stream_pose(ImFont* font, const rect& stream_rect, const rs2_pose& pose_data,
            rs2_stream stream_type, bool fullScreen, float y_offset, viewer_model& viewer);

        void snapshot_frame(const char* filename,viewer_model& viewer) const;

        void draw_stream_metadata( const double timestamp,
                                  const rs2_timestamp_domain timestamp_domain,
                                  const unsigned long long frame_number,
                                  stream_profile profile,
                                  rs2::float2 original_size,
                                  const rect& stream_rect );

        // This function fill details with data
        void create_stream_details( std::vector< attribute >& stream_details,
                                    const double timestamp,
                                    const rs2_timestamp_domain timestamp_domain,
                                    unsigned long long frame_number,
                                    stream_profile profile,
                                    rs2::float2 original_size );

        void begin_stream(std::shared_ptr<subdevice_model> d, rs2::stream_profile p, const viewer_model& viewer);
        bool draw_reflectivity(int x, int y, rs2::depth_sensor ds, const std::map<int, stream_model> &streams, std::stringstream &ss, bool same_line = false);
        rect layout;
        std::shared_ptr<texture_buffer> texture;
        float2 size;
        float2 original_size;
        rect get_stream_bounds() const { return { 0, 0, size.x, size.y };}
        rect get_original_stream_bounds() const { return{ 0, 0, original_size.x, original_size.y };}
        stream_profile original_profile;
        stream_profile profile;
        std::chrono::high_resolution_clock::time_point last_frame;
        double              timestamp = 0.0;
        unsigned long long  frame_number = 0;
        rs2_timestamp_domain timestamp_domain = RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME;
        fps_calc            fps, view_fps;
        int                 count = 0;
        rect                roi_display_rect{};
        float               roi_percentage = 0.4f;
        frame_metadata      frame_md;
        bool                capturing_roi       = false;    // active modification of roi
        std::shared_ptr<subdevice_model> dev;
        float _frame_timeout = RS2_DEFAULT_TIMEOUT;
        float _min_timeout = 167.0f;

        bool _mid_click = false;
        float2 _middle_pos{0, 0};
        rect _normalized_zoom{0, 0, 1, 1};
        int color_map_idx = 1;
        bool show_stream_details = false;
        rect curr_info_rect{};
        temporal_event _stream_not_alive;
        bool show_map_ruler = true;
        bool show_metadata = false;
        bool show_safety_zones_2d = true;

#ifdef BUILD_VIEWPORT_GRID_OVERLAY
        bool show_grid     = false;
        int  grid_h_lines  = 1;
        int  grid_v_lines  = 1;
        int  grid_line_width = 1;
        int  grid_color_r  = 255;
        int  grid_color_g  = 255;
        int  grid_color_b  = 255;
#endif

        std::shared_ptr<graph_model> graph;
        bool show_graph = false;
        bool graph_initialized = false;

        animated<float> _info_height{ 0.f };
        int _prev_mouse_pos_x = 0;
        int _prev_mouse_pos_y = 0;

    private:
        std::unique_ptr< reflectivity > _reflectivity;
        rsutils::number::stabilized_value<float> _stabilized_reflectivity;

        std::string smcu_internal_state_to_string(rs2_metadata_type& attribute_val) const;
        std::string format_value(rs2_frame_metadata_value& md_val, rs2_metadata_type& attribute_val) const;
        bool should_show_in_hex(rs2_frame_metadata_value& md_val) const;
        void create_graph(rs2_stream stream_type);
        void show_metadata_by_default(const rs2::stream_profile& p);
        void add_d585S_metadata_descriptions(std::map<rs2_frame_metadata_value, std::string>& descriptions) const;
        std::string adapt_d585S_metadata_name(const std::string& name) const;
        void add_dds_metadata_descriptions(std::map<rs2_frame_metadata_value, std::string>& descriptions) const;
        void deal_d585S_metadata_md_values_special_cases(const frame& f);
        std::string get_meaning(const rs2_frame_metadata_value& md_val, const std::vector<std::string>& reasons, const std::string& reason_for_zero = "") const;
    };

    
}
