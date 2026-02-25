// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "fw-update-helper.h"
#include "model-views.h"
#include "viewer.h"
#include <realsense_imgui.h>
#include "ux-window.h"

#include <rsutils/os/special-folder.h>
#include "os.h"

#include <rsutils/easylogging/easyloggingpp.h>
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>

#ifdef INTERNAL_FW
#include "common/fw/D4XX_FW_Image.h"
#else
#define FW_D4XX_FW_IMAGE_VERSION ""
const char* fw_get_D4XX_FW_Image(int) { return NULL; }


#endif // INTERNAL_FW

constexpr const char* recommended_fw_url = "https://dev.realsenseai.com/docs/firmware-updates";

namespace rs2
{
    enum firmware_update_ui_state
    {
        RS2_FWU_STATE_INITIAL_PROMPT = 0,
        RS2_FWU_STATE_IN_PROGRESS = 1,
        RS2_FWU_STATE_COMPLETE = 2,
        RS2_FWU_STATE_FAILED = 3,
    };

    bool is_recommended_fw_available(const std::string& product_line, const std::string& pid)
    {
        auto pl = parse_product_line(product_line);
        auto fv = get_available_firmware_version(pl, pid);
        return !(fv == "");
    }

    int parse_product_line(const std::string& product_line)
    {
        if (product_line == "D400") return RS2_PRODUCT_LINE_D400;
        else return -1;
    }

    std::string get_available_firmware_version(int product_line, const std::string& pid)
    {
        if (product_line == RS2_PRODUCT_LINE_D400) return FW_D4XX_FW_IMAGE_VERSION;
        else return "";
    }

    std::vector< uint8_t > get_default_fw_image( int product_line, const std::string & pid )
    {
        std::vector< uint8_t > image;

        switch( product_line )
        {
        case RS2_PRODUCT_LINE_D400: 
        {
            bool allow_rc_firmware = config_file::instance().get_or_default( configurations::update::allow_rc_firmware, false );
            if( strlen( FW_D4XX_FW_IMAGE_VERSION ) && ! allow_rc_firmware )
            {
                int size = 0;
                auto hex = fw_get_D4XX_FW_Image( size );
                image = std::vector< uint8_t >( hex, hex + size );
            }
        }
        break;
        default:
            break;
        }
        return image;
    }

    bool is_upgradeable(const std::string& curr, const std::string& available)
    {
        if (curr == "" || available == "") return false;
        return rsutils::version( curr ) < rsutils::version( available );
    }

    bool firmware_update_manager::check_for(
        std::function<bool()> action, std::function<void()> cleanup,
        std::chrono::system_clock::duration delta)
    {
        using namespace std;
        using namespace std::chrono;

        auto start = system_clock::now();
        auto now = system_clock::now();
        do
        {
            try
            {

                if (action()) return true;
            }
            catch (const error& e)
            {
                fail(error_to_string(e));
                cleanup();
                return false;
            }
            catch (const std::exception& ex)
            {
                fail(ex.what());
                cleanup();
                return false;
            }
            catch (...)
            {
                fail("Unknown error during update.\nPlease reconnect the camera to exit recovery mode");
                cleanup();
                return false;
            }

            now = system_clock::now();
            this_thread::sleep_for(milliseconds(100));
        } while (now - start < delta);
        return false;
    }

    void firmware_update_manager::process_mipi_signed_fw()
    {
        bool is_mipi_recovery = !(strcmp(_dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID), "BBCD"));
        if (!_is_signed)
        {
            fail("Signed FW update for MIPI device - This FW file is not signed ");
            return;
        }
        if (!is_mipi_recovery)
        {
            auto dev_updatable = _dev.as<updatable>();
            if(!(dev_updatable && dev_updatable.check_firmware_compatibility(_fw)))
            {
                std::stringstream ss;
                ss << "The firmware version is not compatible with ";
                ss << _dev.get_info(RS2_CAMERA_INFO_NAME) << std::endl;
                fail(ss.str());
                return;
            }
        }

        _progress = 1;

        auto update_dev = _dev.as<update_device>();
        if (!update_dev)
        {
            std::stringstream ss;
            ss << "The firmware version could not be burnt on ";
            ss << _dev.get_info(RS2_CAMERA_INFO_NAME) << std::endl;
            fail(ss.str());
            return;
        }
        // avoids same log sent multiple times
        static constexpr std::array<std::pair<float,const char*>,3> steps = {{
            {  5.f,  "Burning Signed Firmware on MIPI device" },
            { 95.f,  "Firmware Update completed, waiting for device to reconnect" },
            {100.f,  "FW update process completed successfully" }
        }};

        size_t next = 0;

