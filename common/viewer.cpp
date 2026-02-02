// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "viewer.h"
#include "os.h"

#include "udev-rules.h"

#include <opengl3.h>

#include <imgui_internal.h>
#include <realsense_imgui.h>

#define ARCBALL_CAMERA_IMPLEMENTATION
#include <third-party/arcball_camera.h>

#include <rsutils/os/special-folder.h>
#include <rsutils/string/trim-newlines.h>
#include <common/utilities/imgui/wrap.h>
#include <common/labeled-point-cloud-utilities.h>

#include <rsutils/easylogging/easyloggingpp.h>
#include <regex>

namespace rs2
{
    template <typename T>
    T non_negative(const T& input)
    {
        return std::max(static_cast<T>(0), input);
    }

    // Allocates a frameset from points and texture frames
    frameset_allocator::frameset_allocator(viewer_model* viewer) : owner(viewer),
        filter([this](frame f, frame_source& s)
    {
        std::vector<rs2::frame> frame_vec;
        auto tex = owner->get_last_texture()->get_last_frame(true);
        if (tex)
        {
            frame_vec.push_back(tex);
            frame_vec.push_back(f);
            auto frame = s.allocate_composite_frame(frame_vec);
            if (frame)
                s.frame_ready(std::move(frame));
        }
        else
            s.frame_ready(std::move(f));
    }) {}

    // Need out of class declaration to take reference
    const rs2_option save_to_ply::OPTION_IGNORE_COLOR;
    const rs2_option save_to_ply::OPTION_PLY_MESH;
    const rs2_option save_to_ply::OPTION_PLY_BINARY;
    const rs2_option save_to_ply::OPTION_PLY_NORMALS;

