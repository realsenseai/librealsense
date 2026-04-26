// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <unordered_set>
#include "model-views.h"
#include "device-model.h"
#include "subdevice-model.h"
#include "stream-model.h"
#include "post-processing-filters.h"
#include "notifications.h"
#include "skybox.h"
#include "measurement.h"
#include "updates-model.h"
#include "bag-conversion-helper.h"
#include <librealsense2/hpp/rs_export.hpp>

namespace rs2
{
    struct popup
    {
        std::string header;
        std::string message;
        std::function< void() > custom_command;

        bool operator==( const popup & p ) const
        {
            return p.message == message;
        }
    };

    class viewer_model;

    class frameset_allocator : public filter
    {
    public:
        frameset_allocator(viewer_model* viewer);
    private:
        viewer_model* owner;
    };

    struct export_model
    {
        template<typename T, size_t sz>
        static export_model make_exporter(std::string name, std::string extension, T (&filters_str)[sz])
        {
            return export_model(name, extension, filters_str, sz);

        }
        std::string name;
        std::string extension;
        std::vector<char> filters;
        std::map<rs2_option, int> options;

    private:
        export_model(std::string name, std::string extension, const char* filters_str, size_t filters_size) : name(name),
            extension(extension), filters(filters_str, filters_str + filters_size) {};
    };

    class viewer_model
    {
        bool _disable_log_to_console = false;

    public:
        void reset_camera(float3 pos = { 0.0f, 0.0f, -1.0f });

        void update_configuration(config_file* new_cfg = nullptr);

        const float panel_width = 340.f;
        const float panel_y = 50.f;

        float get_output_height() const { return (float)(not_model->output.get_output_height()); }
        float get_dashboard_width() const { return (float)(not_model->output.get_dashboard_width()); }

        rs2::frame handle_ready_frames(const rect& viewer_rect, ux_window& window, int devices, std::string& error_message);

        ~viewer_model()
        {
            // Stopping post processing filter rendering thread
            ppf.stop();
            streams.clear();
        }

        void begin_stream(std::shared_ptr<subdevice_model> d, rs2::stream_profile p);

        std::shared_ptr<texture_buffer> get_last_texture();

        std::vector<frame> get_frames(frame set);
        frame get_3d_depth_source(frame f);
        frame get_3d_texture_source(frame f);

        bool is_3d_depth_source(frame f);
        bool is_3d_texture_source(frame f) const;

        std::shared_ptr<texture_buffer> upload_frame(frame&& f);

        std::map<int, rect> calc_layout(const rect& r);

        void show_no_stream_overlay(ImFont* font, int min_x, int min_y, int max_x, int max_y);
        void show_no_device_overlay(ImFont* font, int min_x, int min_y);
        void show_rendering_not_supported(ImFont* font_18, int min_x, int min_y, int max_x, int max_y, rs2_format format);

        void show_paused_icon(ImFont* font, int x, int y, int id);
        void show_recording_icon(ImFont* font_18, int x, int y, int id, float alpha_delta);

        void popup_if_error(const ux_window& window, std::string& error_message);

        void show_popup(const ux_window& window, const popup& p);

        void popup_firmware_update_progress(const ux_window& window, const float progress);

        void try_select_pointcloud(ux_window& win);

        void show_3dviewer_header(ux_window& window, rs2::rect stream_rect, bool& paused, std::string& error_message);

        void update_3d_camera(ux_window& win, const rect& viewer_rect, bool force = false);

        // Check if we should render the current frame (sometimes we have information only frames and the frame data itself has no visual affect)
        bool should_render_frame(const rs2::stream_model& model) const;

        void show_top_bar(ux_window& window, const rect& viewer_rect, const device_models_list& devices);

        void render_3d_view(const rect& view_rect, ux_window& win,
            std::shared_ptr<texture_buffer> texture, rs2::points points, rs2::labeled_points);

        void render_2d_view(const rect& view_rect, ux_window& win, int output_height,
            ImFont *font1, ImFont *font2, size_t dev_model_num, const mouse_info &mouse, std::string& error_message);

        void gc_streams();

        bool is_option_skipped(rs2_option opt) const;

        void disable_measurements();

        std::mutex streams_mutex;
        std::map<int, stream_model> streams;
        std::map<int, int> streams_origin;
        bool fullscreen = false;
        stream_model* selected_stream = nullptr;
        std::shared_ptr<syncer_model> syncer;
        post_processing_filters ppf;

        context &ctx;
        std::shared_ptr<notifications_model> not_model = std::make_shared<notifications_model>();
        bool is_3d_view = false;
        bool paused = false;
        bool metric_system = true;
        uint32_t ground_truth_r = 1200;

        enum export_type
        {
            ply
        };
        std::map<export_type, export_model> exporters;
        frameset_allocator frameset_alloc;

