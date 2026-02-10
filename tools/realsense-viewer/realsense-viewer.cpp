// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-24 RealSense, Inc. All Rights Reserved.
#include <librealsense2/rs.hpp>
#include "viewer.h"
#include "os.h"
#include "ux-window.h"
#include "fw-update-helper.h"

#include <common/cli.h>

#include <cstdarg>
#include <thread>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <array>
#include <mutex>
#include <set>
#include <regex>

#include <imgui_internal.h>

#ifdef INTERNAL_FW
#include "common/fw/D4XX_FW_Image.h"
#else
#define FW_D4XX_FW_IMAGE_VERSION ""
#endif // INTERNAL_FW

using namespace rs2;
using namespace rs400;


void update_viewer_configuration(viewer_model& viewer_model)
{
    // Hide options from the Viewer application
    viewer_model._hidden_options.emplace(RS2_OPTION_ENABLE_IR_REFLECTIVITY);
}

void add_playback_device( context & ctx,
                          std::shared_ptr< device_models_list > device_models,
                          std::string & error_message,
                          viewer_model & viewer_model,
                          const std::string & file )
{
    bool was_loaded = false;
    bool failed = false;
    try
    {
        auto dev = ctx.load_device(file);
        was_loaded = true;
        device_models->emplace_back(  // Will cause the new device to appear in the left panel
            new device_model( dev, error_message, viewer_model ) );
        if (auto p = dev.as<playback>())
        {
            auto filename = p.file_name();
            p.set_status_changed_callback(
                [&viewer_model, weak_device_models = std::weak_ptr< device_models_list >( device_models ), filename](
                    rs2_playback_status status )
                {
                    auto device_models = weak_device_models.lock();
                    if( ! device_models )
                        return;
                    if( status == RS2_PLAYBACK_STATUS_STOPPED )
                    {
                        auto it = std::find_if( device_models->begin(),
                                                device_models->end(),
                                                [&]( const std::unique_ptr< device_model > & dm )
                                                {
                                                    if( auto p = dm->dev.as< playback >() )
                                                        return p.file_name() == filename;
                                                    return false;
                                                } );
                        if( it != device_models->end() )
                        {
                            auto subs = ( *it )->subdevices;
                            if( ( *it )->_playback_repeat )
                            {
                                // Calling from different since playback callback is from reading thread
                                std::thread{ [subs, &viewer_model, it]()
                                             {
                                                 if( ! ( *it )->dev_syncer )
                                                     ( *it )->dev_syncer = viewer_model.syncer->create_syncer();

                                                 for( auto && sub : subs )
                                                 {
                                                     if( sub->streaming )
                                                     {
                                                         auto profiles = sub->get_selected_profiles();

                                                         sub->play( profiles, viewer_model, ( *it )->dev_syncer );
                                                     }
                                                 }
                                             } }
                                    .detach();
                            }
                            else
                            {
                                for( auto && sub : subs )
                                {
                                    if( sub->streaming )
                                    {
                                        sub->stop( viewer_model.not_model );
                                    }
                                }
                            }
                        }
                    }
                } );
        }
    }
    catch (const error& e)
    {
        error_message = rsutils::string::from() << "Failed to load file " << file << ". Reason: " << error_to_string(e);
        failed = true;
    }
    catch (const std::exception& e)
    {
        error_message = rsutils::string::from() << "Failed to load file " << file << ". Reason: " << e.what();
        failed = true;
    }
    if (failed && was_loaded)
    {
        try { ctx.unload_device(file); }
        catch (...) {}
    }
}

