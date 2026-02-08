// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "post-processing-filters-list.h"
#include "post-processing-block-model.h"
#include <imgui_internal.h>
#include <realsense_imgui.h>

#include "metadata-helper.h"
#include "subdevice-model.h"

namespace rs2
{
    std::vector<const char*> get_string_pointers(const std::vector<std::string>& vec)
    {
        std::vector<const char*> res;
        for (auto&& s : vec) res.push_back(s.c_str());
        return res;
    }

    std::string get_post_processing_device_sensor_name(subdevice_model* sub)
    {
        std::stringstream ss;
        ss << configurations::viewer::post_processing
            << "." << sub->dev.get_info(RS2_CAMERA_INFO_NAME)
            << "." << sub->s->get_info(RS2_CAMERA_INFO_NAME);
        return ss.str();
    }

    void subdevice_model::populate_options( const std::string & opt_base_label,
                                            bool * options_invalidated,
                                            std::string & error_message )
    {
        try
        {
            for( rs2::option_value option : s->get_supported_option_values() )
            {
                options_metadata[option->id]
                    = create_option_model( option, opt_base_label, this, s, options_invalidated, error_message );
            }

            s->on_options_changed( [this]( const options_list & list )
            {
                for( auto changed_option : list )
                {
                    auto it = options_metadata.find( changed_option->id );
                    if( it != options_metadata.end() && ! _destructing ) // Callback runs in different context, check options_metadata still valid
                    {
                        it->second.update_value( changed_option, *viewer.not_model );
                    }
                }
            } );
        }
        catch( const std::exception & e )
        {
            if( viewer.not_model )
                viewer.not_model->add_log( e.what(), RS2_LOG_SEVERITY_WARN );
        }
    }

    subdevice_model::subdevice_model(
        device& dev,
        std::shared_ptr<sensor> s,
        std::shared_ptr< atomic_objects_in_frame > device_detected_objects,
        std::string& error_message,
        viewer_model& viewer,
        device_model* dev_model,
        bool new_device_connected
    )
        : s(s), dev(dev), ui(), last_valid_ui(), dev_model(dev_model),
        streaming(false), _pause(false),
        depth_colorizer(std::make_shared<rs2::gl::colorizer>()),
        yuy2rgb(std::make_shared<rs2::gl::yuy_decoder>()),
        m420_to_rgb(std::make_shared<rs2::gl::m420_decoder>()),
        y411(std::make_shared<rs2::gl::y411_decoder>()),
        viewer(viewer),
        detected_objects(device_detected_objects),
        _destructing( false )
    {
        supported_options = s->get_supported_options();
        restore_processing_block("colorizer", depth_colorizer);
        restore_processing_block("yuy2rgb", yuy2rgb);
        restore_processing_block("m420_to_rgb", m420_to_rgb);
        restore_processing_block("y411", y411);

        post_processing_enabled = is_post_processing_enabled_in_config_file();

        try
        {
            if (s->supports(RS2_OPTION_ENABLE_AUTO_EXPOSURE))
                auto_exposure_enabled = s->get_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE) > 0;
        }
        catch (...)
        {

        }

        try
        {
            if (s->supports(RS2_OPTION_DEPTH_UNITS))
                depth_units = s->get_option(RS2_OPTION_DEPTH_UNITS);
        }
        catch (...) {}

        try
        {
            if (s->supports(RS2_OPTION_STEREO_BASELINE))
                stereo_baseline = s->get_option(RS2_OPTION_STEREO_BASELINE);
        }
        catch (...) {}

        bool const is_rgb_camera = s->is< color_sensor >();

        for (auto&& f : s->get_recommended_filters())
        {
            auto shared_filter = std::make_shared<filter>(f);
            auto model = std::make_shared<processing_block_model>(
                this, shared_filter->get_info(RS2_CAMERA_INFO_NAME), shared_filter,
                [=](rs2::frame f) { return shared_filter->process(f); }, error_message);

            if (shared_filter->is<hole_filling_filter>())
                model->enable(false);

            if (shared_filter->is<sequence_id_filter>())
                model->enable(false);

            if (shared_filter->is<decimation_filter>())
            {
                if (is_rgb_camera)
                    model->enable(false);
            }

            if( shared_filter->is< rotation_filter >() )
                model->enable( false ); 

            if (shared_filter->is<threshold_filter>())
            {
                if (s->supports(RS2_CAMERA_INFO_PRODUCT_ID))
                {
                    // using short range for D405
                    std::string device_pid = s->get_info(RS2_CAMERA_INFO_PRODUCT_ID);
                    if (device_pid == "0B5B")
                    {
                        std::string error_msg;
                        auto threshold_pb = shared_filter->as<threshold_filter>();
                        threshold_pb.set_option(RS2_OPTION_MIN_DISTANCE, SHORT_RANGE_MIN_DISTANCE);
                        threshold_pb.set_option(RS2_OPTION_MAX_DISTANCE, SHORT_RANGE_MAX_DISTANCE);
                    }
                }
                model->enable( false ); 
            }

            if (shared_filter->is<hdr_merge>())
            {
                // processing block will be skipped if the requested option is not supported
                auto supported_options = s->get_supported_options();
                if (std::find(supported_options.begin(), supported_options.end(), RS2_OPTION_SEQUENCE_ID) == supported_options.end())
                    continue;
            }

            post_processing.push_back(model);
        }

        for (auto&& f : s->query_embedded_filters())
        {
            auto shared_filter = std::make_shared<embedded_filter>(f);

            auto model = std::make_shared<embedded_filter_model>(
                this, shared_filter->get_type(), shared_filter, viewer, error_message);

            embedded_filters.push_back(model);
        }

        if (is_rgb_camera)
        {
            for (auto& create_filter : post_processing_filters_list::get())
            {
                auto filter = create_filter();
                if (!filter)
                    continue;
                filter->start(*this);
                std::shared_ptr< processing_block_model > model(
                    new post_processing_block_model{
                        this, filter,
                        [=](rs2::frame f) { return filter->process(f); },
                        error_message
                    });
                post_processing.push_back(model);
            }
        }

        auto colorizer = std::make_shared<processing_block_model>(
            this, "Depth Visualization", depth_colorizer,
            [=](rs2::frame f) { return depth_colorizer->colorize(f); }, error_message);
        const_effects.push_back(colorizer);


        if (s->supports(RS2_CAMERA_INFO_PRODUCT_ID))
        {
            std::string device_pid = s->get_info(RS2_CAMERA_INFO_PRODUCT_ID);

            // using short range for D405
            if (device_pid == "0B5B")
            {
                std::string error_msg;
                depth_colorizer->set_option(RS2_OPTION_MIN_DISTANCE, SHORT_RANGE_MIN_DISTANCE);
                depth_colorizer->set_option(RS2_OPTION_MAX_DISTANCE, SHORT_RANGE_MAX_DISTANCE);
            }
        }

        // Hack to restore "Enable Histogram Equalization" flag if needed.
        // The flag is set to true by colorizer constructor, but setting min/max_distance options above or during
        // restore_processing_block earlier, causes the registered observer to unset it, which is not the desired
        // behaviour. Observer should affect only if a user is setting the values after construction phase is over.
        if (depth_colorizer->supports(RS2_OPTION_VISUAL_PRESET))
        {
            auto option_value = depth_colorizer->get_option(RS2_OPTION_VISUAL_PRESET);
            depth_colorizer->set_option(RS2_OPTION_VISUAL_PRESET, option_value);
        }