        void draw_viewport(const rect& viewer_rect,
            ux_window& window, int devices, std::string& error_message,
            std::shared_ptr<texture_buffer> texture, rs2::points  f = rs2::points(), 
            rs2::labeled_points lp = rs2::labeled_points());

        bool allow_3d_source_change = true;
        bool allow_stream_close = true;

        std::array<float3, 4> roi_rect;
        bool draw_plane = false;

        bool draw_frustrum = true;
        bool support_non_syncronized_mode = true;
        std::atomic<bool> synchronization_enable;
        std::atomic<bool> synchronization_enable_prev_state;

        int selected_depth_source_uid = -1;
        int selected_labeled_points_source_uid = -1;
        int selected_tex_source_uid = -1;
        std::vector<int> last_tex_sources;
        double texture_update_time = 0.0;

        enum class shader_type
        {
            points,
            flat,
            diffuse
        };
        shader_type selected_shader = shader_type::diffuse;

        enum class lpc_points_size
        {
            lpc_small,
            lpc_medium,
            lpc_large
        };
        lpc_points_size selected_lpc_points_size = lpc_points_size::lpc_small;
        bool show_safety_zones_3d = true;

        float dim_level = 1.f;

        bool continue_with_current_fw = false;

        bool select_3d_source = false;
        bool select_tex_source = false;
        bool select_shader_source = false;
        bool show_help_screen = false;
        bool occlusion_invalidation = true;
        bool glsl_available = false;
        bool modal_notification_on = false; // a notification which was expanded
        bool select_lpc_point_size = false;

        press_button_model grid_object_button{ textual_icons::codepen, textual_icons::codepen,
            "Configure Grid", "Configure Grid", false };

        viewer_model(context &ctx_, bool disable_log_to_console = false );

        std::shared_ptr<updates_model> updates;

        std::shared_ptr<bag_conversion_helper> bag_converter = std::make_shared<bag_conversion_helper>();
        std::unordered_set<int> _hidden_options;
        bool _support_ir_reflectivity;
    private:

        // Process any object detection frames in last_frames alongside their color and depth
        // counterparts, updating detected_objects with UV-mapped, depth-sampled results.
        void process_object_detection_frames( std::map< int, rs2::frame > & last_frames );

        void check_permissions();
        void hide_common_options();
        std::vector<popup> _active_popups;

        struct rgb {
            uint32_t r, g, b;
        };

        struct rgb_per_distance {
            float depth_val;
            rgb rgb_val;
        };

        friend class post_processing_filters;
        std::map<int, rect> get_interpolated_layout(const std::map<int, rect>& l);
        void show_icon(ImFont* font_18, const char* label_str, const char* text, int x, int y,
                       int id, const ImVec4& color, const std::string& tooltip = "");
        void draw_color_ruler(const mouse_info& mouse,
                              const stream_model& s_model,
                              const rect& stream_rect,
                              std::vector<rgb_per_distance> rgb_per_distance_vec,
                              float ruler_length,
                              const std::string& ruler_units);
        float calculate_ruler_max_distance(const std::vector<float>& distances) const;

        void set_export_popup(ImFont* large_font, ImFont* font, rect stream_rect, std::string& error_message, config_file& temp_cfg);
        void init_depth_uid(int& selected_depth_source, std::vector<std::string>& depth_sources_str, std::vector<int>& depth_sources);
        void init_labeled_points_uid();
        void draw_3d_labeled_points(const rect& viewer_rect, rs2::labeled_points labeled_points);
        bool should_texture_frame_be_updated(const rs2::frame& f) const;

        streams_layout _layout;
        streams_layout _old_layout;
        std::chrono::high_resolution_clock::time_point _transition_start_time;

        // 3D-Viewer state
        float3 pos = { 0.0f, 0.0f, -0.5f };
        float3 target = { 0.0f, 0.0f, 0.0f };
        float3 up;
        bool fixed_up = true;

        float view[16];
        GLint texture_border_mode = GL_CLAMP_TO_EDGE;

        rs2::points last_points;
        std::shared_ptr<texture_buffer> last_texture;
        
        rs2::labeled_points last_labeled_points;

        // Infinite pan / rotate feature:
        bool manipulating = false;
        float2 overflow = { 0.f, 0.f };

        rs2::gl::camera_renderer _cam_renderer;
        rs2::gl::pointcloud_renderer _pc_renderer;


        bool _pc_selected = false;


        temporal_event origin_occluded { std::chrono::milliseconds(3000) };

        bool show_skybox = true;
        skybox _skybox;

        measurement _measurements;

        typedef enum class Zone { Danger, Warning, Diagnostic } Zone;
        void set_polygon_color(Zone zone);
        std::vector<vertex> init_zone(Zone zone, const frame& frame, float scale_factor);
        void draw_zone_2d(Zone zone, const rect& draw_within, const frame& frame);
        void draw_zone_3d(Zone zone, const rs2::labeled_points& frame);
        vertex transform_vertex(vertex v, const rect& normalize_from, const rect& unnormalize_to);
    };
}
