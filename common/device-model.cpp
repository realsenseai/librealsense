// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs_advanced_mode.hpp>
#include <librealsense2/rs.hpp>
#include <rs-config.h>

#include <third-party/filesystem/glob.h>

#include <imgui.h>
#include <imgui_internal.h>
#include "imgui-fonts-karla.hpp"
#include "imgui-fonts-fontawesome.hpp"
#include "imgui-fonts-monofont.hpp"
#include <realsense_imgui.h>

#include <rsutils/os/special-folder.h>
#include "os.h"
#include <rsutils/os/os.h>
#include "viewer.h"
#include "on-chip-calib.h"
#include "d500-on-chip-calib.h"
#include "subdevice-model.h"
#include "device-model.h"

using namespace rs400;
using rsutils::json;
using namespace rs2::sw_update;

namespace rs2
{
    void imgui_easy_theming(ImFont*& font_dynamic, ImFont*& font_18, ImFont*& monofont, int& font_size)
    {
        ImGuiStyle& style = ImGui::GetStyle();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        const int OVERSAMPLE = config_file::instance().get(configurations::performance::font_oversample);
        font_size = config_file::instance().get( configurations::window::font_size );

        static const ImWchar icons_ranges[] = { 0xf000, 0xf999, 0 }; // will not be copied by AddFont* so keep in scope.

        {
            ImFontConfig config_words;
            config_words.OversampleV = OVERSAMPLE;
            config_words.OversampleH = OVERSAMPLE;
            font_dynamic = io.Fonts->AddFontFromMemoryCompressedTTF( karla_regular_compressed_data,
                                                                karla_regular_compressed_size,
                                                                (float)font_size );

            ImFontConfig config_glyphs;
            config_glyphs.MergeMode = true;
            config_glyphs.OversampleV = OVERSAMPLE;
            config_glyphs.OversampleH = OVERSAMPLE;
            font_dynamic = io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data,
                font_awesome_compressed_size, 14.f, &config_glyphs, icons_ranges);
        }