    void viewer_model::set_export_popup(ImFont* large_font, ImFont* font, rect stream_rect, std::string& error_message, config_file& temp_cfg)
    {
        float font_size = (float)temp_cfg.get( configurations::window::font_size ); 
        float w = 32.f * font_size;
        float h = 20.f * font_size;
        float x0 = stream_rect.x + stream_rect.w / 3;
        float y0 = stream_rect.y + stream_rect.h / 3;
        ImGui::SetNextWindowPos({ x0, y0 });
        ImGui::SetNextWindowSize({ w, h });

        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

        RsImGui_ScopePushFont(font);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

        static export_type tab = export_type::ply;
        if (ImGui::BeginPopupModal("Export", nullptr, flags))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
            ImGui::PushFont(large_font);
            for (auto& exporter : exporters)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, tab != exporter.first ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != exporter.first ? light_grey : light_blue);
                ImGui::SameLine();
                if (ImGui::Button(exporter.second.name.c_str(), { w / exporters.size() - 50, 30 }))
                {
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                    tab = exporter.first;
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::PopFont();
            if (tab == export_type::ply)
            {
                bool mesh = temp_cfg.get(configurations::ply::mesh);
                bool use_normals = temp_cfg.get(configurations::ply::use_normals);
                if (!mesh) use_normals = false;
                int encoding = temp_cfg.get(configurations::ply::encoding);

                ImGui::PushStyleColor(ImGuiCol_Text, grey);
                ImGui::Text("Polygon File Format defines a flexible systematic scheme for storing 3D data");
                ImGui::PopStyleColor();
                ImGui::NewLine();
                ImGui::Separator();
                if (ImGui::Checkbox("Meshing", &mesh))
                {
                    temp_cfg.set(configurations::ply::mesh, mesh);
                }
                ImGui::PushStyleColor(ImGuiCol_Text, grey);
                ImGui::Text("         Use faces for meshing by connecting each group of 3 adjacent points");
                ImGui::PopStyleColor();
                ImGui::Separator();

                if (!mesh)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, black);
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, black);
                }
                if (ImGui::Checkbox("Normals", &use_normals))
                {
                    if (!mesh)
                        use_normals = false;
                    else
                        temp_cfg.set(configurations::ply::use_normals, use_normals);
                }
                if (!mesh)
                {
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("Enable meshing to allow vertex normals calculation");
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();
                }

                ImGui::PushStyleColor(ImGuiCol_Text, grey);
                ImGui::Text("         Calculate vertex normals and add them to the PLY");
                ImGui::PopStyleColor();
                ImGui::Separator();

                ImGui::Text("Encoding:");
                ImGui::PushStyleColor(ImGuiCol_Text, grey);
                ImGui::Text("Save PLY as binary, or as a larger textual human-readable file");
                ImGui::PopStyleColor();
                if (ImGui::RadioButton("Textual", encoding == configurations::ply::textual))
                {
                    encoding = configurations::ply::textual;
                    temp_cfg.set(configurations::ply::encoding, encoding);
                }
                if (ImGui::RadioButton("Binary", encoding == configurations::ply::binary))
                {
                    encoding = configurations::ply::binary;
                    temp_cfg.set(configurations::ply::encoding, encoding);
                }

                auto curr_exporter = exporters.find(tab);
                if (curr_exporter == exporters.end()) // every tab should have a corresponding exporter
                    error_message = "Exporter not implemented";
                else
                {
                    curr_exporter->second.options[rs2::save_to_ply::OPTION_PLY_MESH] = mesh;
                    curr_exporter->second.options[rs2::save_to_ply::OPTION_PLY_NORMALS] = use_normals;
                    curr_exporter->second.options[rs2::save_to_ply::OPTION_PLY_BINARY] = encoding;
                }
            }

            ImGui::PopStyleColor(2); // button color

            auto apply = [&]() {
                update_configuration(&temp_cfg);
            };

            ImGui::PushStyleColor(ImGuiCol_Button, button_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_color + 0.1f);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_color + 0.1f);

            ImGui::SetCursorScreenPos({ (float)(x0 + w / 2), (float)(y0 + h - 30) });

            if( ImGui::Button( "Export", ImVec2( font_size * 8.f, 0 ) ) )
            {
                apply();
                if (!last_points)
                    error_message = "No depth data available";
                else
                {
                    auto curr_exporter = exporters.find(tab);
                    if (curr_exporter == exporters.end()) // every tab should have a corresponding exporter
                        error_message = "Exporter not implemented";
                    else if (auto ret = file_dialog_open(save_file, curr_exporter->second.filters.data(), NULL, NULL))
                    {
                        auto model = ppf.get_points();
                        frame tex;
                        if (selected_tex_source_uid >= 0 && streams.find(selected_tex_source_uid) != streams.end())
                        {
                            tex = streams[selected_tex_source_uid].texture->get_last_frame(true);
                            if (tex) ppf.update_texture(tex);
                        }

                        std::string fname(ret);
                        if (!ends_with(rsutils::string::to_lower(fname), curr_exporter->second.extension)) fname += curr_exporter->second.extension;

                        std::unique_ptr<rs2::filter> exporter;
                        if (tab == export_type::ply)
                            exporter = std::unique_ptr<rs2::filter>(new rs2::save_to_ply(fname));
                        auto data = frameset_alloc.process(last_points);

                        for (auto& option : curr_exporter->second.options)
                        {
                            exporter->set_option(option.first, static_cast<float>(option.second));
                        }

                        export_frame(fname, std::move(exporter), *not_model, data);
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("%s", "Save settings and export file");
            }
            ImGui::SameLine();
            if( ImGui::Button( "Cancel", ImVec2( font_size * 8.f, 0 ) ) )
            {
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("%s", "Close window without saving any changes to the settings");
            }

            ImGui::PopStyleColor(3);
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

    bool big_button(bool* status,
        ux_window& win,
        float x, float y,
        const char* icon,
        const char* label,
        bool dropdown,
        bool enabled,
        const char* description,
        ImVec4 text_color = light_grey
        )
    {
        auto disabled = !enabled;
        auto font = win.get_font();
        auto large_font = win.get_large_font();
        float font_size = (float)win.get_font_size();

        bool hovered = false;
        bool clicked = false;

        if (!disabled)
        {
            if (*status)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, text_color);
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, header_color);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, header_color);
        }

        float button_width = font_size * 3.8f;

        ImGui::SetCursorPos( { x, y } );
        ImGui::PushFont(large_font);
        clicked = clicked || ImGui::Button( icon, { button_width, 50 } );
        ImGui::PopFont();
        hovered = hovered || ImGui::IsItemHovered();

        ImGui::SetCursorPos( { x + button_width - font_size, y } );
        ImGui::PushFont(font);
        if (dropdown)
        {
            clicked = clicked || ImGui::Button(textual_icons::chevron_down, { font_size, 55 } );
            hovered = hovered || ImGui::IsItemHovered();
        }

        ImGui::SetCursorPos( { x, y + 35 } );
        clicked = clicked || ImGui::Button( label, { button_width, 20 } );
        ImGui::PopFont();
        hovered = hovered || ImGui::IsItemHovered();

        if (hovered)
        {
            win.link_hovered();
            RsImGui::CustomTooltip("%s", description);
        }

        if (clicked && !disabled)
        {
            *status = !(*status);
        }

        ImGui::PopStyleColor(2);

        return clicked && !disabled;
    }

    // Get both font and large_font for the export pop-up
    void viewer_model::show_3dviewer_header(ux_window& win, rs2::rect stream_rect, bool& paused, std::string& error_message)
    {
        auto font = win.get_font();
        auto large_font = win.get_large_font();

        // Draw pose header if pose stream exists
        bool pose_render = false;

        for (auto&& s : streams)
        {
            if (s.second.is_stream_visible() &&
                s.second.profile.stream_type() == RS2_STREAM_POSE)
            {
                pose_render = true;
                break;
            }
        }

        // Initialize selected_labeled_points_source_uid for clearing it when needed
        init_labeled_points_uid();

        // Initialize and prepare depth and texture sources
        int selected_depth_source = -1;
        std::vector<std::string> depth_sources_str;
        std::vector<int> depth_sources;
        init_depth_uid(selected_depth_source, depth_sources_str, depth_sources);

        int selected_tex_source = 0;
        std::vector<std::string> tex_sources_str;
        std::vector<int> tex_sources;
        std::vector<rs2::stream_profile> tex_profiles;
        int i = 0;
        for (auto&& s : streams)
        {
            if (s.second.is_stream_visible() &&
                (s.second.profile.stream_type() == RS2_STREAM_COLOR ||
                 s.second.profile.stream_type() == RS2_STREAM_INFRARED ||
                 s.second.profile.stream_type() == RS2_STREAM_CONFIDENCE ||
                 s.second.profile.stream_type() == RS2_STREAM_DEPTH ||
                 s.second.profile.stream_type() == RS2_STREAM_FISHEYE))
            {
                auto profile_unique_id = s.second.profile.unique_id();
                auto stream_origin_iter = streams_origin.find(profile_unique_id);
                auto profile_found = stream_origin_iter != streams_origin.end();

                if( selected_tex_source_uid == -1 && selected_depth_source_uid != -1 )
                {
                    if( profile_found && streams.find(stream_origin_iter->second ) != streams.end() )
                    {
                        selected_tex_source_uid = streams_origin[profile_unique_id];
                    }
                }
                if( ( profile_found && stream_origin_iter->second == selected_tex_source_uid ) )
                {
                    selected_tex_source = i;
                }

                if( profile_found )
                {
                    // The texture source shall always refer to the raw (original) streams
                    tex_sources.push_back(streams_origin[profile_unique_id]);
                    tex_profiles.push_back(s.second.profile);

                    auto dev_name = s.second.dev ? s.second.dev->dev.get_info(RS2_CAMERA_INFO_NAME) : "Unknown";
                    std::string stream_name = rs2_stream_to_string(s.second.profile.stream_type());
                    if (s.second.profile.stream_index())
                        stream_name += "_" + std::to_string(s.second.profile.stream_index());
                    tex_sources_str.push_back( rsutils::string::from() << dev_name << " " << stream_name );

                    i++;
                }
            }
        }

        for (int i = 0; i < tex_sources.size(); i++)
        {
            auto id = tex_sources[i];
            auto it = std::find(begin(last_tex_sources), end(last_tex_sources), id);
            if (it == last_tex_sources.end())
            {
                // Don't auto-switch to IR stream
                if (tex_profiles[i].format() != RS2_FORMAT_Y8)
                    selected_tex_source_uid = id;
                texture_update_time = glfwGetTime();
            }
        }
        last_tex_sources = tex_sources;

        const auto top_bar_height = 60.f;

        ImGui::PushFont(font);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_Button, header_window_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, header_window_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, header_window_bg);

        std::string label = "header of 3dviewer";

        ImGui::GetWindowDrawList()->AddRectFilled({ stream_rect.x, stream_rect.y },
                     { stream_rect.x + stream_rect.w, stream_rect.y + top_bar_height }, ImColor(sensor_bg));

        ImGui::SetCursorPos({ 0, 0 });
        auto cursor = ImGui::GetCursorScreenPos();

        float left = 5.f;
        float button_width = win.get_font_size() * 3.8f;

        const auto has_stream = tex_sources_str.size() && depth_sources_str.size();

        // ------------ Pause Stream    --------------

        if (paused)
        {
            bool active = true;
            if (big_button(&active, win, 5 + left, 0, textual_icons::play, "Resume", false, has_stream, "Resume streaming"))
            {
                for(auto&& s : streams)
                    if (s.second.dev) s.second.dev->resume();
                paused = false;
            }
        }
        else
        {
            bool active = false;
            if (big_button(&active, win, 5 + left, 0, textual_icons::pause, "Pause", false, has_stream, "Pause streaming"))
            {
                for(auto&& s : streams)
                    if (s.second.dev) s.second.dev->pause();
                paused = true;
            }
        }
        left += button_width;

        // ------------ Reset Viewport ---------------

        bool default_view = (pos - float3{ 0.f, 0.f, -1.f }).length() < 0.001f &&
                            (target - float3{ 0.f, 0.f, 0.f }).length() < 0.001f;
        bool active = false;
        if (big_button(&active, win, 5 + left, 0, textual_icons::rotate, "Reset", false, !default_view, "Reset 3D viewport to initial state"))
        {
            reset_camera();
        }

        left += button_width;

        // ------------    Lock Mode  ---------------

        if (synchronization_enable)
        {
            bool active = true;
            if (big_button(&active, win, 5 + left, 0, textual_icons::lock, "Unlock", false,
                support_non_syncronized_mode && has_stream, "Unlock texture data from pointcloud"))
            {
                synchronization_enable = false;
            }
        }
        else
        {
            bool active = false;
            if (big_button(&active, win, 5 + left, 0, textual_icons::unlock, "Lock", false,
                support_non_syncronized_mode && has_stream, "Lock pointcloud and texture data together"))
            {
                synchronization_enable = true;
            }
        }
        left += button_width + 10;

        ImGui::GetWindowDrawList()->AddLine({ cursor.x + left - 1, cursor.y + 5 },
            { cursor.x + left - 1, cursor.y + top_bar_height - 5 }, ImColor(grey));

        // ------------ Depth Selection --------------

        const auto source_selection_popup = "Source Selection";
        if (big_button(&select_3d_source, win, left, 0, textual_icons::cube,
                       "Source", true,
                       has_stream,
                       "List of available 3D data sources"))
        {
            ImGui::OpenPopup(source_selection_popup);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, black);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::SetNextWindowPos({ cursor.x + left + 5, cursor.y + 60 });
        if (ImGui::BeginPopup(source_selection_popup))
        {
            select_3d_source = true;

            i = 0;
            for (auto&& s : streams)
            {
                if (s.second.is_stream_visible() &&
                    s.second.texture->get_last_frame() &&
                    s.second.profile.stream_type() == RS2_STREAM_DEPTH)
                {
                    std::string id = rsutils::string::from() << depth_sources_str[i] << "##DepthSource-" << i;

                    bool selected = i == selected_depth_source;
                    if (ImGui::MenuItem(id.c_str(), nullptr, &selected))
                    {
                        if (selected)
                        {
                            auto stream_origin_iter = streams_origin.find(s.second.profile.unique_id());

                            if( stream_origin_iter != streams_origin.end() )
                                selected_depth_source_uid = stream_origin_iter->second;
                        }
                    }
                    i++;
                }
            }

            ImGui::EndPopup();
        }
        else
        {
            select_3d_source = false;
        }
        left += button_width + 20;

        // ------------ Texture Selection --------------

        auto t = single_wave(float(glfwGetTime() - texture_update_time) * 2);
        ImVec4 text_color = light_grey * (1.f - t) + light_blue * t;

        const auto tex_selection_popup = "Tex Selection";
        if (big_button(&select_tex_source, win, left, 0, textual_icons::palette,
                       "Texture", true,
                       has_stream,
                       "List of available texture sources", text_color))
        {
            ImGui::OpenPopup(tex_selection_popup);
        }

        ImGui::SetNextWindowPos({ cursor.x + left + 5, cursor.y + 60 });
        if (ImGui::BeginPopup(tex_selection_popup))
        {
            select_tex_source = true;

            for (int i = 0; i < tex_sources_str.size(); i++)
            {
                std::string id = rsutils::string::from() << tex_sources_str[i] << "##TexSource-" << i;

                bool selected = i == selected_tex_source;
                if (ImGui::MenuItem(id.c_str(), nullptr, &selected))
                {
                    if (selected)
                    {
                        selected_tex_source_uid = tex_sources[i];
                    }
                }
            }

            ImGui::EndPopup();
        }
        else
        {
            select_tex_source = false;
        }

        left += button_width + 20;

        // ------------ Shader Selection --------------
        const auto shader_selection_popup = "Shading Selection";
        if (big_button(&select_shader_source, win, left, 0, textual_icons::adjust,
                       "Shading", true, true,
                       "List of available shading modes"))
        {
            ImGui::OpenPopup(shader_selection_popup);
        }

        ImGui::SetNextWindowPos({ cursor.x + left + 5, cursor.y + 60 });
        if (ImGui::BeginPopup(shader_selection_popup))
        {
            select_shader_source = true;

            bool selected = selected_shader == shader_type::points;
            if (ImGui::MenuItem("Raw Point-Cloud", nullptr, &selected))
            {
                if (selected) selected_shader = shader_type::points;
            }

            selected = selected_shader == shader_type::flat;
            if (ImGui::MenuItem("Flat-Shaded Mesh", nullptr, &selected))
            {
                if (selected) selected_shader = shader_type::flat;
            }

            selected = selected_shader == shader_type::diffuse;
            if (ImGui::MenuItem("With Diffuse Lighting", nullptr, &selected, glsl_available))
            {
                if (selected) selected_shader = shader_type::diffuse;
            }

            ImGui::EndPopup();
        }
        else
        {
            select_shader_source = false;
        }
        left += button_width + 20;

        //-----------------------------

        ImGui::PopStyleColor(4);

        ImGui::GetWindowDrawList()->AddLine({ cursor.x + left - 1, cursor.y + 5 },
            { cursor.x + left - 1, cursor.y + top_bar_height - 5 }, ImColor(grey));

        // -------------------- Measure ----------------

        std::string measure_tooltip = "Measure distance between points\nHold shift to connect more than 2 points and measure area";
        if (!glsl_available) measure_tooltip += "\nRequires GLSL acceleration! \nEnable 2 checkboxes in Settings - Performance:  \n- Use GLSL for Rendering \n- Use GLSL for Processing ";
        if (_measurements.is_enabled())
        {
            bool active = true;
            if (big_button(&active, win, 5 + left, 0, textual_icons::ruler, "Measure", false, glsl_available, measure_tooltip.c_str()))
            {
                _measurements.disable();
            }
        }
        else
        {
            bool active = false;
            if (big_button(&active, win, 5 + left, 0, textual_icons::ruler, "Measure", false, glsl_available, measure_tooltip.c_str()))
            {
                _measurements.enable();
            }
        }
        left += button_width;

        // -------------------- Export ------------------

        static config_file temp_cfg;
        set_export_popup(large_font, font, stream_rect, error_message, temp_cfg);

        active = false;
        if (big_button(&active, win, 5 + left, 0, textual_icons::save, "Export", false, last_points, "Export 3D model to 3rd-party application"))
        {
            _measurements.disable();
            temp_cfg = config_file::instance();
            ImGui::OpenPopup("Export");
        }

        left += button_width;

        //-----------------------------
        // -------------------- LPC Settings ----------------
        if (last_labeled_points)
        {
            ImGui::GetWindowDrawList()->AddLine({ cursor.x + left - 1, cursor.y + 5 },
                { cursor.x + left - 1, cursor.y + top_bar_height - 5 }, ImColor(grey));

            left += 10;

            // -------------------- LPC Points Size ----------------
            const auto lpc_popup = "LPC Draw";
            if (big_button(&select_lpc_point_size, win, left, 0, textual_icons::grid_6,
                "Point Size", true, true,
                "Labeled Point Cloud"))
            {
                ImGui::OpenPopup(lpc_popup);
            }

            ImGui::SetNextWindowPos({ cursor.x + left + 5, cursor.y + 60 });
            if (ImGui::BeginPopup(lpc_popup))
            {
                select_lpc_point_size = true;

                bool selected = selected_lpc_points_size == lpc_points_size::lpc_small;
                if (ImGui::MenuItem("Small", nullptr, &selected))
                {
                    if (selected) selected_lpc_points_size = lpc_points_size::lpc_small;
                }

                selected = selected_lpc_points_size == lpc_points_size::lpc_medium;
                if (ImGui::MenuItem("Medium", nullptr, &selected))
                {
                    if (selected) selected_lpc_points_size = lpc_points_size::lpc_medium;
                }

                selected = selected_lpc_points_size == lpc_points_size::lpc_large;
                if (ImGui::MenuItem("Large", nullptr, &selected))
                {
                    if (selected) selected_lpc_points_size = lpc_points_size::lpc_large;
                }
                config_file::instance().set(configurations::viewer::lpc_point_size, static_cast<int>(selected_lpc_points_size));
                ImGui::EndPopup();
            }
            else
            {
                select_lpc_point_size = false;
            }
            left += 80;

            if (show_safety_zones_3d)
            {
                bool active = true;
                if (big_button(&active, win, left, 0, textual_icons::draw_polygon,
                    "S. Zones", false, true,
                    "Show/hide Safety Zones"))
                {
                    show_safety_zones_3d = false;
                    config_file::instance().set(configurations::viewer::show_safety_zones_3d, show_safety_zones_3d);
                }
            }
            else
            {
                bool active = false;
                if (big_button(&active, win, left, 0, textual_icons::draw_polygon,
                    "S. Zones", false, true,
                    "Show/hide Safety Zones"))
                {
                    show_safety_zones_3d = true;
                    config_file::instance().set(configurations::viewer::show_safety_zones_3d, show_safety_zones_3d);
                }
            }
        }


        ImGui::PopStyleColor(5);


        ImGui::PopFont();
    }

    void viewer_model::check_permissions()
    {
#ifdef __linux__

        if (directory_exists("/etc/udev/rules.d") || directory_exists("/lib/udev/rules.d/"))
        {
            const std::string udev_rules_man("/etc/udev/rules.d/99-realsense-libusb.rules");
            const std::string udev_rules_deb("/lib/udev/rules.d/60-librealsense2-udev-rules.rules");
            std::ifstream f_man(udev_rules_man);
            std::ifstream f_deb(udev_rules_deb);

            std::string message = "UDEV-Rules permissions configuration \n for RealSense devices.`\n"
                        "Missing/outdated UDEV-Rules will cause 'Permissions Denied' errors\nunless the application is running under 'sudo' (not recommended)\n"
                        "In case of Debians use: \n"
                        "sudo apt-get upgrade/install librealsense2-udev-rules\n"
                        "To manually install UDEV-Rules in terminal run:\n"
                        "$ sudo cp ~/.99-realsense-libusb.rules /etc/udev/rules.d/99-realsense-libusb.rules && sudo udevadm control --reload-rules && udevadm trigger\n";

            bool create_file = false;

            if(!(f_man.good() || f_deb.good()))
            {
                message = "RealSense UDEV-Rules are missing!\n" + message;
                auto n = not_model->add_notification({ message,
                     RS2_LOG_SEVERITY_WARN,
                     RS2_NOTIFICATION_CATEGORY_COUNT });
                create_file = true;

                n->enable_complex_dismiss = true;
                n->delay_id = "missing-udev";
                if (n->is_delayed()) n->dismiss(true);
            }
            else
            {
                std::ifstream f;
                std::string udev_fname;
                if(f_man.good())
                {
                    if (f_deb.good())
                    {
                        std::string duplicates = "Multiple realsense udev-rules were found! :\n1:" + udev_rules_man
                                    + "\n2: " + udev_rules_deb+ "\nMake sure to remove redundancies!";
                        auto n = not_model->add_notification({ duplicates,
                             RS2_LOG_SEVERITY_WARN,
                             RS2_NOTIFICATION_CATEGORY_COUNT });
                        n->enable_complex_dismiss = true;
                        n->delay_id = "multiple-udev";
                        if (n->is_delayed()) n->dismiss(true);
                    }
                    f.swap(f_man);
                    udev_fname = udev_rules_man;
                    create_file = true;
                }
                else
                {
                    f.swap(f_deb);
                    udev_fname = udev_rules_deb;
                }

                const std::string str((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());

                // The generated array 'realsense_udev_rules' is not NUL-terminated...
                std::string tmp = std::string(realsense_udev_rules, sizeof(realsense_udev_rules));
                tmp.erase(tmp.find_last_of("\n") + 1);
                const std::string udev = tmp;
                float udev_file_ver{0}, built_in_file_ver{0};

                // The udev-rules file shall start with version token expressed as ##Version=xx.yy##
                std::regex udev_ver_regex("^##Version=(\\d+\\.\\d+)##");
                std::smatch match;

                if (std::regex_search(udev.begin(), udev.end(), match, udev_ver_regex))
                    built_in_file_ver = std::stof(std::string(match[1]));

                if (std::regex_search(str.begin(), str.end(), match, udev_ver_regex))
                    udev_file_ver = std::stof(std::string(match[1]));

                if (built_in_file_ver > udev_file_ver)
                {
                    std::stringstream s;
                    s << "RealSense UDEV-Rules file:\n " << udev_fname <<"\n is not up-to date! Version " << built_in_file_ver << " can be applied\n";
                    auto n = not_model->add_notification({
                        s.str() + message,
                        RS2_LOG_SEVERITY_WARN,
                        RS2_NOTIFICATION_CATEGORY_COUNT });

                    n->enable_complex_dismiss = true;
                    n->delay_id = "udev-version";
                    if (n->is_delayed()) n->dismiss(true);
                }
            }

            if (create_file)
            {
                std::string tmp_filename
                    = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data )  // ~/.
                    + "99-realsense-libusb.rules";

                std::ofstream out(tmp_filename.c_str());
                std::string tmp = std::string(realsense_udev_rules, sizeof(realsense_udev_rules));
                out << tmp;
                out.close();
            }
        }

#endif
    }

    // Hide options from both the DQT and Viewer applications
    void viewer_model::hide_common_options()
    {
        _hidden_options.emplace(RS2_OPTION_STREAM_FILTER);
        _hidden_options.emplace(RS2_OPTION_STREAM_FORMAT_FILTER);
        _hidden_options.emplace(RS2_OPTION_STREAM_INDEX_FILTER);
        _hidden_options.emplace(RS2_OPTION_FRAMES_QUEUE_SIZE);
        _hidden_options.emplace(RS2_OPTION_NOISE_ESTIMATION);
        _hidden_options.emplace(RS2_OPTION_REGION_OF_INTEREST);
    }

    void viewer_model::update_configuration(config_file* new_cfg)
    {
        if (new_cfg)
            config_file::instance() = *new_cfg;
        rs2_error* e = nullptr;
        auto version = rs2_get_api_version(&e);
        if (e) rs2::error::handle(e);

        int saved_version = config_file::instance().get_or_default(
            configurations::viewer::sdk_version, 0);

        // Great the user once upon upgrading to a new version
        if (version > saved_version)
        {
            auto n = std::make_shared<version_upgrade_model>(version);
            not_model->add_notification(n);

            config_file::instance().set(configurations::viewer::sdk_version, version);
        }

        continue_with_current_fw = config_file::instance().get_or_default(
            configurations::viewer::continue_with_current_fw, false);

        is_3d_view = config_file::instance().get_or_default(
            configurations::viewer::is_3d_view, true);

        if (bool measurement_enabled = config_file::instance().get_or_default(
            configurations::viewer::is_measuring, false))
            _measurements.enable();

        _measurements.log_function = [this](std::string message) { not_model->add_log(message); };
        _measurements.is_metric = [this]() { return metric_system; };

        glsl_available = config_file::instance().get(
            configurations::performance::glsl_for_rendering);

        occlusion_invalidation = config_file::instance().get_or_default(
            configurations::performance::occlusion_invalidation, true);

        ground_truth_r = config_file::instance().get_or_default(
            configurations::viewer::ground_truth_r, 1200);

        selected_shader = (shader_type)config_file::instance().get_or_default(
            configurations::viewer::shading_mode, 2);

#ifdef BUILD_EASYLOGGINGPP
        auto min_severity = (rs2_log_severity)config_file::instance().get_or_default(
            configurations::viewer::log_severity, 2);

        if( ! _disable_log_to_console )
        {
            if( config_file::instance().get_or_default(
                configurations::viewer::log_to_console, false ) )
            {
                rs2::log_to_console( min_severity );
            }
            else
            {
                rs2::log_to_console( RS2_LOG_SEVERITY_NONE );
            }
        }
        if (config_file::instance().get_or_default(
            configurations::viewer::log_to_file, false))
        {
            std::string filename = config_file::instance().get(
                configurations::viewer::log_filename);

            rs2::log_to_file(min_severity, filename.c_str());
        }
#endif

        show_skybox = config_file::instance().get_or_default(
            configurations::performance::show_skybox, true);

        selected_lpc_points_size = static_cast<lpc_points_size>(config_file::instance().get_or_default(
            configurations::viewer::lpc_point_size, static_cast<int>(lpc_points_size::lpc_small)
        ));

        show_safety_zones_3d = config_file::instance().get_or_default(
            configurations::viewer::show_safety_zones_3d, true
        );
    }



    viewer_model::viewer_model( context & ctx_, bool disable_log_to_console )
        : ppf( *this )
        , ctx( ctx_ )
        , frameset_alloc( this )
        , synchronization_enable( true )
        , synchronization_enable_prev_state(true)
        , _support_ir_reflectivity( false )
        , _disable_log_to_console( disable_log_to_console )
    {

        syncer = std::make_shared<syncer_model>();
        updates = std::make_shared<updates_model>();
        reset_camera();
        rs2_error* e = nullptr;
        not_model->add_log( "librealsense version: " + api_version_to_string( rs2_get_api_version( &e ) ) + "\n" );

        update_configuration();

        check_permissions();
        export_model exp_model = export_model::make_exporter("PLY", ".ply", "Polygon File Format (PLY)\0*.ply\0");
        exporters.insert(std::pair<export_type, export_model>(export_type::ply, exp_model));

        hide_common_options(); // hide this options from both Viewer and DQT applications
    }

    void viewer_model::gc_streams()
    {
        std::lock_guard<std::mutex> lock(streams_mutex);
        std::vector<int> streams_to_remove;
        for (auto&& kvp : streams)
        {
            if (!kvp.second.is_stream_visible() &&
                (!kvp.second.dev || (!kvp.second.dev->is_paused() && !kvp.second.dev->streaming)))
            {
                if (kvp.first == selected_depth_source_uid)
                {
                    ppf.depth_stream_active = false;
                }
                streams_to_remove.push_back(kvp.first);
            }
        }
        for (auto&& i : streams_to_remove) {
            if(selected_depth_source_uid == i)
            {
                last_points = points();
                selected_depth_source_uid = -1;
            }

            if (selected_labeled_points_source_uid == i)
            {
                last_labeled_points = labeled_points();
            }

            if (selected_tex_source_uid == i)
            {
                last_texture.reset();
                selected_tex_source_uid = -1;
            }
            streams.erase(i);

            if(ppf.frames_queue.find(i) != ppf.frames_queue.end())
            {
                ppf.frames_queue.erase(i);
            }
        }
    }

    bool rs2::viewer_model::is_option_skipped(rs2_option opt) const
    {
        return (_hidden_options.find(opt) != _hidden_options.end());
    }

    void rs2::viewer_model::disable_measurements()
    {
        _measurements.disable();
    }

    void rs2::viewer_model::show_popup(const ux_window& window, const popup& p)
    {
        auto font_dynamic = window.get_font();

        RsImGui_ScopePushFont(font_dynamic);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

        ImGui::SetNextWindowSize({520, 180});

        ImGui::OpenPopup(p.header.c_str());

        if (ImGui::BeginPopupModal(p.header.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

            p.custom_command();

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    void viewer_model::popup_if_error(const ux_window& window, std::string& message)
    {
        if (message == "")
            return;

        // The list of errors the user asked not to show again:
        static std::set<std::string> errors_not_to_show;
        static bool dont_show_this_error = false;
        auto simplify_error_message = [](const std::string& s) {
            std::regex e("\\b(0x)([^ ,]*)");
            return std::regex_replace(s, e, "address");
        };

        auto it = std::find_if(_active_popups.begin(), _active_popups.end(), [&](const popup& p) { return message == p.message; });
        if (it != _active_popups.end())
            return;

        auto simplified_error_message = simplify_error_message(message);
        if (errors_not_to_show.count(simplified_error_message))
        {
            not_model->add_notification({ message,
                RS2_LOG_SEVERITY_ERROR,
                RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });
            return;
        }

        std::string header = std::string(textual_icons::exclamation_triangle) + " Oops, something went wrong!";

        auto custom_command = [&]()
        {
            auto msg = _active_popups.front().message;

            // Wrap the text to feet the error pop-up window
            std::string wrapped_msg;
            try
            {
                auto trimmed_msg = rsutils::string::trim_newlines(msg);
                wrapped_msg = utilities::imgui::wrap(trimmed_msg, 500);
            }
            catch (...)
            {
                wrapped_msg = msg; // Revert to original text on wrapping failure
                not_model->output.add_log( RS2_LOG_SEVERITY_WARN,
                                           __FILE__,
                                           __LINE__,
                                           "Wrapping of error message text failed!" );
            }

            ImGui::Text("RealSense error calling:");
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            ImGui::InputTextMultiline("##error", const_cast<char*>(wrapped_msg.c_str()),
                msg.size() + 1, { 500,95 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            //ImGui::SetCursorPos({ 10, 130 });
            ImGui::PopStyleColor(5);

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                if (dont_show_this_error)
                {
                    errors_not_to_show.insert(simplify_error_message(msg));
                }
                ImGui::CloseCurrentPopup();
                _active_popups.erase(_active_popups.begin());
                dont_show_this_error = false;
            }

            ImGui::SameLine();
            ImGui::Checkbox("Don't show this error again", &dont_show_this_error);
        };

        popup p = {header, message, custom_command };
        _active_popups.push_back(p);
    }

    std::vector<uint8_t> read_fw_file(std::string file_path)
    {
        std::vector<uint8_t> rv;

        std::ifstream file(file_path, std::ios::in | std::ios::binary | std::ios::ate);
        if (file.is_open())
        {
            rv.resize(file.tellg());

            file.seekg(0, std::ios::beg);
            file.read((char*)rv.data(), rv.size());
            file.close();
        }

        return rv;
    }

    void rs2::viewer_model::popup_firmware_update_progress(const ux_window& window, const float progress)
    {
        std::string header = "Firmware update in progress";
        std::stringstream message;
        message << std::endl << "Progress: " << (int)(progress *  100.0) << " [%]";

        auto custom_command = [&]()
        {
            ImGui::SetCursorPos({ 10, 100 });
            ImGui::PopStyleColor(5);


            ImGui::ProgressBar(progress, { 300 , 25 }, "Firmware update");
        };

        popup p = { header, message.str(), custom_command };
        _active_popups.push_back(p);
    }

    void viewer_model::show_icon(ImFont* font_18, const char* label_str, const char* text, int x, int y, int id,
        const ImVec4& text_color, const std::string& tooltip)
    {
        RsImGui_ScopePushFont(font_18);

        std::string label = rsutils::string::from() << label_str << id;

        ImGui::SetCursorScreenPos({(float)x, (float)y});
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && tooltip != "")
            RsImGui::CustomTooltip("%s", tooltip.c_str());

    }
    void viewer_model::show_paused_icon(ImFont* font_18, int x, int y, int id)
    {
        show_icon(font_18, "paused_icon", textual_icons::pause, x, y, id, white);
    }
    void viewer_model::show_recording_icon(ImFont* font_18, int x, int y, int id, float alpha_delta)
    {
        show_icon(font_18, "recording_icon", textual_icons::circle, x, y, id, from_rgba(255, 46, 54, static_cast<uint8_t>(alpha_delta * 255)));
    }

    void viewer_model::show_no_stream_overlay(ImFont* font_18, int min_x, int min_y, int max_x, int max_y)
    {
        ImGui::PushFont(font_18);

        auto pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos({ (min_x + max_x) / 2.f - 150, (min_y + max_y) / 2.f - 20 });

        ImGui::PushStyleColor(ImGuiCol_Text, sensor_header_light_blue);
        std::string text = rsutils::string::from() << "Nothing is streaming! Toggle " << textual_icons::toggle_off << " to start";
        ImGui::Text("%s", text.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(pos);

        ImGui::PopFont();
    }

    void viewer_model::show_rendering_not_supported(ImFont* font_18, int min_x, int min_y, int max_x, int max_y, rs2_format format)
    {
        static rsutils::time::periodic_timer update_string(std::chrono::milliseconds(200));
        static int counter = 0;
        static std::string to_print;
        auto pos = ImGui::GetCursorScreenPos();

        ImGui::PushFont(font_18);
        ImGui::SetCursorScreenPos({ min_x + max_x / 2.f - 210, min_y + max_y / 2.f - 20 });
        ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
        std::string text = rsutils::string::from() << textual_icons::exclamation_triangle;
        ImGui::Text("%s", text.c_str());
        ImGui::SetCursorScreenPos({ min_x + max_x / 2.f - 180, min_y + max_y / 2.f - 20 });
        text = rsutils::string::from() <<  " The requested format " << format << " is not supported for rendering  ";

        if (update_string)
        {
            to_print.clear();
            for (int i = 0; i < text.size(); i++)
                to_print += text[(i + counter) % text.size()];
            counter++;
        }

        ImGui::Text("%s", to_print.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(pos);

        ImGui::PopFont();
    }

    void viewer_model::show_no_device_overlay(ImFont* font_18, int x, int y)
    {
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos({ float(x), float(y) });

        ImGui::PushFont(font_18);
        ImGui::PushStyleColor(ImGuiCol_Text, from_rgba(0x70, 0x8f, 0xa8, 0xff));
        ImGui::Text("Connect a RealSense Camera\nor Add Source");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(pos);
    }

    // Generate streams layout, creates a grid-like layout with factor amount of columns
    std::map<int, rect> generate_layout(const rect& r,
        int top_bar_height, size_t factor,
        const std::set<stream_model*>& active_streams,
        std::map<stream_model*, int>& stream_index
    )
    {
        std::map<int, rect> results;
        if (factor == 0) return results;

        // Calc the number of rows
        auto complement = ceil((float)active_streams.size() / factor);

        auto cell_width = static_cast<float>(r.w / factor);
        auto cell_height = static_cast<float>(r.h / complement);

        // using the active streams sorted acc to stream type and stream index
        // typical order will then be: depth, color, ir1, ir2, motion....
        std::vector<stream_model*> active_streams_ordered;
        for (auto&& active_stream : active_streams)
        {
            active_streams_ordered.push_back(active_stream);
        }

        std::sort(active_streams_ordered.begin(), active_streams_ordered.end(),
            [](const stream_model* sm1, const stream_model* sm2)
            {
                return (sm1->profile.stream_type() < sm2->profile.stream_type()) ||
                    ((sm1->profile.stream_type() == sm2->profile.stream_type()) && (sm1->profile.stream_index() < sm2->profile.stream_index()));
            });

        auto it = active_streams_ordered.begin();
        for (auto x = 0; x < factor; x++)
        {
            for (auto y = 0; y < complement; y++)
            {
                // There might be spare boxes at the end (3 streams in 2x2 array for example)
                if (it == active_streams_ordered.end()) break;

                rect rxy = { r.x + x * cell_width, r.y + y * cell_height + top_bar_height,
                    cell_width, cell_height - top_bar_height };
                // Generate box to display the stream in
                results[stream_index[*it]] = rxy.adjust_ratio((*it)->size);
                ++it;
            }
        }

        return results;
    }

    // Return the total display area of the layout
    // The bigger it is, the more details we can see
    float evaluate_layout(const std::map<int, rect>& l)
    {
        float res = 0.f;
        for (auto&& kvp : l) res += kvp.second.area();
        return res;
    }

    void force_minimum_size_for_display(rs2::stream_model& model)
    {
        // patch for safety sensor
        if (model.profile.stream_type() == RS2_STREAM_SAFETY || 
            model.profile.stream_type() == RS2_STREAM_LABELED_POINT_CLOUD)
        {
            // The following values have been chosen so that the safety stream's
            // metadata could be shown entirely (without that,, the safety window
            // is too thin to permit the user to see the metadata fields
            model.size.x = 7.f;
            model.size.y = 10.f;
        }
    }

    std::map<int, rect> viewer_model::calc_layout(const rect& r)
    {
        const int top_bar_height = 32;

        std::set<stream_model*> active_streams;
        std::map<stream_model*, int> stream_index;
        for (auto&& stream : streams)
        {
            if (stream.second.is_stream_visible())
            {
                force_minimum_size_for_display(stream.second);
                
                active_streams.insert(&stream.second);
                stream_index[&stream.second] = stream.first;
            }
        }

        if (fullscreen)
        {
            if (active_streams.count(selected_stream) == 0) fullscreen = false;
        }

        std::map<int, rect> results;

        if (fullscreen)
        {
            results[stream_index[selected_stream]] = { r.x, r.y + top_bar_height,
                                                       r.w, r.h - top_bar_height };
        }
        else
        {
            // Go over all available fx(something) layouts
            for (size_t f = 1; f <= active_streams.size(); f++)
            {
                auto l = generate_layout(r, top_bar_height, f,
                    active_streams, stream_index);

                // Keep the "best" layout in result
                if (evaluate_layout(l) > evaluate_layout(results))
                    results = l;
            }
        }

        return get_interpolated_layout(results);
    }

    rs2::frame viewer_model::handle_ready_frames(const rect& viewer_rect, ux_window& window, int devices, std::string& error_message)
    {
        std::shared_ptr<texture_buffer> texture_frame = nullptr;
        points p;
        labeled_points lab_points;
        frame f{};
        gc_streams();

        std::map<int, frame> last_frames;
        try
        {
            size_t index = 0;
            while (ppf.resulting_queue.poll_for_frame(&f) && ++index < ppf.resulting_queue_max_size)
            {
                // Open the frame-set and validate the incoming frame originated from one of the source streams
                // and save the frames on last_frames
                // if one of the streams is missing we will use the last frame arrived
                // point cloud is not a stream type yet it's a frame we want to display
                if (f.is<rs2::frameset>())
                {
                    for (auto frame : f.as<rs2::frameset>())
                    {
                        auto profile_id = frame.get_profile().unique_id();

                        if (frame.is< points >() || streams.find(profile_id) != streams.end())
                        {
                            last_frames[profile_id] = frame;
                            continue;
                        }

                        auto stream_origin_iter = streams_origin.find(profile_id);
                        if( (stream_origin_iter != streams_origin.end() && streams.find( stream_origin_iter->second ) != streams.end() ))
                        {
                            last_frames[ profile_id ] = frame;
                        }
                    }
                }
                else
                {
                    auto profile_id = f.get_profile().unique_id();

                    if (f.is< points >() || streams.find(profile_id) != streams.end())
                    {
                        last_frames[profile_id] = f;
                        continue;
                    }

                    auto stream_origin_iter = streams_origin.find(profile_id);
                    if ((stream_origin_iter != streams_origin.end() && streams.find(stream_origin_iter->second) != streams.end()))
                    {
                        last_frames[profile_id] = f;
                    }
                }
            }

            for(auto&& f : last_frames)
                not_model->output.update_dashboards(f.second);

            for( auto && frame : last_frames )
            {
                auto f = frame.second;

                if (f.is< points >())  // find and store the 3d points frame for later use
                {
                    if (!paused)
                        p = f.as< points >();
                    continue;
                }

                if (f.is<labeled_points>())
                {
                    if (!paused)
                        lab_points = f.as<labeled_points>();
                }

                auto texture = upload_frame( std::move( f ) );

                if ( should_texture_frame_be_updated(f) )
                {
                    texture_frame = texture;
                }
            }
        }
        catch (const error& ex)
        {
            error_message = error_to_string(ex);
        }
        catch (const std::exception& ex)
        {
            error_message = ex.what();
        }

        window.begin_viewport();

        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5, 5 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        ImGui::SetNextWindowPos({ viewer_rect.x, viewer_rect.y });
        ImGui::SetNextWindowSize({ viewer_rect.w, viewer_rect.h });
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::Begin("Viewport", nullptr,flags);

        try
        {
            draw_viewport( viewer_rect, window, devices, error_message, 
                texture_frame, p, lab_points );

            modal_notification_on = not_model->draw( window,
                                                     static_cast< int >( window.width() ),
                                                     static_cast< int >( window.height() ),
                                                     error_message );
        }
        catch( const error & e )
        {
            error_message = error_to_string( e );
        }
        catch( const std::exception & e )
        {
            error_message = e.what();
        }

        popup_if_error(window, error_message);

        if (!_active_popups.empty())
            show_popup(window, _active_popups.front());

        ImGui::End();
        ImGui::PopStyleVar(2);

        error_message = "";

        return f;
    }

    void viewer_model::reset_camera(float3 p)
    {
        target = { 0.0f, 0.0f, 0.0f };
        pos = p;

        // initialize "up" to be tangent to the sphere!
        // up = cross(cross(look, world_up), look)
        {
            float3 look = { target.x - pos.x, target.y - pos.y, target.z - pos.z };
            look = look.normalized();

            float world_up[3] = { 0.0f, 1.0f, 0.0f };

            float across[3] = {
                look.y * world_up[2] - look.z * world_up[1],
                look.z * world_up[0] - look.x * world_up[2],
                look.x * world_up[1] - look.y * world_up[0],
            };

            up.x = across[1] * look.z - across[2] * look.y;
            up.y = across[2] * look.x - across[0] * look.z;
            up.z = across[0] * look.y - across[1] * look.x;

            float up_len = up.length();
            up.x /= -up_len;
            up.y /= -up_len;
            up.z /= -up_len;
        }
    }

    void viewer_model::draw_color_ruler(const mouse_info& mouse,
                                        const stream_model& s_model,
                                        const rect& stream_rect,
                                        std::vector<rgb_per_distance> rgb_per_distance_vec,
                                        float ruler_length,
                                        const std::string& ruler_units)
    {
        if (rgb_per_distance_vec.empty() || (ruler_length <= 0.f))
            return;

        ruler_length = std::ceil(ruler_length);
        std::sort(rgb_per_distance_vec.begin(), rgb_per_distance_vec.end(), [](const rgb_per_distance& a,
            const rgb_per_distance& b) {
            return a.depth_val < b.depth_val;
        });

        const auto stream_height = stream_rect.y + stream_rect.h;
        const auto stream_width = stream_rect.x + stream_rect.w;

        static const auto ruler_distance_offset = 10;
        auto bottom_y_ruler = stream_height - ruler_distance_offset;
        if (s_model.texture->zoom_preview)
        {
            bottom_y_ruler = s_model.texture->curr_preview_rect.y - ruler_distance_offset;
        }

        static const auto top_y_offset = 50;
        auto top_y_ruler = stream_rect.y + top_y_offset;
        if (s_model.show_stream_details)
        {
            top_y_ruler = s_model.curr_info_rect.y + s_model.curr_info_rect.h + ruler_distance_offset;
        }

        static const auto left_x_colored_ruler_offset = 55;
        static const auto colored_ruler_width = 20;
        const auto left_x_colored_ruler = stream_width - left_x_colored_ruler_offset;
        const auto right_x_colored_ruler = stream_width - (left_x_colored_ruler_offset - colored_ruler_width);
        assert((bottom_y_ruler - top_y_ruler) != 0.f);
        const auto ratio = (bottom_y_ruler - top_y_ruler) / ruler_length;

        // Draw numbered ruler
        float y_ruler_val = top_y_ruler;
        static const auto numbered_ruler_width = 20.f;

        const auto right_x_numbered_ruler = right_x_colored_ruler + numbered_ruler_width;
        static const auto hovered_numbered_ruler_opac = 0.8f;
        static const auto unhovered_numbered_ruler_opac = 0.6f;
        float colored_ruler_opac = unhovered_numbered_ruler_opac;
        float numbered_ruler_background_opac = unhovered_numbered_ruler_opac;
        bool is_ruler_hovered = false;
        if (mouse.cursor.x >= left_x_colored_ruler &&
            mouse.cursor.x <= right_x_numbered_ruler &&
            mouse.cursor.y >= top_y_ruler &&
            mouse.cursor.y <= bottom_y_ruler)
            is_ruler_hovered = true;

        if (is_ruler_hovered)
        {
            std::stringstream ss;
            auto relative_mouse_y = ImGui::GetMousePos().y - top_y_ruler;
            auto y = (bottom_y_ruler - top_y_ruler) - relative_mouse_y;
            ss << std::fixed << std::setprecision(2) << (y / ratio) << ruler_units;
            RsImGui::CustomTooltip("%s", ss.str().c_str());
            colored_ruler_opac = 1.f;
            numbered_ruler_background_opac = hovered_numbered_ruler_opac;
        }

        // Draw a background to the numbered ruler
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0, 0.0, 0.0, numbered_ruler_background_opac);
        glBegin(GL_POLYGON);
        glVertex2f(right_x_colored_ruler, top_y_ruler);
        glVertex2f(right_x_numbered_ruler , top_y_ruler);
        glVertex2f(right_x_numbered_ruler , bottom_y_ruler);
        glVertex2f(right_x_colored_ruler, bottom_y_ruler);
        glEnd();


        const float x_ruler_val = right_x_colored_ruler + 4.0f;
        ImGui::SetCursorScreenPos({ x_ruler_val, y_ruler_val });
        const auto font_size = ImGui::GetFontSize();
        ImGui::TextUnformatted(std::to_string(static_cast<int>(ruler_length)).c_str());
        const auto skip_numbers = ((ruler_length / 10.f) - 1.f);
        auto to_skip = (skip_numbers < 0.f)?0.f: skip_numbers;
        for (int i = static_cast<int>(ruler_length - 1); i > 0; --i)
        {
            y_ruler_val += ((bottom_y_ruler - top_y_ruler) / ruler_length);
            ImGui::SetCursorScreenPos({ x_ruler_val, y_ruler_val - font_size / 2 });
            if (((to_skip--) > 0))
                continue;

            ImGui::TextUnformatted(std::to_string(i).c_str());
            to_skip = skip_numbers;
        }
        y_ruler_val += ((bottom_y_ruler - top_y_ruler) / ruler_length);
        ImGui::SetCursorScreenPos({ x_ruler_val, y_ruler_val - font_size });
        ImGui::Text("0");

        auto total_depth_scale = rgb_per_distance_vec.back().depth_val - rgb_per_distance_vec.front().depth_val;
        static const auto sensitivity_factor = 0.01f;
        auto sensitivity = sensitivity_factor * total_depth_scale;

        // Draw colored ruler
        auto last_y = bottom_y_ruler;
        auto last_depth_value = 0.f;
        size_t last_index = 0;
        for (size_t i = 1; i < rgb_per_distance_vec.size(); ++i)
        {
            auto curr_depth = rgb_per_distance_vec[i].depth_val;
            if ((((curr_depth - last_depth_value) < sensitivity) && (i != rgb_per_distance_vec.size() - 1)))
                continue;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_QUADS);
            glColor4f(rgb_per_distance_vec[last_index].rgb_val.r / 255.f,
                      rgb_per_distance_vec[last_index].rgb_val.g / 255.f,
                      rgb_per_distance_vec[last_index].rgb_val.b / 255.f,
                      colored_ruler_opac);
            glVertex2f(left_x_colored_ruler, last_y);
            glVertex2f(right_x_colored_ruler, last_y);

            last_depth_value = curr_depth;
            last_index = i;

            auto y = bottom_y_ruler - ((rgb_per_distance_vec[i].depth_val) * ratio);
            if ((i == (rgb_per_distance_vec.size() - 1)) || (std::ceil(curr_depth) > ruler_length))
                y = top_y_ruler;

            glColor4f(rgb_per_distance_vec[i].rgb_val.r / 255.f,
                      rgb_per_distance_vec[i].rgb_val.g / 255.f,
                      rgb_per_distance_vec[i].rgb_val.b / 255.f,
                      colored_ruler_opac);

            glVertex2f(right_x_colored_ruler, y);
            glVertex2f(left_x_colored_ruler, y);
            last_y = y;
            glEnd();
        }

        // Draw ruler border
        static const auto top_line_offset = 0.5f;
        static const auto right_line_offset = top_line_offset / 2;
        glColor4f(0.0, 0.0, 0.0, colored_ruler_opac);
        glBegin(GL_LINE_LOOP);
        glVertex2f(left_x_colored_ruler - top_line_offset, top_y_ruler - top_line_offset);
        glVertex2f(right_x_numbered_ruler + right_line_offset / 2, top_y_ruler - top_line_offset);
        glVertex2f(right_x_numbered_ruler + right_line_offset / 2, bottom_y_ruler + top_line_offset);
        glVertex2f(left_x_colored_ruler - top_line_offset, bottom_y_ruler + top_line_offset);
        glEnd();
    }

    float viewer_model::calculate_ruler_max_distance(const std::vector<float>& distances) const
    {
        assert(!distances.empty());

        float mean = std::accumulate(distances.begin(),
            distances.end(), 0.0f) / distances.size();

        float e = 0;
        float inverse = 1.f / distances.size();
        for (auto elem : distances)
        {
            e += static_cast<float>(pow(elem - mean, 2));
        }

        auto standard_deviation = sqrt(inverse * e);
        static const auto length_jump = 4.f;
        return std::ceil((mean + 1.5f * standard_deviation) / length_jump) * length_jump;
    }

    void viewer_model::render_2d_view(const rect& view_rect,
        ux_window& win, int output_height,
        ImFont *font1, ImFont *font2, size_t dev_model_num,
        const mouse_info &mouse, std::string& error_message)
    {
        static rsutils::time::periodic_timer every_sec(std::chrono::seconds(1));
        static bool icon_visible = false;
        if (every_sec) icon_visible = !icon_visible;
        float alpha = icon_visible ? 1.f : 0.2f;

        glViewport(0, 0,
            static_cast<GLsizei>(win.framebuf_width()), static_cast<GLsizei>(win.framebuf_height()));
        glLoadIdentity();
        glOrtho(0, win.width(), win.height(), 0, -1, +1);

        auto layout = calc_layout(view_rect);

        if ((layout.size() == 0) && (dev_model_num > 0))
        {
            show_no_stream_overlay(font2, static_cast<int>(view_rect.x), static_cast<int>(view_rect.y), static_cast<int>(win.width()), static_cast<int>(win.height() - output_height));
        }
        for (auto &&kvp : layout)
        {
            auto&& view_rect = kvp.second;
            auto stream = kvp.first;
            auto&& stream_mv = streams[stream];
            auto&& stream_size = stream_mv.size;
            auto stream_rect = view_rect.adjust_ratio(stream_size).grow(-3);

            if (should_render_frame(stream_mv)) {
                stream_mv.show_frame(stream_rect, mouse, error_message);
            }

            auto p = stream_mv.dev->dev.as<playback>();
            float posX = stream_rect.x + 9;
            float posY = stream_rect.y - 26;

            if (!stream_mv.is_stream_alive())
            {
                std::string message = rsutils::string::from() << textual_icons::exclamation_triangle << " No Frames Received!";
                show_icon(font2, "warning_icon", message.c_str(),
                    static_cast<int>(stream_rect.center().x - 100),
                    static_cast<int>(stream_rect.center().y - 25),
                    stream_mv.profile.unique_id(),
                    blend(dark_red, alpha),
                    "Did not receive frames from the platform within a reasonable time window,\nplease try reducing the FPS or the resolution");
            }

            stream_mv.show_stream_header(font1, stream_rect, *this);
            stream_mv.show_stream_footer(font1, stream_rect, mouse, streams, *this);


            if (val_in_range(stream_mv.profile.format(), { RS2_FORMAT_RAW10 , RS2_FORMAT_RAW16, RS2_FORMAT_MJPEG, RS2_FORMAT_M420 }))
            {
                show_rendering_not_supported(font2, static_cast<int>(stream_rect.x), static_cast<int>(stream_rect.y), static_cast<int>(stream_rect.w),
                    static_cast<int>(stream_rect.h), stream_mv.profile.format());
            }

            if (stream_mv.dev->_is_being_recorded)
            {
                show_recording_icon(font2, static_cast<int>(posX), static_cast<int>(posY), stream_mv.profile.unique_id(), alpha);
                posX += 23;
            }
            if (stream_mv.dev->is_paused() || (p && p.current_status() == RS2_PLAYBACK_STATUS_PAUSED))
                show_paused_icon(font2, static_cast<int>(posX), static_cast<int>(posY), stream_mv.profile.unique_id());

            auto stream_type = stream_mv.profile.stream_type();

            if (streams[stream].is_stream_visible())
                switch (stream_type)
            {
                {
                    case RS2_STREAM_GYRO: /* Fall Through */
                    case RS2_STREAM_ACCEL:
                        if( rs2::frame frame = streams[stream].texture->get_last_frame() )
                        {
                            auto motion = frame.as< motion_frame >();
                            if( motion.get() )
                            {
                                auto axis = motion.get_motion_data();
                                stream_mv.show_stream_imu( font1,
                                                           stream_rect,
                                                           axis,
                                                           mouse,
                                                           stream_type == RS2_STREAM_GYRO ? "Radians/Sec" : "Meter/Sec^2" );
                            }
                        }
                        break;

                    case RS2_STREAM_MOTION:
                        if( rs2::frame frame = streams[stream].texture->get_last_frame() )
                        {
                            auto & motion = *reinterpret_cast< const rs2_combined_motion * >( frame.get_data() );
                            auto height = stream_mv.show_stream_imu( font1,
                                                                     stream_rect,
                                                                     { (float)motion.linear_acceleration.x,
                                                                       (float)motion.linear_acceleration.y,
                                                                       (float)motion.linear_acceleration.z },
                                                                     mouse,
                                                                     "Meter/Sec^2",
                                                                     "Linear Acceletion" );
                            stream_mv.show_stream_imu( font1,
                                                       stream_rect,
                                                       { (float)motion.angular_velocity.x,
                                                         (float)motion.angular_velocity.y,
                                                         (float)motion.angular_velocity.z },
                                                       mouse,
                                                       "Radians/Sec",
                                                       "Angular Velocity",
                                                       height + 9 );
                        }
                        break;

                    case RS2_STREAM_POSE:
                    {
                        if (streams[stream].show_stream_details)
                        {
                            auto pose = streams[stream].texture->get_last_frame().as<pose_frame>();
                            if (pose.get())
                            {
                                auto pose_data = pose.get_pose_data();
                                if (stream_rect.contains(win.get_mouse().cursor))
                                    stream_mv.show_stream_pose(font1,
                                    stream_rect, pose_data,
                                    stream_type, (*this).fullscreen, 30.0f, *this);
                            }
                        }

                        break;
                    }
                    case RS2_STREAM_OCCUPANCY:
                        auto frame = streams[stream].texture->get_last_frame();
                        auto zoom = streams[stream].dev->normalized_zoom.w;
                        if (frame && frame.get_data() && streams[stream].show_safety_zones_2d && zoom == 1)
                        {
                            draw_zone_2d(Zone::Diagnostic, stream_rect, frame);
                            draw_zone_2d(Zone::Warning, stream_rect, frame);
                            draw_zone_2d(Zone::Danger, stream_rect, frame);
                        }
                        break;
                }
            }

            //switch( stream_mv.profile.stream_type() )
            {
                static std::vector< std::pair< ImColor, bool > > colors =
                {
                    { ImColor( 1.f, 1.f, 1.f, 1.f ), false },  // the default color
                    { ImColor( 1.f, 0.f, 0.f, 1.f ), false },
                    { ImColor( 0.f, 1.f, 0.f, 1.f ), false },
                    { ImColor( 0.f, 0.f, 1.f, 1.f ), false },
                    { ImColor( 1.f, 0.f, 1.f, 1.f ), false },
                    { ImColor( 1.f, 1.f, 0.f, 1.f ), false },
                };
                typedef size_t ColorIdx;
                static std::map< size_t, ColorIdx > id2color;

                // Returns the color (from those pre-defined above) for the given object, based on its ID
                auto get_color = [&]( object_in_frame const & object ) -> ImColor
                {
                    ColorIdx & color = id2color[object.id];  // Creates it with 0 as default if not already there
                    if( color < int( colors.size() ))
                    {
                        if( color > 0 )
                            return colors[color].first;  // Return an already-assigned color
                        // Find the next available color
                        size_t x = 0;
                        for( auto & p : colors )
                        {
                            bool & in_use = p.second;
                            if( !in_use )
                            {
                                in_use = true;
                                color = x;
                                return p.first;
                            }
                            ++x;
                        }
                        // No available color; use the default and mark the object so we don't do this again
                        color = 100;
                    }
                    // If we're here it's because there're more objects than colors
                    return colors[0].first;
                };

                glColor3f( 1, 1, 1 );
                auto p_objects = stream_mv.dev->detected_objects;
                std::lock_guard< std::mutex > lock( p_objects->mutex );

                // Mark colors that are no longer in use
                for( auto it = id2color.begin(); it != id2color.end(); )
                {
                    size_t id = it->first;
                    bool found = false;
                    for( object_in_frame & object : *p_objects )
                    {
                        if( object.id == id )
                        {
                            found = true;
                            break;
                        }
                    }
                    if( !found )
                    {
                        colors[it->second].second = false;  // no longer in use
                        auto it_to_erase = it++;
                        id2color.erase( it_to_erase );
                    }
                    else
                    {
                        ++it;
                    }
                }


                for( object_in_frame & object : *p_objects )
                {
                    rect const & normalized_bbox = stream_mv.profile.stream_type() == RS2_STREAM_DEPTH
                        ? object.normalized_depth_bbox
                        : object.normalized_color_bbox;
                    rect bbox = normalized_bbox.unnormalize( stream_rect );
                    bbox.grow( 10, 5 );  // Allow more text, and easier identification of the face

                    float const max_depth = 2.f;
                    float const min_depth = 0.8f;
                    float const depth_range = max_depth - min_depth;
                    float usable_depth = std::min( object.mean_depth, max_depth );
                    float a = 0.75f * (max_depth - usable_depth) / depth_range + 0.25f;

                    // Don't draw text in boxes that are too small...
                    auto h = bbox.h;
                    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.f, 1.f, 1.f, a ) );
                    ImColor bg( dark_sensor_bg.x, dark_sensor_bg.y, dark_sensor_bg.z, dark_sensor_bg.w * a );

                    if( fabs(object.mean_depth) > 0.f )
                    {
                        std::string str = rsutils::string::from() << std::setprecision( 2 ) << object.mean_depth << " m";
                        auto size = ImGui::CalcTextSize( str.c_str() );
                        if( size.y < h  &&  size.x < bbox.w )
                        {
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                { bbox.x + 1, bbox.y + 1 },
                                { bbox.x + size.x + 20, bbox.y + size.y + 6 },
                                bg );
                            ImGui::SetCursorScreenPos( { bbox.x + 10, bbox.y + 3 } );
                            ImGui::Text("%s",  str.c_str() );
                            h -= size.y;
                        }
                    }
                    if( ! object.name.empty() )
                    {
                        auto size = ImGui::CalcTextSize( object.name.c_str() );
                        if( size.y < h  &&  size.x < bbox.w )
                        {
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                { bbox.x + bbox.w - size.x - 20, bbox.y + bbox.h - size.y - 6 },
                                { bbox.x + bbox.w - 1, bbox.y + bbox.h - 1 },
                                bg );
                            ImGui::SetCursorScreenPos( { bbox.x + bbox.w - size.x - 10, bbox.y + bbox.h - size.y - 4 } );
                            ImGui::Text("%s",  object.name.c_str() );
                            h -= size.y;
                        }
                    }

                    ImGui::PopStyleColor();

                    // The rectangle itself is always drawn, in the same color as the text
                    auto frame_color = get_color( object );
                    glColor3f( a * frame_color.Value.x, a * frame_color.Value.y, a * frame_color.Value.z );
                    draw_rect( bbox );
                }
            }

            glColor3f(header_window_bg.x, header_window_bg.y, header_window_bg.z);
            stream_rect.y -= 32;
            stream_rect.h += 32;
            stream_rect.w += 1;
            draw_rect(stream_rect);

            auto frame = streams[stream].texture->get_last_frame().as<video_frame>();
            auto textured_frame = streams[stream].texture->get_last_frame(true).as<video_frame>();
            if (streams[stream].show_map_ruler && frame && textured_frame &&
                RS2_STREAM_DEPTH == stream_mv.profile.stream_type() &&
                RS2_FORMAT_Z16 == stream_mv.profile.format())
            {
                if(RS2_FORMAT_RGB8 == textured_frame.get_profile().format())
                {
                    static const std::string depth_units = "m";
                    float ruler_length = 0.f;
                    auto depth_vid_profile = stream_mv.profile.as<video_stream_profile>();
                    auto depth_width = depth_vid_profile.width();
                    auto depth_height = depth_vid_profile.height();
                    auto depth_data = static_cast<const uint16_t*>(frame.get_data());
                    auto textured_depth_data = static_cast<const uint8_t*>(textured_frame.get_data());
                    static const auto skip_pixels_factor = 30;
                    std::vector<rgb_per_distance> rgb_per_distance_vec;
                    std::vector<float> distances;
                    for (uint64_t i = 0; i < depth_height; i+= skip_pixels_factor)
                    {
                        for (uint64_t j = 0; j < depth_width; j+= skip_pixels_factor)
                        {
                            auto depth_index = i*depth_width + j;
                            auto length = depth_data[depth_index] * stream_mv.dev->depth_units;
                            if (length > 0.f)
                            {
                                auto textured_depth_index = depth_index * 3;
                                auto r = textured_depth_data[textured_depth_index];
                                auto g = textured_depth_data[textured_depth_index + 1];
                                auto b = textured_depth_data[textured_depth_index + 2];
                                rgb_per_distance_vec.push_back({ length, { r, g, b } });
                                distances.push_back(length);
                            }
                        }
                    }

                    if (!distances.empty())
                    {
                        ruler_length = calculate_ruler_max_distance(distances);
                        draw_color_ruler(mouse, streams[stream], stream_rect, rgb_per_distance_vec, ruler_length, depth_units);
                    }
                }
            }
        }
    }

    void viewer_model::render_3d_view(const rect& viewer_rect, ux_window& win,
        std::shared_ptr<texture_buffer> texture, rs2::points points, rs2::labeled_points labeled_points)
    {
        auto top_bar_height = 60.f;

        if (points)
        {
            last_points = points;
        }
        if (texture)
        {
            last_texture = texture;
        }

        if (labeled_points)
        {
            last_labeled_points = labeled_points;
        }

        auto bottom_y = win.framebuf_height() - viewer_rect.y - viewer_rect.h;

        glViewport(static_cast<GLint>(non_negative(viewer_rect.x)), static_cast<GLint>(non_negative(bottom_y)),
            static_cast<GLsizei>(non_negative(viewer_rect.w)), static_cast<GLsizei>(non_negative(viewer_rect.h - top_bar_height)));

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLoadIdentity();

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        gluPerspective(45, non_negative(viewer_rect.w / win.framebuf_height()), 0.001f, 100.0f);
        matrix4 perspective_mat;
        glGetFloatv(GL_PROJECTION_MATRIX, perspective_mat);
        glPopMatrix();

        check_gl_error();

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadMatrixf((float*)perspective_mat.mat);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf(view);

        matrix4 view_mat;
        memcpy(&view_mat, view, sizeof(matrix4));

        glDisable(GL_TEXTURE_2D);

        glEnable(GL_DEPTH_TEST);

        glDepthMask(GL_FALSE);
        if (show_skybox) _skybox.render(pos);
        glDepthMask(GL_TRUE);

        auto r1 = matrix4::identity();
        auto r2 = matrix4::identity();

        if (draw_plane && !paused)
        {
            glPushAttrib(GL_LINE_BIT);
            glLineWidth(2);
            glBegin(GL_LINES);

            if (is_valid(roi_rect))
            {
                glColor4f(yellow.x, yellow.y, yellow.z, 1.f);

                auto rects = subdivide(roi_rect);
                for (auto&& r : rects)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        auto j = (i + 1) % 4;
                        glVertex3f(r[i].x, r[i].y, r[i].z);
                        glVertex3f(r[j].x, r[j].y, r[j].z);
                    }
                }
            }

            glEnd();
            glPopAttrib();
        }

        {
            float tiles = 24;
            if (!metric_system) tiles *= 1.f / FEET_TO_METER;

            glTranslatef(0, 0, -1);
            glLineWidth(1);

            // Render "floor" grid
            glBegin(GL_LINES);

            auto T = tiles * 0.5f;

            if (!metric_system) T *= FEET_TO_METER;

            for (int i = 0; i <= ceil(tiles); i++)
            {
                float I = float(i);
                if (!metric_system) I *= FEET_TO_METER;

                if (i == tiles / 2) glColor4f(0.7f, 0.7f, 0.7f, 1.f);
                else glColor4f(0.4f, 0.4f, 0.4f, 1.f);

                glVertex3f(I - T, 1, -T);
                glVertex3f(I - T, 1, T);
                glVertex3f(-T, 1, I - T);
                glVertex3f(T, 1, I - T);
            }
            glEnd();
        }

        check_gl_error();

        if (!_pc_selected)
        {
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadMatrixf(r1 * view_mat);
            texture_buffer::draw_axes(0.4f, 1);
            glPopMatrix();
        }

        check_gl_error();

        glColor4f(1.f, 1.f, 1.f, 1.f);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf(r1 * view_mat);
        if (draw_frustrum && last_points)
        {
            glLineWidth(1.f);
            glBegin(GL_LINES);

            auto intrin = last_points.get_profile().as<video_stream_profile>().get_intrinsics();

            if (_pc_selected) glColor4f(light_blue.x, light_blue.y, light_blue.z, 0.5f);
            else glColor4f(sensor_bg.x, sensor_bg.y, sensor_bg.z, 0.5f);

            for (float d = 1; d < 6; d += 2)
            {
                auto get_point = [&](float x, float y) -> float3
                {
                    float point[3];
                    float pixel[2]{ x, y };
                    rs2_deproject_pixel_to_point(point, &intrin, pixel, d);
                    glVertex3f(0.f, 0.f, 0.f);
                    glVertex3fv(point);
                    return{ point[0], point[1], point[2] };
                };

                auto top_left = get_point(0, 0);
                auto top_right = get_point(static_cast<float>(intrin.width), 0);
                auto bottom_right = get_point(static_cast<float>(intrin.width), static_cast<float>(intrin.height));
                auto bottom_left = get_point(0, static_cast<float>(intrin.height));

                glVertex3fv(&top_left.x); glVertex3fv(&top_right.x);
                glVertex3fv(&top_right.x); glVertex3fv(&bottom_right.x);
                glVertex3fv(&bottom_right.x); glVertex3fv(&bottom_left.x);
                glVertex3fv(&bottom_left.x); glVertex3fv(&top_left.x);
            }

            glEnd();

            glColor4f(1.f, 1.f, 1.f, 1.f);
        }

        check_gl_error();

        if (last_points && last_texture)
        {
            auto vf_profile = last_points.get_profile().as<video_stream_profile>();
            // Non-linear correspondence customized for non-flat surface exploration

            if (vf_profile.width() <= 0)
                throw std::runtime_error("Profile width must be greater than 0.");

            glPointSize(std::sqrt(viewer_rect.w / vf_profile.width()));

            auto tex = last_texture->get_gl_handle();
            glBindTexture(GL_TEXTURE_2D, tex);
            glEnable(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_border_mode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_border_mode);

            _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_FILLED, selected_shader != shader_type::points ? 1.f : 0.f);
            _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_SHADED, selected_shader == shader_type::diffuse ? 1.f : 0.f);

            _pc_renderer.set_matrix(RS2_GL_MATRIX_CAMERA, r2 * view_mat);
            _pc_renderer.set_matrix(RS2_GL_MATRIX_PROJECTION, perspective_mat);

            _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_SELECTED, _pc_selected ? 1.f : 0.f);

            auto viewport_rect = viewer_rect;

            auto cursor = win.get_mouse().cursor;
            auto scaled_cursor = cursor;
            scaled_cursor.x *= win.get_scale_factor();
            scaled_cursor.y *= win.get_scale_factor();
            if (viewport_rect.contains(scaled_cursor))
            {
                _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_MOUSE_PICK, 1.f);
                _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_SCALE_FACTOR, win.get_scale_factor());

                _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_MOUSE_X, cursor.x);
                _pc_renderer.set_option(gl::pointcloud_renderer::OPTION_MOUSE_Y, win.framebuf_height() / win.get_scale_factor() - cursor.y);
            }

            // Render Point-Cloud
            last_points.apply_filter(_pc_renderer);

            if (_pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_ID) > 0.f)
            {
                float3 p {
                    _pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_X),
                    _pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_Y),
                    _pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_Z),
                };
                float3 normal {
                    _pc_renderer.get_option(gl::pointcloud_renderer::OPTION_NORMAL_X),
                    _pc_renderer.get_option(gl::pointcloud_renderer::OPTION_NORMAL_Y),
                    _pc_renderer.get_option(gl::pointcloud_renderer::OPTION_NORMAL_Z),
                };
                _measurements.mouse_pick(win, p, normal);

                // Adjust track-ball controller based on picked position
                // 1. Place target at the closest point to p, along (pos, target) interval
                // 2. When zooming-in, move camera target and position toward p
                if (!win.get_mouse().mouse_down[0])
                {
                    auto x1x2 = target - pos;
                    auto x1x0 = p - pos;
                    auto t = (x1x2 * x1x0) / (x1x2 * x1x2);
                    auto p1 = pos + x1x2* t;

                    if (t > 0) { // Don't adjust if pointcloud is behind us
                        target = lerp(p1, target, 0.9f);

                        if (win.get_mouse().mouse_wheel > 0)
                        {
                            pos = lerp(p, pos, 0.9f);
                            target = lerp(p, target, 0.9f);
                        }
                    }
                }

                //try_select_pointcloud(win);
            }

            glDisable(GL_TEXTURE_2D);

            _cam_renderer.set_matrix(RS2_GL_MATRIX_CAMERA, r2 * view_mat);
            _cam_renderer.set_matrix(RS2_GL_MATRIX_PROJECTION, perspective_mat);

            if (streams.find(selected_depth_source_uid) != streams.end())
            {
                auto source_frame = streams[selected_depth_source_uid].texture->get_last_frame();
                if (source_frame)
                {
                    glDisable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);

                    glBlendFunc(GL_ONE, GL_ONE);

                    // _cam_renderer.set_option(gl::camera_renderer::OPTION_SELECTED, _pc_selected ? 1.f : 0.f);

                    // if (viewer_rect.contains(cursor))
                    // {
                    //     _cam_renderer.set_option(gl::camera_renderer::OPTION_MOUSE_PICK, 1.f);
                    //     _cam_renderer.set_option(gl::camera_renderer::OPTION_MOUSE_X, cursor.x);
                    //     _cam_renderer.set_option(gl::camera_renderer::OPTION_MOUSE_Y, cursor.y);
                    // }

                    // only display camera when it does not occlude point cloud
                    origin_occluded.add_value(_pc_renderer.get_option(gl::pointcloud_renderer::OPTION_ORIGIN_PICKED) <= 0.f);
                    _cam_renderer.set_option(RS2_OPTION_FILTER_MAGNITUDE, powf(origin_occluded.get_stat(), 2.f));
                    source_frame.apply_filter(_cam_renderer);

                    // if (_cam_renderer.get_option(gl::camera_renderer::OPTION_WAS_PICKED) > 0.f)
                    // {
                    //     try_select_pointcloud(win);
                    // }

                    glDisable(GL_BLEND);
                    glEnable(GL_DEPTH_TEST);
                }
            }
        }

        check_gl_error();

        if (last_labeled_points)
        {
            draw_3d_labeled_points(viewer_rect, last_labeled_points);
        }

        _measurements.draw(win);

        glPopMatrix();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glDisable(GL_DEPTH_TEST);

        if (ImGui::GetIO().KeysDown[ImGuiKey_R])
        {
            reset_camera();
        }
    }

    bool viewer_model::should_render_frame(const rs2::stream_model& model) const
    {
        if (model.profile.stream_type() == RS2_STREAM_SAFETY)
            return false;

        return true;
    }

    void viewer_model::show_top_bar(ux_window& window, const rect& viewer_rect, const device_models_list& devices)
    {
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::SetNextWindowPos({ panel_width, 0 });
        ImGui::SetNextWindowSize({ window.width() - panel_width, panel_y });

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, button_color);
        ImGui::Begin("Toolbar Panel", nullptr, flags);

        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Border, black);

        int buttons = window.is_fullscreen() ? 4 : 3;

        ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons));
        ImGui::PushStyleColor(ImGuiCol_Text, is_3d_view ? light_grey : light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, is_3d_view ? light_grey : light_blue);
        if (ImGui::Button("2D", { panel_y, panel_y }))
        {
            is_3d_view = false;
            config_file::instance().set(configurations::viewer::is_3d_view, is_3d_view);
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons - 1));
        auto pos1 = ImGui::GetCursorScreenPos();

        ImGui::PushStyleColor(ImGuiCol_Text, !is_3d_view ? light_grey : light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, !is_3d_view ? light_grey : light_blue);
        if (ImGui::Button("3D", { panel_y,panel_y }))
        {
            is_3d_view = true;
            config_file::instance().set(configurations::viewer::is_3d_view, is_3d_view);
            update_3d_camera(window, viewer_rect, true);
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons - 2));

        static bool settings_open = false;
        ImGui::PushStyleColor(ImGuiCol_Text, !settings_open ? light_grey : light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, !settings_open ? light_grey : light_blue);

        if (ImGui::Button(textual_icons::cog, { panel_y,panel_y }))
        {
            ImGui::OpenPopup("More Options");
        }
        ImGui::PopFont();
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", "More Options...");
        }

        if (window.is_fullscreen())
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons - 4));

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_color);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_red);
            if (ImGui::Button(textual_icons::power_off, { panel_y,panel_y }))
            {
                exit(0);
            }
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("Exit the App");
                window.link_hovered();
            }
            ImGui::PopStyleColor(3);
        }


        ImGui::PushStyleColor(ImGuiCol_Text, black);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));

        ImGui::PushFont(window.get_font());

        const char* menu_items[] = { "Report Issue", "RS Store", "Settings", "About" };
        bool open_settings_popup = false;
        bool open_about_popup = false;

        ImGui::SetNextWindowPos({ window.width() - 130, panel_y });
        auto separator_height = 2;
        auto menu_items_len = sizeof( menu_items ) / sizeof( menu_items[0] );
        auto popup_height = ( ImGui::GetTextLineHeightWithSpacing() + 2 ) * menu_items_len + separator_height;
        ImVec2 popup_size = { 6.f * window.get_font_size(), popup_height };
        ImGui::SetNextWindowSize( popup_size );

        if (ImGui::BeginPopup("More Options"))
        {
            settings_open = true;

            if( ImGui::Selectable( menu_items[0] ) )
            {
                open_issue(devices);
            }

            if( ImGui::Selectable( menu_items[1] ) )
            {
                open_url("https://store.realsenseai.com/");
            }

            if( ImGui::Selectable( menu_items[2] ) )
            {
                open_settings_popup = true;
            }

            ImGui::Separator();

            if( ImGui::Selectable( menu_items[3] ) )
            {
                open_about_popup = true;
            }

            ImGui::EndPopup();
        }
        else
        {
            settings_open = false;
        }

        static config_file temp_cfg;
        static bool reload_required = false;
        static bool refresh_required = false;
        static bool refresh_updates = false;

        static int tab = 0;

        if (open_settings_popup)
        {
            _measurements.disable();
            temp_cfg = config_file::instance();
            ImGui::OpenPopup(menu_items[2]);
            reload_required = false;
            refresh_required = false;
            tab = config_file::instance().get_or_default(configurations::viewer::settings_tab, 0);
        }

        {
            float w  = window.width()  * 0.6f;
            float h  = window.height() * 0.6f;
            float x0 = window.width()  * 0.2f;
            float y0 = window.height() * 0.2f;
            ImGui::SetNextWindowPos({ x0, y0 });
            ImGui::SetNextWindowSize({ w, h });

            flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

            RsImGui_ScopePushFont(window.get_font());
            ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

            if (ImGui::BeginPopupModal(menu_items[2], nullptr, flags))
            {
                if (ImGui::IsWindowHovered()) window.set_hovered_over_input();

                ImGui::SetCursorScreenPos({ (float)(x0 + w / 2 - 280), (float)(y0 + 27) });
                ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                ImGui::PushFont(window.get_large_font());

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 0 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 0 ? light_grey : light_blue);
                if (ImGui::Button("Playback & Record", { 170, 30}))
                {
                    tab = 0;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 1 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 1 ? light_grey : light_blue);
                if (ImGui::Button("Performance", { 150, 30}))
                {
                    tab = 1;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 2 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 2 ? light_grey : light_blue);
                if (ImGui::Button("General", { 100, 30}))
                {
                    tab = 2;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 3 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 3 ? light_grey : light_blue);

                if (ImGui::Button("Updates", { 120, 30 }))
                {
                    tab = 3;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }

                ImGui::PopStyleColor(2);
                ImGui::PopFont();
                ImGui::PopStyleColor(2); // button color

                ImGui::SetCursorScreenPos({ (float)(x0 + 15), (float)(y0 + 65) });
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, white);
                ImGui::PushStyleColor(ImGuiCol_PopupBg, dark_window_background);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
                ImVec2 child_size = ImVec2(0, ImGui::GetContentRegionAvail().y-50);
                if (ImGui::BeginChild("ScrollableRegion", child_size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
                {
                if (tab == 0)
                {
                    int recording_setting = temp_cfg.get(configurations::record::file_save_mode);
                    ImGui::Text("When starting a new recording:");
                    if (ImGui::RadioButton("Select filename automatically", recording_setting == 0))
                    {
                        recording_setting = 0;
                        temp_cfg.set(configurations::record::file_save_mode, recording_setting);
                    }
                    if (ImGui::RadioButton("Ask me every time", recording_setting == 1))
                    {
                        recording_setting = 1;
                        temp_cfg.set(configurations::record::file_save_mode, recording_setting);
                    }
                    ImGui::Text("Default recording folder: ");
                    ImGui::SameLine();
                    static char path[256];
                    memset(path, 0, 256);
                    std::string path_str = temp_cfg.get(configurations::record::default_path);
                    memcpy(path, path_str.c_str(), std::min(255, (int)path_str.size()));

                    if (ImGui::InputText("##default_record_path", path, 255))
                    {
                        path_str = path;
                        temp_cfg.set(configurations::record::default_path, path_str);
                    }

                    ImGui::Separator();

                    ImGui::Text("ROS-bag Compression:");
                    int recording_compression = temp_cfg.get(configurations::record::compression_mode);
                    if (ImGui::RadioButton("Always Compress (might cause frame drops)", recording_compression == 0))
                    {
                        recording_compression = 0;
                        temp_cfg.set(configurations::record::compression_mode, recording_compression);
                    }
                    if (ImGui::RadioButton("Never Compress (larger .bag file size)", recording_compression == 1))
                    {
                        recording_compression = 1;
                        temp_cfg.set(configurations::record::compression_mode, recording_compression);
                    }
                    if (ImGui::RadioButton("Use device defaults", recording_compression == 2))
                    {
                        recording_compression = 2;
                        temp_cfg.set(configurations::record::compression_mode, recording_compression);
                    }
                }

                if (tab == 1)
                {
                    // Font oversample slider
                    int font_samples = temp_cfg.get(configurations::performance::font_oversample);
                    ImGui::Text("Font Samples: ");
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Increased font samples produce nicer text, but require more GPU memory, sometimes resulting in boxes instead of font characters");

                    ImGui::SameLine();
                    ImGui::PushItemWidth(80);
                    float slider_position_x = ImGui::GetCursorPosX();

                    if (ImGui::SliderInt("##font_samples", &font_samples, 1, 8))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::font_oversample, font_samples);
                    }
                    ImGui::PopItemWidth();

                    // Font slider
                    int font_size = temp_cfg.get( configurations::window::font_size );
                    ImGui::Text( "Font Size: " );
                    if( ImGui::IsItemHovered() )
                        RsImGui::CustomTooltip( "Viewer Font Size" );
                    
                    ImGui::SameLine();
                    ImGui::PushItemWidth( 80 );
                    ImGui::SetCursorPosX( slider_position_x );

                    if( ImGui::SliderInt( "##font_size", &font_size, 16, 20 ) )
                    {
                        reload_required = true;
                        temp_cfg.set( configurations::window::font_size, font_size );
                    }
                    ImGui::PopItemWidth();

#ifndef __APPLE__ // Not available at the moment on Mac
                    bool gpu_rendering = temp_cfg.get(configurations::performance::glsl_for_rendering);
                    if (ImGui::Checkbox("Use GLSL for Rendering", &gpu_rendering))
                    {
                        refresh_required = true;
                        temp_cfg.set(configurations::performance::glsl_for_rendering, gpu_rendering);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Using OpenGL 3 shaders is a widely supported way to boost rendering speeds on modern GPUs.");

                    bool gpu_processing = temp_cfg.get(configurations::performance::glsl_for_processing);
                    if (ImGui::Checkbox("Use GLSL for Processing", &gpu_processing))
                    {
                        refresh_required = true;
                        temp_cfg.set(configurations::performance::glsl_for_processing, gpu_processing);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Using OpenGL 3 shaders for depth data processing can reduce CPU utilisation.");

                    if (gpu_processing && !gpu_rendering)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                        std::string excl_icon_str = std::string(rsutils::string::from()
                            << textual_icons::exclamation_triangle
                            << " Using GLSL for processing but not for rendering can reduce CPU utilisation, but is likely to hurt overall performance!");
                        ImGui::Text("%s", excl_icon_str.c_str());
                        ImGui::PopStyleColor();
                    }
#endif
                    bool msaa = temp_cfg.get(configurations::performance::enable_msaa);
                    if (ImGui::Checkbox("Enable Multisample Anti-Aliasing (MSAA)", &msaa))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::enable_msaa, msaa);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("MSAA will improve the rendering quality of edges at expense of greater GPU memory utilisation.");

                    if (msaa)
                    {
                        int samples = temp_cfg.get(configurations::performance::msaa_samples);
                        ImGui::Text("MSAA Samples: "); ImGui::SameLine();
                        ImGui::PushItemWidth(160);
                        if (ImGui::SliderInt("##samples", &samples, 2, 16))
                        {
                            reload_required = true;
                            temp_cfg.set(configurations::performance::msaa_samples, samples);
                        }
                        ImGui::PopItemWidth();
                    }

                    bool show_fps = temp_cfg.get(configurations::performance::show_fps);
                    if (ImGui::Checkbox("Show Application FPS (rendering FPS)", &show_fps))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::show_fps, show_fps);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Show application refresh rate in window title\nThis rate is unrelated to camera FPS and measures application responsivness");


                    bool vsync = temp_cfg.get(configurations::performance::vsync);
                    if (ImGui::Checkbox("Enable VSync", &vsync))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::vsync, vsync);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Vertical sync will try to synchronize application framerate to the monitor refresh-rate (usually limiting the framerate to 60)");

                    bool fullscreen = temp_cfg.get(configurations::window::is_fullscreen);
                    if (ImGui::Checkbox("Fullscreen (F8)", &fullscreen))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::window::is_fullscreen, fullscreen);
                    }

                    bool show_skybox = temp_cfg.get(configurations::performance::show_skybox);
                    if (ImGui::Checkbox("Show Skybox in 3D View", &show_skybox))
                    {
                        temp_cfg.set(configurations::performance::show_skybox, show_skybox);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("When enabled, this option provides background to the 3D view, instead of leaving it blank.\nThis is purely cosmetic");

                    bool enable_occlusion_invalidation = temp_cfg.get(configurations::performance::occlusion_invalidation);
                    if (ImGui::Checkbox("Perform Occlusion Invalidation", &enable_occlusion_invalidation))
                    {
                        temp_cfg.set(configurations::performance::occlusion_invalidation, enable_occlusion_invalidation);
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Occlusions are a natural side-effect of having multiple sensors\nWhen this option is enabled, the SDK will filter out occluded pixels");
                }

                if( tab == 2 )
                {
                    ImGui::Text( "Units of Measurement: " );
                    ImGui::SameLine();

                    int metric_system = temp_cfg.get( configurations::viewer::metric_system );
                    std::vector<std::string> unit_systems;
                    unit_systems.push_back( "Imperial System" );
                    unit_systems.push_back( "Metric System" );

                    ImGui::PushItemWidth( 150 );
                    if( draw_combo_box( "##units_system", unit_systems, metric_system ) )
                    {
                        temp_cfg.set( configurations::viewer::metric_system, metric_system );
                    }
                    ImGui::PopItemWidth();

                    ImGui::Separator();

                    ImGui::Text( "librealsense has built-in logging capabilities." );
                    ImGui::Text( "Logs may contain API calls, timing of frames, OS error messages and file-system links, but no actual frame content." );

                    bool log_to_console = temp_cfg.get( configurations::viewer::log_to_console );
                    if( _disable_log_to_console )
                        ImGui::PushStyleVar( ImGuiStyleVar_Alpha, 0.6f );
                    if( ImGui::Checkbox( "Output librealsense log to console", &log_to_console ) )
                        temp_cfg.set( configurations::viewer::log_to_console, log_to_console );
                    if( _disable_log_to_console )
                    {
                        ImGui::PopStyleVar();
                        if( ImGui::IsItemHovered() )
                        {
                            RsImGui::CustomTooltip( "%s", "--debug was specified; this cannot be applied without a restart" );
                        }
                    }
                    bool log_to_file = temp_cfg.get(configurations::viewer::log_to_file);
                    if (ImGui::Checkbox("Output librealsense log to file", &log_to_file))
                    {
                        temp_cfg.set(configurations::viewer::log_to_file, log_to_file);
                    }
                    if (log_to_file)
                    {
                        ImGui::Text("Log file name:");
                        ImGui::SameLine();
                        static char logpath[256];
                        memset(logpath, 0, 256);
                        std::string path_str = temp_cfg.get(configurations::viewer::log_filename);
                        memcpy(logpath, path_str.c_str(), std::min(255, (int)path_str.size()));

                        if (ImGui::InputText("##default_log_path", logpath, 255))
                        {
                            path_str = logpath;
                            temp_cfg.set(configurations::viewer::log_filename, path_str);
                        }
                    }
                    if (log_to_console || log_to_file)
                    {
                        int new_severity = temp_cfg.get(configurations::viewer::log_severity);

                        std::vector<std::string> severities;
                        for (int i = 0; i < RS2_LOG_SEVERITY_COUNT; i++)
                            severities.push_back(rs2_log_severity_to_string((rs2_log_severity)i));

                        ImGui::Text("Minimal log severity:");
                        ImGui::SameLine();

                        ImGui::PushItemWidth(150);
                        if (draw_combo_box("##log_severity", severities, new_severity))
                        {
                            temp_cfg.set(configurations::viewer::log_severity, new_severity);
                        }
                        ImGui::PopItemWidth();
                    }


                    ImGui::Separator();

                    {
                        ImGui::Text("Commands.xml Path:");
                        ImGui::SameLine();
                        static char logpath[256];
                        memset(logpath, 0, 256);
                        std::string path_str = temp_cfg.get(configurations::viewer::commands_xml);
                        memcpy(logpath, path_str.c_str(), std::min(255, (int)path_str.size()));

                        if (ImGui::InputText("##commands_xml_path", logpath, 255))
                        {
                            path_str = logpath;
                            temp_cfg.set(configurations::viewer::commands_xml, path_str);
                        }
                    }

                    {
                        ImGui::Text("FW logs XML file:");
                        ImGui::SameLine();
                        static char logpath[256];
                        memset(logpath, 0, 256);
                        std::string path_str = temp_cfg.get(configurations::viewer::hwlogger_xml);
                        memcpy(logpath, path_str.c_str(), std::min(255, (int)path_str.size()));

                        if (ImGui::InputText("##fw_log_xml_path", logpath, 255))
                        {
                            path_str = logpath;
                            temp_cfg.set(configurations::viewer::hwlogger_xml, path_str);
                        }

                        ImGui::SameLine();
                        if( ImGui::Button( "FW logs XML" ) )
                        {
                            auto ret = file_dialog_open(open_file, "XML file\0*.xml\0", NULL, NULL);
                            if( ret )
                            {
                                memset( logpath, 0, 256 );
                                memcpy( logpath, ret, std::min( 255, static_cast< int >( strlen( ret ) ) ) );
                                path_str = logpath;
                                temp_cfg.set( configurations::viewer::hwlogger_xml, path_str );
                            }
                        }
                    }

                    ImGui::Separator();

                    ImGui::Text("RealSense tools settings capture the state of UI, and not of the hardware:");

                    if (ImGui::Button(" Restore Defaults "))
                    {
                        reload_required = true;
                        temp_cfg = config_file();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(" Export Settings "))
                    {
                        auto ret = file_dialog_open(save_file, "JavaScript Object Notation (JSON)\0*.json\0", NULL, NULL);
                        if (ret)
                        {
                            try
                            {
                                std::string filename = ret;
                                filename = rsutils::string::to_lower(filename);
                                if (!ends_with(filename, ".json")) filename += ".json";
                                temp_cfg.save(filename.c_str());
                            }
                            catch (...){}
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(" Import Settings "))
                    {
                        auto ret = file_dialog_open(open_file, "JavaScript Object Notation (JSON)\0*.json\0", NULL, NULL);
                        if (ret)
                        {
                            try
                            {
                                config_file file(ret);
                                temp_cfg = file;
                                reload_required = true;
                            }
                            catch (...){}
                        }
                    }

                                        
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                    std::string excl_icon_str = rsutils::string::from() << textual_icons::exclamation_triangle
                                                << " The following changes will take effect only after restarting the application";
                    ImGui::Text( "%s", excl_icon_str.c_str() );
                    bool allow_partial_device = temp_cfg.get_nested< bool >( "context.partial-device-allowed", false );
                    if( ImGui::Checkbox( "Allow partial device initialization", &allow_partial_device ) )
                    {
                        temp_cfg.set_nested( "context.partial-device-allowed", allow_partial_device );
                    }
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip( "%s", "In case of incomplete device initalization\n"
                                                "allow a device with partial capabilities to be used" );
                    }
                    bool enable_dds = temp_cfg.get_nested<bool>("context.dds.enabled" , false);
                    if( ImGui::Checkbox( "Enable DDS", &enable_dds ) )
                    {
                        temp_cfg.set_nested("context.dds.enabled", enable_dds);
                    }
                    if( enable_dds )
                    {
                        int domain_id = temp_cfg.get_nested< int >( "context.dds.domain", 0 );
                        ImGui::SameLine();
                        ImGui::SetCursorPosX( ImGui::GetCursorPosX() + 50 );
                        ImGui::PushItemWidth( 150.0f );
                        ImGui::Text( "Domain ID" );
                        ImGui::SameLine();
                        if( ImGui::InputInt( "##Domain ID", &domain_id ) )
                        {
                            if( domain_id < 0 )
                                domain_id = 0;
                            else if( domain_id > 232 )
                                domain_id = 232;
                            temp_cfg.set_nested("context.dds.domain", domain_id);
                        }
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
                }

                if (tab == 3)
                {
                    bool recommend_fw_updates = temp_cfg.get(configurations::update::recommend_updates);
                    if (ImGui::Checkbox("Recommend Bundled Firmware", &recommend_fw_updates))
                    {
                        temp_cfg.set(configurations::update::recommend_updates, recommend_fw_updates);
                        refresh_updates = true;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("%s", "When firmware of the device is below the version bundled with this software release\nsuggest firmware update");
                    }
#ifdef CHECK_FOR_UPDATES
                    ImGui::Separator();

                    ImGui::Text("%s", "SW/FW Updates From Server:");
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("%s", "Select the server URL of the SW/FW updates information");
                    }
                    ImGui::SameLine();

                    static bool official_url(temp_cfg.get(configurations::update::sw_updates_official_server));
                    static char custom_url[256] = { 0 };
                    static std::string url_str = (temp_cfg.get(configurations::update::sw_updates_url));
                    memcpy(custom_url, url_str.c_str(), std::min(255, (int)url_str.size()));
                    if (ImGui::RadioButton("Official Server", official_url))
                    {
                        official_url = true;
                        temp_cfg.set(configurations::update::sw_updates_url, server_versions_db_url);
                        temp_cfg.set(configurations::update::sw_updates_official_server, true);
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Custom Server", !official_url))
                    {
                        official_url = false;
                        temp_cfg.set(configurations::update::sw_updates_official_server, false);
                        
                        // Load last saved custom URL
                        url_str = custom_url;
                        temp_cfg.set(configurations::update::sw_updates_url, url_str);

                    }
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("%s", "Add file:// prefix to use a local DB file ");
                    }

                    if (!official_url)
                    {
                        if (ImGui::InputText("##custom_server_url", custom_url, 255))
                        {
                            url_str = custom_url;
                            temp_cfg.set(configurations::update::sw_updates_url, url_str);
                        }
                    }
#endif
                }
                }
                ImGui::EndChild();

                ImGui::Separator();

                ImGui::GetWindowDrawList()->AddRectFilled({ (float)x0, (float)(y0 + h - 60) },
                    { (float)(x0 + w), (float)(y0 + h) }, ImColor(sensor_bg));

                ImGui::SetCursorScreenPos({ (float)(x0 + 15), (float)(y0 + h - 60) });
                if (reload_required)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    std::string excl_icon_str = std::string(rsutils::string::from()
                        << textual_icons::exclamation_triangle
                        << " The application will be restarted in order for new settings to take effect");
                    ImGui::Text("%s", excl_icon_str.c_str());
                    ImGui::PopStyleColor();
                }

                auto apply = [&](){
                    window.on_reload_complete = [this](){
                        _skybox.reset();
                    };
                    if (reload_required) window.reload();
                    else if (refresh_required) window.refresh();
                    update_configuration(&temp_cfg);

                    if (refresh_updates)
                        for (auto&& dev : devices)
                            dev->refresh_notifications(*this);
                };

                ImGui::SetCursorScreenPos({ (float)(x0 + w / 2 - 190), (float)(y0 + h - 30) });
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    apply();
                }
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Save settings and close");
                }
                ImGui::SameLine();

                auto configs_same = temp_cfg == config_file::instance();
                ImGui::PushStyleColor(ImGuiCol_Text, configs_same ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, configs_same ? light_grey : light_blue);
                if (ImGui::Button("Apply", ImVec2(120, 0)))
                {
                    apply();
                }
                ImGui::PopStyleColor(2);
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Save settings");
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Close window without saving any changes to the settings");
                }
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }

        if (open_about_popup)
        {
            _measurements.disable();
            ImGui::OpenPopup(menu_items[3]);
        }

        {
            float w = 590.f + window.get_font_size() * 6.f;
            float h = 300.f + window.get_font_size();
            float x0 = (window.width() - w) / 2.f;
            float y0 = (window.height() - h) / 2.f;
            ImGui::SetNextWindowPos({ x0, y0 });
            ImGui::SetNextWindowSize({ w, h });

            flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

            RsImGui_ScopePushFont(window.get_font());
            ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

            if (ImGui::BeginPopupModal(menu_items[3], nullptr, flags))
            {
                // splash original width, height
                auto width = 1920;
                auto height = 1080;

                // considering the pixels in which the Realsense logo is within splash image
                ImVec2 uv0 = ImVec2(200.f / width, 300.f / height);
                ImVec2 uv1 = ImVec2(1700.f / width, 600.f / height);

                ImGui::Image((void*)(intptr_t)window.get_splash().get_gl_handle(),
                             ImVec2(w - 30, 100), uv0, uv1);

                auto realsense_pos = ImGui::GetCursorPos();
                ImGui::Text("RealSense is a suite of depth-sensing and motion-tracking technologies.");

                ImGui::Text("librealsense is an open-source cross-platform SDK for working with RealSense devices.");

                ImGui::Text("Full source code is available at"); ImGui::SameLine();
                auto github_pos = ImGui::GetCursorPos();
                ImGui::Text("github.com/realsenseai/librealsense.");

                ImGui::Text("This software is distributed under the"); ImGui::SameLine();
                auto license_pos = ImGui::GetCursorPos();
                ImGui::Text("Apache License, Version 2.0.");

                ImGui::Text("Copyright RealSense Inc 2018-2025.");

                if( RS2_API_BUILD_VERSION )
                {
                    ImGui::Text( "---" );
                    ImGui::Text( "Full version: " );
                    ImGui::SameLine();
                    ImGui::Text( RS2_API_FULL_VERSION_STR );
                }

                ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_Text, light_blue);

                ImGui::SetCursorPos({ realsense_pos.x - 4, realsense_pos.y - 3 });

                hyperlink(window, "RealSense", "https://realsenseai.com/");

                ImGui::SetCursorPos({ github_pos.x - 4, github_pos.y - 3 });
                hyperlink(window, "github.com/realsenseai/librealsense", "https://github.com/realsenseai/librealsense/");

                ImGui::SetCursorPos({ license_pos.x - 4, license_pos.y - 3 });

                hyperlink(window, "Apache License, Version 2.0", "https://raw.githubusercontent.com/realsenseai/librealsense/master/LICENSE");

                ImGui::PopStyleColor(4);


                ImGui::SetCursorScreenPos({ (float)(x0 + w / 2 - 60), (float)(y0 + h - 30) });
                if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(7);

        ImGui::GetWindowDrawList()->AddLine({ pos1.x, pos1.y + 10 }, { pos1.x,pos1.y + panel_y - 10 }, ImColor(light_grey));


        ImGui::SameLine();
        ImGui::SetCursorPosX(window.width() - panel_width - panel_y);

        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void viewer_model::update_3d_camera(
        ux_window& win,
        const rect& viewer_rect, bool force)
    {
        if (_measurements.manipulating()) return;
        if (win.get_hovered_over_input()) return;

        mouse_info& mouse = win.get_mouse();
        auto now = std::chrono::high_resolution_clock::now();
        static auto view_clock = std::chrono::high_resolution_clock::now();
        auto sec_since_update = std::chrono::duration<float, std::milli>(now - view_clock).count() / 1000;
        view_clock = now;

        if (fixed_up)
            up = { 0.f, -1.f, 0.f };

        auto dir = target - pos;
        auto x_axis = cross(dir, up);
        auto step = sec_since_update * 0.3f;

        if (ImGui::GetIO().KeysDown[ImGuiKey_W])
        {
            pos = pos + dir * step;
            target = target + dir * step;
        }
        if (ImGui::GetIO().KeysDown[ImGuiKey_S])
        {
            pos = pos - dir * step;
            target = target - dir * step;
        }
        if (ImGui::GetIO().KeysDown[ImGuiKey_D])
        {
            pos = pos + x_axis * step;
            target = target + x_axis * step;
        }
        if (ImGui::GetIO().KeysDown[ImGuiKey_A])
        {
            pos = pos - x_axis * step;
            target = target - x_axis * step;
        }

        if (viewer_rect.contains(mouse.cursor) || force)
        {
            // Whenever the mouse reaches the end of the window
            // and jump back to the start, it will add to the overflow
            // counter, so by adding the overflow value
            // we essentially emulate an infinite display
            auto cx = mouse.cursor.x + overflow.x;
            auto px = mouse.prev_cursor.x + overflow.x;
            auto cy = mouse.cursor.y + overflow.y;
            auto py = mouse.prev_cursor.y + overflow.y;

            auto dragging = ImGui::GetIO().KeysDown[GLFW_KEY_LEFT_CONTROL];

            // Limit how much user mouse can jump between frames
            // This can work poorly when the app FPS is really terrible (< 10)
            // but overall works resonably well
            const auto MAX_MOUSE_JUMP = 200;
            const auto SCROLL_SLOW_MAX_TIME_MS = 50;
            const auto SCROLL_FAST_MIN_TIME_MS = 500;
            float zoom_per_tick = 0.2f;
            static auto prev_scroll_time = std::chrono::high_resolution_clock::now();
            if (mouse.mouse_wheel != 0)
            {
                auto scroll_time = std::chrono::high_resolution_clock::now();
                auto delta_scroll_time = std::chrono::duration_cast<std::chrono::milliseconds>(scroll_time - prev_scroll_time).count();
                prev_scroll_time = scroll_time;

                // scrolling impact is scaled up / down if the scrolling speed is fast / slow
                if (delta_scroll_time < SCROLL_SLOW_MAX_TIME_MS)
                    zoom_per_tick *= 2.f;
                else if (delta_scroll_time > SCROLL_FAST_MIN_TIME_MS)
                    zoom_per_tick *= 0.5f;
            }


            if (std::abs(cx - px) < MAX_MOUSE_JUMP &&
                std::abs(cy - py) < MAX_MOUSE_JUMP )
                arcball_camera_update(
                (float*)&pos, (float*)&target, (float*)&up, view,
                sec_since_update,
                zoom_per_tick,
                -0.1f, // pan speed
                3.0f, // rotation multiplier
                static_cast<int>(viewer_rect.w), static_cast<int>(viewer_rect.h), // screen (window) size
                static_cast<int>(px), static_cast<int>(cx),
                static_cast<int>(py), static_cast<int>(cy),
                (ImGui::GetIO().MouseDown[2] ||
                 ImGui::GetIO().MouseDown[1] ||
                (ImGui::GetIO().MouseDown[0] && dragging)) ? 1 : 0,
                (ImGui::GetIO().MouseDown[0] && !dragging) ? 1 : 0,
                mouse.mouse_wheel,
                0);

            // If we are pressing mouse button
            // inside the 3D viewport
            // we should remember that we
            // are in the middle of manipulation
            // and adjust when mouse leaves the area
            if (ImGui::GetIO().MouseDown[0] ||
                ImGui::GetIO().MouseDown[1] ||
                ImGui::GetIO().MouseDown[2])
            {
                manipulating = true;
            }
        }

        auto rect = viewer_rect;
        rect.w -= 10;

        // If we started manipulating the camera
        // and left the viewport
        if (manipulating && !rect.contains(mouse.cursor))
        {
            // If mouse is no longer pressed,
            // abort the manipulation
            if (!ImGui::GetIO().MouseDown[0] &&
                !ImGui::GetIO().MouseDown[1] &&
                !ImGui::GetIO().MouseDown[2])
            {
                manipulating = false;
                overflow = float2{ 0.f, 0.f };
            }
            else
            {
                // Wrap-around the mouse in X direction
                auto startx = (mouse.cursor.x - rect.x);
                if (startx < 0)
                {
                    overflow.x -= rect.w;
                    startx += rect.w;
                }
                if (startx > rect.w)
                {
                    overflow.x += rect.w;
                    startx -= rect.w;
                }
                startx += rect.x;

                // Wrap-around the mouse in Y direction
                auto starty = (mouse.cursor.y - rect.y);
                if (starty < 0)
                {
                    overflow.y -= rect.h;
                    starty += rect.h;
                }
                if (starty > rect.h)
                {
                    overflow.y += rect.h;
                    starty -= rect.h;
                }
                starty += rect.y;

                // Set new cursor position
                glfwSetCursorPos(win, startx, starty);
            }
        }
        else overflow = float2{ 0.f, 0.f };

        mouse.prev_cursor = mouse.cursor;
    }

    void viewer_model::begin_stream(std::shared_ptr<subdevice_model> d, rs2::stream_profile p)
    {
        // Starting post processing filter rendering thread
        ppf.start();
        streams[p.unique_id()].begin_stream(d, p, *this);
        ppf.frames_queue.emplace(p.unique_id(), rs2::frame_queue(5));
    }

    bool viewer_model::is_3d_texture_source(frame f) const
    {
        auto profile = f.get_profile().as<video_stream_profile>();
        auto index = profile.unique_id();
        int mapped_index = -2;
        auto iter = streams_origin.find(index);
        if (iter != streams_origin.end())
            mapped_index = iter->second;

        if (!is_rasterizeable(profile.format()))
            return false;

        if (index == selected_tex_source_uid || mapped_index == selected_tex_source_uid || selected_tex_source_uid == -1)
            return true;
        return false;
    }

    std::shared_ptr<texture_buffer> viewer_model::get_last_texture()
    {
        return last_texture;
    }

    std::vector<frame> rs2::viewer_model::get_frames(frame frame)
    {
        std::vector<rs2::frame> res;

        if (auto set = frame.as<frameset>())
            for (auto&& f : set)
                res.push_back(f);

        else
            res.push_back(frame);

        return res;
    }

    frame viewer_model::get_3d_depth_source(frame f)
    {
        auto frames = get_frames(f);

        for (auto&& f : frames)
        {
            if (is_3d_depth_source(f))
                return f;
        }
        return nullptr;
    }

    frame rs2::viewer_model::get_3d_texture_source(frame f)
    {
        auto frames = get_frames(f);

        for (auto&& f : frames)
        {
            if (is_3d_texture_source(f))
                return f;
        }
        return nullptr;
    }

    bool viewer_model::is_3d_depth_source(frame f)
    {

        auto index = f.get_profile().unique_id();
        auto mapped_index = streams_origin[index];

        if(index == selected_depth_source_uid || mapped_index  == selected_depth_source_uid
                ||(selected_depth_source_uid == -1 && f.get_profile().stream_type() == RS2_STREAM_DEPTH))
            return true;
        return false;
    }

    std::shared_ptr<texture_buffer> viewer_model::upload_frame(frame&& f)
    {
        if (f.get_profile().stream_type() == RS2_STREAM_DEPTH)
            ppf.depth_stream_active = true;

        auto index = f.get_profile().unique_id();

        std::lock_guard<std::mutex> lock(streams_mutex);
        auto stream_origin_iter = streams_origin.find(index);
        if ( stream_origin_iter != streams_origin.end() && streams.find( stream_origin_iter->second ) != streams.end())
            return streams[stream_origin_iter->second].upload_frame(std::move(f));
        else return nullptr;
    }

    void viewer_model::draw_viewport(const rect& viewer_rect,
        ux_window& window, int devices, std::string& error_message,
        std::shared_ptr<texture_buffer> texture, points points, labeled_points labeled_points)
    {
        if (!modal_notification_on)
            updates->draw(not_model, window, error_message);

        static bool first = true;
        if (first)
        {
            update_3d_camera(window, viewer_rect, true);
            first = false;
        }

        if (!is_3d_view)
        {
            render_2d_view(viewer_rect, window,
                static_cast<int>(get_output_height()), window.get_font(), window.get_large_font(),
                devices, window.get_mouse(), error_message);
        }
        else
        {
            if (paused)
                show_paused_icon(window.get_large_font(), static_cast<int>(panel_width + 15), static_cast<int>(panel_y + 15 + 60), 0);

            show_3dviewer_header(window, viewer_rect, paused, error_message);

            update_3d_camera(window, viewer_rect);

            _measurements.update_input(window, viewer_rect);

            rect window_size{ 0, 0, (float)window.width(), (float)window.height() };
            rect fb_size{ 0, 0, (float)window.framebuf_width(), (float)window.framebuf_height() };
            rect new_rect = viewer_rect.normalize(window_size).unnormalize(fb_size);

            render_3d_view(new_rect, window, texture, points, labeled_points);

            auto rect_copy = viewer_rect;
            rect_copy.y += 60;
            rect_copy.h -= 60;

            if (rect_copy.contains(window.get_mouse().cursor))
                _measurements.show_tooltip(window);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Space))
        {
            if (paused)
            {
                for (auto&& s : streams)
                {
                    if (s.second.dev)
                    {
                        s.second.dev->resume();
                        if (auto p = s.second.dev->dev.as<playback>())
                        {
                            p.resume();
                        }
                    }
                }
            }
            else
            {
                for (auto&& s : streams)
                {
                    if (s.second.dev)
                    {
                        s.second.dev->pause();
                        if (s.second.dev->dev.is<playback>())
                        {
                            auto p = s.second.dev->dev.as<playback>();
                            p.pause();
                        }
                    }
                }
            }
            paused = !paused;
        }
    }

    std::map<int, rect> viewer_model::get_interpolated_layout(const std::map<int, rect>& l)
    {
        using namespace std::chrono;
        auto now = high_resolution_clock::now();
        if (l != _layout) // detect layout change
        {
            _transition_start_time = now;
            _old_layout = _layout;
            _layout = l;
        }

        //if (_old_layout.size() == 0 && l.size() == 1) return l;

        auto diff = now - _transition_start_time;
        auto ms = duration_cast<milliseconds>(diff).count();
        auto t = smoothstep(static_cast<float>(ms), 0, 100);

        std::map<int, rect> results;
        for (auto&& kvp : l)
        {
            auto stream = kvp.first;
            if (_old_layout.find(stream) == _old_layout.end())
            {
                _old_layout[stream] = _layout[stream].center();
            }
            results[stream] = lerp( _old_layout[stream], t, _layout[stream] );
        }

        return results;
    }

    void viewer_model::init_depth_uid(int& selected_depth_source, std::vector<std::string>& depth_sources_str, std::vector<int>& depth_sources)
    {
        int i = 0;

        for (auto&& s : streams)
        {
            if (s.second.is_stream_visible() &&
                s.second.profile.stream_type() == RS2_STREAM_DEPTH)
            {
                auto stream_origin_iter = streams_origin.find(s.second.profile.unique_id());
                if (selected_depth_source_uid == -1)
                {
                    if (stream_origin_iter != streams_origin.end() &&
                        streams.find(stream_origin_iter->second) != streams.end())
                    {
                        selected_depth_source_uid = stream_origin_iter->second;
                    }
                }
                if (stream_origin_iter != streams_origin.end() && stream_origin_iter->second == selected_depth_source_uid)
                {
                    selected_depth_source = i;
                }

                depth_sources.push_back(s.second.profile.unique_id());

                auto dev_name = s.second.dev ? s.second.dev->dev.get_info(RS2_CAMERA_INFO_NAME) : "Unknown";
                auto stream_name = rs2_stream_to_string(s.second.profile.stream_type());

                depth_sources_str.push_back(rsutils::string::from() << dev_name << " " << stream_name);

                i++;
            }
        }
    }

    void viewer_model::init_labeled_points_uid()
    {
        for (auto&& s : streams)
        {
            if (s.second.is_stream_visible() &&
                s.second.profile.stream_type() == RS2_STREAM_LABELED_POINT_CLOUD)
            {
                auto stream_origin_iter = streams_origin.find(s.second.profile.unique_id());
                if (stream_origin_iter != streams_origin.end() &&
                    streams.find(stream_origin_iter->second) != streams.end() &&
                    selected_labeled_points_source_uid != stream_origin_iter->second)
                {
                    selected_labeled_points_source_uid = stream_origin_iter->second;
                }
            }
        }
    }

    void viewer_model::draw_zone_3d(Zone zone, const rs2::labeled_points& frame)
    {
        glLineWidth(4.0f);
        glBegin(GL_LINE_LOOP);

        const auto MM_TO_METER_SCALE = 0.001f; // coords are in mm, converts to meters
        auto zone_to_draw = init_zone(zone, frame, MM_TO_METER_SCALE); 
        set_polygon_color(zone);

        for (vertex& v : zone_to_draw)
        {
            // we transform LPC to depth - z=0 in LPC will be transformed to depth sensor (=camera) height
            // - drawn after the matrix rotation & translation
            glVertex3f(v.x, v.y, 0); 
        }

        glEnd();
        glLineWidth(1.0f);
    }

    void viewer_model::draw_3d_labeled_points(const rect& viewer_rect, rs2::labeled_points labeled_points)
    {
        auto labeled_points_profile = last_labeled_points.get_profile().as<video_stream_profile>();
        // Non-linear correspondence customized for non-flat surface exploration
        if (labeled_points_profile.width() <= 0)
            throw std::runtime_error("Profile width must be greater than 0.");

        float point_size = std::sqrt(viewer_rect.w / labeled_points_profile.width());
        if (selected_lpc_points_size == lpc_points_size::lpc_medium)
            point_size *= 3.f;
        else if (selected_lpc_points_size == lpc_points_size::lpc_large)
            point_size *= 5.f;
        glPointSize(point_size);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_border_mode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_border_mode);
        
        auto& lpc_stream_model = streams.at(labeled_points_profile.unique_id());
        
        rs2_extrinsics lpc_to_depth = lpc_stream_model.dev->get_extrinsics_from_depth();
        auto rot = lpc_to_depth.rotation;
        GLfloat rotation_matrix[16] = { rot[0], rot[3], rot[6], 0,
                                        rot[1], rot[4], rot[7], 0,
                                        rot[2], rot[5], rot[8], 0,
                                         0    ,   0,      0,    1 };
        glMultMatrixf(rotation_matrix);

        // getting inverse of the translation, in depth coordinates, as this is the one needed by OpenGL
        glTranslatef(-lpc_to_depth.translation[0], -lpc_to_depth.translation[1], -lpc_to_depth.translation[2]);
        
        if (show_safety_zones_3d)
        {
            draw_zone_3d(Zone::Danger, labeled_points);
            draw_zone_3d(Zone::Warning, labeled_points);
            draw_zone_3d(Zone::Diagnostic, labeled_points);
        }

        glBegin(GL_POINTS);
        {
            auto vertices = last_labeled_points.get_vertices();
            auto vertices_size = last_labeled_points.size();
            auto labels = last_labeled_points.get_labels();
            auto label_to_color3f = labeled_point_cloud_utilities::get_label_to_color3f();

            /* this segment actually renders the labeled pointcloud */
            for (int i = 0; i < vertices_size; ++i)
            {
                // Set the vertex color from the label value
                auto label = labels[i];
                auto color = label_to_color3f[static_cast<rs2_point_cloud_label>(label)];
                glColor3f(color.x, color.y, color.z);

                // Draw the vertex
                rs2::vertex vtx = { vertices[i].x, vertices[i].y, vertices[i].z };
                glVertex3fv(std::move(vtx));
            }
        }
        glEnd();

        glColor4f(1.f, 1.f, 1.f, 1.f);

        check_gl_error();
    }

    bool viewer_model::should_texture_frame_be_updated(const rs2::frame& f) const
    {
        return (f.get_profile().stream_type() != RS2_STREAM_LABELED_POINT_CLOUD &&
            ((selected_tex_source_uid == -1 && f.get_profile().format() == RS2_FORMAT_Z16)
                || (f.get_profile().format() != RS2_FORMAT_ANY && is_3d_texture_source(f))));
    }

    void viewer_model::set_polygon_color(Zone zone)
    {
        switch (zone)
        {
        case Zone::Danger:
            glColor3f(red.x, red.y, red.z);
            break;
        case Zone::Warning:
            glColor3f(yellow.x, yellow.y, yellow.z);
            break;
        case Zone::Diagnostic:
            glColor3f(regular_blue.x, regular_blue.y, regular_blue.z);
            break;
        default:
            LOG_ERROR("Invalid zone, got: " << static_cast<int>(zone));
            return;
        }
    }

    std::vector<vertex> viewer_model::init_zone(Zone zone, const frame& frame, float scale_factor)
    {
        std::vector<vertex> points;
        rs2_frame_metadata_value md_value;
        switch (zone)
        {
        case Zone::Danger:
            md_value = RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_X_CORD;
            break;
        case Zone::Warning:
            md_value = RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_X_CORD;
            break;
        case Zone::Diagnostic:
            md_value = RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_X_CORD;
            break;
        default:
            LOG_ERROR("Invalid zone, got: " << static_cast<int>(zone));
            return points;
        }

        // assuming all md values are subsequent 
        vertex x0 = { static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value))) * scale_factor,
                        static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 1))) * scale_factor, 0 };
        vertex x1 = { static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 2))) * scale_factor,
                        static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 3))) * scale_factor, 0 };
        vertex x2 = { static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 4))) * scale_factor,
                        static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 5))) * scale_factor, 0 };
        vertex x3 = { static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 6))) * scale_factor,
                        static_cast<float>(frame.get_frame_metadata(static_cast<rs2_frame_metadata_value>(md_value + 7))) * scale_factor, 0 };
        points = { x0, x1, x2, x3 };
        return points;
    }

    // get a vertex in LPC corrdinates, and transform to openGL corrdinates
    // we can't use the rotation matrix (from the extrinsics) here as this is only a transformation in 2D and not in 3D
    // the transformation we need is expected to stay the same - mirror the values on X and Y axis
    vertex viewer_model::transform_vertex(vertex v, const rect& normalize_from, const rect& unnormalize_to)
    {
        vertex v2 = { 0,0,0 };

        // normalize each value between 0 and 1, v is expected to be within normalize_from
        // note v.y -> v2.x and v.x -> v2.y, this is a part of the transformation
        v2.x = (v.y - normalize_from.x) / normalize_from.w;
        v2.y = (v.x - normalize_from.y) / normalize_from.h;

        // since x and y are normalized to be between 0 and 1, we can use this to mirror them
        v2.x = 1 - v2.x;
        v2.y = 1 - v2.y;

        // 'unnormalize' the values back, so they fit in the wanted frame, unnormalize_to
        v2.x = v2.x * unnormalize_to.w + unnormalize_to.x;
        v2.y = v2.y * unnormalize_to.h + unnormalize_to.y;

        return v2;
    }


    void viewer_model::draw_zone_2d(Zone zone, const rect& draw_within, const frame& frame)
    {
        glLineWidth(3.0f);
        glBegin(GL_LINE_LOOP);

        auto MM_TO_CM_SCALE = 0.1f;  // coords are in mm, converts to cm
        auto zone_to_draw = init_zone(zone, frame, MM_TO_CM_SCALE);
        set_polygon_color(zone);

        constexpr GLfloat width = 512; // range of Y values for polygons - -2.56 - +2.56 meters
        constexpr GLfloat height = 640; // range of X values for polygons - 0-6.4 meters
        rect safety_regions_rect = { -256, 0, width, height };  //-256 is the minimum Y value, 0 is the minimum X value, all zones are within this area
        for (vertex& v : zone_to_draw)
        {
            auto vertex = transform_vertex(v, safety_regions_rect, draw_within);
            glVertex2f(vertex.x, vertex.y);
        }

        glEnd();
        glLineWidth(1.0f);
    }

        
}