// This function is called every frame
// If between the frames there was an asyncronous connect/disconnect event
// the function will pick up on this and add the device to the viewer
void refresh_devices(std::mutex& m,
    context& ctx,
    device_changes& devices_connection_changes,
    std::vector<device>& current_connected_devices,
    std::vector<std::pair<std::string, std::string>>& device_names,
    device_models_list& device_models,
    viewer_model& viewer_model,
    std::string& error_message)
{
    
    event_information info({}, {});
    if (!devices_connection_changes.try_get_next_changes(info))
        return ;
    try
    {
        //Remove disconnected
        auto dev_itr = begin(current_connected_devices);
        while (dev_itr != end(current_connected_devices))
        {
            auto dev = *dev_itr;
            if (info.was_removed(dev))
            {
                //Notify change
                viewer_model.not_model->add_notification({ get_device_name(dev).first + " Disconnected\n",
                    RS2_LOG_SEVERITY_INFO, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });

                //Remove from devices
                auto dev_model_itr = std::find_if(begin(device_models), end(device_models),
                    [&](const std::unique_ptr<device_model>& other) { return get_device_name(other->dev) == get_device_name(dev); });

                if (dev_model_itr != end(device_models))
                {
                    for (auto&& s : (*dev_model_itr)->subdevices)
                        s->streaming = false;

                    dev_model_itr->reset();
                    device_models.erase(dev_model_itr);

                    if (device_models.size() == 0)
                    {
                        viewer_model.ppf.depth_stream_active = false;

                        // Stopping post processing filter rendering thread in case of disconnection
                        viewer_model.ppf.stop();
                    }
                }
                auto dev_name_itr = std::find(begin(device_names), end(device_names), get_device_name(dev));
                if (dev_name_itr != end(device_names))
                    device_names.erase(dev_name_itr);

                dev_itr = current_connected_devices.erase(dev_itr);
                continue;
            }
            ++dev_itr;
        }

        //Add connected
        static bool initial_refresh = true;
        try
        {
            for (auto dev : info.get_new_devices())
            {
                auto dev_descriptor = get_device_name(dev);
                device_names.push_back(dev_descriptor);

                bool added = false;
                if (device_models.size() == 0 &&
                    dev.supports(RS2_CAMERA_INFO_NAME) && std::string(dev.get_info(RS2_CAMERA_INFO_NAME)) != "Platform Camera" && std::string(dev.get_info(RS2_CAMERA_INFO_NAME)).find("IP Device") == std::string::npos)
                {
                    try
                    {
                        device_models.emplace_back(new device_model(dev, error_message, viewer_model));
                    }
                    catch (const std::exception& e)
                    {
                        log(RS2_LOG_SEVERITY_ERROR, "Exception raised on device_model creation");
                        auto dev_name_itr = std::find(begin(device_names), end(device_names), get_device_name(dev));
                        if (dev_name_itr != end(device_names))
                        {
                            device_names.erase(dev_name_itr);
                        }
                        throw e;
                    }

                    viewer_model.not_model->add_log(
                        rsutils::string::from() << ( *device_models.rbegin() )->dev.get_info( RS2_CAMERA_INFO_NAME )
                                                << " was selected as a default device" );
                    added = true;
                }

                if (!initial_refresh)
                {
                    if (added || dev.is<playback>())
                        viewer_model.not_model->add_notification({ dev_descriptor.first + " Connected\n",
                            RS2_LOG_SEVERITY_INFO, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });
                    else if (added || dev.supports(RS2_CAMERA_INFO_IP_ADDRESS))
                        viewer_model.not_model->add_notification({ dev_descriptor.first + " Connected\n",
                            RS2_LOG_SEVERITY_INFO, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });
                    else
                        viewer_model.not_model->add_notification({ dev_descriptor.first + " Connected\n",
                            RS2_LOG_SEVERITY_INFO, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR },
                            [&device_models, &viewer_model, &error_message, dev] {
                                auto device = dev;
                                device_models.emplace_back(
                                    new device_model(device, error_message, viewer_model));
                            });
                }

                current_connected_devices.push_back(dev);
                for (auto&& s : dev.query_sensors())
                {
                    s.set_notifications_callback([&, dev_descriptor](const notification& n)
                        {
                            if (n.get_category() == RS2_NOTIFICATION_CATEGORY_HARDWARE_EVENT)
                            {
                                auto data = n.get_serialized_data();
                                if (!data.empty())
                                {
                                    auto dev_model_itr = std::find_if(begin(device_models), end(device_models),
                                        [&](const std::unique_ptr<device_model>& other)
                                        { return get_device_name(other->dev) == dev_descriptor; });

                                    if (dev_model_itr == end(device_models))
                                        return;

                                    (*dev_model_itr)->handle_hardware_events(data);
                                }
                            }
                            viewer_model.not_model->add_notification({ n.get_description(), n.get_severity(), n.get_category() });
                        });
                }


            }
        }
        catch (std::exception& e) {
            std::stringstream s;
            s << "Couldn't refresh devices - "  << e.what();
            log(RS2_LOG_SEVERITY_WARN, s.str().c_str());
        }
        initial_refresh = false;
    }
    catch (const error& e)
    {
        error_message = error_to_string(e);
    }
    catch (const std::exception& e)
    {
        error_message = e.what();
    }
    catch (...)
    {
        error_message = "Unknown error";
    }
    return;
}