        update_dev.update(_fw, [&](float progress01)
        {
            _progress = progress01 * 100.f;

            if (next < steps.size() && _progress >= steps[next].first)
            {
                log(steps[next].second);
                if (steps[next].first >= 100.f)
                    _done = true;
                ++next;
            }
        });

        if (is_mipi_recovery)
        {
            log("For GMSL MIPI device please reboot, or reload d4xx driver\n"\
                "sudo rmmod d4xx && sudo modprobe d4xx\n"\
                "and restart the realsense-viewer");
            LOG_INFO("For GMSL MIPI device please reboot, or reload d4xx driver\n"\
                     "sudo rmmod d4xx && sudo modprobe d4xx\n"\
                     "and restart the realsense-viewer");
        }

        // Restart the device to reconstruct with the new version information
        _dev.hardware_reset();
    }

    void firmware_update_manager::backup_firmware(updatable& upd, int& next_progress, const std::string& serial)
    {
        log("Trying to back-up camera flash memory");

        std::string log_backup_status;
        try
        {
            auto flash = upd.create_flash_backup([&](const float progress)
                {
                    _progress = ((ceil(progress * 5) / 5) * (30 - next_progress)) + next_progress;
                });

            // Not all cameras supports this feature
            if (!flash.empty())
            {
                auto temp = rsutils::os::get_special_folder(rsutils::os::special_folder::app_data);
                temp += serial + "." + get_timestamped_file_name() + ".bin";

                {
                    std::ofstream file(temp.c_str(), std::ios::binary);
                    file.write((const char*)flash.data(), flash.size());
                    log_backup_status = "Backup completed and saved as '" + temp + "'";
                }
            }
            else
            {
                log_backup_status = "Backup flash is not supported";
            }
        }
        catch (const std::exception& e)
        {
            if (auto not_model_protected = get_protected_notification_model())
            {
                log_backup_status = "WARNING: backup failed; continuing without it...";
                not_model_protected->output.add_log(RS2_LOG_SEVERITY_WARN,
                    __FILE__,
                    __LINE__,
                    log_backup_status + ", Error: " + e.what());
            }
        }
        catch (...)
        {
            if (auto not_model_protected = get_protected_notification_model())
            {
                log_backup_status = "WARNING: backup failed; continuing without it...";
                not_model_protected->add_log(log_backup_status + ", Unknown error occurred");
            }
        }

        if (!log_backup_status.empty())
            log(log_backup_status);
    }

    void firmware_update_manager::switch_device_to_recovery_mode(updatable& upd, const std::string& serial, update_device& dfu, std::function<void()> cleanup)
    {
        // in order to update device to DFU state, it will be disconnected then switches to DFU state
        // if querying devices is called while device still switching to DFU state, an exception will be thrown
        // to prevent that, a blocking is added to make sure device is updated before continue to next step of querying device
        upd.enter_update_state();

        if (!check_for([this, serial, &dfu]() {
            auto devs = _ctx.query_devices();

            for (uint32_t j = 0; j < devs.size(); j++)
            {
                try
                {
                    auto d = devs[j];
                    if (d.is<update_device>())
                    {
                        if (d.supports(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID))
                        {
                            if (serial == d.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID))
                            {
                                log("DFU device '" + serial + "' found");
                                dfu = d;
                                return true;
                            }
                        }
                    }
                }
                catch (std::exception& e) {
                    if (auto not_model_protected = get_protected_notification_model())
                    {
                        not_model_protected->output.add_log(RS2_LOG_SEVERITY_WARN,
                            __FILE__,
                            __LINE__,
                            rsutils::string::from() << "Exception caught in FW Update process-flow: " << e.what() << "; Retrying...");
                    }
                }
                catch (...) {}
            }

            return false;
            }, cleanup, std::chrono::seconds(60)))
        {
            fail("Recovery device did not connect in time!");
            return;
        }
    }

    void firmware_update_manager::process_flow(
        std::function<void()> cleanup,
        invoker invoke)
    {
        // if device is MIPI device, and fw is signed - using mipi specific procedure
        bool is_mipi_device = !strcmp(_dev.get_info(RS2_CAMERA_INFO_CONNECTION_TYPE), "GMSL");
        if (_is_signed && is_mipi_device)
        {
            process_mipi_signed_fw();
            return;
        }

        std::string serial = "";
        if (_dev.supports(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID))
            serial = _dev.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
        else
            serial = _dev.query_sensors().front().get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);

        // Clear FW update related notification to avoid dismissing the notification on ~device_model()
        // We want the notification alive during the whole process.
        _model.related_notifications.erase(
            std::remove_if( _model.related_notifications.begin(),
                            _model.related_notifications.end(),
                            []( std::shared_ptr< notification_model > n ) {
                                return n->is< fw_update_notification_model >();
                            } ) , end(_model.related_notifications));

        for (auto&& n : _model.related_notifications)
        {
            if (n->is< fw_update_notification_model >()
                || n->is< sw_recommended_update_alert_model >())
                n->dismiss(false);
        }

        _progress = 5;

        int next_progress = 10;

        update_device dfu{};

        if (auto upd = _dev.as<updatable>())
        {
            // checking firmware version compatibility with device
            if (_is_signed)
            {
                if (!upd.check_firmware_compatibility(_fw))
                {
                    std::stringstream ss;
                    ss << "The firmware version is not compatible with ";
                    ss << _dev.get_info(RS2_CAMERA_INFO_NAME) << std::endl;
                    fail(ss.str());
                    return;
                }
            }

            if (!is_mipi_device)
            {
                backup_firmware(upd, next_progress, serial);
            }

            next_progress = static_cast<int>(_progress) + 10;

            if (_is_signed)
            {
                log("Requesting to switch to recovery mode");

                switch_device_to_recovery_mode(upd, serial, dfu, cleanup);
            }
        }
        else
        {
            dfu = _dev.as<update_device>();
        }

        if (dfu)
        {
            _progress = float(next_progress);

            log("Recovery device connected, starting update..\n"
                "Internal write is in progress\n"
                "Please DO NOT DISCONNECT the camera");

            dfu.update(_fw, [&](const float progress)
            {
                _progress = ((ceil(progress * 10) / 10 * (90 - next_progress)) + next_progress);
            });

            log( "Firmware Download completed, await DFU transition event" );
            std::this_thread::sleep_for( std::chrono::seconds( 3 ) );
        }
        else
        {
            auto upd = _dev.as<updatable>();
            upd.update_unsigned(_fw, [&](const float progress)
            {
                _progress = (ceil(progress * 10) / 10 * (90 - next_progress)) + next_progress;
            });
            log("Firmware Update completed, waiting for device to reconnect");

            if (is_mipi_device)
            {
                _dev.hardware_reset();
            }
        }

        if (!check_for([this, serial]() {
            auto devs = _ctx.query_devices();

            for (uint32_t j = 0; j < devs.size(); j++)
            {
                try
                {
                    auto d = devs[j];

                    if (d.query_sensors().size() && d.query_sensors().front().supports(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID))
                    {
                        auto s = d.query_sensors().front().get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
                        if (s == serial)
                        {
                            log("Discovered connection of the original device");
                            return true;
                        }
                    }
                }
                catch (...) {}
            }

            return false;
        }, cleanup, std::chrono::seconds(60)))
        {
            fail("Original device did not reconnect in time!");
            return;
        }

        log( "Device reconnected successfully!\n"
             "FW update process completed successfully" );

        _progress = 100;

        _done = true;
    }

    void fw_update_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        using namespace std;
        using namespace chrono;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
        { float(x + width), float(y + 25) }, ImColor(shadow));

        if (update_state != RS2_FWU_STATE_COMPLETE)
        {
            ImGui::Text("Firmware Update Recommended!");

            ImGui::SetCursorScreenPos({ float(x + 5), float(y + 27) });

            draw_text(get_title().c_str(), x, y, height - 50);

            ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 67) });

            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1.f - t));

            if (update_state == RS2_FWU_STATE_INITIAL_PROMPT)
                ImGui::Text("Firmware updates offer critical bug fixes and\nunlock new camera capabilities.");
            else
                ImGui::Text("Firmware update is underway...\nPlease do not disconnect the device");

            ImGui::PopStyleColor();
        }
        else
        {
            //ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_blue, 1.f - t));
            ImGui::Text("Update Completed");
            //ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
            ImGui::PushFont(win.get_large_font());
            std::string txt = rsutils::string::from() << textual_icons::trophy;
            ImGui::Text("%s", txt.c_str());
            ImGui::PopFont();

            ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });
            ImGui::Text("Camera Firmware Updated Successfully");
        }

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        const auto bar_width = width - 115;

        if (update_manager)
        {
            if (update_state == RS2_FWU_STATE_INITIAL_PROMPT)
            {
                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                std::string button_name = rsutils::string::from() << "Install" << "##fwupdate" << index;

                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }) || update_manager->started())
                {
                    // stopping stream before starting fw update
                    auto fw_update_manager = dynamic_cast<firmware_update_manager*>(update_manager.get());
                    if( ! fw_update_manager )
                        throw std::runtime_error( "Cannot convert firmware_update_manager" );
                    std::for_each(fw_update_manager->get_device_model().subdevices.begin(),
                        fw_update_manager->get_device_model().subdevices.end(),
                        [&](const std::shared_ptr<subdevice_model>& sm)
                        {
                            if (sm->streaming)
                            {
                                try
                                {
                                    sm->stop(fw_update_manager->get_protected_notification_model());
                                }
                                catch (...)
                                {
                                    // avoiding exception that can be sent by stop method
                                    // this could happen if the sensor is not streaming and the stop method is called - for example
                                }
                            }
                        });

                    // Wait for all background stop operations to complete before FW update
                    std::for_each(fw_update_manager->get_device_model().subdevices.begin(),
                        fw_update_manager->get_device_model().subdevices.end(),
                        [](const std::shared_ptr<subdevice_model>& sm)
                        {
                            sm->wait_for_stop();
                        });

                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };

                    if (!update_manager->started())
                        update_manager->start(invoke);

                    update_state = RS2_FWU_STATE_IN_PROGRESS;
                    enable_dismiss = false;
                    _progress_bar.last_progress_time = system_clock::now();
                }
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "New firmware will be flashed to the device");
                }
            }
            else if (update_state == RS2_FWU_STATE_IN_PROGRESS)
            {
                if (update_manager->done())
                {
                    update_state = RS2_FWU_STATE_COMPLETE;
                    pinned = false;
                    _progress_bar.last_progress_time = last_interacted = system_clock::now();
                }

                if (!expanded)
                {
                    if (update_manager->failed())
                    {
                        update_manager->check_error(error_message);
                        update_state = RS2_FWU_STATE_FAILED;
                        pinned = false;
                        dismiss(false);
                    }

                    draw_progress_bar(win, bar_width);

                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

                    string id = rsutils::string::from() << "Expand" << "##" << index;
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    if (ImGui::Button(id.c_str(), { 100, 20 }))
                    {
                        expanded = true;
                    }

                    ImGui::PopStyleColor();
                }
            }
        }
        else
        {
            std::string button_name = rsutils::string::from() << "Learn More..." << "##" << index;

            if (ImGui::Button(button_name.c_str(), { float(bar_width), 20 }))
            {
                open_url(recommended_fw_url);
            }
            if (ImGui::IsItemHovered())
            {
                win.link_hovered();
                RsImGui::CustomTooltip("%s", "Internet connection required");
            }
        }
    }

    void fw_update_notification_model::draw_expanded(ux_window& win, std::string& error_message)
    {
        if (update_manager->started() && update_state == RS2_FWU_STATE_INITIAL_PROMPT)
            update_state = RS2_FWU_STATE_IN_PROGRESS;

        auto flags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(500, 100));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        std::string title = "Firmware Update";
        if (update_manager->failed()) title += " Failed";

        ImGui::OpenPopup(title.c_str());
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, flags))
        {
            ImGui::SetCursorPosX(200);
            std::string progress_str = rsutils::string::from() << "Progress: " << update_manager->get_progress() << "%";
            ImGui::Text("%s", progress_str.c_str());

            ImGui::SetCursorPosX(5);

            draw_progress_bar(win, 490);

            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            auto s = update_manager->get_log();
            ImGui::InputTextMultiline("##fw_update_log", const_cast<char*>(s.c_str()),
                s.size() + 1, { 490,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(190);
            if (visible || update_manager->done() || update_manager->failed())
            {
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (update_manager->done() || update_manager->failed())
                    {
                        update_state = RS2_FWU_STATE_FAILED;
                        pinned = false;
                        dismiss(false);
                    }
                    expanded = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
                ImGui::PushStyleColor(ImGuiCol_Text, transparent);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, transparent);
                ImGui::Button("OK", ImVec2(120, 0));
                ImGui::PopStyleColor(5);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        error_message = "";
    }

    int fw_update_notification_model::calc_height()
    {
        if (update_state != RS2_FWU_STATE_COMPLETE) return 150;
        else return 65;
    }

    void fw_update_notification_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);

        ImGui::PopStyleColor(1);

        ImVec4 c;

        if (update_state == RS2_FWU_STATE_COMPLETE)
        {
            c = alpha(saturate(light_blue, 0.7f), 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            c = alpha(sensor_bg, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
    }

    fw_update_notification_model::fw_update_notification_model(std::string name,
        std::shared_ptr<firmware_update_manager> manager, bool exp)
        : process_notification_model(manager)
    {
        enable_expand = false;
        expanded = exp;
        if (expanded) visible = false;

        message = name;
        this->severity = RS2_LOG_SEVERITY_INFO;
        this->category = RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED;
        pinned = true;
        forced = true;
    }
}