        std::stringstream ss;
        ss << "##" << dev.get_info(RS2_CAMERA_INFO_NAME)
            << "/" << s->get_info(RS2_CAMERA_INFO_NAME)
            << "/" << (long long)this;

        if (s->supports(RS2_CAMERA_INFO_PHYSICAL_PORT) && dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE))
        {
            std::string product = dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
            std::string id = s->get_info(RS2_CAMERA_INFO_PHYSICAL_PORT);

            bool has_metadata = !rs2::metadata_helper::instance().can_support_metadata(product)
                || rs2::metadata_helper::instance().is_enabled(id);
            static bool showed_metadata_prompt = false;

            if (!has_metadata && !showed_metadata_prompt)
            {
                auto n = std::make_shared<metadata_warning_model>();
                viewer.not_model->add_notification(n);
                showed_metadata_prompt = true;
            }
        }

        try
        {
            auto sensor_profiles = s->get_stream_profiles();
            reverse(begin(sensor_profiles), end(sensor_profiles));
            std::map<int, rs2_format> def_format{ {0, RS2_FORMAT_ANY} };
            auto default_resolution = std::make_pair(1280, 720);
            auto default_fps = 30;
            for (auto&& profile : sensor_profiles)
            {
                std::stringstream res;
                if (auto vid_prof = profile.as<video_stream_profile>())
                {
                    if (profile.is_default())
                    {
                        default_resolution = std::pair<int, int>(vid_prof.width(), vid_prof.height());
                        default_fps = profile.fps();

                        if (is_rgb_camera)
                        {
                            auto intrinsics = vid_prof.get_intrinsics();
                            if (intrinsics.model == RS2_DISTORTION_INVERSE_BROWN_CONRADY
                                && (std::abs(intrinsics.coeffs[0]) > std::numeric_limits< float >::epsilon() ||
                                    std::abs(intrinsics.coeffs[1]) > std::numeric_limits< float >::epsilon() ||
                                    std::abs(intrinsics.coeffs[2]) > std::numeric_limits< float >::epsilon() ||
                                    std::abs(intrinsics.coeffs[3]) > std::numeric_limits< float >::epsilon() ||
                                    std::abs(intrinsics.coeffs[4]) > std::numeric_limits< float >::epsilon()))
                            {
                                uvmapping_calib_full = true;
                            }
                        }
                    }
                    res << vid_prof.width() << " x " << vid_prof.height();
                    push_back_if_not_exists(res_values, std::pair<int, int>(vid_prof.width(), vid_prof.height()));
                    push_back_if_not_exists(resolutions, res.str());
                    push_back_if_not_exists(resolutions_per_stream[profile.stream_type()], std::pair<int, int>(vid_prof.width(), vid_prof.height()));
                }

                std::stringstream fps;
                fps << profile.fps();
                push_back_if_not_exists(fps_values_per_stream[profile.unique_id()], profile.fps());
                push_back_if_not_exists(shared_fps_values, profile.fps());
                push_back_if_not_exists(fpses_per_stream[profile.unique_id()], fps.str());
                push_back_if_not_exists(shared_fpses, fps.str());
                stream_display_names[profile.unique_id()] = profile.stream_name();

                std::string format = rs2_format_to_string(profile.format());

                push_back_if_not_exists(formats[profile.unique_id()], format);
                push_back_if_not_exists(format_values[profile.unique_id()], profile.format());

                if (profile.is_default())
                {
                    stream_enabled[profile.unique_id()] = true;
                    def_format[profile.unique_id()] = profile.format();
                }

                profiles.push_back(profile);
            }
            auto any_stream_enabled = std::any_of(std::begin(stream_enabled), std::end(stream_enabled), [](const std::pair<int, bool>& p) { return p.second; });
            if (!any_stream_enabled)
            {
                if (sensor_profiles.size() > 0)
                    stream_enabled[sensor_profiles.rbegin()->unique_id()] = true;
            }

            for (auto&& fps_list : fps_values_per_stream)
            {
                sort_together(fps_list.second, fpses_per_stream[fps_list.first]);
            }
            sort_together(shared_fps_values, shared_fpses);
            for (auto&& res_list : resolutions_per_stream)
            {
                sort_resolutions(res_list.second);
            }
            sort_together(res_values, resolutions);

            show_single_fps_list = is_there_common_fps();

            int selection_index{};

            if (!show_single_fps_list)
            {
                for (auto fps_array : fps_values_per_stream)
                {
                    if (get_default_selection_index(fps_array.second, default_fps, &selection_index))
                    {
                        ui.selected_fps_id[fps_array.first] = selection_index;
                        break;
                    }
                }
            }
            else
            {
                if (get_default_selection_index(shared_fps_values, default_fps, &selection_index))
                    ui.selected_shared_fps_id = selection_index;
            }

            for (auto format_array : format_values)
            {
                if (get_default_selection_index(format_array.second, def_format[format_array.first], &selection_index))
                {
                    ui.selected_format_id[format_array.first] = selection_index;
                }
            }

            if (is_multiple_resolutions_supported())
            {
                ui.is_multiple_resolutions = true;
                for (auto res_array : resolutions_per_stream)
                {
                    ui.selected_stream_to_res[res_array.first] = default_resolution;
                }
            }
            else
            {
                get_default_selection_index(res_values, default_resolution, &selection_index);
                ui.selected_res_id = selection_index;
            }

            if (ui.is_multiple_resolutions)
            {
                for (auto it = ui.selected_stream_to_res.begin(); it != ui.selected_stream_to_res.end(); ++it)
                {
                    if (!is_selected_combination_supported())
                    {
                        auto cur_stream = it->first;
                        auto resolutions_for_current_stream = resolutions_per_stream[cur_stream];
                        if (resolutions_for_current_stream.size() == 0)
                            throw std::runtime_error("Multiple Resolution Issue, please check your requested resolution");

                        auto res_it = resolutions_for_current_stream.end() - 1;
                        ui.selected_stream_to_res[cur_stream] = *res_it;

                        while (res_it->first && !is_selected_combination_supported())
                            --res_it;
                    }
                }
            }
            else
            {
                while (ui.selected_res_id >= 0 && !is_selected_combination_supported())
                    ui.selected_res_id--;
            }
            last_valid_ui = ui;
        }
        catch (const error& e)
        {
            error_message = error_to_string(e);
        }
        populate_options(ss.str().c_str(), &_options_invalidated, error_message);

    }

    subdevice_model::~subdevice_model()
    {
        _destructing = true;
        wait_for_stop();
        try
        {
            s->on_options_changed( []( const options_list & list ) {} );
        }
        catch( ... )
        {
        }
    }

    bool subdevice_model::is_post_processing_enabled_in_config_file() const
    {
        bool is_enabled = false;

        std::string device_name(dev.get_info(RS2_CAMERA_INFO_NAME));
        std::string sensor_name(s->get_info(RS2_CAMERA_INFO_NAME));

        std::stringstream ss;
        ss << configurations::viewer::post_processing
            << "." << device_name
            << "." << sensor_name;
        auto key = ss.str();

        if (config_file::instance().contains(key.c_str()))
        {
            is_enabled = config_file::instance().get(key.c_str());
        }
        return is_enabled;
    }

    void subdevice_model::sort_resolutions(std::vector<std::pair<int, int>>& resolutions) const
    {
        std::sort(resolutions.begin(), resolutions.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                if (a.first != b.first)
                    return (a.first < b.first);
                return (a.second <= b.second);
            });
    }

    bool subdevice_model::is_there_common_fps()
    {
        std::vector<int> first_fps_group;
        size_t group_index = 0;
        for (; group_index < fps_values_per_stream.size(); ++group_index)
        {
            if (!fps_values_per_stream[(rs2_stream)group_index].empty())
            {
                first_fps_group = fps_values_per_stream[(rs2_stream)group_index];
                break;
            }
        }

        for (size_t i = group_index + 1; i < fps_values_per_stream.size(); ++i)
        {
            auto fps_group = fps_values_per_stream[(rs2_stream)i];
            if (fps_group.empty())
                continue;

            auto fps1 = first_fps_group[0];
            auto it = std::find_if( std::begin( fps_group ),
                                    std::end( fps_group ),
                                    [&]( const int & fps2 ) { return fps2 == fps1; } );
            if( it == std::end( fps_group ) )
                return false;
        }
        return true;
    }

    bool subdevice_model::draw_resolutions(std::string& error_message, std::string& label, std::function<void()> streaming_tooltip, float col0, float col1)
    {
        bool res = false;

        // Draw combo-box with all resolution options for this device
        auto res_chars = get_string_pointers(resolutions);
        if (res_chars.size() > 0)
        {
            ImGui::Text("Resolution:");
            streaming_tooltip();
            ImGui::SameLine(); ImGui::SetCursorPosX(col1);

            label = rsutils::string::from() << "##" << dev.get_info(RS2_CAMERA_INFO_NAME)
                << s->get_info(RS2_CAMERA_INFO_NAME) << " resolution";

            if (!allow_change_resolution_while_streaming && streaming)
            {
                ImGui::Text("%s", res_chars[ui.selected_res_id]);
                streaming_tooltip();
            }
            else
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 25 ); // Set the width for the combo box itself with a 25 buffer 
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1,1,1,1 });
                auto tmp_selected_res_id = ui.selected_res_id;
                if (RsImGui::CustomComboBox(label.c_str(), &tmp_selected_res_id, res_chars.data(),
                    static_cast<int>(res_chars.size())))
                {
                    res = true;
                    _options_invalidated = true;

                    ui.selected_res_id = tmp_selected_res_id;

                }
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();
            }
            ImGui::SetCursorPosX(col0);
        }
        return res;
    }

    bool subdevice_model::draw_fps(std::string& error_message, std::string& label, std::function<void()> streaming_tooltip, float col0, float col1)
    {
        bool res = false;
        // FPS
        if (show_single_fps_list)
        {
            auto fps_chars = get_string_pointers(shared_fpses);
            ImGui::Text("Frame Rate (FPS):");
            streaming_tooltip();
            ImGui::SameLine(); ImGui::SetCursorPosX(col1);

            label = rsutils::string::from()
                << "##" << dev.get_info(RS2_CAMERA_INFO_NAME) << s->get_info(RS2_CAMERA_INFO_NAME) << " fps";

            if (!allow_change_fps_while_streaming && streaming)
            {
                ImGui::Text("%s", fps_chars[ui.selected_shared_fps_id]);
                streaming_tooltip();
            }
            else
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 25); // Set the width for the combo box itself with a 25 buffer 
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1,1,1,1 });
                if (RsImGui::CustomComboBox(label.c_str(), &ui.selected_shared_fps_id, fps_chars.data(),
                    static_cast<int>(fps_chars.size())))
                {
                    res = true;
                }
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();
            }

            ImGui::SetCursorPosX(col0);
        }
        return res;
    }

    bool subdevice_model::draw_streams_and_formats(std::string& error_message, std::string& label, std::function<void()> streaming_tooltip, float col0, float col1)
    {
        bool res = false;

        if (!streaming)
        {
            ImGui::Text("Available Streams:");
        }

        // Draw combo-box with all format options for current device
        for (auto&& f : formats)
        {
            // Format
            if (f.second.size() == 0)
                continue;

            auto formats_chars = get_string_pointers(f.second);
            if (!streaming || (streaming && stream_enabled[f.first]))
            {
                if (streaming)
                {
                    label = rsutils::string::from()
                        << stream_display_names[f.first] << (show_single_fps_list ? "" : " stream:");
                    ImGui::Text("%s", label.c_str());
                    streaming_tooltip();
                }
                else
                {
                    auto tmp = stream_enabled;
                    label = rsutils::string::from() << stream_display_names[f.first] << "##" << f.first;
                    if (ImGui::Checkbox(label.c_str(), &stream_enabled[f.first]))
                    {
                        prev_stream_enabled = tmp;
                    }
                }
            }

            if (stream_enabled[f.first])
            {
                if (show_single_fps_list)
                {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(col1);
                }

                label = rsutils::string::from()
                    << "##" << dev.get_info(RS2_CAMERA_INFO_NAME) << s->get_info(RS2_CAMERA_INFO_NAME) << " "
                    << f.first << " format";

                if (!show_single_fps_list)
                {
                    ImGui::Text("Format:");
                    streaming_tooltip();
                    ImGui::SameLine(); ImGui::SetCursorPosX(col1);
                }

                if (streaming)
                {
                    ImGui::Text("%s", formats_chars[ui.selected_format_id[f.first]]);
                    streaming_tooltip();
                }
                else
                {
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 25); // Set the width for the combo box itself with a 25 buffer 
                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1,1,1,1 });
                    RsImGui::CustomComboBox(label.c_str(), &ui.selected_format_id[f.first], formats_chars.data(),
                        static_cast<int>(formats_chars.size()));
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                }
                ImGui::SetCursorPosX(col0);
                // FPS
                // Draw combo-box with all FPS options for this device
                if (!show_single_fps_list && !fpses_per_stream[f.first].empty() && stream_enabled[f.first])
                {
                    auto fps_chars = get_string_pointers(fpses_per_stream[f.first]);
                    ImGui::Text("Frame Rate (FPS):");
                    streaming_tooltip();
                    ImGui::SameLine(); ImGui::SetCursorPosX(col1);

                    label = rsutils::string::from() << "##" << s->get_info(RS2_CAMERA_INFO_NAME)
                        << s->get_info(RS2_CAMERA_INFO_NAME) << f.first << " fps";

                    if (streaming)
                    {
                        ImGui::Text("%s", fps_chars[ui.selected_fps_id[f.first]]);
                        streaming_tooltip();
                    }
                    else
                    {
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 25); // Set the width for the combo box itself with a 25 buffer 
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1,1,1,1 });
                        RsImGui::CustomComboBox(label.c_str(), &ui.selected_fps_id[f.first], fps_chars.data(),
                            static_cast<int>(fps_chars.size()));
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();
                    }
                    ImGui::SetCursorPosX(col0);
                }
            }
        }

        return res;
    }

    int subdevice_model::get_res_id_in_resolutions_array(const std::vector<const char*>& res_chars, const std::pair<int, int>& res) const
    {
        int id = -1;
        std::stringstream ss;
        ss << res.first << "x" << res.second;
        for (int i = 0; i < res_chars.size(); ++i)
        {
            auto cur_res = std::string(res_chars[i]);
            if (cur_res == ss.str())
            {
                id = i;
                break;
            }
        }
        if (id == -1)
            throw std::runtime_error("Multiple Resolution Issue, please check the requested resolution");

        return id;
    }

    std::pair<int, int> subdevice_model::get_resolution_from_res_chars_id(const std::vector<const char*>& res_chars, int id_in_res_chars) const
    {
        std::string res_str = res_chars[id_in_res_chars];
        std::pair<int, int> res;
        auto width_str = res_str.substr(0, res_str.find('x'));
        auto height_str = res_str.substr(res_str.find('x') + 1, res_str.size());
        res.first = std::atoi(width_str.c_str());
        res.second = std::atoi(height_str.c_str());

        return res;
    }

    bool subdevice_model::draw_resolutions_combo_box_multiple_resolutions(std::string& error_message, std::string& label, std::function<void()> streaming_tooltip, float col0, float col1,
        rs2_stream stream_type)
    {
        bool res = false;

        auto res_pairs = resolutions_per_stream[stream_type];
        std::vector<std::string> resolutions_str;
        for (int i = 0; i < res_pairs.size(); ++i)
        {
            std::stringstream ss;
            ss << res_pairs[i].first << "x" << res_pairs[i].second;
            resolutions_str.push_back(ss.str());
        }

        auto res_chars = get_string_pointers(resolutions_str);

        std::map<const char*, std::pair<int, std::pair<int, int>>> res_char_to_id_and_res;
        for (int i = 0; i < res_chars.size(); ++i)
        {
            res_char_to_id_and_res[res_chars[i]] = std::make_pair(i, res_pairs[i]);
        }
        
        if (res_chars.size() > 0)
        {
            if (!(streaming && !streaming_map[stream_type]))
            {
                // resolution
                // Draw combo-box with all resolution options for this stream type
                ImGui::Text("Resolution:");
                streaming_tooltip();
                ImGui::SameLine(); ImGui::SetCursorPosX(col1);

                label = rsutils::string::from() << "##" << dev.get_info(RS2_CAMERA_INFO_NAME)
                    << s->get_info(RS2_CAMERA_INFO_NAME) << " resolution for " << rs2_stream_to_string(stream_type);

                int id_in_res_chars = get_res_id_in_resolutions_array(res_chars, ui.selected_stream_to_res[stream_type]);
                if (!allow_change_resolution_while_streaming && streaming)
                {
                    ImGui::Text("%s", res_chars[id_in_res_chars]);
                    streaming_tooltip();
                }
                else
                {
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 25); // Set the width for the combo box itself with a 25 buffer 
                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1,1,1,1 });
                    auto tmp_selected_res = ui.selected_stream_to_res[stream_type];

                    if (RsImGui::CustomComboBox(label.c_str(), &id_in_res_chars, res_chars.data(),
                        static_cast<int>(res_chars.size())))
                    {
                        res = true;
                        _options_invalidated = true;
                        
                        ui.selected_stream_to_res[stream_type] = get_resolution_from_res_chars_id(res_chars, id_in_res_chars);

                    }
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                }
            }
            ImGui::SetCursorPosX(col0);
        }

        
        return res;
    }

    bool subdevice_model::draw_formats_combo_box_multiple_resolutions(std::string& error_message, std::string& label, std::function<void()> streaming_tooltip, 
        float col0, float col1, rs2_stream stream_type)
    {
        bool res = false;

        for (auto&& f : formats)
        {
            if (f.second.size() == 0)
                continue;

            if (stream_type == RS2_STREAM_DEPTH && f.second[0] != std::string("Z16") ||
                stream_type == RS2_STREAM_INFRARED && f.second[0] == std::string("Z16"))
                continue;

            auto formats_chars = get_string_pointers(f.second);
            if (!streaming || (streaming && stream_enabled[f.first]))
            {
                if (streaming)
                {
                    label = rsutils::string::from()
                        << stream_display_names[f.first] << (show_single_fps_list ? "" : " stream:");
                    ImGui::Text("%s", label.c_str());
                    streaming_tooltip();
                }
                else
                {
                    res = true;
                    auto tmp = stream_enabled;
                    label = rsutils::string::from() << stream_display_names[f.first] << "##" << f.first;
                    if (ImGui::Checkbox(label.c_str(), &stream_enabled[f.first]))
                    {
                        prev_stream_enabled = tmp;
                    }
                }
            }

            if (stream_enabled[f.first])
            {
                if (show_single_fps_list)
                {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(col1);
                }

                label = rsutils::string::from()
                    << "##" << dev.get_info(RS2_CAMERA_INFO_NAME) << s->get_info(RS2_CAMERA_INFO_NAME) << " "
                    << f.first << " format";

                if (!show_single_fps_list)
                {
                    ImGui::Text("Format:");
                    streaming_tooltip();
                    ImGui::SameLine(); ImGui::SetCursorPosX(col1);
                }

                if (streaming)
                {
                    ImGui::Text("%s", formats_chars[ui.selected_format_id[f.first]]);
                    streaming_tooltip();
                }
                else
                {
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 25); // Set the width for the combo box itself with a 25 buffer 
                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1,1,1,1 });
                    RsImGui::CustomComboBox(label.c_str(), &ui.selected_format_id[f.first], formats_chars.data(),
                        static_cast<int>(formats_chars.size()));
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                }
                ImGui::SetCursorPosX(col0);
            }
        }
        return res;
    }


    bool subdevice_model::draw_res_stream_formats(std::string& error_message, std::string& label, std::function<void()> streaming_tooltip, float col0, float col1)
    {
        bool res = false;

        std::vector<rs2_stream> relevant_streams = { RS2_STREAM_DEPTH, RS2_STREAM_INFRARED };
        for (auto&& stream_type : relevant_streams)
        {
            // resolution
            // Draw combo-box with all resolution options for this stream type
            res |= draw_resolutions_combo_box_multiple_resolutions(error_message, label, streaming_tooltip, col0, col1, stream_type);

            if (draw_streams_selector) 
            {
                // stream and formats
                // Draw combo-box with all format options for current stream type
                res |= draw_formats_combo_box_multiple_resolutions(error_message, label, streaming_tooltip, col0, col1, stream_type);
            }
        }

        return res;
    }
    // The function returns true if one of the configuration parameters changed
    bool subdevice_model::draw_stream_selection(std::string& error_message)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        bool res = false;

        std::string label = rsutils::string::from()
            << "Stream Selection Columns##" << dev.get_info(RS2_CAMERA_INFO_NAME)
            << s->get_info(RS2_CAMERA_INFO_NAME);

        auto streaming_tooltip = [&]() {
            if ((!allow_change_resolution_while_streaming && streaming)
                && ImGui::IsItemHovered())
                RsImGui::CustomTooltip("Can't modify while streaming");
        };

        auto col0 = ImGui::GetCursorPosX();
        auto col1 = 9.f * (float)config_file::instance().get( configurations::window::font_size );

        if (ui.is_multiple_resolutions)
        {
            if (draw_fps_selector)
            {
                res |= draw_fps(error_message, label, streaming_tooltip, col0, col1);
            }

            if (!streaming)
            {
                ImGui::Text("Available Streams:");
            }

            res |= draw_res_stream_formats(error_message, label, streaming_tooltip, col0, col1);
        }
        else
        {
            res |= draw_resolutions(error_message, label, streaming_tooltip, col0, col1);

            if (draw_fps_selector)
            {
                res |= draw_fps(error_message, label, streaming_tooltip, col0, col1);
            }

            if (draw_streams_selector)
            {
                res |= draw_streams_and_formats(error_message, label, streaming_tooltip, col0, col1);
            }
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
        return res;
    }

    bool subdevice_model::is_selected_combination_supported()
    {
        bool enforce_inter_stream_policies = false;
        std::vector<stream_profile> results = get_selected_profiles(enforce_inter_stream_policies);

        if (results.size() == 0)
            return false;
        // Verify that the number of found matches corrseponds to the number of the requested streams
        // TODO - review whether the comparison can be made strict (==)
        return results.size() >= size_t(std::count_if(stream_enabled.begin(), stream_enabled.end(), [](const std::pair<int, bool>& kpv)-> bool { return kpv.second == true; }));
    }

    void subdevice_model::update_ui(std::vector<stream_profile> profiles_vec)
    {
        if (profiles_vec.empty())
            return;
        for (auto& s : stream_enabled)
            s.second = false;
        for (auto& p : profiles_vec)
        {
            stream_enabled[p.unique_id()] = true;

            // update format
            auto format_vec = format_values[p.unique_id()];
            for (int i = 0; i < format_vec.size(); i++)
            {
                if (format_vec[i] == p.format())
                {
                    ui.selected_format_id[p.unique_id()] = i;
                    break;
                }
            }

            // update resolution
            if (!ui.is_multiple_resolutions)
            {
                for (int i = 0; i < res_values.size(); i++)
                {
                    if (auto vid_prof = p.as<video_stream_profile>())
                        if (res_values[i].first == vid_prof.width() && res_values[i].second == vid_prof.height())
                        {
                            ui.selected_res_id = i;
                            break;
                        }
                }
            }
            else
            {
                auto res_vec = resolutions_per_stream[p.stream_type()];
                for (int i = 0; i < res_vec.size(); i++)
                {
                    if (auto vid_prof = p.as<video_stream_profile>())
                        if (res_vec[i].first == vid_prof.width() && res_vec[i].second == vid_prof.height())
                        {
                            ui.selected_stream_to_res[p.stream_type()] = res_vec[i];
                            break;
                        }
                }
            }

            // update fps
            for (int i = 0; i < shared_fps_values.size(); i++)
            {
                if (shared_fps_values[i] == p.fps())
                {
                    ui.selected_shared_fps_id = i;
                    break;
                }
            }
        }
        last_valid_ui = ui;
        prev_stream_enabled = stream_enabled; // prev differs from curr only after user changes
    }

    template<typename T, typename V>
    bool subdevice_model::check_profile(stream_profile p, T cond, std::map<V, std::map<int, stream_profile>>& profiles_map,
        std::vector<stream_profile>& results, V key, int num_streams, stream_profile& def_p)
    {
        bool found = false;
        if (auto vid_prof = p.as<video_stream_profile>())
        {
            for (auto& s : stream_enabled)
            {
                // find profiles that have an enabled stream and match the required condition
                if (s.second == true && vid_prof.unique_id() == s.first && cond(vid_prof))
                {
                    profiles_map[key].insert(std::pair<int, stream_profile>(p.unique_id(), p));
                    if (profiles_map[key].size() == num_streams)
                    {
                        results.clear(); // make sure only current profiles are saved
                        for (auto& it : profiles_map[key])
                            results.push_back(it.second);
                        found = true;
                    }
                    else if (results.empty() && num_streams > 1 && profiles_map[key].size() == num_streams - 1)
                    {
                        for (auto& it : profiles_map[key])
                            results.push_back(it.second);
                    }
                }
                else if (!def_p.get() && cond(vid_prof))
                    def_p = p; // in case no matching profile for current stream will be found, we'll use some profile that matches the condition
            }
        }
        return found;
    }


    void subdevice_model::get_sorted_profiles(std::vector<stream_profile>& profiles)
    {
        auto fps = shared_fps_values[ui.selected_shared_fps_id];
        int width = 0;
        int height = 0;
        std::vector<std::pair<int, int>> selected_resolutions;
        if (!ui.is_multiple_resolutions)
        {
            width = res_values[ui.selected_res_id].first;
            height = res_values[ui.selected_res_id].second;
        }
        else
        {
            for (auto it = resolutions_per_stream.begin(); it != resolutions_per_stream.end(); ++it)
            {
                selected_resolutions.push_back(ui.selected_stream_to_res[it->first]);
            }
        }
        std::sort(profiles.begin(), profiles.end(), [&](stream_profile a, stream_profile b) {
            int score_a = 0, score_b = 0;
            if (a.fps() != fps)
                score_a++;
            if (b.fps() != fps)
                score_b++;

            if (a.format() != format_values[a.unique_id()][ui.selected_format_id[a.unique_id()]])
                score_a++;
            if (b.format() != format_values[b.unique_id()][ui.selected_format_id[b.unique_id()]])
                score_b++;

            auto a_vp = a.as<video_stream_profile>();
            auto b_vp = a.as<video_stream_profile>();
            if (!a_vp || !b_vp)
                return score_a < score_b;

            if (!ui.is_multiple_resolutions)
            {
                if (a_vp.width() != width || a_vp.height() != height)
                    score_a++;
                if (b_vp.width() != width || b_vp.height() != height)
                    score_b++;
            }
            else
            {
                bool a_same_res_found = false;
                bool b_same_res_found = false;
                for (int i = 0; i < selected_resolutions.size(); ++i)
                {
                    if (a_vp.width() == selected_resolutions[i].first && a_vp.height() == selected_resolutions[i].second)
                        a_same_res_found = true;
                    if (b_vp.width() == selected_resolutions[i].first && b_vp.height() == selected_resolutions[i].second)
                        b_same_res_found = true;
                }
                if (!a_same_res_found)
                    score_a++;
                if (!b_same_res_found)
                    score_b++;
            }

            return score_a < score_b;
            });
    }

    std::vector<stream_profile> subdevice_model::get_supported_profiles()
    {
        std::vector<stream_profile> results;
        if (!show_single_fps_list || res_values.size() == 0)
            return results;

        int num_streams = 0;
        for (auto& s : stream_enabled)
            if (s.second == true)
                num_streams++;
        stream_profile def_p;
        auto fps = shared_fps_values[ui.selected_shared_fps_id];
        int width = 0;
        int height = 0;
        std::vector<std::pair<int, int>> selected_resolutions;
        if (!ui.is_multiple_resolutions)
        {
            width = res_values[ui.selected_res_id].first;
            height = res_values[ui.selected_res_id].second;
        }
        else
        {
            for (auto it = resolutions_per_stream.begin(); it != resolutions_per_stream.end(); ++it)
            {
                selected_resolutions.push_back(ui.selected_stream_to_res[it->first]);
            }
        }

        std::vector<stream_profile> sorted_profiles = profiles;

        if (!ui.is_multiple_resolutions && (ui.selected_res_id != last_valid_ui.selected_res_id))
        {
            get_sorted_profiles(sorted_profiles);
            std::map<int, std::map<int, stream_profile>> profiles_by_fps;
            for (auto&& p : sorted_profiles)
            {
                if (check_profile(p, [&](video_stream_profile vsp)
                    { return (vsp.width() == width && vsp.height() == height); },
                    profiles_by_fps, results, p.fps(), num_streams, def_p))
                    break;
            }
        }
        else if (ui.is_multiple_resolutions && (ui.selected_stream_to_res != last_valid_ui.selected_stream_to_res))
        {
            get_sorted_profiles(sorted_profiles);
            std::map<int, std::map<int, stream_profile>> profiles_by_fps;
            for (auto&& p : sorted_profiles)
            {
                if (check_profile(p, [&](video_stream_profile vsp)
                    {
                        bool res = false;
                        std::pair<int, int> cur_res;
                        if (p.stream_type() == RS2_STREAM_DEPTH)
                            cur_res = selected_resolutions[0];
                        else
                            cur_res = selected_resolutions[1];
                        return (vsp.width() == cur_res.first && vsp.height() == cur_res.second);
                    },
                    profiles_by_fps, results, p.fps(), num_streams, def_p))
                    break;
            }
        }
        else if (ui.selected_shared_fps_id != last_valid_ui.selected_shared_fps_id)
        {
            get_sorted_profiles(sorted_profiles);
            std::map<std::tuple<int, int>, std::map<int, stream_profile>> profiles_by_res;

            for (auto&& p : sorted_profiles)
            {
                if (auto vid_prof = p.as<video_stream_profile>())
                {
                    if (check_profile(p, [&](video_stream_profile vsp) { return (vsp.fps() == fps); },
                        profiles_by_res, results, std::make_tuple(vid_prof.width(), vid_prof.height()), num_streams, def_p))
                        break;
                }
            }
        }
        else if (ui.selected_format_id != last_valid_ui.selected_format_id)
        {
            if (num_streams == 0)
            {
                last_valid_ui = ui;
                return results;
            }
            get_sorted_profiles(sorted_profiles);
            std::vector<stream_profile> matching_profiles;
            std::map<std::tuple<int, int, int>, std::map<int, stream_profile>> profiles_by_fps_res; //fps, width, height
            rs2_format format;
            int stream_id;
            // find the stream to which the user made changes
            for (auto& it : ui.selected_format_id)
            {
                if (stream_enabled[it.first])
                {
                    auto last_valid_it = last_valid_ui.selected_format_id.find(it.first);
                    if ((last_valid_it == last_valid_ui.selected_format_id.end() || it.second != last_valid_it->second))
                    {
                        format = format_values[it.first][it.second];
                        stream_id = it.first;
                    }
                }
            }
            for (auto&& p : sorted_profiles)
            {
                if (auto vid_prof = p.as<video_stream_profile>())
                    if (p.unique_id() == stream_id && p.format() == format) // && stream_enabled[stream_id]
                    {
                        profiles_by_fps_res[std::make_tuple(p.fps(), vid_prof.width(), vid_prof.height())].insert(std::pair<int, stream_profile>(p.unique_id(), p));
                        matching_profiles.push_back(p);
                        if (!def_p.get())
                            def_p = p;
                    }
            }
            // take profiles not in matching_profiles with enabled stream and fps+resolution matching some profile in matching_profiles
            for (auto&& p : sorted_profiles)
            {
                if (auto vid_prof = p.as<video_stream_profile>())
                {
                    if (check_profile(p, [&](stream_profile prof) { return (std::find_if(matching_profiles.begin(), matching_profiles.end(), [&](stream_profile sp)
                        { return (stream_id != p.unique_id() && sp.fps() == p.fps() && sp.as<video_stream_profile>().width() == vid_prof.width() &&
                            sp.as<video_stream_profile>().height() == vid_prof.height()); }) != matching_profiles.end()); },
                        profiles_by_fps_res, results, std::make_tuple(p.fps(), vid_prof.width(), vid_prof.height()), num_streams, def_p))
                        break;
                }
            }
        }
        else if (stream_enabled != prev_stream_enabled)
        {
            if (num_streams == 0)
                return results;
            get_sorted_profiles(sorted_profiles);
            std::vector<stream_profile> matching_profiles;
            std::map<rs2_format, std::map<int, stream_profile>> profiles_by_format;

            for (auto&& p : sorted_profiles)
            {
                // first try to find profile from the new stream to match the current configuration
                if (!ui.is_multiple_resolutions)
                {
                    if (check_profile(p, [&](video_stream_profile vid_prof)
                        { return (p.fps() == fps && vid_prof.width() == width && vid_prof.height() == height); },
                        profiles_by_format, results, p.format(), num_streams, def_p))
                        break;
                }
                else
                {
                    if (check_profile(p, [&](video_stream_profile vid_prof)
                        {
                            bool res = false;
                            for (int i = 0; i < selected_resolutions.size(); ++i)
                            {
                                auto cur_res = selected_resolutions[i];
                                if (p.fps() == fps && vid_prof.width() == cur_res.first && vid_prof.height() == cur_res.second)
                                {
                                    res = true;
                                    break;
                                }
                            }
                            return res;
                        },
                        profiles_by_format, results, p.format(), num_streams, def_p))
                        break;
                }
            }
            if (results.size() < num_streams)
            {
                results.clear();
                std::map<std::tuple<int, int, int>, std::map<int, stream_profile>> profiles_by_fps_res;
                for (auto&& p : sorted_profiles)
                {
                    if (auto vid_prof = p.as<video_stream_profile>())
                    {
                        // if no stream with current configuration was found, try to find some configuration to match all enabled streams
                        if (check_profile(p, [&](video_stream_profile vsp) { return true; }, profiles_by_fps_res, results,
                            std::make_tuple(p.fps(), vid_prof.width(), vid_prof.height()), num_streams, def_p))
                            break;
                    }
                }
            }
        }
        if (results.empty())
            results.push_back(def_p);
        update_ui(results);
        return results;
    }

    bool subdevice_model::is_ir_calibration_profile() const
    {
        // checking format
        bool is_cal_format = false;
        // checking that the SKU is D405 - otherwise, this method should return false
        if (dev.supports(RS2_CAMERA_INFO_PRODUCT_ID) && !strcmp(dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID), "0B5B"))
        {
            for (auto it = stream_enabled.begin(); it != stream_enabled.end(); ++it)
            {
                if (it->second)
                {
                    int selected_format_index = -1;
                    if (ui.selected_format_id.count(it->first) > 0)
                        selected_format_index = ui.selected_format_id.at(it->first);

                    if (format_values.count(it->first) > 0 && selected_format_index > -1)
                    {
                        auto formats = format_values.at(it->first);
                        if (formats.size() > selected_format_index)
                        {
                            auto format = formats[selected_format_index];
                            if (format == RS2_FORMAT_Y16)
                            {
                                is_cal_format = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        return is_cal_format;
    }

    bool subdevice_model::is_depth_calibration_profile() const
    {
        // Check if D555 at depth resolution of 1280x800
        std::string dev_name = "";
        if( dev.supports( RS2_CAMERA_INFO_NAME ) )
            dev_name = dev.get_info( RS2_CAMERA_INFO_NAME );

        if( dev_name.find( "D555" ) != std::string::npos )
        {
            // More efficient to check resolution before format
            if( ui.selected_res_id > 0 && res_values.size() > ui.selected_res_id &&  // Verify res_values is initialized
                res_values[ui.selected_res_id].first == 1280 && res_values[ui.selected_res_id].second == 800 )
            {
                for( auto it = stream_enabled.begin(); it != stream_enabled.end(); ++it )
                {
                    if( it->second )
                    {
                        int selected_format_index = -1;
                        if( ui.selected_format_id.count( it->first ) > 0 )
                            selected_format_index = ui.selected_format_id.at( it->first );

                        if( format_values.count( it->first ) > 0 && selected_format_index > -1 )
                        {
                            auto formats = format_values.at( it->first );
                            if( formats.size() > selected_format_index )
                            {
                                auto format = formats[selected_format_index];
                                if( format == RS2_FORMAT_Z16 )
                                    return true;
                            }
                        }
                    }
                }
            }
        }

        return false;
    }

    bool subdevice_model::is_multiple_resolutions_supported() const
    {
        std::string product_line = dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
        std::string sensor_name = s->get_info(RS2_CAMERA_INFO_NAME);

        return product_line == "D500" && sensor_name == "Stereo Module";
    }

    std::pair<int, int> subdevice_model::get_max_resolution(rs2_stream stream) const
    {
        if (resolutions_per_stream.count(stream) > 0)
            return resolutions_per_stream.at(stream).back();

        std::stringstream error_message;
        error_message << "The stream ";
        error_message << rs2_stream_to_string(stream);
        error_message << " is not available with this sensor ";
        error_message << s->get_info(RS2_CAMERA_INFO_NAME);
        throw std::runtime_error(error_message.str());
    }

    void subdevice_model::select_resolution( int width, int height, rs2_stream stream )
    {
        if( ui.is_multiple_resolutions )
        {
            // (0, 0) indicates keep current resolution
            if( width != 0 && height != 0 && stream != RS2_STREAM_ANY )
                ui.selected_stream_to_res[stream] = { width, height };
        }
        else
        {
            for( int i = 0; i < res_values.size(); i++ )
            {
                auto kvp = res_values[i];
                if( kvp.first == width && kvp.second == height )
                    ui.selected_res_id = i;
            }
        }
    }

    std::vector<stream_profile> subdevice_model::get_selected_profiles(bool enforce_inter_stream_policies)
    {
        std::vector<stream_profile> results;

        std::stringstream error_message;
        error_message << "The profile ";

        bool is_cal_profile = is_ir_calibration_profile();

        for (auto&& f : formats)
        {
            auto stream = f.first;
            if (stream_enabled[stream])
            {
                auto format = format_values[stream][ui.selected_format_id[stream]];

                auto fps = 0;
                if (show_single_fps_list)
                    fps = shared_fps_values[ui.selected_shared_fps_id];
                else
                    fps = fps_values_per_stream[stream][ui.selected_fps_id[stream]];

                for (auto&& p : profiles)
                {
                    if (auto vid_prof = p.as<video_stream_profile>())
                    {
                        if (res_values.size() > 0)
                        {
                            int width = 0;
                            int height = 0;
                            std::map<rs2_stream, std::pair<int, int>> stream_to_selected_resolution;
                            if (!ui.is_multiple_resolutions)
                            {
                                width = res_values[ui.selected_res_id].first;
                                height = res_values[ui.selected_res_id].second;
                                error_message << "\n{" << stream_display_names[stream] << ","
                                    << width << "x" << height << " at " << fps << "Hz, "
                                    << rs2_format_to_string(format) << "} ";
                            }
                            else
                            {
                                stream_to_selected_resolution[p.stream_type()] = ui.selected_stream_to_res[p.stream_type()];
  
                                error_message << "\n{" << stream_display_names[stream] << ","
                                    << stream_to_selected_resolution[p.stream_type()].first << "x" 
                                    << stream_to_selected_resolution[p.stream_type()].second << " at " << fps << "Hz, "
                                    << rs2_format_to_string(format) << "} ";
                            }

                            if (p.unique_id() == stream && p.fps() == fps && p.format() == format)
                            {
                                // permitting to add color stream profile to depth sensor
                                // when infrared calibration is active
                                if (is_cal_profile && p.stream_type() == RS2_STREAM_COLOR)
                                {
                                    auto max_color_res = get_max_resolution(RS2_STREAM_COLOR);
                                    if (vid_prof.width() == max_color_res.first && vid_prof.height() == max_color_res.second)
                                        results.push_back(p);
                                }
                                else
                                {
                                    if (!ui.is_multiple_resolutions)
                                    {
                                        if (vid_prof.width() == width && vid_prof.height() == height)
                                            results.push_back(p);
                                    }
                                    else
                                    {
                                        std::pair<int, int> cur_res;
                                        cur_res = stream_to_selected_resolution[p.stream_type()];
                                        if (vid_prof.width() == cur_res.first && vid_prof.height() == cur_res.second)
                                            results.push_back(p);
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        error_message << "\n{" << stream_display_names[stream] << ", at " << fps << "Hz, "
                            << rs2_format_to_string(format) << "} ";

                        if (p.fps() == fps &&
                            p.unique_id() == stream &&
                            p.format() == format)
                            results.push_back(p);
                    }
                }
            }
        }
        if (results.size() == 0 && enforce_inter_stream_policies)
        {
            error_message << " is unsupported!";
            throw std::runtime_error(error_message.str());
        }
        return results;
    }

    void subdevice_model::wait_for_stop()
    {
        std::future< void > local;
        {
            std::lock_guard< std::mutex > lock(_stop_mutex);
            local = std::move(_stop_future);
        }
        if (local.valid())
            local.get();
    }

    void subdevice_model::stop(std::shared_ptr<notifications_model> not_model)
    {
        if (not_model)
            not_model->add_log("Stopping streaming");

        // --- Immediate UI state (synchronous) ---
        streaming = false;
        _pause = false;

        if (ui.is_multiple_resolutions)
        {
            streaming_map[RS2_STREAM_DEPTH] = false;
            streaming_map[RS2_STREAM_INFRARED] = false;
        }

        if (profiles[0].stream_type() == RS2_STREAM_COLOR)
        {
            std::lock_guard< std::mutex > lock(detected_objects->mutex);
            detected_objects->clear();
            detected_objects->sensor_is_on = false;
        }
        else if (profiles[0].stream_type() == RS2_STREAM_DEPTH)
        {
            viewer.disable_measurements();
        }

        // --- Heavy operations (background) ---
        // Chain with any prior pending stop without blocking the caller:
        // move the old future into the lambda so it waits internally.
        std::lock_guard< std::mutex > lock(_stop_mutex);
        auto prev_stop = std::move(_stop_future);

        auto sensor_ptr = s;
        _stop_future = std::async(std::launch::async, [this, sensor_ptr, prev_stop = std::move(prev_stop)]() mutable
            {
                try
                {
                    if (prev_stop.valid())
                        prev_stop.get();

                    sensor_ptr->stop();

                    _options_invalidated = true;

                    queues.foreach([&](frame_queue& q)
                        {
                            frame f;
                            while (q.poll_for_frame(&f));
                        });

                    sensor_ptr->close();
                }
                catch (const std::exception& e)
                {
                    if (viewer.not_model)
                        viewer.not_model->add_log(
                            rsutils::string::from() << "Error stopping sensor: " << e.what(),
                            RS2_LOG_SEVERITY_WARN);
                }
                catch (...) {}
            });
    }

    bool subdevice_model::is_paused() const
    {
        return _pause.load();
    }

    void subdevice_model::pause()
    {
        _pause = true;
        auto playback_dev = dev.as<rs2::playback>();
        if (playback_dev && playback_dev.current_status() == RS2_PLAYBACK_STATUS_PLAYING)
        {
            playback_dev.pause();
        }
    }

    void subdevice_model::resume()
    {
        _pause = false;
        auto playback_dev = dev.as<rs2::playback>();
        if (playback_dev && playback_dev.current_status() == RS2_PLAYBACK_STATUS_PAUSED)
        {
            playback_dev.resume();
        }
    }

    //The function decides if specific frame should be sent to the syncer
    bool subdevice_model::is_synchronized_frame(viewer_model& viewer, const frame& f)
    {
        if (!viewer.is_3d_view || viewer.is_3d_depth_source(f) || viewer.is_3d_texture_source(f))
            return true;

        return false;
    }

    void subdevice_model::avoid_streaming_on_embedded_filters_not_matching_configuration() const
    {
        // check if sensor is depth
        // check if embedded decimation filter is ON
        // check if reolution is different from 640 X 360
        if (s->is<depth_sensor>())
            {
            auto current_depth_sensor = s->as<depth_sensor>();

            std::shared_ptr<embedded_filter_model> embedded_decimation = nullptr;
            for (auto& ef : embedded_filters)
            {
                if (ef->get_filter()->get_type() == RS2_EMBEDDED_FILTER_TYPE_DECIMATION)
                {
                    embedded_decimation = ef;
                    break;
                }
            }
            if (embedded_decimation &&
                embedded_decimation->get_filter()->get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED))
            {
                // check if resolution is different from 640 X 360
                int width = 0;
                int height = 0;
                if (!ui.is_multiple_resolutions)
                {
                    width = res_values[ui.selected_res_id].first;
                    height = res_values[ui.selected_res_id].second;
                }
                else
                {
                    auto res_pair = ui.selected_stream_to_res.at(RS2_STREAM_DEPTH);
                    width = res_pair.first;
                    height = res_pair.second;
                }
                if (width != 640 || height != 360)
                {
                    throw std::runtime_error("Cannot start streaming: Embedded Decimation filter to be used only with resolution 640x360.");
                }
            }
        }
    }

    void subdevice_model::play(const std::vector<stream_profile>& profiles, viewer_model& viewer, std::shared_ptr<rs2::asynchronous_syncer> syncer)
    {
        wait_for_stop();
        avoid_streaming_on_embedded_filters_not_matching_configuration();
        set_extrinsics_from_depth_if_needed();

        std::stringstream ss;
        ss << "Starting streaming of ";
        for (size_t i = 0; i < profiles.size(); i++)
        {
            ss << profiles[i].stream_type();
            if (i < profiles.size() - 1) ss << ", ";
        }
        ss << "...";
        viewer.not_model->add_log(ss.str());

        s->open(profiles);
        try {
            s->start([&, syncer](frame f)
                {
                    // The condition here must match the condition inside render_loop()!
                    if (viewer.synchronization_enable)
                    {
                        syncer->invoke(f);
                    }
                    else
                    {
                        auto id = f.get_profile().unique_id();
                        viewer.ppf.frames_queue[id].enqueue(f);

                        on_frame();
                    }
                });
        }

        catch (...)
        {
            s->close();
            throw;
        }

        _options_invalidated = true;
        streaming = true;

        if (ui.is_multiple_resolutions)
        {
            for (size_t i = 0; i < profiles.size(); i++)
            {
                streaming_map[profiles[i].stream_type()] = true;
            }
        }

        if (s->is< color_sensor >())
        {
            std::lock_guard< std::mutex > lock(detected_objects->mutex);
            detected_objects->sensor_is_on = true;
        }
    }
    void subdevice_model::update(std::string& error_message, notifications_model& notifications)
    {
        if (_options_invalidated)
        {
            next_option = 0;
            _options_invalidated = false;

            save_processing_block_to_config_file("colorizer", depth_colorizer);
            save_processing_block_to_config_file("yuy2rgb", yuy2rgb);
            save_processing_block_to_config_file("m420_to_rgb", m420_to_rgb);
            save_processing_block_to_config_file("y411", y411);

            for (auto&& pbm : post_processing) pbm->save_to_config_file();
        }

        if (next_option < supported_options.size())
        {
            auto next = supported_options[next_option];
            if (options_metadata.find(static_cast<rs2_option>(next)) != options_metadata.end())
            {
                auto& opt_md = options_metadata[static_cast<rs2_option>(next)];
                opt_md.update_all_fields(error_message, notifications);

                if (next == RS2_OPTION_ENABLE_AUTO_EXPOSURE)
                {
                    auto old_ae_enabled = auto_exposure_enabled;
                    auto_exposure_enabled = opt_md.value_as_float() > 0;

                    if (!old_ae_enabled && auto_exposure_enabled)
                    {
                        try
                        {
                            if (s->is<roi_sensor>())
                            {
                                auto r = s->as<roi_sensor>().get_region_of_interest();
                                roi_rect.x = static_cast<float>(r.min_x);
                                roi_rect.y = static_cast<float>(r.min_y);
                                roi_rect.w = static_cast<float>(r.max_x - r.min_x);
                                roi_rect.h = static_cast<float>(r.max_y - r.min_y);
                            }
                        }
                        catch (...)
                        {
                            auto_exposure_enabled = false;
                        }
                    }
                }

                if (next == RS2_OPTION_DEPTH_UNITS)
                {
                    opt_md.dev->depth_units = opt_md.value_as_float();
                }

                if (next == RS2_OPTION_STEREO_BASELINE)
                    opt_md.dev->stereo_baseline = opt_md.value_as_float();
            }

            next_option++;
        }
    }

    void subdevice_model::draw_options(const std::vector<rs2_option>& drawing_order,
        bool update_read_only_options, std::string& error_message,
        notifications_model& notifications)
    {
        for (auto& opt : drawing_order)
        {
            draw_option(opt, update_read_only_options, error_message, notifications);
        }

        for (auto i = 0; i < RS2_OPTION_COUNT; i++)
        {
            auto opt = static_cast<rs2_option>(i);
            if (viewer.is_option_skipped(opt)) continue;
            if (std::find(drawing_order.begin(), drawing_order.end(), opt) == drawing_order.end())
            {
                draw_option(opt, update_read_only_options, error_message, notifications);
            }
        }
    }

    uint64_t subdevice_model::num_supported_non_default_options() const
    {
        return (uint64_t)std::count_if(
            std::begin(options_metadata),
            std::end(options_metadata),
            [&](const std::pair<int, option_model>& p) {return p.second.supported && !viewer.is_option_skipped(p.second.opt); });
    }

    bool subdevice_model::supports_on_chip_calib()
    {
        bool is_d400 = s->supports(RS2_CAMERA_INFO_PRODUCT_LINE) ?
            std::string(s->get_info(RS2_CAMERA_INFO_PRODUCT_LINE)) == "D400" : false;
        std::string fw_version = s->supports(RS2_CAMERA_INFO_FIRMWARE_VERSION) ?
            s->get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION) : "";
        bool supported_fw = s->supports(RS2_CAMERA_INFO_FIRMWARE_VERSION) ?
            is_upgradeable("05.11.12.0", fw_version) : false;
        bool d400_on_chip_calib_supported = s->is<rs2::depth_sensor>() && is_d400 && supported_fw;

        bool is_d500 = s->supports(RS2_CAMERA_INFO_PRODUCT_LINE) ?
            std::string(s->get_info(RS2_CAMERA_INFO_PRODUCT_LINE)) == "D500" : false;
        bool is_depth_sensor = s->supports(RS2_CAMERA_INFO_NAME) ?
            std::string(s->get_info(RS2_CAMERA_INFO_NAME)) == "Stereo Module" : false;
        bool d500_on_chip_calib_supported = is_depth_sensor && is_d500;

        return d400_on_chip_calib_supported || d500_on_chip_calib_supported;
    }

    void subdevice_model::set_extrinsics_from_depth_if_needed()
    {
        std::string pid = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
        std::string sensor_name = s->get_info(RS2_CAMERA_INFO_NAME);
        if (pid == "0B6B" && sensor_name == "Depth Mapping Camera")
        {
            //_labeled_point_cloud_to_depth_extrinsics
            stream_profile depth_profile;

            auto depth_sensor = dev.first<rs2::depth_sensor>();
            auto profiles = depth_sensor.get_stream_profiles();
            for (auto&& p : profiles)
            {
                if (p.stream_type() == RS2_STREAM_DEPTH)
                {
                    depth_profile = p;
                    break;
                }
            }
            stream_profile lpc_profile;
            profiles = s->get_stream_profiles();
            for (auto&& p : profiles)
            {
                if (p.stream_type() == RS2_STREAM_LABELED_POINT_CLOUD)
                {
                    lpc_profile = p;
                    break;
                }
            }
            _extrinsics_from_depth = depth_profile.get_extrinsics_to(lpc_profile);
        }
    }

}