int main(int argc, const char** argv) try
{
    rs2::cli cmd( "realsense-viewer" );
    auto settings = cmd.process( argc, argv );

    std::shared_ptr<device_models_list> device_models = std::make_shared<device_models_list>();

    context ctx( settings.dump() );
    ux_window window("RealSense Viewer", ctx);

    // Create RealSense Context
    device_changes devices_connection_changes(ctx);
    std::vector<std::pair<std::string, std::string>> device_names;

    std::string error_message;
    std::string label;

    device_model* device_to_remove = nullptr;

#ifdef BUILD_EASYLOGGINGPP
    bool const disable_log_to_console = cmd.debug_arg.getValue();
#else
    bool const disable_log_to_console = false;
#endif

    viewer_model viewer_model( ctx, disable_log_to_console );

    update_viewer_configuration(viewer_model);

    std::vector<device> connected_devs;
    std::mutex m;

#ifdef BUILD_EASYLOGGINGPP
    std::weak_ptr<notifications_model> notifications = viewer_model.not_model;
    rs2::log_to_callback( RS2_LOG_SEVERITY_INFO,
        [notifications]( rs2_log_severity severity, rs2::log_message const& msg )
        {
            if (auto not_model = notifications.lock())
            {
                not_model->output.add_log(severity, msg.filename(), (int)(msg.line_number()), msg.raw());
            }
        });
#endif 

    window.on_file_drop = [&](std::string filename)
    {
        add_playback_device(ctx, device_models, error_message, viewer_model, filename);
        if (!error_message.empty())
        {
            viewer_model.not_model->add_notification({ error_message,
                RS2_LOG_SEVERITY_ERROR, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });
        }
    };

    for (int i = 1; i < argc; i++)
    {
        try
        {
            const char* arg = argv[i];
            std::ifstream file(arg);
            if (!file.good())
                continue;

            add_playback_device(ctx, device_models, error_message, viewer_model, arg);
        }
        catch (const rs2::error& e)
        {
            error_message = error_to_string(e);
        }
        catch (const std::exception& e)
        {
            error_message = e.what();
        }
    }

    window.on_load = [&]()
    {
        refresh_devices(m, ctx, devices_connection_changes, connected_devs,
            device_names, *device_models, viewer_model, error_message);
        return true;
    };

    // Closing the window
    while (window)
    {
        refresh_devices(m, ctx, devices_connection_changes, connected_devs,
            device_names, *device_models, viewer_model, error_message);

        auto output_height = viewer_model.get_output_height();

        rect viewer_rect = { viewer_model.panel_width,
                             viewer_model.panel_y, window.width() -
                             viewer_model.panel_width,
                             window.height() - viewer_model.panel_y - output_height };

        // Flags for pop-up window - no window resize, move or collaps
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ viewer_model.panel_width, viewer_model.panel_y });

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Add Device Panel", nullptr, flags);

        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_PopupBg, from_rgba(230, 230, 230, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, from_rgba(0, 0xae, 0xff, 255));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, from_rgba(255, 255, 255, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::SetNextWindowPos({ 0, viewer_model.panel_y });

        std::string add_source_button_text = rsutils::string::from() 
                                          << " " << textual_icons::plus_circle 
                                          << "  Add Source (" << (device_names.size() - device_models->size()) 
                                          << " available)\t\t\t\t\t\t\t\t\t\t\t";

        if (ImGui::Button(add_source_button_text.c_str(), { viewer_model.panel_width - 1, viewer_model.panel_y }))
            ImGui::OpenPopup("select");

        auto new_devices_count = device_names.size() + 1;

        for (auto&& dev_model : *device_models)
        {
            auto connected_devs_itr = std::find_if(begin(connected_devs), end(connected_devs),
                [&](const device& d) { return get_device_name(d) == get_device_name(dev_model->dev); });

            if (connected_devs_itr != end(connected_devs) || dev_model->dev.is<playback>())
                new_devices_count--;
        }


        ImGui::PushFont(window.get_font());

        int multiline_devices_names = 0;
        for (size_t i = 0; i < device_names.size(); i++)
        {
            if (device_names[i].first.find("\n") != std::string::npos)
            {
                bool show_device_in_list = true;
                for (auto&& dev_model : *device_models)
                {
                    if (get_device_name(dev_model->dev) == device_names[i])
                    {
                        show_device_in_list = false;
                        break;
                    }
                }
                if (show_device_in_list)
                {
                    multiline_devices_names++;
                }
            }
        }

        float line_h = ImGui::GetTextLineHeightWithSpacing() + 2;
        float separator_h = new_devices_count > 1 ? ImGui::GetStyle().ItemSpacing.y : 0;
        float popup_select_h = line_h * ( new_devices_count + multiline_devices_names ) + separator_h ;
        ImVec2 popup_select_size = { viewer_model.panel_width, popup_select_h };
        ImGui::SetNextWindowSize( popup_select_size );

        if (ImGui::BeginPopup("select"))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, dark_grey);
            ImGui::Columns(1, "DevicesList", false);
            for (int i = 0; i < device_names.size(); i++)
            {
                bool skip = false;
                for (auto&& dev_model : *device_models)
                    if (get_device_name(dev_model->dev) == device_names[i]) skip = true;
                if (skip) continue;

                auto dev = connected_devs[i];
                std::string dev_type;
                if (dev.supports(RS2_CAMERA_INFO_CONNECTION_TYPE))
                {
                    dev_type = dev.get_info(RS2_CAMERA_INFO_CONNECTION_TYPE);
                    if (dev_type == "USB" && dev.supports( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR ))
                        dev_type += dev.get_info( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR );
                }

                std::string line = rsutils::string::from() << dev.get_info( RS2_CAMERA_INFO_NAME ) << " (" << dev_type
                                                           << ") S/N " << device_names[i].second.c_str();

                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(line.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)/* || switch_to_newly_loaded_device*/)
                {
                    try
                    {
                        device_models->emplace_back(new device_model(dev, error_message, viewer_model));
                    }
                    catch (const error& e)
                    {
                        error_message = error_to_string(e);
                    }
                    catch (const std::exception& e)
                    {
                        error_message = e.what();
                    }
                }
                ImGui::PopID();
            }

            if (new_devices_count > 1) ImGui::Separator();

            if (ImGui::Selectable("Load Recorded Sequence", false, ImGuiSelectableFlags_SpanAllColumns))
            {
                if (auto ret = file_dialog_open(open_file, "ROS2-bag\0*.db3\0", NULL, NULL))
                {
                    add_playback_device(ctx, device_models, error_message, viewer_model, ret);
                }
            }
            ImGui::NextColumn();

            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::End();
        ImGui::PopStyleVar();


        viewer_model.show_top_bar(window, viewer_rect, *device_models);

        auto output_rect = rect{ viewer_model.panel_width,
            window.height() - viewer_model.get_output_height(),
            window.width() - viewer_model.panel_width, float(viewer_model.get_output_height()) };

        viewer_model.not_model->output.draw(window, output_rect, *device_models);

        // Set window position and size
        ImGui::SetNextWindowPos({ 0, viewer_model.panel_y });
        ImGui::SetNextWindowSize({ viewer_model.panel_width, window.height() - viewer_model.panel_y });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, sensor_bg);

        // *********************
        // Creating window menus
        // *********************
        ImGui::Begin("Control Panel", nullptr, flags | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (device_models->size() > 0)
        {
            std::vector<std::function<void()>> draw_later;
            auto windows_width = ImGui::GetContentRegionMax().x;

            for (auto&& dev_model : *device_models)
            {
                dev_model->draw_controls(viewer_model.panel_width, viewer_model.panel_y,
                    window, error_message, device_to_remove, viewer_model, windows_width, draw_later);
            }
            if (viewer_model.ppf.is_rendering())
            {
                if (!std::any_of(device_models->begin(), device_models->end(),
                    [](const std::unique_ptr<device_model>& dm)
                {
                    return dm->is_streaming();
                }))
                {
                    // Stopping post processing filter rendering thread
                    viewer_model.ppf.stop();
                }
            }

            auto pos = ImGui::GetCursorScreenPos();
            auto h = ImGui::GetWindowHeight();
            if (h > pos.y - viewer_model.panel_y)
            {
                ImGui::GetWindowDrawList()->AddLine({ pos.x,pos.y }, { pos.x + viewer_model.panel_width,pos.y }, ImColor(from_rgba(0, 0, 0, 0xff)));
                ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImGui::GetContentRegionAvail().y));
                ImGui::GetWindowDrawList()->AddRectFilled(bb.GetTL(), bb.GetBR(), ImColor(dark_window_background));
            }

            for (auto&& lambda : draw_later)
            {
                try
                {
                    lambda();
                }
                catch (const error& e)
                {
                    error_message = error_to_string(e);
                }
                catch (const std::exception& e)
                {
                    error_message = e.what();
                }
            }

            if (device_to_remove)
            {
                if (auto p = device_to_remove->dev.as<playback>())
                {
                    ctx.unload_device(p.file_name());
                }
                viewer_model.syncer->remove_syncer(device_to_remove->dev_syncer);
                auto it = std::find_if(begin(*device_models), end(*device_models),
                    [&](const std::unique_ptr<device_model>& other)
                { return get_device_name(other->dev) == get_device_name(device_to_remove->dev); });

                if (it != device_models->end())
                {
                    it->reset();
                    device_models->erase(it);
                }

                device_to_remove = nullptr;
            }
        }
        else
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImGui::GetContentRegionAvail().y));
            ImGui::GetWindowDrawList()->AddRectFilled(bb.GetTL(), bb.GetBR(), ImColor(dark_window_background));

            viewer_model.show_no_device_overlay(window.get_large_font(), 50, static_cast<int>(viewer_model.panel_y + 50));
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // Fetch and process frames from queue
        viewer_model.handle_ready_frames(viewer_rect, window, static_cast<int>(device_models->size()), error_message);
        }

    // Stopping post processing filter rendering thread
    viewer_model.ppf.stop();

    // Stop all subdevices
    for (auto&& device_model : *device_models)
        for (auto&& sub : device_model->subdevices)
        {
            if (sub->streaming)
                sub->stop(viewer_model.not_model);
        }

    return EXIT_SUCCESS;
}
catch (const error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