        // Load 18px size fonts
        {
            ImFontConfig config_words;
            config_words.OversampleV = OVERSAMPLE;
            config_words.OversampleH = OVERSAMPLE;
            font_18 = io.Fonts->AddFontFromMemoryCompressedTTF(karla_regular_compressed_data, karla_regular_compressed_size, 21.f, &config_words);

            ImFontConfig config_glyphs;
            config_glyphs.MergeMode = true;
            config_glyphs.OversampleV = OVERSAMPLE;
            config_glyphs.OversampleH = OVERSAMPLE;
            font_18 = io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data,
                font_awesome_compressed_size, 20.f, &config_glyphs, icons_ranges);

        }

        // Load monofont
        {
            ImFontConfig config_words;
            config_words.OversampleV = OVERSAMPLE;
            config_words.OversampleH = OVERSAMPLE;
            monofont = io.Fonts->AddFontFromMemoryCompressedTTF(monospace_compressed_data, monospace_compressed_size, 15.f);

            ImFontConfig config_glyphs;
            config_glyphs.MergeMode = true;
            config_glyphs.OversampleV = OVERSAMPLE;
            config_glyphs.OversampleH = OVERSAMPLE;
            monofont = io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data,
                font_awesome_compressed_size, 14.f, &config_glyphs, icons_ranges);
        }

        style.WindowRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;

        style.Colors[ImGuiCol_WindowBg] = dark_window_background;
        style.Colors[ImGuiCol_Border] = black;
        style.Colors[ImGuiCol_BorderShadow] = transparent;
        style.Colors[ImGuiCol_FrameBg] = dark_window_background;
        style.Colors[ImGuiCol_ScrollbarBg] = scrollbar_bg;
        style.Colors[ImGuiCol_ScrollbarGrab] = scrollbar_grab;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = scrollbar_grab + 0.1f;
        style.Colors[ImGuiCol_ScrollbarGrabActive] = scrollbar_grab + (-0.1f);
        style.Colors[ImGuiCol_PopupBg] = dark_window_background;
        style.Colors[ImGuiCol_CheckMark] = regular_blue;
        style.Colors[ImGuiCol_SliderGrab] = regular_blue;
        style.Colors[ImGuiCol_SliderGrabActive] = regular_blue;
        style.Colors[ImGuiCol_Button] = button_color;
        style.Colors[ImGuiCol_ButtonHovered] = button_color + 0.1f;
        style.Colors[ImGuiCol_ButtonActive] = button_color + (-0.1f);
        style.Colors[ImGuiCol_Header] = header_color;
        style.Colors[ImGuiCol_HeaderActive] = header_color + (-0.1f);
        style.Colors[ImGuiCol_HeaderHovered] = header_color + 0.1f;
        style.Colors[ImGuiCol_TitleBg] = title_color;
        style.Colors[ImGuiCol_TitleBgCollapsed] = title_color;
        style.Colors[ImGuiCol_TitleBgActive] = header_color;
    }

    void open_issue(std::string body)
    {
        std::string link = "https://github.com/realsenseai/librealsense/issues/new?body=" + url_encode(body);
        open_url(link.c_str());
    }

    void open_issue(const device_models_list& devices)
    {
        std::stringstream ss;

        rs2_error* e = nullptr;

        ss << "| | |\n";
        ss << "|---|---|\n";
        ss << "|**librealsense**|" << api_version_to_string(rs2_get_api_version(&e)) << (is_debug() ? " DEBUG" : " RELEASE") << "|\n";
        ss << "|**OS**|" << rsutils::os::get_os_name() << "|\n";

        for (auto& dm : devices)
        {
            for (auto& kvp : dm->infos)
            {
                if (kvp.first != "Recommended Firmware Version" &&
                    kvp.first != "Debug Op Code" &&
                    kvp.first != "Physical Port" &&
                    kvp.first != "Product Id")
                    ss << "|**" << kvp.first << "**|" << kvp.second << "|\n";
            }
        }

        ss << "\nPlease provide a description of the problem";

        open_issue(ss.str());
    }

    std::pair<std::string, std::string> get_device_name(const device& dev)
    {
        // retrieve device name
        std::string name = (dev.supports(RS2_CAMERA_INFO_NAME)) ? dev.get_info(RS2_CAMERA_INFO_NAME) : "Unknown";

        // retrieve device serial number
        std::string serial = (dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER)) ? dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) : "Unknown";

        std::stringstream s;

        if (dev.is<playback>())
        {
            auto playback_dev = dev.as<playback>();

            s << "Playback device: ";
            name += (rsutils::string::from() << " (File: " << playback_dev.file_name() << ")");
        }
        s << std::setw(25) << std::left << name;
        return std::make_pair(s.str(), serial);        // push name and sn to list
    }

    void device_model::reset()
    {
        syncer->remove_syncer(dev_syncer);
        subdevices.resize(0);
        _recorder.reset();
    }

    device_model::~device_model()
    {
        try
        {
            {
                std::lock_guard<std::mutex> lock(dev_mutex);
                for (auto&& n : related_notifications) n->dismiss(false);

                _updates->set_device_status(*_updates_profile, false);

                stopping = true;
            }

            if (check_for_device_updates_thread.joinable())
            {
                LOG_DEBUG("Waiting for device updates thread to finish...");
                check_for_device_updates_thread.join();
                LOG_DEBUG("Device updates thread joined");
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(rsutils::string::from() << "Exception in device_model destructor: " << e.what());
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception in device_model destructor");
        }
    }

    bool device_model::check_for_bundled_fw_update(const rs2::context &ctx, std::shared_ptr<notifications_model> not_model , bool reset_delay )
    {
        // LibRS can have a "bundled" FW binary downloaded during CMake. That's the version
        // "available" to us, but it may not be there (e.g., no internet connection to download
        // it). Lacking an available version, we try to let the user choose a "recommended"
        // version for download. The recommended version is defined by the device (and comes
        // from a #define).

        // 'notification_type_is_displayed()' is used to detect if fw_update notification is on to avoid displaying it during FW update process when
        // the device enters recovery mode
        std::lock_guard<std::mutex> lock(dev_mutex); // locking to fix a bug where the destructor was called, destroying dev and causing access violation on this thread
        if (stopping) return false;

        if( ! not_model->notification_type_is_displayed< fw_update_notification_model >()
            && ( dev.is< updatable >() || dev.is< update_device >() ) )
        {
            std::string fw;
            std::string recommended_fw_ver;
            int product_line = 0;

            // Override with device info if info is available
            if (dev.is<updatable>())
            {
                fw = dev.supports( RS2_CAMERA_INFO_FIRMWARE_VERSION )
                       ? dev.get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION )
                       : "";

                recommended_fw_ver = dev.supports(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION)
                    ? dev.get_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION)
                    : "";
            }

            product_line = dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE)
                ? parse_product_line(dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE))
                : -1; // invalid product line, will be handled later on

            bool allow_rc_firmware = config_file::instance().get_or_default(
                configurations::update::allow_rc_firmware,
                false );

            bool is_rc = ( product_line == RS2_PRODUCT_LINE_D400 ) && allow_rc_firmware;
            std::string pid = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
            std::string available_fw_ver = get_available_firmware_version( product_line, pid);
            std::shared_ptr< firmware_update_manager > manager = nullptr;

            if( dev.is<update_device>() || is_upgradeable( fw, available_fw_ver) )
            {
                recommended_fw_ver = available_fw_ver;
                auto image = get_default_fw_image(product_line, pid);
                if (image.empty())
                {
                    not_model->add_log("could not detect a bundled FW version for the connected device", RS2_LOG_SEVERITY_WARN);
                    return false;
                }

                manager = std::make_shared< firmware_update_manager >( not_model,
                                                                       *this,
                                                                       dev,
                                                                       ctx,
                                                                       image,
                                                                       true );
            }

            auto dev_name = get_device_name(dev);

            // TODO - D436. Don't suggest to update FW as it doesn't support D436. Revert after next release with bundle supporting FW
            if( ( dev.is<update_device>() || is_upgradeable( fw, recommended_fw_ver) ) && pid != "1156") //0x1156 is pid for D436
            {
                std::stringstream msg;

                if (dev.is<update_device>())
                {
                    msg << dev_name.first << "\n(S/N " << dev.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID) << ")\n";
                }
                else
                {
                    msg << dev_name.first << " (S/N " << dev_name.second << ")\n"
                        << "Current Version: " << fw << "\n";
                }

                if (is_rc)
                    msg << "Release Candidate: " << recommended_fw_ver << " Pre-Release";
                else
                    msg << "Recommended Version: " << recommended_fw_ver;

                auto n = std::make_shared< fw_update_notification_model >( msg.str(),
                                                                           manager,
                                                                           false );
                // The FW update delay ID include the dismissed recommended version and the device serial number
                // This way a newer FW recommended version will not be dismissed
                n->delay_id = "fw_update_alert." + recommended_fw_ver + "." + dev_name.second;
                n->enable_complex_dismiss = true;

                // If a delay request received in the past, reset it.
                if( reset_delay ) n->reset_delay();

                if( ! n->is_delayed() )
                {
                    not_model->add_notification( n );
                    related_notifications.push_back( n );
                    return true;
                }
            }
            else
            {
                if( ! fw.empty() && ! recommended_fw_ver.empty() )
                {
                    std::stringstream msg;
                    msg << "Current FW >= Bundled FW for: " << dev_name.first << " (S/N " << dev_name.second << ")\n"
                        << "Current Version: " << fw << "\n"
                        << "Recommended Version: " << recommended_fw_ver;

                    not_model->add_log(msg.str(), RS2_LOG_SEVERITY_DEBUG);
                }
            }
        }
        return false;
    }

    void device_model::refresh_notifications(viewer_model& viewer)
    {
        for (auto&& n : related_notifications) n->dismiss(false);

        auto name = get_device_name(dev);

        // Inhibit on DQT / Playback device
        if( _allow_remove && ( ! dev.is< playback >() ) )
            check_for_device_updates(viewer);

        if ((bool)config_file::instance().get(configurations::update::recommend_calibration))
        {
            for (auto&& model : subdevices)
            {
                if (model->supports_on_chip_calib())
                {
                    // Make sure we don't spam calibration remainders too often:
                    time_t rawtime;
                    time(&rawtime);
                    std::string id = rsutils::string::from() << configurations::viewer::last_calib_notice << "." << name.second;
                    long long last_time = config_file::instance().get_or_default(id.c_str(), (long long)0);

                    std::string msg = rsutils::string::from()
                        << name.first << " (S/N " << name.second << ")";
                    auto manager = std::make_shared<on_chip_calib_manager>(viewer, model, *this, dev);
                    auto n = std::make_shared<autocalib_notification_model>(
                        msg, manager, false);

                    // Recommend calibration once a week per device
                    if (rawtime - last_time < 60)
                    {
                        n->snoozed = true;
                    }

                    // NOTE: For now do not pre-emptively suggest auto-calibration
                    // TODO: Revert in later release
                    //viewer.not_model->add_notification(n);
                    //related_notifications.push_back(n);
                }
            }
        }
    }

    device_model::device_model(device& dev, std::string& error_message, viewer_model& viewer, bool new_device_connected, bool remove)
        : dev(dev),
        _calib_model(dev, viewer.not_model),
        syncer(viewer.syncer),
        _update_readonly_options_timer(std::chrono::seconds(6)),
        _detected_objects(std::make_shared< atomic_objects_in_frame >()),
        _updates(viewer.updates),
        _updates_profile(std::make_shared<dev_updates_profile::update_profile>()),
        _allow_remove(remove),
        _dds_model(dev),
        _hdr_model(dev)
    {
        auto name = get_device_name(dev);
        id = rsutils::string::from() << name.first << ", " << name.second;

        for (auto&& sub : dev.query_sensors())
        {
            auto s = std::make_shared<sensor>(sub);
            auto objects = std::make_shared< atomic_objects_in_frame >();
            // checking if the sensor is color_sensor or is D405 (with integrated RGB in depth sensor)
            if (s->is<color_sensor>() || (dev.supports(RS2_CAMERA_INFO_PRODUCT_ID) && !strcmp(dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID), "0B5B")))
                objects = _detected_objects;
            auto model = std::make_shared<subdevice_model>(dev, std::make_shared<sensor>(sub), objects, error_message, viewer, this, new_device_connected);
            subdevices.push_back(model);
        }

        // Initialize static camera info:
        for (auto i = 0; i < RS2_CAMERA_INFO_COUNT; i++)
        {
            auto info = static_cast<rs2_camera_info>(i);

            try
            {
                if (dev.supports(info))
                {
                    auto value = dev.get_info(info);
                    infos.push_back({ std::string(rs2_camera_info_to_string(info)),
                                      std::string(value) });

                    if( info == RS2_CAMERA_INFO_PRODUCT_LINE )
                        _is_d500_device = strcmp( value, "D500" ) == 0;
                }
            }
            catch (...)
            {
                infos.push_back({ std::string(rs2_camera_info_to_string(info)),
                                  std::string("???") });
            }
        }

        if (dev.is<playback>())
        {
            for (auto&& sub : subdevices)
            {
                for (auto&& p : sub->profiles)
                {
                    sub->stream_enabled[p.unique_id()] = true;
                }
            }
            play_defaults(viewer);
        }

        refresh_notifications(viewer);

        auto path = rsutils::os::get_special_folder( rsutils::os::special_folder::user_documents );
        path += "librealsense2/presets/";
        try
        {
            std::string name = dev.get_info(RS2_CAMERA_INFO_NAME);
            std::smatch match;
            if( ! std::regex_search( name, match, std::regex( "^Intel RealSense (\\S+)" ) ) )
                throw std::runtime_error( "cannot parse device name from '" + name + "'" );

            glob(
                path,
                std::string( match[1] ) + " *.preset",
                [&]( std::string const & file ) {
                    advanced_mode_settings_file_names.insert( path + file );
                },
                false );  // recursive
        }
        catch( const std::exception & e )
        {
            LOG_WARNING( "Exception caught trying to detect presets: " << e.what() );
        }
    }
    void device_model::play_defaults(viewer_model& viewer)
    {
        if (!dev_syncer)
            dev_syncer = viewer.syncer->create_syncer();
        for (auto&& sub : subdevices)
        {
            if (!sub->streaming)
            {
                std::vector<rs2::stream_profile> profiles;
                try
                {
                    profiles = sub->get_selected_profiles();
                }
                catch (...)
                {
                    continue;
                }

                if (profiles.empty())
                    continue;

                std::string friendly_name = sub->s->get_info(RS2_CAMERA_INFO_NAME);
                if ((friendly_name.find("Tracking") != std::string::npos) ||
                    (friendly_name.find("Motion") != std::string::npos))
                {
                    viewer.synchronization_enable_prev_state = viewer.synchronization_enable.load();
                    viewer.synchronization_enable = false;
                }
                sub->play(profiles, viewer, dev_syncer);

                for (auto&& profile : profiles)
                {
                    viewer.begin_stream(sub, profile);
                }
            }
        }
    }

    void device_model::start_recording(const std::string& path, std::string& error_message)
    {
        if (_recorder != nullptr)
        {
            return; //already recording
        }

        try
        {
            int compression_mode = config_file::instance().get(configurations::record::compression_mode);
            if (compression_mode == 2)
                _recorder = std::make_shared<recorder>(path, dev);
            else
                _recorder = std::make_shared<recorder>(path, dev, compression_mode == 0);

            for (auto&& sub_dev_model : subdevices)
            {
                sub_dev_model->_is_being_recorded = true;
            }
            is_recording = true;
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

    void device_model::stop_recording(viewer_model& viewer)
    {
        auto saved_to_filename = _recorder->filename();
        _recorder.reset();
        for (auto&& sub_dev_model : subdevices)
        {
            sub_dev_model->_is_being_recorded = false;
        }
        is_recording = false;
        notification_data nd{ rsutils::string::from() << "Saved recording to:\n" << saved_to_filename,
            RS2_LOG_SEVERITY_INFO,
            RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR };
        viewer.not_model->add_notification(nd);
    }

    void device_model::pause_record()
    {
        _recorder->pause();
    }

    void device_model::resume_record()
    {
        _recorder->resume();
    }

    int device_model::draw_playback_controls(ux_window& window, ImFont* font, viewer_model& viewer)
    {
        auto p = dev.as<playback>();
        rs2_playback_status current_playback_status = p.current_status();

        const int playback_control_height = 35;
        const float combo_box_width = 90.f;
        const float icon_width = 28;
        const float line_width = 255; //Ideally should use: ImGui::GetContentRegionMax().x
        //Line looks like this ("-" == space, "[]" == icon, "[     ]" == combo_box):  |-[]-[]-[]-[]-[]-[     ]-[]-|
        const int num_icons_in_line = 6;
        const int num_combo_boxes_in_line = 1;
        const int num_spaces_in_line = num_icons_in_line + num_combo_boxes_in_line + 1;
        const float required_row_width = (num_combo_boxes_in_line * combo_box_width) + (num_icons_in_line * icon_width);
        float space_width = std::max(line_width - required_row_width, 0.f) / num_spaces_in_line;
        ImVec2 button_dim = { icon_width, icon_width };

        const bool supports_playback_step = current_playback_status == RS2_PLAYBACK_STATUS_PAUSED;

        ImGui::PushFont(font);

        //////////////////// Step Backwards Button ////////////////////
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space_width);
        std::string label = rsutils::string::from() << textual_icons::step_backward << "##Step Backward " << id;
        RsImGui::RsImButton([&](){
        if (ImGui::ButtonEx(label.c_str(), button_dim))
        {
            int fps = 0;
            for (auto&& s : viewer.streams)
            {
                if (s.second.profile.fps() > fps)
                    fps = s.second.profile.fps();
            }
            auto curr_frame = p.get_position();
            uint64_t step = fps ? uint64_t(1000.0 / (float)fps * 1e6) : 1000000ULL;
            if (curr_frame >= step)
            {
                p.seek(std::chrono::nanoseconds(curr_frame - step));
            }
        }
        }, !supports_playback_step);
        if (ImGui::IsItemHovered())
        {
            std::string tooltip = rsutils::string::from() << "Step Backwards" << (supports_playback_step ? "" : "(Not available)");
            RsImGui::CustomTooltip("%s", tooltip.c_str());
        }
        ImGui::SameLine();
        //////////////////// Step Backwards Button ////////////////////


        //////////////////// Stop Button ////////////////////
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space_width);
        label = rsutils::string::from() << textual_icons::stop << "##Stop Playback " << id;

        if (ImGui::ButtonEx(label.c_str(), button_dim))
        {
            bool prev = _playback_repeat;
            _playback_repeat = false;
            p.stop();
            _playback_repeat = prev;
        }
        if (ImGui::IsItemHovered())
        {
            std::string tooltip = rsutils::string::from() << "Stop Playback";
            RsImGui::CustomTooltip("%s", tooltip.c_str());
        }
        ImGui::SameLine();
        //////////////////// Stop Button ////////////////////



        //////////////////// Pause/Play Button ////////////////////
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space_width);
        if (current_playback_status == RS2_PLAYBACK_STATUS_PAUSED || current_playback_status == RS2_PLAYBACK_STATUS_STOPPED)
        {
            label = rsutils::string::from() << textual_icons::play << "##Play " << id;
            if (ImGui::ButtonEx(label.c_str(), button_dim))
            {
                if (current_playback_status == RS2_PLAYBACK_STATUS_STOPPED)
                {
                    play_defaults(viewer);
                }
                else
                {
                    syncer->on_frame = [] {};
                    for (auto&& s : subdevices)
                    {
                        s->on_frame = [] {};
                        if (s->streaming)
                            s->resume();
                    }
                    viewer.paused = false;
                    p.resume();
                }

            }
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip(current_playback_status == RS2_PLAYBACK_STATUS_PAUSED ? "Resume Playback" : "Start Playback");
            }
        }
        else
        {
            label = rsutils::string::from() << textual_icons::pause << "##Pause Playback " << id;
            if (ImGui::ButtonEx(label.c_str(), button_dim))
            {
                p.pause();
                for (auto&& s : subdevices)
                {
                    if (s->streaming)
                        s->pause();
                }
                viewer.paused = true;
            }
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("Pause Playback");
            }
        }

        ImGui::SameLine();
        //////////////////// Pause/Play Button ////////////////////




        //////////////////// Step Forward Button ////////////////////
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space_width);
        label = rsutils::string::from() << textual_icons::step_forward << "##Step Forward " << id;
        RsImGui::RsImButton([&]() {
        if (ImGui::ButtonEx(label.c_str(), button_dim))
        {
            int fps = 0;
            for (auto&& s : viewer.streams)
            {
                if (s.second.profile.fps() > fps)
                    fps = s.second.profile.fps();
            }
            auto curr_frame = p.get_position();
            uint64_t step = fps ? uint64_t(1000.0 / (float)fps * 1e6) : 1000000ULL;
            p.seek(std::chrono::nanoseconds(curr_frame + step));
        }}, !supports_playback_step);
        if (ImGui::IsItemHovered())
        {
            std::string tooltip = rsutils::string::from() << "Step Forward" << (supports_playback_step ? "" : "(Not available)");
            RsImGui::CustomTooltip("%s", tooltip.c_str());
        }
        ImGui::SameLine();
        //////////////////// Step Forward Button ////////////////////




        /////////////////// Repeat Button /////////////////////
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space_width);
        if (_playback_repeat)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        }
        label = rsutils::string::from() << textual_icons::repeat << "##Repeat " << id;
        if (ImGui::ButtonEx(label.c_str(), button_dim))
        {
            _playback_repeat = !_playback_repeat;
        }
        if (ImGui::IsItemHovered())
        {
            std::string tooltip = rsutils::string::from() << (_playback_repeat ? "Disable " : "Enable ") << "Repeat ";
            RsImGui::CustomTooltip("%s", tooltip.c_str());
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        /////////////////// Repeat Button /////////////////////


        //////////////////// Speed combo box ////////////////////
        auto pos = ImGui::GetCursorPos();
        const float speed_combo_box_v_alignment = 3.f;
        ImGui::SetCursorPos({ pos.x + space_width, pos.y + speed_combo_box_v_alignment });
        ImGui::PushItemWidth(combo_box_width);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);

        label = rsutils::string::from() << "## " << id;
        if (ImGui::Combo(label.c_str(), &playback_speed_index, "Speed:   x0.25\0Speed:   x0.5\0Speed:   x1\0Speed:   x1.5\0Speed:   x2\0\0", -1))
        {
            float speed = 1;
            switch (playback_speed_index)
            {
            case 0: speed = 0.25f; break;
            case 1: speed = 0.5f; break;
            case 2: speed = 1.0f; break;
            case 3: speed = 1.5f; break;
            case 4: speed = 2.0f; break;
            default:
                throw std::runtime_error( rsutils::string::from() << "Speed #" << playback_speed_index << " is unhandled");
            }
            p.set_playback_speed(speed);
        }
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("Change playback speed rate");
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        //Restore Y movement
        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos({ pos.x, pos.y - speed_combo_box_v_alignment });
        //////////////////// Speed combo box ////////////////////

        ////////////////////    Info Icon    ////////////////////
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space_width);
        draw_info_icon(window, font, button_dim);
        ////////////////////    Info Icon    ////////////////////

        ImGui::PopFont();

        return playback_control_height;
    }

    std::string device_model::pretty_time(std::chrono::nanoseconds duration)
    {
        using namespace std::chrono;
        auto hhh = duration_cast<hours>(duration);
        duration -= hhh;
        auto mm = duration_cast<minutes>(duration);
        duration -= mm;
        auto ss = duration_cast<seconds>(duration);
        duration -= ss;
        auto ms = duration_cast<milliseconds>(duration);

        std::ostringstream stream;
        stream << std::setfill('0') << std::setw(hhh.count() >= 10 ? 2 : 1) << hhh.count() << ':' <<
            std::setfill('0') << std::setw(2) << mm.count() << ':' <<
            std::setfill('0') << std::setw(2) << ss.count() << '.' <<
            std::setfill('0') << std::setw(3) << ms.count();
        return stream.str();
    }

    int device_model::draw_seek_bar()
    {
        auto pos = ImGui::GetCursorPos();

        auto p = dev.as<playback>();
        //rs2_playback_status current_playback_status = p.current_status();
        int64_t playback_total_duration = p.get_duration().count();
        auto progress = p.get_position();
        double part = (1.0 * progress) / playback_total_duration;
        seek_pos = static_cast<int>(std::max(0.0, std::min(part, 1.0)) * 100);
        auto playback_status = p.current_status();
        if (seek_pos != 0 && playback_status == RS2_PLAYBACK_STATUS_STOPPED)
        {
            seek_pos = 0;
        }
        float seek_bar_width = 300.f;
        ImGui::PushItemWidth(seek_bar_width);
        std::string label1 = "## " + id;
        if (ImGui::SliderInt(label1.c_str(), &seek_pos, 0, 100, ""))
        {
            //Seek was dragged
            if (playback_status != RS2_PLAYBACK_STATUS_STOPPED) //Ignore seek when playback is stopped
            {
                auto duration_db = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(p.get_duration());
                auto single_percent = duration_db.count() / 100;
                auto seek_time = std::chrono::duration<double, std::nano>(seek_pos * single_percent);
                p.seek(std::chrono::duration_cast<std::chrono::nanoseconds>(seek_time));
            }
        }

        ImGui::SetCursorPos({ pos.x, pos.y + 17 });

        std::string time_elapsed = pretty_time(std::chrono::nanoseconds(progress));
        std::string duration_str = pretty_time(std::chrono::nanoseconds(playback_total_duration));
        ImGui::Text("%s", time_elapsed.c_str());
        ImGui::SameLine();
        float pos_y = ImGui::GetCursorPosY();

        ImGui::SetCursorPos({ pos.x + seek_bar_width - 45 , pos_y });
        ImGui::Text("%s", duration_str.c_str());

        return 50;
    }

    int device_model::draw_playback_panel(ux_window& window, ImFont* font, viewer_model& view)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, from_rgba(0, 0xae, 0xff, 255));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);


        auto pos = ImGui::GetCursorPos();
        auto controls_height = draw_playback_controls(window, font, view);
        float seek_bar_left_alignment = 4.f;
        ImGui::SetCursorPos({ pos.x + seek_bar_left_alignment, pos.y + controls_height });
        ImGui::PushFont(font);
        auto seek_bar_height = draw_seek_bar();
        ImGui::PopFont();
        ImGui::PopStyleColor(5);
        return controls_height + seek_bar_height;

    }

    bool device_model::prompt_toggle_advanced_mode(bool enable_advanced_mode, const std::string& message_text, std::vector<std::string>& restarting_device_info, viewer_model& view, ux_window& window, const std::string& error_message)
    {
        bool keep_showing = true;
        bool yes_was_chosen = false;
        if (yes_no_dialog("Advanced Mode", message_text, yes_was_chosen, window, error_message))
        {
            // meaning the user confirmed the advanced mode toggling
            if (yes_was_chosen)
            {
                dev.as<advanced_mode>().toggle_advanced_mode(enable_advanced_mode);
                restarting_device_info = get_device_info(dev, false);
                view.not_model->add_log(enable_advanced_mode ? "Turning on advanced mode..." : "Turning off  advanced mode...");
                
                for( auto&& subdevice : subdevices )
                {
                    if( subdevice->s->is< rs2::depth_sensor >() )
                    {
                        subdevice->repopulate_options();
                        break;
                    }
                }
            }
            keep_showing = false;
        }
        return keep_showing;
    }


    bool device_model::draw_advanced_controls(viewer_model& view, ux_window& window, std::string& error_message, bool is_streaming)
    {
        bool was_set = false;

        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 0.9f, 0.9f, 0.9f, 1 });

        auto is_advanced_mode = dev.is<advanced_mode>();
        if (is_advanced_mode && ImGui::TreeNode("Advanced Controls"))
        {
            try
            {
                auto advanced = dev.as<advanced_mode>();
                if (advanced.is_enabled())
                {
                    std::string dev_name = dev.supports(RS2_CAMERA_INFO_NAME) ? dev.get_info(RS2_CAMERA_INFO_NAME) : "";
                    bool d457_device = (dev_name.find("D457") != std::string::npos);

                    draw_advanced_mode_controls(advanced, amc, get_curr_advanced_controls, was_set, error_message, d457_device);
                }
                else
                {
                    if( _is_d500_device )  // D500 cannot toggle Advanced Mode
                    {
                        ImGui::TextColored( redish, "Device FW does not support advanced mode" );
                    }
                    else if (is_streaming)
                    {
                        ImGui::TextColored( redish, "Advanced mode cannot be enabled\nwhen streaming" );
                    }
                    else
                    {
                        ImGui::TextColored( redish, "Device is not in advanced mode" );
                        std::string button_text = rsutils::string::from() << "Turn on Advanced Mode" << "##" << id;
                        static bool show_yes_no_modal = false;
                        if (ImGui::Button(button_text.c_str(), ImVec2{ 226, 0 }))
                        {
                            show_yes_no_modal = true;
                        }
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("Advanced mode is a persistent camera state unlocking calibration formats and depth generation controls\nYou can always reset the camera to factory defaults by disabling advanced mode");
                        }
                        if (show_yes_no_modal)
                        {
                            show_yes_no_modal = prompt_toggle_advanced_mode(true, "\t\tAre you sure you want to turn on Advanced Mode?\t\t", restarting_device_info, view, window, error_message);
                        }
                    }
                }
            }
            catch (const std::exception& ex)
            {
                error_message = ex.what();
            }

            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
        return was_set;
    }

    void device_model::draw_info_icon(ux_window& window, ImFont* font, const ImVec2& size)
    {
        ImGui::PopFont();
        ImGui::PushFont(window.get_large_font());
        std::string info_button_name = rsutils::string::from() << textual_icons::info_circle << "##" << id;
        auto info_button_color = show_device_info ? light_blue : light_grey;
        ImGui::PushStyleColor(ImGuiCol_Text, info_button_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, info_button_color);
        if (ImGui::Button(info_button_name.c_str(), size))
        {
            show_device_info = !show_device_info;
        }
        ImGui::PopFont();
        ImGui::PushFont(window.get_font());
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", show_device_info ? "Hide Device Details" : "Show Device Details");
            window.link_hovered();
        }
        ImGui::PopStyleColor(2);
    }

    void device_model::begin_update_unsigned(viewer_model& viewer, std::string& error_message)
    {
        try
        {
            std::vector<uint8_t> data;
            auto ret = file_dialog_open(open_file, "Unsigned Firmware Image\0*.bin\0*.img\0", NULL, NULL);
            if (ret)
            {
                std::ifstream file(ret, std::ios::binary | std::ios::in);
                if (file.good())
                {
                    data = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
                }
                else
                {
                    error_message = rsutils::string::from() << "Could not open file '" << ret << "'";
                    return;
                }
            }

            else return; // Aborted by the user

            auto manager = std::make_shared<firmware_update_manager>(viewer.not_model, *this, dev, viewer.ctx, std::move( data ), false);

            auto n = std::make_shared<fw_update_notification_model>(
                "Manual Update requested", manager, true);
            viewer.not_model->add_notification(n);

            for (auto&& n : related_notifications)
                if (dynamic_cast<fw_update_notification_model*>(n.get()))
                    n->dismiss(false);

            auto invoke = [n](std::function<void()> action) {
                n->invoke(action);
            };
            manager->start(invoke);
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

    void device_model::begin_update(std::vector<uint8_t> data, viewer_model& viewer, std::string& error_message)
    {
        try
        {
            if (data.size() == 0)
            {
                auto ret = file_dialog_open(open_file, "Signed Firmware Image\0*.bin\0*.img\0", NULL, NULL);
                if (ret)
                {
                    std::ifstream file(ret, std::ios::binary | std::ios::in);
                    if (file.good())
                    {
                        data = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
                    }
                    else
                    {
                        error_message = rsutils::string::from() << "Could not open file '" << ret << "'";
                        return;
                    }
                }
                else return; // Aborted by the user
            }

            auto manager = std::make_shared<firmware_update_manager>(viewer.not_model, *this, dev, viewer.ctx, std::move( data ), true);

            auto n = std::make_shared<fw_update_notification_model>(
                "Manual Update requested", manager, true);
            viewer.not_model->add_notification(n);

            for (auto&& n : related_notifications)
                n->dismiss(false);

            auto invoke = [n](std::function<void()> action) {
                n->invoke(action);
            };

            manager->start(invoke);
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

    void device_model::check_for_device_updates(viewer_model& viewer, bool activated_by_user )
    {
        std::weak_ptr< updates_model > updates_model_protected( viewer.updates );
        std::weak_ptr< dev_updates_profile::update_profile > update_profile_protected(
            _updates_profile );
        std::weak_ptr< notifications_model > notification_model_protected( viewer.not_model );
        const context & ctx( viewer.ctx );
        if (check_for_device_updates_thread.joinable())
            check_for_device_updates_thread.join();
        check_for_device_updates_thread = std::thread( [ctx,
                                                      updates_model_protected,
                                                      notification_model_protected,
                                                      this,
                                                      update_profile_protected,
                                                      activated_by_user]() {
            try
            {
                bool need_to_check_bundle = true;
                std::string server_url
                    = config_file::instance().get( configurations::update::sw_updates_url );
                bool use_local_file = false;
                const std::string local_file_prefix = "file://";

                // If URL contain a "file://"  prefix, we open it as local file and not downloading
                // it from a server
                if( server_url.find( local_file_prefix ) == 0 )
                {
                    use_local_file = true;
                    server_url.erase( 0, local_file_prefix.length() );
                }
                sw_update::dev_updates_profile updates_profile( dev, server_url, use_local_file );

                bool fail_access_db = false;
                bool sw_online_update_available = updates_profile.retrieve_updates( sw_update::LIBREALSENSE, fail_access_db);
                bool fw_online_update_available = updates_profile.retrieve_updates( sw_update::FIRMWARE, fail_access_db);
                bool fw_bundled_update_available = false;
                if (sw_online_update_available || fw_online_update_available)
                {
                    if (auto update_profile = update_profile_protected.lock())
                    {
                        *update_profile = updates_profile.get_update_profile();
                        updates_model::update_profile_model updates_profile_model(*update_profile,
                            ctx,
                            this);

                        dev_updates_profile::version_info sw_update_info, fw_update_info;
                        bool essential_sw_update_found = update_profile->get_sw_update(sw_update::ESSENTIAL, sw_update_info);
                        bool essential_fw_update_found = update_profile->get_fw_update(sw_update::ESSENTIAL, fw_update_info);
                        if (essential_sw_update_found || essential_fw_update_found)
                        {
                            if (auto viewer_updates = updates_model_protected.lock())
                            {
                                viewer_updates->add_profile(updates_profile_model);
                                need_to_check_bundle = false;

                                // Log the essential updates
                                if (auto nm = notification_model_protected.lock())
                                {
                                    if( essential_sw_update_found )
                                        nm->add_log(
                                            rsutils::string::from()
                                                << update_profile->device_name << " (S/N "
                                                << update_profile->serial_number << ")\n"
                                                << "Current SW version: " << std::string( update_profile->software_version )
                                                << "\nEssential SW version: "
                                                << std::string( sw_update_info.ver ),
                                            RS2_LOG_SEVERITY_WARN );

                                    if( essential_fw_update_found )
                                        nm->add_log(
                                            rsutils::string::from()
                                                << update_profile->device_name << " (S/N "
                                                << update_profile->serial_number << ")\n"
                                                << "Current FW version: " << std::string( update_profile->firmware_version )
                                                << "\nEssential FW version: "
                                                << std::string( fw_update_info.ver ),
                                            RS2_LOG_SEVERITY_WARN );
                                }
                            }
                        }
                        else
                        {
                            if (auto viewer_updates = updates_model_protected.lock())
                            {
                                // Do not create pop ups if the viewer updates windows is on
                                if (viewer_updates->has_updates())
                                {
                                    need_to_check_bundle = false;
                                }
                                else
                                {
                                    if (sw_online_update_available)
                                    {
                                        if (auto nm = notification_model_protected.lock())
                                        {
                                            handle_online_sw_update( nm, update_profile, activated_by_user);
                                        }
                                    }
                                    if (fw_online_update_available)
                                    {
                                        if (auto nm = notification_model_protected.lock())
                                        {
                                            need_to_check_bundle = !handle_online_fw_update( ctx, nm, update_profile , activated_by_user);
                                        }
                                    }
                                }
                            }
                            // For updating current device profile if exists (Could update firmware version)
                            if (auto viewer_updates = updates_model_protected.lock())
                            {
                                viewer_updates->update_profile(updates_profile_model);
                            }
                        }
                    }
                }
                else if( auto nm = notification_model_protected.lock() )
                {
                    if ( activated_by_user && fail_access_db )
                        nm->add_notification( { rsutils::string::from() << textual_icons::wifi << "  Unable to retrieve updates.\nPlease check your network connection.\n",
                                                RS2_LOG_SEVERITY_ERROR,
                                                RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR } );
                    else
                        nm->add_log( "No online SW / FW updates available" );
                }

                // If no on-line updates notification, offer bundled FW update if needed
                if( need_to_check_bundle
                    && (bool)config_file::instance().get( configurations::update::recommend_updates ) )
                {
                    if( auto nm = notification_model_protected.lock() )
                    {
                        fw_bundled_update_available = check_for_bundled_fw_update( ctx, nm , activated_by_user);
                    }
                }

                // When no updates available (on-line + bundled), add a notification to indicate "all up to date"
                if( activated_by_user && ! fail_access_db && ! sw_online_update_available
                    && ! fw_online_update_available && ! fw_bundled_update_available )
                {
                    auto n = std::make_shared< sw_update_up_to_date_model >();
                    auto name = get_device_name(dev);
                    n->delay_id = "no_updates_alert." + name.second;

                    if (auto nm = notification_model_protected.lock())
                    {
                        nm->add_notification(n);
                        related_notifications.push_back(n);
                    }
                }
            }
            catch( const std::exception & e )
            {
                auto error = e.what();
            }
        } );

    }

    float device_model::draw_device_panel(float panel_width,
        ux_window& window,
        std::string& error_message,
        viewer_model& viewer)
    {
        /*
        =============================
        [o]     [@]     [i]     [=]
        Record   Sync    Info    More
        =============================
        */


        const bool is_playback_device = dev.is<playback>();
        const float device_panel_height = 60.0f;
        auto panel_pos = ImGui::GetCursorPos();

        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));

        ////////////////////////////////////////
        // Draw recording icon
        ////////////////////////////////////////
        bool is_streaming = std::any_of(subdevices.begin(), subdevices.end(), [](const std::shared_ptr<subdevice_model>& sm)
        {
            return sm->streaming;
        });
        textual_icon button_icon = is_recording ? textual_icons::stop : textual_icons::circle;
        const float icons_width = 78.0f;
        const ImVec2 device_panel_icons_size{ icons_width, 25 };
        std::string record_button_name = rsutils::string::from() << button_icon << "##" << id;
        auto record_button_color = is_recording ? light_blue : light_grey;
        ImGui::PushStyleColor(ImGuiCol_Text, record_button_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, record_button_color);

        RsImGui::RsImButton([&]() {
        if (ImGui::ButtonEx(record_button_name.c_str(), device_panel_icons_size))
        {
            if (is_recording) //is_recording is changed inside stop/start_recording
            {
                stop_recording(viewer);
            }
            else
            {
                int recording_setting = config_file::instance().get(configurations::record::file_save_mode);
                std::string path = "";
                std::string default_path = config_file::instance().get(configurations::record::default_path);
                if (!ends_with(default_path, "/") && !ends_with(default_path, "\\")) default_path += "/";
                std::string default_filename = rs2::get_timestamped_file_name() + ".bag";
                if (recording_setting == 0 && default_path.size() > 1 )
                {
                    path = default_path + default_filename;
                }
                else
                {
                    if (const char* ret = file_dialog_open(file_dialog_mode::save_file, "ROS-bag\0*.bag\0",
                        default_path.c_str(), default_filename.c_str()))
                    {
                        path = ret;
                        if (!ends_with(rsutils::string::to_lower(path), ".bag")) path += ".bag";
                    }
                }

                if (path != "") start_recording(path, error_message);
            }
        }}, disable_record_button_logic(is_streaming, is_playback_device));
        ImGui::PopFont();
        ImGui::PushFont(window.get_font());
        if (ImGui::IsItemHovered())
        {
            std::string record_button_hover_text = get_record_button_hover_text(is_streaming);
            RsImGui::CustomTooltip("%s", record_button_hover_text.c_str());
            if (is_streaming) window.link_hovered();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        ////////////////////////////////////////
        // Draw Sync icon
        ////////////////////////////////////////
        std::string sync_button_name = rsutils::string::from() << textual_icons::refresh << "##" << id;
        bool is_sync_enabled = false; //TODO: use device's member
        auto sync_button_color = is_sync_enabled ? light_blue : light_grey;
        ImGui::PushStyleColor(ImGuiCol_Text, sync_button_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, sync_button_color);
        ImGui::PopFont();
        ImGui::PushFont(window.get_large_font());
        RsImGui::RsImButton([&]() {
        if (ImGui::ButtonEx(sync_button_name.c_str(), device_panel_icons_size))
        {
            is_sync_enabled = !is_sync_enabled;
        }
        }, true);
        ImGui::PopStyleColor(2);
        ImGui::PopFont();
        ImGui::PushFont(window.get_font());
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", is_sync_enabled ? "Disable streams synchronization" : "Enable streams synchronization");
        }
        ImGui::SameLine();
        ////////////////////////////////////////
        // Draw Info icon
        ////////////////////////////////////////
        draw_info_icon(window, window.get_font(), device_panel_icons_size);
        ImGui::SameLine();

        ////////////////////////////////////////
        // Draw Menu icon
        ////////////////////////////////////////
        std::string label = rsutils::string::from() << "device_menu" << id;
        std::string bars_button_name = rsutils::string::from() << textual_icons::bars << "##" << id;
        ImGui::PopFont();
        ImGui::PushFont(window.get_large_font());
        if (ImGui::Button(bars_button_name.c_str(), device_panel_icons_size))
        {
            ImGui::OpenPopup(label.c_str());
        }
        ImGui::PopFont();
        ImGui::PushFont(window.get_font());
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", "Click for more");
            window.link_hovered();
        }
        bool open_calibration_ui = false;
        if (ImGui::BeginPopup(label.c_str()))
        {

            bool something_to_show = false;
            ImGui::PushStyleColor(ImGuiCol_Text, dark_grey);

            if (_allow_remove)
            {
                something_to_show = true;

                if (auto adv = dev.as<advanced_mode>())
                {
                    const bool is_advanced_mode_enabled = adv.is_enabled();
                    bool selected = is_advanced_mode_enabled;
                    // D500 cannot toggle Advanced Mode
                    if( ! _is_d500_device && ImGui::MenuItem( "Advanced Mode", nullptr, &selected, !is_streaming ) )
                    {
                        show_advanced_mode_popup = true;

                        ImGui::Separator();
                    }
                }

                if (ImGui::Selectable("Hardware Reset"))
                {
                    try
                    {
                        restarting_device_info = get_device_info(dev, false);
                        dev.hardware_reset();
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

                // fw update disabled when any sensor is streaming
                ImGuiSelectableFlags updateFwFlags = (is_streaming) ? ImGuiSelectableFlags_Disabled : 0;

                if (dev.is<rs2::updatable>() || dev.is<rs2::update_device>())
                {
                    if (ImGui::Selectable("Update Firmware", false, updateFwFlags))
                    {
                        begin_update({}, viewer, error_message);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        std::string tooltip = rsutils::string::from()
                                           << "Install official signed firmware from file to the device"
                                           << ( is_streaming ? " (Disabled while streaming)" : "" );
                        RsImGui::CustomTooltip("%s", tooltip.c_str());
                    }

                    if( dev.supports( RS2_CAMERA_INFO_PRODUCT_LINE )
                        && ( dev.get_info( RS2_CAMERA_INFO_PRODUCT_LINE ) ) )
                    {
                        if( ImGui::Selectable( "Check For Updates", false, updateFwFlags ) )
                        {
                            // Remove all previous SW/FW update notifications before triggering checking for updates logic
                            for( auto && n : related_notifications )
                            {
                                if( n->is< fw_update_notification_model >()
                                    || n->is< sw_recommended_update_alert_model >() )
                                    n->dismiss( false ); // No need for snooze, if needed a new notification will be popped
                            }

                            check_for_device_updates( viewer , true);
                        }
                    }

                    if (ImGui::IsItemHovered())
                    {
                        std::string tooltip = rsutils::string::from() << "Check for SW / FW updates";
                        RsImGui::CustomTooltip("%s", tooltip.c_str());
                    }
                }

                bool is_locked = true;
                if (dev.supports(RS2_CAMERA_INFO_CAMERA_LOCKED))
                    is_locked = std::string(dev.get_info(RS2_CAMERA_INFO_CAMERA_LOCKED)) == "YES" ? true : false;

                if (dev.is<rs2::updatable>() && !is_locked)
                {
                    auto pl = dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
                    bool is_d400_device = (std::string(pl) == "D400");
                        
                    if( is_d400_device )
                    {    
                        if (ImGui::Selectable("Update Unsigned Firmware...", false, updateFwFlags))
                        {
                            begin_update_unsigned(viewer, error_message);
                        }
                        if (ImGui::IsItemHovered())
                        {
                            std::string tooltip = rsutils::string::from()
                                               << "Install non official unsigned firmware from file to the device"
                                               << ( is_streaming ? " (Disabled while streaming)" : "" );
                            RsImGui::CustomTooltip("%s", tooltip.c_str());
                        }
                    }
                }

                ImGuiSelectableFlags is_streaming_flag = (is_streaming) ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None;
                if( _dds_model.supports_DDS() )
                {
                    if( ImGui::Selectable( "DDS Configuration", false, is_streaming_flag ) )
                    {
                        _dds_model.open_dds_tool_window();
                    }
                    if( ImGui::IsItemHovered() )
                    {
                        std::string tooltip = rsutils::string::from()
                                           << "Change the configuration of Ethernet based devices"
                                           << ( is_streaming ? " (Disabled while streaming)" : "" );
                        RsImGui::CustomTooltip( "%s", tooltip.c_str() );
                    }
                }
            }

            draw_device_panel_auto_calib(viewer, something_to_show, error_message);

            ImGui::PopStyleColor();

            ImGui::EndPopup();

        }


        if (show_advanced_mode_popup)
        {
            const bool is_advanced_mode_enabled = dev.as<advanced_mode>().is_enabled();
            std::string msg = rsutils::string::from()
                           << "\t\tAre you sure you want to "
                           << ( is_advanced_mode_enabled ? "turn off Advanced mode" : "turn on Advanced mode" )
                           << "\t\t";
            show_advanced_mode_popup = prompt_toggle_advanced_mode(!is_advanced_mode_enabled, msg, restarting_device_info, viewer, window, error_message);
        }

        _calib_model.update(window, error_message);

        if ( _hdr_model.supports_HDR() )
            _hdr_model.render_hdr_config_window( window, error_message );

        if( _dds_model.supports_DDS() )
        {
            _dds_model.render_dds_config_window( window, error_message );
        }


        ////////////////////////////////////////
        // Draw icons names
        ////////////////////////////////////////
        //Move to next line, and we want to keep the horizontal alignment
        ImGui::SetCursorPos({ panel_pos.x, ImGui::GetCursorPosY() });
        //Using transparent-non-actionable buttons to have the same locations
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        const ImVec2 device_panel_icons_text_size = { icons_width, 5 };

        ImGui::PushStyleColor(ImGuiCol_Text, record_button_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, record_button_color);
        RsImGui::RsImButton([&]() {ImGui::ButtonEx(is_recording ? "Stop" : "Record", device_panel_icons_size);}, !is_streaming);
        if (ImGui::IsItemHovered() && is_streaming) window.link_hovered();
        ImGui::PopStyleColor(2);

        ImGui::SameLine();  
        RsImGui::RsImButton([&]() {ImGui::ButtonEx("Sync", device_panel_icons_size);},true);

        auto info_button_color = show_device_info ? light_blue : light_grey;
        ImGui::PushStyleColor(ImGuiCol_Text, info_button_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, info_button_color);
        ImGui::SameLine(); ImGui::ButtonEx("Info", device_panel_icons_size);
        if (ImGui::IsItemHovered()) window.link_hovered();
        ImGui::PopStyleColor(2);

        ImGui::SameLine(); ImGui::ButtonEx("More", device_panel_icons_size);
        if (ImGui::IsItemHovered()) window.link_hovered();
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(7);
        ImGui::PopFont();

        return device_panel_height;
    }

    void device_model::save_viewer_configurations(std::ofstream& outfile, json& j)
    {
        for (auto&& sub : subdevices)
        {
            int num_irs = 0;
            bool video_profile_saved = false;
            for (auto&& p : sub->get_selected_profiles())
            {
                rs2_stream stream_type = p.stream_type();
                std::string stream_format_key
                    = rsutils::string::from()
                   << "stream-" << rsutils::string::to_lower( rs2_stream_to_string( stream_type ) ) << "-format";
                std::string stream_format_value = rs2_format_to_string(p.format());

                if (stream_type == RS2_STREAM_DEPTH)
                {
                    stream_format_key = "stream-depth-format";
                }
                else if (stream_type == RS2_STREAM_INFRARED)
                {
                    stream_format_key = "stream-ir-format";
                    num_irs++;
                    if (num_irs == 2)
                    {
                        stream_format_value = "R8L8";
                    }
                }
                else
                {
                    continue; //TODO: Ignoring other streams for now
                }

                j[stream_format_key] = stream_format_value;
                if (!video_profile_saved)
                {
                    if (auto vp = p.as<rs2::video_stream_profile>())
                    {
                        j["stream-width"] = std::to_string(vp.width());
                        j["stream-height"] = std::to_string(vp.height());
                        j["stream-fps"] = std::to_string(vp.fps());
                        video_profile_saved = true;
                    }
                }
            }
        }
    }

    void rs2::device_model::handle_online_sw_update(
        std::shared_ptr< notifications_model > nm,
        std::shared_ptr< dev_updates_profile::update_profile > update_profile,
        bool reset_delay )
    {
        dev_updates_profile::version_info recommended_sw_update_info;
        update_profile->get_sw_update(sw_update::RECOMMENDED, recommended_sw_update_info);
        auto n = std::make_shared< sw_recommended_update_alert_model >(
            RS2_API_FULL_VERSION_STR,
            recommended_sw_update_info.ver,
            recommended_sw_update_info.download_link);

        auto dev_name = get_device_name(dev);
        // The SW update delay ID include the dismissed recommended version and the device serial number
        // This way a newer SW recommended version will not be dismissed.
        n->delay_id = "sw_update_alert." + std::string(recommended_sw_update_info.ver) + "." + dev_name.second;
        n->enable_complex_dismiss = true;  // allow advanced dismiss menu

        if ( reset_delay ) n->reset_delay();

        if (!n->is_delayed())
        {
            nm->add_notification(n);
            related_notifications.push_back(n);
        }
    }

    bool rs2::device_model::handle_online_fw_update(
        const context & ctx,
        std::shared_ptr< notifications_model > nm,
        std::shared_ptr< dev_updates_profile::update_profile > update_profile,
        bool reset_delay )
    {
        bool fw_update_notification_raised = false;
        std::shared_ptr< firmware_update_manager > manager = nullptr;

        std::vector< uint8_t > fw_data;
        http::http_downloader downloader;

        // Try to download the recommended FW binary file
        int download_retries = 3;
        dev_updates_profile::version_info recommended_fw_update_info;
        update_profile->get_fw_update(sw_update::RECOMMENDED, recommended_fw_update_info);

        while (download_retries > 0)
        {
            if (downloader.download_to_bytes_vector(
                recommended_fw_update_info.download_link,
                fw_data))
                download_retries = 0;
            else
                --download_retries;
        }

        // If the download process finished successfully, pop up a
        // notification for the FW update process
        if (!fw_data.empty())
        {
            manager = std::make_shared< firmware_update_manager >(nm,
                *this,
                dev,
                ctx,
                fw_data,
                true);

            auto dev_name = get_device_name(dev);
            std::stringstream msg;
            msg << dev_name.first << " (S/N " << dev_name.second << ")\n"
                << "Current Version: "
                << std::string(update_profile->firmware_version) << "\n";

            msg << "Recommended Version: "
                << std::string(recommended_fw_update_info.ver);

            auto n = std::make_shared< fw_update_notification_model >(
                msg.str(),
                manager,
                false);
            n->delay_id = "fw_update_alert." +  std::string(recommended_fw_update_info.ver) + "." +  dev_name.second;
            n->enable_complex_dismiss = true;

            if ( reset_delay ) n->reset_delay();

            if (!n->is_delayed())
            {
                nm->add_notification(n);
                related_notifications.push_back(n);
                fw_update_notification_raised = true;
            }
        }
        else
            nm->output.add_log( RS2_LOG_SEVERITY_WARN,
                                __FILE__,
                                __LINE__,
                                rsutils::string::from() << "Error in downloading FW binary file: "
                                                        << recommended_fw_update_info.download_link );

        return fw_update_notification_raised;
    }

    // Load viewer configuration for stereo module (depth/infrared streams) only
    void device_model::load_viewer_configurations(const std::string& json_str)
    {
        json j;
        json raw_j = json::parse(json_str);

        // If the viewer contain a viewer section, look inside it for the viewer parameters,
        // for backward compatibility, if no viewer section exist, look at the root level
        auto viewer_section = raw_j.find("viewer");
        j = (viewer_section != raw_j.end()) ? viewer_section.value() : raw_j;

        struct video_stream
        {
            rs2_format format = RS2_FORMAT_ANY;
            int width = 0;
            int height = 0;
            int fps = 0;
        };

        std::map<std::pair<rs2_stream, int>, video_stream> requested_streams;
        
        auto it = j.find("stream-depth-format");
        if (it != j.end())
        {
            assert(it.value().is_string());
            std::string formatstr = it.value();
            bool found = false;
            for (int i = 0; i < static_cast<int>(RS2_FORMAT_COUNT); i++)
            {
                auto f = static_cast<rs2_format>(i);
                auto fstr = rs2_format_to_string(f);
                if (formatstr == fstr)
                {
                    requested_streams[std::make_pair(RS2_STREAM_DEPTH, 0)].format = f;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                throw std::runtime_error( rsutils::string::from() << "Unsupported stream-depth-format: " << formatstr );
            }

            // Disable depth stream on all sub devices
            for (auto&& sub : subdevices)
            {
                for (auto&& profile : sub->profiles)
                {
                    if (profile.stream_type() == RS2_STREAM_DEPTH)
                    {
                        sub->stream_enabled[profile.unique_id()] = false;
                        break;
                    }
                }
            }
        }

        it = j.find("stream-ir-format");
        if (it != j.end())
        {
            assert(it.value().is_string());
            std::string formatstr = it.value();

            bool found = false;
            for (int i = 0; i < RS2_FORMAT_COUNT; i++)
            {
                auto format = (rs2_format)i;
                if (ends_with(rs2_format_to_string(format), formatstr))
                {
                    requested_streams[std::make_pair(RS2_STREAM_INFRARED, 1)].format = format;
                    found = true;
                }
            }

            if (!found)
            {
                if (formatstr == "R8L8")
                {
                    requested_streams[std::make_pair(RS2_STREAM_INFRARED, 1)].format = RS2_FORMAT_Y8;
                    requested_streams[std::make_pair(RS2_STREAM_INFRARED, 2)].format = RS2_FORMAT_Y8;
                }
                else
                {
                    throw std::runtime_error( rsutils::string::from()
                                              << "Unsupported stream-ir-format: " << formatstr );
                }
            }

            // Disable infrared stream on all sub devices
            for (auto&& sub : subdevices)
            {
                for (auto&& profile : sub->profiles)
                {
                    if (profile.stream_type() == RS2_STREAM_INFRARED)
                    {
                        sub->stream_enabled[profile.unique_id()] = false;
                        break;
                    }
                }
            }
        }

        // Setting the same Width,Height,FPS to every requested stereo module streams (depth,infrared) according to loaded JSON
        if (!requested_streams.empty())
        {
            try
            {
                std::string wstr = j["stream-width"];
                std::string hstr = j["stream-height"];
                std::string fstr = j["stream-fps"];
                int width = std::stoi(wstr);
                int height = std::stoi(hstr);
                int fps = std::stoi(fstr);
                for (auto& kvp : requested_streams)
                {
                    kvp.second.width = width;
                    kvp.second.height = height;
                    kvp.second.fps = fps;
                }
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error( rsutils::string::from() << "Error parsing streams from JSON: " << e.what() );
            }
        }

        // Enable requested stereo module streams (depth,infrared)
        for (auto&& kvp : requested_streams)
        {
            std::string stream_name = rsutils::string::from()
                                   << rs2_stream_to_string( kvp.first.first )
                                   << ( kvp.first.second > 0 ? ( " " + std::to_string( kvp.first.second ) ) : "" );
            for (auto&& sub : subdevices)
            {
                auto itr = std::find_if(sub->stream_display_names.begin(), sub->stream_display_names.end(), [stream_name](const std::pair<int, std::string>& p) { return p.second == stream_name; });
                if (itr != sub->stream_display_names.end())
                {
                    int uid = itr->first;
                    sub->stream_enabled[uid] = true;

                    //Find format
                    size_t format_id = 0;
                    rs2_format requested_format = kvp.second.format;
                    for (; format_id < sub->format_values[uid].size(); format_id++)
                    {
                        if (sub->format_values[uid][format_id] == requested_format)
                            break;
                    }
                    if (format_id == sub->format_values[uid].size())
                    {
                        throw std::runtime_error( rsutils::string::from()
                                                  << "No match found for requested format: " << requested_format );
                    }
                    sub->ui.selected_format_id[uid] = static_cast<int>(format_id);

                    //Find fps
                    int requested_fps = kvp.second.fps;
                    size_t fps_id = 0;
                    for (; fps_id < sub->shared_fps_values.size(); fps_id++)
                    {
                        if (sub->shared_fps_values[fps_id] == requested_fps)
                            break;
                    }
                    if (fps_id == sub->shared_fps_values.size())
                    {
                        throw std::runtime_error( rsutils::string::from()
                                                  << "No match found for requested fps: " << requested_fps );
                    }
                    sub->ui.selected_shared_fps_id = static_cast<int>(fps_id);

                    //Find Resolution
                    std::pair<int, int> requested_res{ kvp.second.width,kvp.second.height };
                    if (!sub->ui.is_multiple_resolutions)
                    {
                        size_t res_id = 0;
                        for (; res_id < sub->res_values.size(); res_id++)
                        {
                            if (sub->res_values[res_id] == requested_res)
                                break;
                        }
                        if (res_id == sub->res_values.size())
                        {
                            throw std::runtime_error(rsutils::string::from()
                                << "No match found for requested resolution: " << requested_res.first
                                << "x" << requested_res.second);
                        }
                        sub->ui.selected_res_id = static_cast<int>(res_id);
                    }
                    else
                        sub->ui.selected_stream_to_res[kvp.first.first] = requested_res;
                }
            }
        }
    }

    float device_model::draw_preset_panel(float panel_width,
        ux_window& window,
        std::string& error_message,
        viewer_model& viewer,
        bool update_read_only_options,
        bool load_json_if_streaming,
        json_loading_func json_loading)
    {
        const float panel_height = 40.f;

        auto panel_pos = ImGui::GetCursorPos();
        ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushFont(window.get_font());
        auto serializable = dev.as<serializable_device>();

        const auto load_json = [&, serializable](const std::string f) {
            std::ifstream file(f);
            if (!file.good())
            {
                //Failed to read file, removing it from the available ones
                advanced_mode_settings_file_names.erase(f);
                selected_file_preset.clear();
                throw std::runtime_error( rsutils::string::from() << "Failed to read configuration file:\n\"" << f
                                                                  << "\"\nRemoving it from presets." );
            }
            std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            if (serializable)
            {
                serializable.load_json(str);
                for (auto&& sub : subdevices)
                {
                    //If json was loaded correctly, we want the presets combo box to show the name of the configuration file
                    // And as a workaround, set the current preset to "custom", so that if the file is removed the preset will show "custom"
                    if (auto dpt = sub->s->as<depth_sensor>())
                    {
                        auto itr = sub->options_metadata.find(RS2_OPTION_VISUAL_PRESET);
                        if (itr != sub->options_metadata.end())
                        {
                            // Make sure we actually have a "Custom" preset
                            if( std::string( "Custom", 6 )
                                == itr->second.endpoint->get_option_value_description(
                                    RS2_OPTION_VISUAL_PRESET,
                                    RS2_RS400_VISUAL_PRESET_CUSTOM ) )
                                itr->second.endpoint->set_option(RS2_OPTION_VISUAL_PRESET, RS2_RS400_VISUAL_PRESET_CUSTOM);
                        }
                    }
                }
            }
            load_viewer_configurations(str);
            get_curr_advanced_controls = true;
            advanced_mode_settings_file_names.insert(f);
            selected_file_preset = f;
            viewer.not_model->add_log( rsutils::string::from() << "Loaded settings from \"" << f << "\"..." );
        };

        const auto save_to_json = [&, serializable](std::string full_filename)
        {
            if (!ends_with(rsutils::string::to_lower(full_filename), ".json")) full_filename += ".json";
            std::ofstream outfile(full_filename);

            json j;
            json *saved_configuraion = nullptr;
            
            // place all of the viewer parameters under a viewer section
            if (serializable)
            {
                j = json::parse(serializable.serialize_json());
                j["viewer"] = json::object();
                saved_configuraion = &j["viewer"]; // point to the viewer section inside the root
            }
            else
            {
                saved_configuraion = &j;
            }


            save_viewer_configurations(outfile, *saved_configuraion);
            outfile << j.dump(4); // output all the data to the file (starting at the root)
            outfile.close();
            advanced_mode_settings_file_names.insert(full_filename);
            selected_file_preset = full_filename;
            viewer.not_model->add_log( rsutils::string::from() << "Saved settings to \"" << full_filename << "\"...");

        };
        static const std::string popup_message = "\t\tTo use this feature, the device must be in Advanced Mode.\t\t\n\n\t\tWould you like to turn Advanced Mode?\t\t";
        ////////////////////////////////////////
        // Draw Combo Box
        ////////////////////////////////////////
        for (auto&& sub : subdevices)
        {
            if (auto dpt = sub->s->as<depth_sensor>())
            {
                if (dpt.supports(RS2_OPTION_VISUAL_PRESET))
                {
                    ImGui::SetCursorPos({ panel_pos.x + 8, ImGui::GetCursorPosY() + 10 });
                    //TODO: set this once!
                    const auto draw_preset_combo_box = [&](option_model& opt_model, std::string& error_message, notifications_model& model)
                    {
                        bool is_clicked = false;
                        assert(opt_model.opt == RS2_OPTION_VISUAL_PRESET);
                        ImGui::Text("Preset: ");
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("Select a preset configuration (or use the load button)");
                        }

                        ImGui::SameLine();
                        ImGui::PushItemWidth(185);

                        ///////////////////////////////////////////
                        //TODO: make this a member function
                        int selected;
                        std::vector< const char * > labels = opt_model.get_combo_labels( &selected );
                        std::vector< float > counters;
                        for (auto i = opt_model.range.min; i <= opt_model.range.max; i += opt_model.range.step)
                            counters.push_back(i);
                        ///////////////////////////////////////////

                        RsImGui_ScopePushStyleColor(ImGuiCol_TextSelectedBg, white);
                        RsImGui_ScopePushStyleColor(ImGuiCol_Button, button_color);
                        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonHovered, button_color + 0.1f);
                        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonActive, button_color + 0.1f);
                        ImVec2 padding{ 2,2 };
                        RsImGui_ScopePushStyleVar(ImGuiStyleVar_FramePadding, padding);
                        ImGui::PushStyleColor(ImGuiCol_PopupBg, black);
                        ///////////////////////////////////////////
                        // Go over the loaded files and add them to the combo box
                        std::vector<std::string> full_files_names(advanced_mode_settings_file_names.begin(), advanced_mode_settings_file_names.end());
                        std::vector<std::string> files_labels;
                        int i = static_cast<int>(labels.size());
                        for (auto&& file : full_files_names)
                        {
                            files_labels.push_back(get_file_name(file));
                            if (selected_file_preset == file)
                            {
                                selected = i;
                            }
                            i++;
                        }
                        std::transform(files_labels.begin(), files_labels.end(), std::back_inserter(labels), [](const std::string& s) { return s.c_str(); });

                        try
                        {
                            if (RsImGui::CustomComboBox(opt_model.id.c_str(), &selected, labels.data(),
                                static_cast<int>(labels.size())))
                            {
                                *opt_model.invalidate_flag = true;

                                auto advanced = dev.as<advanced_mode>();
                                if (advanced)
                                    if (!advanced.is_enabled())
                                        sub->draw_advanced_mode_prompt = false;


                                if (!sub->draw_advanced_mode_prompt)
                                {
                                    if (selected < static_cast<int>(labels.size() - files_labels.size()))
                                    {
                                        //Known preset was chosen
                                        auto new_val = counters[selected];
                                        model.add_log(rsutils::string::from()
                                            << "Setting " << opt_model.opt << " to " << new_val << " ("
                                            << labels[selected] << ")");

                                        opt_model.set_option(opt_model.opt, static_cast<float>(new_val), error_message);

                                        // Only apply preset to GUI if set_option was succesful
                                        selected_file_preset = "";
                                        is_clicked = true;
                                    }
                                    else
                                    {
                                        //File was chosen
                                        auto file = selected - static_cast<int>(labels.size() - files_labels.size());
                                        if (file < 0 || file >= full_files_names.size())
                                            throw std::runtime_error("not a valid format");
                                        auto f = full_files_names[file];
                                        error_message = safe_call([&]() { load_json(f); });
                                        selected_file_preset = f;
                                    }
                                }
                            }
                            if (sub->draw_advanced_mode_prompt)
                            {
                                sub->draw_advanced_mode_prompt = prompt_toggle_advanced_mode(true, popup_message, restarting_device_info, viewer, window, error_message);
                            }
                        }
                        catch (const error& e)
                        {
                            error_message = error_to_string(e);
                        }

                        ImGui::PopStyleColor(1);
                        ImGui::PopItemWidth();
                        return is_clicked;
                    };

                    auto & visual_preset_opt_model = sub->options_metadata.at(RS2_OPTION_VISUAL_PRESET);
                    visual_preset_opt_model.custom_draw_method = draw_preset_combo_box;
                    
                    if (sub->draw_option(RS2_OPTION_VISUAL_PRESET, dev.is<playback>() || update_read_only_options, error_message, *viewer.not_model))
                    {
                        get_curr_advanced_controls = true;
                        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) ); // Give FW time to update advanced mode controls before reading them
                        selected_file_preset.clear();
                    }
                }
            }
        }

        ImGui::SameLine();
        const ImVec2 icons_size{ 20, 20 };
        //TODO: Change this once we have support for loading jsons with more data than only advanced controls
        bool is_streaming = std::any_of(subdevices.begin(), subdevices.end(), [](const std::shared_ptr<subdevice_model>& sm) { return sm->streaming; });
        const bool buttons_disable = !serializable;
        static bool require_advanced_mode_enable_prompt = false;
        auto advanced_dev = dev.as<advanced_mode>();
        auto is_advanced_device = false;
        auto is_advanced_mode_enabled = false;
        if (advanced_dev)
        {
            is_advanced_device = true;
            try
            {
                // Prevent intermittent errors in polling mode to keep imgui in sync
                is_advanced_mode_enabled = advanced_dev.is_enabled();
            }
            catch (...){}
        }

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 3);

        ////////////////////////////////////////
        // Draw Load Icon
        ////////////////////////////////////////
        std::string upload_button_name = rsutils::string::from() << textual_icons::upload << "##" << id;
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        
        bool load_button_disabled = (is_streaming && !load_json_if_streaming) || buttons_disable;
        RsImGui::RsImButton([&]() {
        if (ImGui::ButtonEx(upload_button_name.c_str(), icons_size))
        {
            if (serializable && (!is_advanced_device || is_advanced_mode_enabled))
            {
                json_loading([&]()
                {
                    for( auto && sub : subdevices )
                    {
                        if( auto dpt = sub->s->as< depth_sensor >() )
                        {
                            sub->_options_invalidated = true;
                        }
                    }
                    auto ret = file_dialog_open(open_file, "JavaScript Object Notation (JSON | PRESET)\0*.json\0*.preset\0", NULL, NULL);
                    if (ret)
                    {
                        error_message = safe_call([&]() { load_json(ret); });
                    }
                });
            }
            else
            {
                if( ! _is_d500_device )
                    require_advanced_mode_enable_prompt = true;
            }
        }}, load_button_disabled);
        if (ImGui::IsItemHovered())
        {
            std::string tooltip = rsutils::string::from()
                               << "Load pre-configured device settings"
                               << ( is_streaming && ! load_json_if_streaming ? " (Disabled while streaming)" : "" );
            RsImGui::CustomTooltip("%s", tooltip.c_str());
        }

        ImGui::SameLine();

        ////////////////////////////////////////
        // Draw Save Icon
        ////////////////////////////////////////
        std::string save_button_name = rsutils::string::from() << textual_icons::download << "##" << id;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1); //Align the two icons to buttom
        RsImGui::RsImButton([&]() {
        if (ImGui::ButtonEx(save_button_name.c_str(), icons_size))
        {
            if (serializable && (!is_advanced_device || is_advanced_mode_enabled))
            {
                auto ret = file_dialog_open(save_file, "JavaScript Object Notation (JSON)\0*.json\0", NULL, NULL);
                if (ret)
                {
                    error_message = safe_call([&]() { save_to_json(ret); });
                }
            }
            else
            {
                require_advanced_mode_enable_prompt = true;
            }

        }}, buttons_disable);
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("Save current device settings to file");
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        if (require_advanced_mode_enable_prompt)
        {
            require_advanced_mode_enable_prompt = prompt_toggle_advanced_mode(true, popup_message, restarting_device_info, viewer, window, error_message);
        }

        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(7);

        return panel_height;
    }


    bool rs2::device_model::is_streaming() const
    {
        return std::any_of(subdevices.begin(), subdevices.end(), [](const std::shared_ptr<subdevice_model>& sm)
        {
            return sm->streaming;
        });
    }

    bool rs2::device_model::is_color_streaming() const
    {
        for( const auto & sub : subdevices )
        {
            if (sub->s->is<color_sensor>() && sub->streaming)
                return true;
        }
        return false;
    }

    void device_model::draw_controls(float panel_width, float panel_height,
        ux_window& window,
        std::string& error_message,
        device_model*& device_to_remove,
        viewer_model& viewer, float windows_width,
        std::vector<std::function<void()>>& draw_later,
        bool load_json_if_streaming,
        json_loading_func json_loading,
        bool draw_device_outline)
    {
        ////////////////////////////////////////
        // draw device header
        ////////////////////////////////////////
        const bool is_playback_device = dev.is<playback>();
        bool is_ip_device = dev.supports(RS2_CAMERA_INFO_IP_ADDRESS);
        auto header_h = panel_height;
        if (is_playback_device || is_ip_device) header_h += 15;

        ImColor device_header_background_color = title_color;
        const float left_space = 3.f;
        const float upper_space = 3.f;

        bool update_read_only_options = false; // _update_readonly_options_timer;

        const ImVec2 initial_screen_pos = ImGui::GetCursorScreenPos();
        //Upper Space
        ImGui::GetWindowDrawList()->AddRectFilled({ initial_screen_pos.x,initial_screen_pos.y }, { initial_screen_pos.x + panel_width,initial_screen_pos.y + upper_space }, ImColor(black));
        if (draw_device_outline)
        {
            //Upper Line
            ImGui::GetWindowDrawList()->AddLine({ initial_screen_pos.x,initial_screen_pos.y + upper_space }, { initial_screen_pos.x + panel_width,initial_screen_pos.y + upper_space }, ImColor(header_color));
        }
        //Device Header area
        ImGui::GetWindowDrawList()->AddRectFilled({ initial_screen_pos.x + 1,initial_screen_pos.y + upper_space + 1 }, { initial_screen_pos.x + panel_width, initial_screen_pos.y + header_h + upper_space }, device_header_background_color);

        auto pos = ImGui::GetCursorPos();
        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4&)device_header_background_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4&)device_header_background_color);

        ////////////////////////////////////////
        // Draw device name
        ////////////////////////////////////////
        const ImVec2 name_pos = { pos.x + 9, pos.y + 17 };
        ImGui::SetCursorPos(name_pos);
        std::stringstream ss;
        if (dev.supports(RS2_CAMERA_INFO_NAME))
            ss << dev.get_info(RS2_CAMERA_INFO_NAME);
        if (is_ip_device)
        {
            ImGui::Text(" %s", ss.str().substr(0, ss.str().find("\n IP Device")).c_str());

            ImGui::PushFont(window.get_font());
            ImGui::Text("\tNetwork Device at %s", dev.get_info(RS2_CAMERA_INFO_IP_ADDRESS));
            ImGui::PopFont();
        }
        else
        {
            ImGui::Text(" %s", ss.str().c_str());
            if (dev.supports(RS2_CAMERA_INFO_CONNECTION_TYPE))
            {
                std::string connection_type = dev.get_info(RS2_CAMERA_INFO_CONNECTION_TYPE);
                if (connection_type == "USB" && dev.supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
                {
                    std::string desc = dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
                    ss.str("");
                    ss << "   " << textual_icons::usb << " " << desc;
                    ImGui::SameLine();
                    if (!starts_with(desc, "3.")) ImGui::PushStyleColor(ImGuiCol_Text, yellow);
                    else ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    ImGui::Text(" %s", ss.str().c_str());
                    ImGui::PopStyleColor();
                    ss.str("");
                    ss << "The camera was detected by the OS as connected to a USB " << desc << " port";
                    ImGui::PushFont(window.get_font());
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip(" %s", ss.str().c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                }
                else
                {
                    ss.str("");
                    ss << "   " << connection_type;
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, white);
                    ImGui::Text(" %s", ss.str().c_str());
                    ImGui::PopStyleColor();
                }
            }
        }

        //ImGui::Text(" %s", dev.get_info(RS2_CAMERA_INFO_NAME));
        ImGui::PopFont();

        ////////////////////////////////////////
        // Draw X Button
        ////////////////////////////////////////
        ImGui::PushFont(window.get_font());
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        if (_allow_remove)
        {
            ImGui::Columns(1);
            float horizontal_distance_from_right_side_of_panel = 47;
            ImGui::SetCursorPos({ panel_width - horizontal_distance_from_right_side_of_panel, pos.y + 9 + (header_h - panel_height) / 2 });
            std::string remove_source_button_label = rsutils::string::from() << textual_icons::times << "##" << id;
            if (ImGui::Button(remove_source_button_label.c_str(), { 33,35 }))
            {
                for (auto&& sub : subdevices)
                {
                    if (sub->streaming)
                        sub->stop(viewer.not_model);
                }
                device_to_remove = this;
            }

            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("Remove selected device from current view\n(can be restored by clicking Add Source)");
                window.link_hovered();
            }
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::PopFont();

        ////////////////////////////////////////
        // Draw playback file name
        ////////////////////////////////////////
        ImGui::SetCursorPos({ pos.x + 10, pos.y + panel_height - 9 });
        if (auto p = dev.as<playback>())
        {
            ImGui::PushFont(window.get_font());
            auto full_path = p.file_name();
            auto filename = get_file_name(full_path);
            std::string file_name_and_icon = rsutils::string::from() << " " << textual_icons::file_movie << " File: \"" << filename << "\"";
            ImGui::Text("%s", file_name_and_icon.c_str());
            if (ImGui::IsItemHovered())
                RsImGui::CustomTooltip("%s", full_path.c_str());
            ImGui::PopFont();
        }
        ImGui::SetCursorPos({ 0, pos.y + header_h });

        ////////////////////////////////////////
        // draw device control panel
        ////////////////////////////////////////
        if (!is_playback_device) //Not displaying these controls for playback devices (since only info is supported)
        {
            pos = ImGui::GetCursorPos();
            const float vertical_space_before_device_control = 10.0f;
            const float horizontal_space_before_device_control = 3.0f;
            auto device_panel_pos = ImVec2{ pos.x + horizontal_space_before_device_control, pos.y + vertical_space_before_device_control };
            ImGui::SetCursorPos(device_panel_pos);
            const float device_panel_height = draw_device_panel(panel_width, window, error_message, viewer);
            ImGui::SetCursorPos({ device_panel_pos.x, device_panel_pos.y + device_panel_height });
        }

        ////////////////////////////////////////
        // draw advanced mode panel
        ////////////////////////////////////////
        auto serialize = dev.is<serializable_device>();
        if (serialize)
        {
            pos = ImGui::GetCursorPos();
            const float vertical_space_before_advanced_mode_control = 10.0f;
            const float horizontal_space_before_device_control = 3.0f;
            auto advanced_mode_pos = ImVec2{ pos.x + horizontal_space_before_device_control, pos.y + vertical_space_before_advanced_mode_control };
            ImGui::SetCursorPos(advanced_mode_pos);
            const float advanced_mode_panel_height = draw_preset_panel(panel_width, window, error_message, viewer, update_read_only_options, load_json_if_streaming, json_loading);
            ImGui::SetCursorPos({ advanced_mode_pos.x, advanced_mode_pos.y + advanced_mode_panel_height });
        }

        ////////////////////////////////////////
        // draw playback control panel
        ////////////////////////////////////////
        if (auto p = dev.as<playback>())
        {
            pos = ImGui::GetCursorPos();
            float space_before_playback_control = 18.0f;
            auto playback_panel_pos = ImVec2{ pos.x + 10, pos.y + space_before_playback_control };
            ImGui::SetCursorPos(playback_panel_pos);
            auto playback_panel_height = draw_playback_panel(window, window.get_font(), viewer);
            ImGui::SetCursorPos({ playback_panel_pos.x, playback_panel_pos.y + playback_panel_height });
        }

        bool is_streaming = std::any_of(subdevices.begin(), subdevices.end(), [](const std::shared_ptr<subdevice_model>& sm)
        {
            return sm->streaming;
        });

        pos = ImGui::GetCursorPos();

        ImVec2 rc;

        int info_control_panel_height = 0;
        if (show_device_info)
        {
            ImGui::PushFont(window.get_font());
            int line_h = 22;
            info_control_panel_height = (int)infos.size() * line_h + 5;
            for (auto&& pair : infos)
            {
                rc = ImGui::GetCursorPos();
                ImGui::SetCursorPos({ rc.x + 12, rc.y + 4 });
                std::string info_category;
                if (pair.first == "Recommended Firmware Version")
                {
                    info_category = "Min FW Version";
                }
                else
                {
                    info_category = pair.first.c_str();
                }
                ImGui::Text("%s:", info_category.c_str());
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
                ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                ImGui::SetCursorPos({ rc.x + 9.f * window.get_font_size(), rc.y + 1 });
                std::string label = rsutils::string::from() << "##" << id << " " << pair.first;
                ImGui::InputText(label.c_str(),
                    (char*)pair.second.data(),
                    pair.second.size() + 1,
                    ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(3);
                ImGui::SetCursorPos({ rc.x, rc.y + line_h });
            }

            ImGui::SetCursorPos({ rc.x + 225, rc.y - 107 });
            ImGui::PopFont();
        }

        ImGui::SetCursorPos({ 0, pos.y + info_control_panel_height });
        ImGui::PopStyleColor(2);

        auto sensor_top_y = ImGui::GetCursorPosY();
        ImGui::SetWindowSize(ImVec2(windows_width -36, 0.0f));

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushFont(window.get_font());

        // Draw menu foreach subdevice with its properties
        for (auto&& sub : subdevices)
        {
            if (show_depth_only && !sub->s->is<depth_sensor>()) continue;

            const ImVec2 pos = ImGui::GetCursorPos();
            //const ImVec2 abs_pos = ImGui::GetCursorScreenPos();
            //model_to_y[sub.get()] = pos.y;
            //model_to_abs_y[sub.get()] = abs_pos.y;

            if (!show_depth_only)
            {
                draw_later.push_back([&error_message, windows_width, &window, sub, pos, &viewer, this]()
                {
                    bool stop_recording = false;

                    ImGui::SetCursorPos({ windows_width - 60, pos.y + 7 });
                    RsImGui_ScopePushFont(window.get_font());

                    RsImGui_ScopePushStyleColor(ImGuiCol_Button, sensor_bg);
                    RsImGui_ScopePushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                    RsImGui_ScopePushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
                    int font_size = window.get_font_size();
                    ImVec2 button_size = { font_size * 3.0f, font_size * 1.0f };
                    
                    if (!sub->streaming)
                    {
                        std::string label = rsutils::string::from()
                                         <<textual_icons::toggle_off<<"   off "<< id << ", "
                                         << sub->s->get_info( RS2_CAMERA_INFO_NAME );

                        RsImGui_ScopePushStyleColor(ImGuiCol_Text, redish);
                        RsImGui_ScopePushStyleColor(ImGuiCol_TextSelectedBg, redish + 0.1f);

                        std::vector<stream_profile> profiles;
                        auto is_comb_supported = sub->is_selected_combination_supported();
                        bool can_stream = false;
                        if (is_comb_supported)
                            can_stream = true;
                        else
                        {
                            profiles = sub->get_supported_profiles();
                            if (!profiles.empty())
                                can_stream = true;
                            else
                            {
                                std::string text = rsutils::string::from() << "  " << textual_icons::toggle_off << "\noff   ";
                                ImGui::TextDisabled("%s", text.c_str());
                                if (std::any_of(sub->stream_enabled.begin(), sub->stream_enabled.end(), [](std::pair<int, bool> const& s) { return s.second; }))
                                {
                                    if (ImGui::IsItemHovered())
                                    {
                                        RsImGui::CustomTooltip("Selected value is not supported");
                                    }
                                }
                                else
                                {
                                    if (ImGui::IsItemHovered())
                                    {
                                        RsImGui::CustomTooltip("No stream selected");
                                    }
                                }
                            }
                        }
                        if (can_stream)
                        {
                            if( ImGui::Button( label.c_str(), button_size ) )
                            {
                                if (profiles.empty()) // profiles might be already filled
                                    profiles = sub->get_selected_profiles();
                                try
                                {
                                    if (!dev_syncer)
                                        dev_syncer = viewer.syncer->create_syncer();

                                    std::string friendly_name = sub->s->get_info(RS2_CAMERA_INFO_NAME);
                                    if (((friendly_name.find("Tracking") != std::string::npos) ||
                                        (friendly_name.find("Motion") != std::string::npos)))
                                    {
                                        viewer.synchronization_enable_prev_state = viewer.synchronization_enable.load();
                                        viewer.synchronization_enable = false;
                                    }
                                    _update_readonly_options_timer.set_expired();
                                    sub->play(profiles, viewer, dev_syncer);
                                }
                                catch (const error& e)
                                {
                                    error_message = error_to_string(e);
                                }
                                catch (const std::exception& e)
                                {
                                    error_message = e.what();
                                }

                                for (auto&& profile : profiles)
                                {
                                    viewer.begin_stream(sub, profile);
                                }
                            }
                            if (ImGui::IsItemHovered())
                            {
                                window.link_hovered();
                                RsImGui::CustomTooltip("Start streaming data from this sensor");
                            }
                        }
                    }
                    else
                    {
                        std::string label = rsutils::string::from()
                                         << textual_icons::toggle_on << "   on  " << id << ","
                                         << sub->s->get_info( RS2_CAMERA_INFO_NAME );
                        RsImGui_ScopePushStyleColor(ImGuiCol_Text, light_blue);
                        RsImGui_ScopePushStyleColor(ImGuiCol_TextSelectedBg, light_blue + 0.1f);

                        if( ImGui::Button( label.c_str(), button_size ) )
                        {
                            sub->stop(viewer.not_model);
                            std::string friendly_name = sub->s->get_info(RS2_CAMERA_INFO_NAME);
                            if ((friendly_name.find("Tracking") != std::string::npos) ||
                                (friendly_name.find("Motion") != std::string::npos))
                            {
                                viewer.synchronization_enable = viewer.synchronization_enable_prev_state.load();
                            }

                            if (!std::any_of(subdevices.begin(), subdevices.end(),
                                [](const std::shared_ptr<subdevice_model>& sm)
                            {
                                return sm->streaming;
                            }))
                            {
                                stop_recording = true;
                                _update_readonly_options_timer.set_expired();
                            }
                        }
                        if (ImGui::IsItemHovered())
                        {
                            window.link_hovered();
                            RsImGui::CustomTooltip("Stop streaming data from selected sub-device");
                        }
                    }

                    if (is_recording && stop_recording)
                    {
                        this->stop_recording(viewer);
                        for (auto&& sub : subdevices)
                        {
                            //TODO: Fix case where sensor X recorded stream 0, then stopped, and then started recording stream 1 (need 2 sensors for this to happen)
                            if (sub->is_selected_combination_supported())
                            {
                                auto profiles = sub->get_selected_profiles();
                                for (auto&& profile : profiles)
                                {
                                    viewer.streams[profile.unique_id()].dev = sub;
                                }
                            }
                        }
                    }
                });
            }

            //ImGui::GetWindowDrawList()->AddLine({ abs_pos.x, abs_pos.y - 1 },
            //{ abs_pos.x + panel_width, abs_pos.y - 1 },
            //    ImColor(black), 1.f);

            std::string label = rsutils::string::from() << sub->s->get_info(RS2_CAMERA_INFO_NAME) << "##" << id;
            ImGui::PushStyleColor(ImGuiCol_Header, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, sensor_bg);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10, 10 });
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, { 0, 0 });
            ImGuiTreeNodeFlags flags{};
            ImGui::AlignTextToFramePadding();//Ensures that text aligns visually with UI elements that have padding (for the TreeNode visual alignment)
            if (show_depth_only) flags = ImGuiTreeNodeFlags_DefaultOpen;
            if (ImGui::TreeNodeEx(label.c_str(), flags))
            {
                ImGui::PopStyleVar();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 2, 2 });

                if (show_stream_selection)
                    sub->draw_stream_selection(error_message);

                static const std::vector<rs2_option> drawing_order = serialize ?
                    std::vector<rs2_option>{                           RS2_OPTION_EMITTER_ENABLED, RS2_OPTION_ENABLE_AUTO_EXPOSURE, RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE }
                : std::vector<rs2_option>{ RS2_OPTION_VISUAL_PRESET, RS2_OPTION_EMITTER_ENABLED, RS2_OPTION_ENABLE_AUTO_EXPOSURE, RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE };

                for (auto& opt : drawing_order)
                {
                    if (sub->draw_option(opt, dev.is<playback>() || update_read_only_options, error_message, *viewer.not_model))
                    {
                        get_curr_advanced_controls = true;
                        selected_file_preset.clear();
                    }
                }

                if (sub->num_supported_non_default_options())
                {
                    label = rsutils::string::from() << "Controls ##" << sub->s->get_info(RS2_CAMERA_INFO_NAME) << "," << id;
                    if (ImGui::TreeNode(label.c_str()))
                    {
                        auto const & supported_options = sub->options_metadata;

                        // moving the color dedicated options to the end of the vector
                        std::vector<rs2_option> color_options = {
                            RS2_OPTION_BACKLIGHT_COMPENSATION,
                            RS2_OPTION_BRIGHTNESS,
                            RS2_OPTION_CONTRAST,
                            RS2_OPTION_GAMMA,
                            RS2_OPTION_HUE,
                            RS2_OPTION_SATURATION,
                            RS2_OPTION_SHARPNESS,
                            RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE,
                            RS2_OPTION_WHITE_BALANCE
                        };

                        std::vector<rs2_option> so_ordered;

                        for( auto const id_model : sub->supported_options )
                        {
                            auto it = find( color_options.begin(), color_options.end(), id_model );
                            if (it == color_options.end())
                                so_ordered.push_back( id_model );
                        }

                        std::for_each( color_options.begin(),
                                       color_options.end(),
                                       [&]( rs2_option opt )
                                       {
                                           auto it = supported_options.find( opt );
                                           if( it != supported_options.end() )
                                               so_ordered.push_back( opt );
                                       } );

                        for (auto opt : so_ordered)
                        {
                            if( viewer.is_option_skipped( opt ) )
                                continue;
                            if (std::find(drawing_order.begin(), drawing_order.end(), opt) == drawing_order.end())
                            {
                                if (serialize && opt == RS2_OPTION_VISUAL_PRESET)
                                    continue;

                                if (sub->draw_option(opt, dev.is<playback>() || update_read_only_options, error_message, *viewer.not_model))
                                {
                                    get_curr_advanced_controls = true;
                                    selected_file_preset.clear();
                                }
                            }
                        }

                        ImGui::TreePop();
                    }
                }
                if (dev.is<advanced_mode>() && sub->s->is<depth_sensor>())
                {
                    if (draw_advanced_controls(viewer, window, error_message, is_streaming))
                    {
                        sub->_options_invalidated = true;
                        selected_file_preset.clear();
                    }
                }

                if (sub->s->is<depth_sensor>()) {
                    for (auto&& pb : sub->const_effects)
                    {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

                        label = rsutils::string::from() << pb->get_name() << "##" << id;
                        if (ImGui::TreeNode(label.c_str()))
                        {
                            pb->draw_options( viewer,
                                              dev.is< playback >() || update_read_only_options,
                                              false,
                                              error_message );

                            ImGui::TreePop();
                        }
                    }
                }

                draw_embedded_filters(sub, windows_width, window, viewer,
                    error_message, label, draw_later, update_read_only_options);

                draw_processing_blocks(sub, windows_width, window, viewer, 
                    error_message, label, draw_later, update_read_only_options);

                ImGui::TreePop();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        }

        for (auto&& sub : subdevices)
        {
            sub->update(error_message, *viewer.not_model);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopFont();

        auto end_screen_pos = ImGui::GetCursorScreenPos();

        if (draw_device_outline)
        {
            //Left space
            ImGui::GetWindowDrawList()->AddRectFilled({ initial_screen_pos.x,initial_screen_pos.y }, { end_screen_pos.x + left_space, end_screen_pos.y }, ImColor(black));
            //Left line
            ImGui::GetWindowDrawList()->AddLine({ initial_screen_pos.x + left_space,initial_screen_pos.y + upper_space }, { end_screen_pos.x + left_space, end_screen_pos.y }, ImColor(header_color));
            //Right line
            const float compenstaion_right = 17.f;;
            ImGui::GetWindowDrawList()->AddLine({ initial_screen_pos.x + panel_width - compenstaion_right, initial_screen_pos.y + upper_space }, { end_screen_pos.x + panel_width - compenstaion_right, end_screen_pos.y }, ImColor(header_color));
            //Button line
            const float compenstaion_button = 1.0f;
            ImGui::GetWindowDrawList()->AddLine({ end_screen_pos.x + left_space, end_screen_pos.y - compenstaion_button }, { end_screen_pos.x + left_space + panel_width, end_screen_pos.y - compenstaion_button }, ImColor(header_color));
        }
    }

    void device_model::draw_processing_blocks(std::shared_ptr<subdevice_model> sub, float windows_width,
        ux_window& window, viewer_model& viewer, std::string& error_message, std::string& label,
        std::vector<std::function<void()>>& draw_later, const bool& update_read_only_options)
    {
        if (sub->post_processing.size() > 0)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            const ImVec2 pos = ImGui::GetCursorPos();

            draw_later.push_back([windows_width, &window, sub, pos, &viewer, this]() {
                ImGui::SetCursorPos({ windows_width - 41, pos.y - 3 });

                try
                {

                    ImGui::PushFont(window.get_font());

                    ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);

                    if (!sub->post_processing_enabled)
                    {
                        std::string label = rsutils::string::from()
                            << " " << textual_icons::toggle_off << "##" << id << ","
                            << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ",post";

                        ImGui::PushStyleColor(ImGuiCol_Text, redish);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, redish + 0.1f);

                        if (ImGui::Button(label.c_str(), { 30,24 }))
                        {
                            sub->post_processing_enabled = true;
                            config_file::instance().set(get_post_processing_device_sensor_name(sub.get()).c_str(),
                                sub->post_processing_enabled);
                            for (auto&& pb : sub->post_processing)
                            {
                                if (!pb->visible)
                                    continue;
                                if (pb->is_enabled())
                                    pb->processing_block_enable_disable(true);
                            }
                        }
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("Enable post-processing filters");
                            window.link_hovered();
                        }
                    }
                    else
                    {
                        std::string label = rsutils::string::from()
                            << " " << textual_icons::toggle_on << "##" << id << ","
                            << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ",post";
                        ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue + 0.1f);

                        if (ImGui::Button(label.c_str(), { 30,24 }))
                        {
                            sub->post_processing_enabled = false;
                            config_file::instance().set(get_post_processing_device_sensor_name(sub.get()).c_str(),
                                sub->post_processing_enabled);
                            for (auto&& pb : sub->post_processing)
                            {
                                if (!pb->visible)
                                    continue;
                                if (pb->is_enabled())
                                    pb->processing_block_enable_disable(false);
                            }
                        }
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("Disable post-processing filters");
                            window.link_hovered();
                        }
                    }
                    ImGui::PopStyleColor(5);
                    ImGui::PopFont();
                }
                catch (...)
                {
                    ImGui::PopStyleColor(5);
                    ImGui::PopFont();
                    throw;
                }
                });

            label = rsutils::string::from() << "Post-Processing##" << id;
            if (ImGui::TreeNode(label.c_str()))
            {
                for (auto&& pb : sub->post_processing)
                {
                    if (!pb->visible) continue;

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

                    const ImVec2 pos = ImGui::GetCursorPos();

                    draw_later.push_back([windows_width, &window, sub, pos, &viewer, this, pb]() {
                        ImGui::SetCursorPos({ windows_width - 42, pos.y - 3 });

                        try
                        {
                            ImGui::PushFont(window.get_font());

                            ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
                            int font_size = window.get_font_size();
                            const ImVec2 button_size = { font_size * 2.f, font_size * 1.5f };

                            if (!sub->post_processing_enabled)
                            {
                                if (!pb->is_enabled())
                                {
                                    std::string label = rsutils::string::from()
                                        << " " << textual_icons::toggle_off << "##" << id << ","
                                        << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ","
                                        << pb->get_name();

                                    ImGui::PushStyleColor(ImGuiCol_Text, redish);
                                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, redish + 0.1f);
                                    RsImGui::RsImButton([&]() {ImGui::ButtonEx(label.c_str(), button_size); }, true);
                                }
                                else
                                {
                                    std::string label = rsutils::string::from()
                                        << " " << textual_icons::toggle_on << "##" << id << ","
                                        << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ","
                                        << pb->get_name();
                                    ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue + 0.1f);
                                    RsImGui::RsImButton([&]() {ImGui::ButtonEx(label.c_str(), button_size); }, true);
                                }
                            }
                            else
                            {
                                if (!pb->is_enabled())
                                {
                                    std::string label = rsutils::string::from()
                                        << " " << textual_icons::toggle_off << "##" << id << ","
                                        << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ","
                                        << pb->get_name();

                                    ImGui::PushStyleColor(ImGuiCol_Text, redish);
                                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, redish + 0.1f);

                                    if (ImGui::Button(label.c_str(), button_size))
                                    {
                                        pb->enable(true);
                                        pb->save_to_config_file();
                                    }
                                    if (ImGui::IsItemHovered())
                                    {
                                        label = rsutils::string::from() << "Enable " << pb->get_name() << " post-processing filter";
                                        RsImGui::CustomTooltip("%s", label.c_str());
                                        window.link_hovered();
                                    }
                                }
                                else
                                {
                                    std::string label = rsutils::string::from()
                                        << " " << textual_icons::toggle_on << "##" << id << ","
                                        << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ","
                                        << pb->get_name();
                                    ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue + 0.1f);

                                    if (ImGui::Button(label.c_str(), button_size))
                                    {
                                        pb->enable(false);
                                        pb->save_to_config_file();
                                    }
                                    if (ImGui::IsItemHovered())
                                    {
                                        label = rsutils::string::from()
                                            << "Disable " << pb->get_name() << " post-processing filter";
                                        RsImGui::CustomTooltip("%s", label.c_str());
                                        window.link_hovered();
                                    }
                                }
                            }

                            ImGui::PopStyleColor(5);
                            ImGui::PopFont();
                        }
                        catch (...)
                        {
                            ImGui::PopStyleColor(5);
                            ImGui::PopFont();
                            throw;
                        }
                        });

                    label = rsutils::string::from() << pb->get_name() << "##" << id;
                    if (ImGui::TreeNode(label.c_str()))
                    {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                        pb->draw_options(viewer,
                            dev.is< playback >() || update_read_only_options,
                            false,
                            error_message);

                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    void device_model::draw_embedded_filters(std::shared_ptr<subdevice_model> sub, float windows_width,
        ux_window& window, viewer_model& viewer, std::string& error_message, std::string& label,
        std::vector<std::function<void()>>& draw_later, const bool& update_read_only_options)
    {
        if (sub->embedded_filters.size() > 0)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            const ImVec2 pos = ImGui::GetCursorPos();

            label = rsutils::string::from() << "Embedded-Filters##" << id;
            if (ImGui::TreeNode(label.c_str()))
            {
                for (auto&& pb : sub->embedded_filters)
                {
                    if (!pb->_is_visible) continue;

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

                    const ImVec2 pos = ImGui::GetCursorPos();

                    draw_later.push_back([windows_width, &window, sub, pos, &viewer, this, pb]() {
                        ImGui::SetCursorPos({ windows_width - 42, pos.y - 3 });

                        try
                        {
                            ImGui::PushFont(window.get_font());

                            ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
                            int font_size = window.get_font_size();
                            const ImVec2 button_size = { font_size * 2.f, font_size * 1.5f };

                            if (!pb->is_enabled())
                            {
                                std::string label = rsutils::string::from()
                                    << " " << textual_icons::toggle_off << "##" << id << ","
                                    << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ","
                                    << pb->get_name();

                                ImGui::PushStyleColor(ImGuiCol_Text, redish);
                                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, redish + 0.1f);

                                if (ImGui::Button(label.c_str(), button_size))
                                {
                                    pb->enable(true);
                                }
                                if (ImGui::IsItemHovered())
                                {
                                    label = rsutils::string::from() << "Enable " << pb->get_name() << " embedded filter";
                                    RsImGui::CustomTooltip("%s", label.c_str());
                                    window.link_hovered();
                                }
                            }
                            else
                            {
                                std::string label = rsutils::string::from()
                                    << " " << textual_icons::toggle_on << "##" << id << ","
                                    << sub->s->get_info(RS2_CAMERA_INFO_NAME) << ","
                                    << pb->get_name();
                                ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue + 0.1f);

                                if (ImGui::Button(label.c_str(), button_size))
                                {
                                    pb->enable(false);
                                }
                                if (ImGui::IsItemHovered())
                                {
                                    label = rsutils::string::from()
                                        << "Disable " << pb->get_name() << " embedded filter";
                                    RsImGui::CustomTooltip("%s", label.c_str());
                                    window.link_hovered();
                                }
                            }

                            ImGui::PopStyleColor(5);
                            ImGui::PopFont();
                        }
                        catch (...)
                        {
                            ImGui::PopStyleColor(5);
                            ImGui::PopFont();
                            throw;
                        }
                        });

                    label = rsutils::string::from() << pb->get_name() << "##" << id;
                    if (ImGui::TreeNode(label.c_str()))
                    {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                        pb->draw_options(viewer,
                            dev.is< playback >() || update_read_only_options,
                            false,
                            error_message);

                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    void device_model::handle_hardware_events(const std::string& serialized_data)
    {
        //TODO: Move under hour glass
    }

    bool device_model::disable_record_button_logic(bool is_streaming, bool is_playback_device)
    {
        bool depth_mapping_camera_streaming_alone = is_depth_mapping_camera_streaming_alone();
        return (!is_streaming || is_playback_device || depth_mapping_camera_streaming_alone);
    }

    std::string device_model::get_record_button_hover_text(bool is_streaming)
    {
        bool depth_mapping_camera_streaming_alone = is_depth_mapping_camera_streaming_alone();
        std::string record_button_hover_text;
        if (!is_streaming)
        {
            record_button_hover_text = "Start streaming to enable recording";
        }
        else
        {
            if (depth_mapping_camera_streaming_alone)
            {
                record_button_hover_text = "To record Depth Mapping Camera also stream Stereo Module";
            }
            else
            { 
                record_button_hover_text = is_recording ? "Stop Recording" : "Start Recording";
            }
        }
        return record_button_hover_text;
    }

    //In order to record LPC and enable 3D we need to also record depth stereo sensor
    bool device_model::is_depth_mapping_camera_streaming_alone()
    {
        std::string pid = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
        if (pid == "0B6B")
        {
            bool depth_mapping_sensor_streaming = false;
            bool depth_stereo_sensor_streaming = false;
            for (auto&& sub : subdevices)
            {
                if (sub->s->is<rs2::depth_mapping_sensor>() && sub->streaming)
                {
                    depth_mapping_sensor_streaming = true;
                }
                if (sub->s->is<rs2::depth_stereo_sensor>() && sub->streaming)
                {
                    depth_stereo_sensor_streaming = true;
                }
            }
            if (depth_mapping_sensor_streaming && !depth_stereo_sensor_streaming)
            {
                return true;
            }
        }
        return false;
    }
    

    std::vector<std::pair<std::string, std::string>> get_devices_names(const device_list& list)
    {
        std::vector<std::pair<std::string, std::string>> device_names;

        for (uint32_t i = 0; i < list.size(); i++)
        {
            try
            {
                auto dev = list[i];
                device_names.push_back(get_device_name(dev));        // push name and sn to list
            }
            catch (...)
            {
                device_names.push_back(std::pair<std::string, std::string>(rsutils::string::from() << "Unknown Device #" << i, ""));
            }
        }
        return device_names;
    }

    device_changes::device_changes(rs2::context& ctx)
    {
        _changes.emplace(rs2::device_list{}, ctx.query_devices(RS2_PRODUCT_LINE_ANY));
        ctx.set_devices_changed_callback([&](event_information& info)
            {
                add_changes(info);
            });
    }

    void device_changes::add_changes(const event_information& c)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _changes.push(c);
    }

    bool device_changes::try_get_next_changes(event_information& removed_and_connected)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_changes.empty())
            return false;

        removed_and_connected = std::move(_changes.front());
        _changes.pop();
        return true;
    }

    void device_model::draw_device_panel_auto_calib(viewer_model& viewer, bool& something_to_show,
        std::string& error_message)
    {
        bool has_autocalib = false;
        bool is_d500 = dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE) && (std::string(dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE)) == "D500");
        if (is_d500)
        {
            has_autocalib = draw_device_panel_auto_calib_d500(viewer, something_to_show, error_message);
        }
        else
        {
            has_autocalib = draw_device_panel_auto_calib_d400(viewer, something_to_show, error_message);
        }

        if (!has_autocalib)
        {
            bool selected = false;
            something_to_show = true;
            ImGui::Selectable("On-Chip Calibration", &selected, ImGuiSelectableFlags_Disabled);
            ImGui::Selectable("Tare Calibration", &selected, ImGuiSelectableFlags_Disabled);
        }

        if (!something_to_show)
        {
            ImGui::Text("This device has no additional options");
        }
    }

    bool device_model::draw_device_panel_auto_calib_d400(viewer_model& viewer, bool& something_to_show,
        std::string& error_message)
    {
        bool has_autocalib = false;

        bool color_streaming = is_color_streaming();
        ImGuiSelectableFlags avoid_selection_flag = (color_streaming) ? ImGuiSelectableFlags_Disabled : 0;

        for (auto&& sub : subdevices)
        {
            if (sub->supports_on_chip_calib() && !has_autocalib)
            {
                something_to_show = true;

                std::string device_pid = sub->s->supports(RS2_CAMERA_INFO_PRODUCT_ID) ? sub->s->get_info(RS2_CAMERA_INFO_PRODUCT_ID) : "unknown";
                std::string device_usb_type = sub->s->supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR) ? sub->s->get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR) : "unknown";

                bool show_disclaimer = val_in_range(device_pid, { std::string("0AD2"), std::string("0AD3") }); // Specific for D410/5
                bool disable_fl_cal = (((device_pid == "0B5C") || show_disclaimer) &&
                    (!starts_with(device_usb_type, "3."))); // D410/D15/D455@USB2

                if (ImGui::Selectable("On-Chip Calibration", false , avoid_selection_flag))
                {
                    try
                    {
                        if (show_disclaimer)
                        {
                            auto disclaimer_notice = std::make_shared<ucal_disclaimer_model>();
                            viewer.not_model->add_notification(disclaimer_notice);
                        }


                        auto manager = std::make_shared<on_chip_calib_manager>(viewer, sub, *this, dev);
                        auto n = std::make_shared<autocalib_notification_model>("", manager, false);
                        viewer.not_model->add_notification(n);
                        n->forced = true;
                        n->update_state = autocalib_notification_model::RS2_CALIB_STATE_SELF_INPUT;

                        for (auto&& n : related_notifications)
                            if (dynamic_cast<autocalib_notification_model*>(n.get()))
                                n->dismiss(false);

                        related_notifications.push_back(n);
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
                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("This will improve the depth noise.\n"
                        "Point at a scene that normally would have > 50 %% valid depth pixels,\n"
                        "then press calibrate."
                        "The health-check will be calculated.\n"
                        "If >0.25 we recommend applying the new calibration.\n"
                        "\"White wall\" mode should only be used when pointing at a flat white wall with projector on");

                // Focal Length Calibration is disabled for D421, since this process has not been optimized for this device
                std::string pid = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
                if (pid != "1155" && ImGui::Selectable("Focal Length Calibration"))
                {
                    try
                    {
                        if (disable_fl_cal)
                        {
                            auto disable_fl_notice = std::make_shared<fl_cal_limitation_model>();
                            viewer.not_model->add_notification(disable_fl_notice);
                        }
                        else
                        {
                            std::shared_ptr< subdevice_model> sub_color;
                            for (auto&& sub2 : subdevices)
                            {
                                if (sub2->s->is<rs2::color_sensor>())
                                {
                                    sub_color = sub2;
                                    break;
                                }
                            }

                            if (show_disclaimer)
                            {
                                auto disclaimer_notice = std::make_shared<ucal_disclaimer_model>();
                                viewer.not_model->add_notification(disclaimer_notice);
                            }
                            auto manager = std::make_shared<on_chip_calib_manager>(viewer, sub, *this, dev, sub_color);
                            auto n = std::make_shared<autocalib_notification_model>("", manager, false);
                            viewer.not_model->add_notification(n);
                            n->forced = true;
                            n->update_state = autocalib_notification_model::RS2_CALIB_STATE_FL_INPUT;

                            for (auto&& n : related_notifications)
                                if (dynamic_cast<autocalib_notification_model*>(n.get()))
                                    n->dismiss(false);

                            related_notifications.push_back(n);
                            manager->start_fl_viewer();
                        }
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
                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("Focal length calibration is used to adjust camera focal length with specific target.");

                if (ImGui::Selectable("Tare Calibration", false, avoid_selection_flag))
                {
                    try
                    {
                        if (show_disclaimer)
                        {
                            auto disclaimer_notice = std::make_shared<ucal_disclaimer_model>();
                            viewer.not_model->add_notification(disclaimer_notice);
                        }
                        auto manager = std::make_shared<on_chip_calib_manager>(viewer, sub, *this, dev);
                        auto n = std::make_shared<autocalib_notification_model>("", manager, false);
                        viewer.not_model->add_notification(n);
                        n->forced = true;
                        n->update_state = autocalib_notification_model::RS2_CALIB_STATE_TARE_INPUT;

                        for (auto&& n : related_notifications)
                            if (dynamic_cast<autocalib_notification_model*>(n.get()))
                                n->dismiss(false);

                        related_notifications.push_back(n);
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
                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("Tare calibration is used to adjust camera absolute distance to flat target.\n"
                        "User needs either to enter the known ground truth or use the get button\n"
                        "with specific target to get the ground truth.");

                //#define UVMAP_CAL
#ifdef UVMAP_CAL // Disabled due to stability and maturity levels
                try
                {
                    for (auto&& sub2 : subdevices)
                    {
                        if (sub2->s->is<rs2::color_sensor>())
                        {
                            if (ImGui::Selectable("UV-Mapping Calibration"))
                            {
                                if (show_disclaimer)
                                {
                                    auto disclaimer_notice = std::make_shared<ucal_disclaimer_model>();
                                    viewer.not_model->add_notification(disclaimer_notice);
                                }
                                auto manager = std::make_shared<on_chip_calib_manager>(viewer, sub, *this, dev, sub2, sub2->uvmapping_calib_full);
                                auto n = std::make_shared<autocalib_notification_model>("", manager, false);
                                viewer.not_model->add_notification(n);
                                n->forced = true;
                                n->update_state = autocalib_notification_model::RS2_CALIB_STATE_UVMAPPING_INPUT;

                                for (auto&& n : related_notifications)
                                    if (dynamic_cast<autocalib_notification_model*>(n.get()))
                                        n->dismiss(false);

                                related_notifications.push_back(n);
                                manager->start_uvmapping_viewer();
                            }

                            if (ImGui::IsItemHovered())
                                RsImGui::CustomTooltip("UV-Mapping calibration is used to improve UV-Mapping with specific target.");
                        }
                    }
                }
                catch (const error& e)
                {
                    error_message = error_to_string(e);
                }
                catch (const std::exception& e)
                {
                    error_message = e.what();
                }
#endif //UVMAP_CAL

                //if (ImGui::Selectable("Focal Length Plus Calibration"))
                //{
                //    try
                //    {
                //        std::shared_ptr< subdevice_model> sub_color;
                //        for (auto&& sub2 : subdevices)
                //        {
                //            if (sub2->s->is<rs2::color_sensor>())
                //            {
                //                sub_color = sub2;
                //                break;
                //            }
                //        }

                //        auto manager = std::make_shared<on_chip_calib_manager>(viewer, sub, *this, dev, sub_color);
                //        auto n = std::make_shared<autocalib_notification_model>("", manager, false);

                //        viewer.not_model->add_notification(n);
                //        n->forced = true;
                //        n->update_state = autocalib_notification_model::RS2_CALIB_STATE_FL_PLUS_INPUT;

                //        for (auto&& n : related_notifications)
                //            if (dynamic_cast<autocalib_notification_model*>(n.get()))
                //                n->dismiss(false);

                //        related_notifications.push_back(n);
                //        manager->start_fl_plus_viewer();
                //    }
                //    catch (const error& e)
                //    {
                //        error_message = error_to_string(e);
                //    }
                //    catch (const std::exception& e)
                //    {
                //        error_message = e.what();
                //    }
                //}
                //if (ImGui::IsItemHovered())
                //    RsImGui::CustomTooltip("Focal length plus calibration is used to adjust camera focal length and principal points with specific target.");

                if (_calib_model.supports())
                {
                    if (ImGui::Selectable("Calibration Data"))
                    {
                        _calib_model.open();
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Access low level camera calibration parameters");
                }

                if (auto fwlogger = dev.as<rs2::firmware_logger>())
                {
                    if (ImGui::Selectable("Recover Logs from Flash"))
                    {
                        try
                        {
                            bool has_parser = false;
                            std::string hwlogger_xml = config_file::instance().get(configurations::viewer::hwlogger_xml);
                            std::ifstream f(hwlogger_xml.c_str());
                            if (f.good())
                            {
                                try
                                {
                                    std::string str((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
                                    fwlogger.init_parser(str);
                                    has_parser = true;
                                }
                                catch (const std::exception& ex)
                                {
                                    viewer.not_model->output.add_log(
                                        RS2_LOG_SEVERITY_WARN,
                                        __FILE__,
                                        __LINE__,
                                        rsutils::string::from()
                                        << "Invalid Hardware Logger XML at '" << hwlogger_xml
                                        << "': " << ex.what() << "\nEither configure valid XML or remove it");
                                }
                            }

                            auto message = fwlogger.create_message();

                            while (fwlogger.get_flash_log(message))
                            {
                                auto parsed = fwlogger.create_parsed_message();
                                auto parsed_ok = false;

                                if (has_parser)
                                {
                                    if (fwlogger.parse_log(message, parsed))
                                    {
                                        parsed_ok = true;

                                        viewer.not_model->output.add_log(message.get_severity(),
                                            parsed.file_name(),
                                            parsed.line(),
                                            rsutils::string::from()
                                            << "FW-LOG [" << parsed.thread_name()
                                            << "] " << parsed.message());
                                    }
                                }

                                if (!parsed_ok)
                                {
                                    std::stringstream ss;
                                    for (auto& elem : message.data())
                                        ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(elem) << " ";
                                    viewer.not_model->output.add_log(message.get_severity(), __FILE__, 0, ss.str());
                                }
                            }
                        }
                        catch (const std::exception& ex)
                        {
                            viewer.not_model->output.add_log(
                                RS2_LOG_SEVERITY_WARN,
                                __FILE__,
                                __LINE__,
                                rsutils::string::from() << "Failed to fetch firmware logs: " << ex.what());
                        }
                    }
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("Recovers last set of firmware logs prior to camera shutdown / disconnect");
                }

                has_autocalib = true;
            }
        }
        return has_autocalib;
    }

    bool device_model::draw_device_panel_auto_calib_d500(viewer_model& viewer, bool& something_to_show,
        std::string& error_message)
    {
        bool has_autocalib = false;

        bool streaming = is_streaming();
        ImGuiSelectableFlags avoid_selection_flag = (streaming) ? ImGuiSelectableFlags_Disabled : 0;

        for (auto&& sub : subdevices)
        {
            if (sub->supports_on_chip_calib() && !has_autocalib)
            {
                bool is_d555 = false;

                if( dev.supports( RS2_CAMERA_INFO_NAME ) )
                {
                    auto dev_name = std::string( dev.get_info( RS2_CAMERA_INFO_NAME ) );
                    if( dev_name.find( "D555" ) != std::string::npos )
                        is_d555 = true;
                }

                if (ImGui::Selectable("On-Chip Calibration", false, avoid_selection_flag))
                {
                    try
                    {
                        std::shared_ptr< process_manager > manager;
                        std::shared_ptr< process_notification_model > n;
                        if( is_d555 )
                        {
                            // D555 OHM is same as D455, using D400 calibration algorithms.
                            manager = std::make_shared< on_chip_calib_manager >( viewer, sub, *this, dev );
                            n = std::make_shared< autocalib_notification_model >( "", manager, false );
                        }
                        else
                        {
                            manager = std::make_shared<d500_on_chip_calib_manager>(viewer, sub, *this, dev );
                            n = std::make_shared< d500_autocalib_notification_model >( "", manager, false );
                        }
                        viewer.not_model->add_notification( n );
                        n->forced = true;
                        n->update_state = d500_autocalib_notification_model::RS2_CALIB_STATE_INIT_CALIB;

                        for( auto && n : related_notifications )
                            if( dynamic_cast< d500_autocalib_notification_model * >( n.get() ) )
                                n->dismiss( false );

                        related_notifications.push_back( n );
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
                if (ImGui::IsItemHovered())
                {
                    std::string tooltip;
                    if( is_d555 )
                        tooltip = "This will improve the depth noise.\n"
                                  "Point at a scene that normally would have > 50 %% valid depth pixels,\n"
                                  "then press calibrate."
                                  "The health-check will be calculated.\n"
                                  "If >0.25 we recommend applying the new calibration.\n"
                                  "\"White wall\" mode should only be used when pointing at a flat white wall "
                                  "with projector on";
                    else
                        tooltip = rsutils::string::from() << "On-Chip Calibration" << (streaming ? " (Disabled while streaming)" : "");
                    RsImGui::CustomTooltip("%s", tooltip.c_str());
                }

                if( !is_d555 )
                {
                    if( ImGui::Selectable( "Dry Run On-Chip Calibration", false, avoid_selection_flag ) )
                    {
                        try
                        {
                            auto manager = std::make_shared<d500_on_chip_calib_manager>(viewer, sub, *this, dev);
                            auto n = std::make_shared<d500_autocalib_notification_model>("", manager, false);
                            viewer.not_model->add_notification(n);
                            n->forced = true;
                            n->update_state = d500_autocalib_notification_model::RS2_CALIB_STATE_INIT_DRY_RUN;

                            for (auto&& n : related_notifications)
                                if (dynamic_cast<autocalib_notification_model*>(n.get()))
                                    n->dismiss(false);

                            related_notifications.push_back(n);
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
                    if (ImGui::IsItemHovered())
                    {
                        std::string tooltip = rsutils::string::from()
                                           << "Dry Run On-Chip Calibration"
                                           << ( streaming ? " (Disabled while streaming)" : "" );
                        RsImGui::CustomTooltip("%s", tooltip.c_str());
                    }
                }
                else
                {
                    if( ImGui::Selectable( "Focal Length Calibration" ) )
                    {
                        try
                        {
                            std::shared_ptr< subdevice_model > sub_color;
                            for( auto && sub2 : subdevices )
                            {
                                if( sub2->s->is< rs2::color_sensor >() )
                                {
                                    sub_color = sub2;
                                    break;
                                }
                            }

                            auto manager = std::make_shared< on_chip_calib_manager >( viewer, sub, *this, dev, sub_color );
                            auto n = std::make_shared< autocalib_notification_model >( "", manager, false );
                            viewer.not_model->add_notification( n );
                            n->forced = true;
                            n->update_state = autocalib_notification_model::RS2_CALIB_STATE_FL_INPUT;

                            for( auto && n : related_notifications )
                                if( dynamic_cast< autocalib_notification_model * >( n.get() ) )
                                    n->dismiss( false );

                            related_notifications.push_back( n );
                            manager->start_fl_viewer();
                        }
                        catch( const error & e )
                        {
                            error_message = error_to_string( e );
                        }
                        catch( const std::exception & e )
                        {
                            error_message = e.what();
                        }
                    }
                    if( ImGui::IsItemHovered() )
                        RsImGui::CustomTooltip( "Focal length calibration is used to adjust camera focal length with specific target." );

                    if( ImGui::Selectable( "Tare Calibration" ) )
                    {
                        try
                        {
                            auto manager = std::make_shared< on_chip_calib_manager >( viewer, sub, *this, dev );
                            auto n = std::make_shared< autocalib_notification_model >( "", manager, false );
                            viewer.not_model->add_notification( n );
                            n->forced = true;
                            n->update_state = autocalib_notification_model::RS2_CALIB_STATE_TARE_INPUT;

                            for( auto && n : related_notifications )
                                if( dynamic_cast< autocalib_notification_model * >( n.get() ) )
                                    n->dismiss( false );

                            related_notifications.push_back( n );
                        }
                        catch( const error & e )
                        {
                            error_message = error_to_string( e );
                        }
                        catch( const std::exception & e )
                        {
                            error_message = e.what();
                        }
                    }
                    if( ImGui::IsItemHovered() )
                        RsImGui::CustomTooltip( "Tare calibration is used to adjust camera absolute distance to flat target.\n"
                                           "User needs either to enter the known ground truth or use the get button\n"
                                           "with specific target to get the ground truth." );

                    if (_calib_model.supports())
                    {
                        if (ImGui::Selectable("Calibration Data"))
                        {
                            _calib_model.open();
                        }
                        if (ImGui::IsItemHovered())
                            RsImGui::CustomTooltip("Access low level camera calibration parameters");
                    }

                }

                has_autocalib = true;
            }
        }

        return has_autocalib;
    }

    void device_model::open_hdr_config_tool_window()
    {
        _hdr_model.open_hdr_tool_window();
    }
}
