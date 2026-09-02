// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.


#include <viewer.h>
#include "d500-on-chip-calib.h"

namespace rs2
{
    d500_on_chip_calib_manager::d500_on_chip_calib_manager(viewer_model& viewer, std::shared_ptr<subdevice_model> sub,
        device_model& model, device dev)
        : process_manager("D500 On-Chip Calibration"),
        _model(model),
        _dev(dev),
        _sub(sub),
        _viewer(viewer)
    {
        if (dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE) &&
            std::string(dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE)) != "D500")
        {
            throw std::runtime_error("This Calibration Process cannot be processed with this device");
        }
    }

    bool d500_on_chip_calib_manager::start_viewer(int w, int h, int fps, invoker invoke)
    {
        bool frame_arrived = false;
        try
        {
            int uid = 0;
            bool found_z16 = false;
            for (const auto& format : _sub->formats)
            {
                if (format.second[0] == "Z16")
                {
                    uid = format.first;
                    found_z16 = true;
                    break;
                }
            }
            if (!found_z16)
                return false;  // depth subdevice with no Z16 profile — abort rather than enable the wrong stream

            _sub->select_resolution(w, h, RS2_STREAM_DEPTH);

            _sub->stream_enabled.clear();
            _sub->stream_enabled[uid] = true;
            _sub->ui.selected_format_id.clear();
            _sub->ui.selected_format_id[uid] = 0;

            for (size_t i = 0; i < _sub->shared_fps_values.size(); i++)
            {
                if (_sub->shared_fps_values[i] == fps)
                    _sub->ui.selected_shared_fps_id = static_cast<int>(i);
            }

            if (!_sub->is_selected_combination_supported())
                return false;

            auto profiles = _sub->get_selected_profiles();

            invoke([&]()
            {
                if (!_model.dev_syncer)
                    _model.dev_syncer = _viewer.syncer->create_syncer();

                _sub->play(profiles, _viewer, _model.dev_syncer);
                for (auto&& profile : profiles)
                    _viewer.begin_stream(_sub, profile);
            });

            int count = 0;
            while (!frame_arrived && count++ < 200)
            {
                for (auto&& stream : _viewer.streams)
                {
                    if (std::find(profiles.begin(), profiles.end(),
                        stream.second.original_profile) != profiles.end())
                    {
                        auto now = std::chrono::high_resolution_clock::now();
                        if (now - stream.second.last_frame < std::chrono::milliseconds(100))
                            frame_arrived = true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (...) {}
        return frame_arrived;
    }

    void d500_on_chip_calib_manager::try_start_viewer(int w, int h, int fps, invoker invoke)
    {
        bool started = start_viewer(w, h, fps, invoke);
        if (!started)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            started = start_viewer(w, h, fps, invoke);
        }
        if (!started)
        {
            stop_viewer(invoke);
            throw std::runtime_error(rsutils::string::from()
                << "Failed to start streaming (" << w << ", " << h << ", " << fps << ")!");
        }
    }

    void d500_on_chip_calib_manager::stop_viewer(invoker invoke)
    {
        try
        {
            invoke([&]()
            {
                if (_sub->streaming)
                    _sub->stop(_viewer.not_model);
            });
        }
        catch (...) {}
    }

    void d500_on_chip_calib_manager::restore_workspace(invoker invoke)
    {
        if (!_saved_ui)
            return;  // no auto-start happened on this manager

        stop_viewer(invoke);

        _sub->ui = *_saved_ui;
        _sub->stream_enabled = _saved_stream_enabled;
        _saved_ui.reset();

        if (_was_streaming)
        {
            try
            {
                auto profiles = _sub->get_selected_profiles();
                if (!profiles.empty())
                {
                    invoke([&]()
                    {
                        if (!_model.dev_syncer)
                            _model.dev_syncer = _viewer.syncer->create_syncer();
                        _sub->play(profiles, _viewer, _model.dev_syncer);
                        for (auto&& profile : profiles)
                            _viewer.begin_stream(_sub, profile);
                    });
                }
            }
            catch (...) {}
        }
        _was_streaming = false;
    }

    std::string d500_on_chip_calib_manager::convert_action_to_json_string()
    {
        std::stringstream ss;
        switch (action)
        {
        case RS2_CALIB_ACTION_ON_CHIP_CALIB:         ss << "{\n calib run }";      break;
        case RS2_CALIB_ACTION_ON_CHIP_CALIB_DRY_RUN: ss << "{\n calib dry run }";  break;
        case RS2_CALIB_ACTION_ON_CHIP_CALIB_ABORT:   ss << "{\n calib abort }";    break;
        case RS2_CALIB_ACTION_ON_CHIP_CALIB_COMMIT:  ss << "{\n calib commit }";   break;
        case RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_NEW: ss << "{\n calib try new }";  break;
        case RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_OLD: ss << "{\n calib try old }";  break;
        default:
            throw std::runtime_error("unknown calib_action in convert_action_to_json_string");
        }
        return ss.str();
    }

    bool d500_on_chip_calib_manager::uses_interactive_triggered_calibration() const
    {
        // Mirrors ds::uses_interactive_triggered_calibration in src/ds/d500/d500-private.h — the viewer cannot
        // include SDK-internal headers. Keep the two lists in sync when adding new PIDs.
        // 0C01-0C08: D5x5 family (D535/D585 2C/3C/F/proto). 0B6B: D585S safety.
        static const std::set< std::string > interactive_triggered_calibration_pids = {
            "0C01", "0C02", "0C03", "0C04", "0C05", "0C06", "0C07", "0C08", "0B6B"
        };
        return interactive_triggered_calibration_pids.count(get_device_pid()) > 0;
    }

    void d500_on_chip_calib_manager::process_flow(std::function<void()> cleanup, invoker invoke)
    {
        const bool interactive = uses_interactive_triggered_calibration();
        const bool interactive_run = interactive && action == RS2_CALIB_ACTION_ON_CHIP_CALIB;
        const bool interactive_terminal = interactive
            && (action == RS2_CALIB_ACTION_ON_CHIP_CALIB_COMMIT
                || action == RS2_CALIB_ACTION_ON_CHIP_CALIB_ABORT);
        const bool interactive_try = interactive
            && (action == RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_NEW
                || action == RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_OLD);

        // D5x5 interactive TC needs a live depth stream during the RUN phase (algorithm consumes depth frames).
        // Mirrors the D400 pattern: 1280x720 @ 30fps on Z16. Skip for TRY/COMMIT/ABORT — those reuse the already-live stream.
        // Auto-start fires on the first progress callback of a RUN — cannot key on the progress value itself while FW
        // still reports 0xFF (parsed as -1). If try_start_viewer throws, swallow it locally: rs2::update_progress_callback
        // has no try/catch (rs_device.hpp:231), so letting the exception escape aborts RUN without a CANCEL and the
        // precondition check on the next RUN would then wedge the device.
        bool streaming_started = false;

        try
        {
            std::string json = convert_action_to_json_string();
            auto calib_dev = _dev.as<auto_calibrated_device>();
            float health = 0.f;
            int timeout_ms = 240000; // increased to 4 minutes for additional algo processing
            auto ans = calib_dev.run_on_chip_calibration(json, &health,
                [&](const float progress)
                {
                    _progress = progress;
                    if (interactive_run && !streaming_started)
                    {
                        streaming_started = true;  // set before start attempt — even a failure must not retry every poll
                        _saved_ui = std::make_shared<subdevice_ui_selection>(_sub->ui);
                        _saved_stream_enabled = _sub->stream_enabled;
                        _was_streaming = _sub->streaming;
                        try
                        {
                            try_start_viewer(1280, 720, 30, invoke);
                        }
                        catch (const std::exception & e)
                        {
                            // Continue un-streamed; FW will surface a FAILED_TO_CONVERGE terminal state if it needed frames.
                            _saved_ui.reset();
                            _saved_stream_enabled.clear();
                            _was_streaming = false;
                            LOG_WARNING("Interactive TC: depth auto-start failed (" << e.what()
                                        << ") — calibration will proceed without a viewer stream");
                        }
                    }
                }, timeout_ms);

            // For D5x5 interactive triggered calibration, the initial RUN call returns at HEALTH_CHECK — populate scalar health
            // so the UI can render pass/fail; the flow is not "done" until a subsequent COMMIT reaches COMPLETE.
            if (interactive_run)
            {
                _scalar_health = health;
                // `health < 0.f` is the SDK's sentinel for "FW did not report SUCCESS" (see
                // d500_auto_calibrated::run_interactive_triggered_calibration). In that case there is no meaningful
                // candidate to Commit/Try/Discard — surface the run as failed so the notification model shows the
                // FAILED popup (update_ui_on_failure) instead of the HEALTH_CHECK screen, matching the D585S legacy UX.
                if (health < 0.f)
                {
                    restore_workspace(invoke);   // stop the auto-started depth stream, restore user's prior stream
                    _failed = true;
                }
                else
                {
                    _done = true;   // "done" here means "phase complete"; the notification UI transitions to HEALTH_CHECK
                }
                return;             // on success, leave streaming on — TRY/COMMIT/ABORT need it live
            }

            // TRY_NEW/TRY_OLD: do not stop/restart the stream — the USB reconfigure prevents FW from applying the switch.
            if (interactive_try)
            {
                _done = true;
                return;
            }

            // Interactive terminal (COMMIT / ABORT) — mark done and restore the user's pre-calibration workspace.
            if (interactive_terminal)
            {
                _done = true;
                restore_workspace(invoke);
                return;
            }

            // Legacy D500 path (D585_LEGACY).
            if (_progress == 100.0)
                _done = true;
            else
                _failed = true;  // exception must have been thrown from run_on_chip_calibration call
        }
        catch (...)
        {
            // Any failure of a phase where we own the stream → restore before propagating so the user isn't left mid-flight.
            restore_workspace(invoke);
            throw;
        }
    }

    bool d500_on_chip_calib_manager::abort()
    {
        auto calib_dev = _dev.as<auto_calibrated_device>();
        float health = 0.f;
        int timeout_ms = 50000; // 50 seconds
        std::string json = convert_action_to_json_string();
        auto ans = calib_dev.run_on_chip_calibration(json, &health,
            [&](const float progress) {}, timeout_ms);

        // returns 1 on success, 0 on failure
        return (ans[0] == 1);
    }

    void d500_on_chip_calib_manager::prepare_for_calibration()
    {
        // safety sensor in service mode - if safety sensor exists
        auto sensors = _dev.query_sensors();
        for (auto&& s : sensors)
        {
            if (s.is<rs2::safety_sensor>())
            {
                rs2::safety_sensor safety_s = s.as<rs2::safety_sensor>();
                set_option_if_needed<rs2::safety_sensor>(safety_s, RS2_OPTION_SAFETY_MODE, RS2_SAFETY_MODE_SERVICE);
                break;
            }
        }

        // set depth preset as default preset, turn projector ON and depth AE ON
        if (_sub->s->supports(RS2_CAMERA_INFO_NAME) && 
            (std::string(_sub->s->get_info(RS2_CAMERA_INFO_NAME)) == "Stereo Module"))
        {
            auto depth_sensor = _sub->s->as <rs2::depth_sensor>();

            // disabling the depth visual preset change for D555 - not needed
            std::string dev_name = _dev.supports( RS2_CAMERA_INFO_NAME ) ? _dev.get_info( RS2_CAMERA_INFO_NAME ) : "";
            if( dev_name.find( "D555" ) == std::string::npos )
            {
                // set depth preset as default preset
                set_option_if_needed<rs2::depth_sensor>(depth_sensor, RS2_OPTION_VISUAL_PRESET, 1);
            }

            // turn projector ON
            set_option_if_needed<rs2::depth_sensor>(depth_sensor, RS2_OPTION_EMITTER_ENABLED, 1);

            // turn depth AE ON
            set_option_if_needed<rs2::depth_sensor>(depth_sensor, RS2_OPTION_ENABLE_AUTO_EXPOSURE, 1);
        }
    }

    std::string d500_on_chip_calib_manager::get_device_pid() const
    {
        std::string pid_str;
        if (_dev.supports(RS2_CAMERA_INFO_PRODUCT_ID))
            pid_str = _dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
        return pid_str;
    }

    d500_autocalib_notification_model::d500_autocalib_notification_model(std::string name, 
        std::shared_ptr<process_manager> manager, bool exp)
        : process_notification_model(manager)
    {
        enable_expand = false;
        enable_dismiss = true;
        expanded = exp;
        if (expanded) visible = false;

        message = name;
        this->severity = RS2_LOG_SEVERITY_INFO;
        this->category = RS2_NOTIFICATION_CATEGORY_HARDWARE_EVENT;

        pinned = true;
    }

    void d500_autocalib_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        const auto bar_width = width - 115;
        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
            { float(x + width), float(y + 25) }, ImColor(shadow));

        if (update_state != RS2_CALIB_STATE_COMPLETE)
        {
            if (get_manager().action == d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB)
                ImGui::Text("%s", "On-Chip Calibration");
            else if (get_manager().action == d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_DRY_RUN)
                ImGui::Text("%s", "Dry Run On-Chip Calibration");

            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1.f - t));

            if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                enable_dismiss = false;
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });
                ImGui::Text("%s", "Camera is being calibrated...\n");
                draw_abort(win, x, y);
            }
            else if (update_state == RS2_CALIB_STATE_ABORT)
            {
                get_manager().action = d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_ABORT;
                auto _this = shared_from_this();
                auto invoke = [_this](std::function<void()> action) {_this->invoke(action); };
                try
                {
                    update_state = RS2_CALIB_STATE_ABORT_CALLED;
                    _has_abort_succeeded = get_manager().abort();
                }
                catch (...)
                {
                    throw std::runtime_error("Abort could not be performed!");
                }
            }
            else if (update_state == RS2_CALIB_STATE_ABORT_CALLED)
            {
                update_ui_after_abort_called(win, x, y);
            }
            else if (update_state == RS2_CALIB_STATE_INIT_CALIB ||
                update_state == RS2_CALIB_STATE_INIT_DRY_RUN)
            {
                calibration_button(win, x, y, bar_width);
            }
            else if (update_state == RS2_CALIB_STATE_FAILED)
            {
                update_ui_on_failure(win, x, y);
            }
            else if (update_state == RS2_CALIB_STATE_HEALTH_CHECK)
            {
                draw_health_check(win, x, y, bar_width);
            }
            else if (update_state == RS2_CALIB_STATE_COMMIT_IN_PROGRESS)
            {
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });
                ImGui::Text("%s", "Committing calibration to flash...");
            }

            ImGui::PopStyleColor();
        }
        else
        {
            update_ui_on_calibration_complete(win, x, y);
            if (get_manager().get_device_pid() == "0B6B")
            {
                if (!reset_called &&
                    get_manager().action != d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_ABORT)
                {
                    get_manager().reset_device();
                    reset_called = true;
                }
            }
        }

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        if (update_manager)
        {
            const bool commit_phase =
                get_manager().action == d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_COMMIT;
            if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS ||
                update_state == RS2_CALIB_STATE_COMMIT_IN_PROGRESS)
            {
                if (update_manager->done())
                {
                    // D5x5 interactive: the first RUN phase completes at HEALTH_CHECK, not COMPLETE — the user has yet to
                    // approve. The COMMIT phase, in contrast, ends at COMPLETE.
                    const bool interactive_first_phase = get_manager().uses_interactive_triggered_calibration() && ! commit_phase;
                    if (interactive_first_phase && update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
                    {
                        update_state = RS2_CALIB_STATE_HEALTH_CHECK;
                    }
                    else
                    {
                        update_state = RS2_CALIB_STATE_COMPLETE;
                    }
                    enable_dismiss = true;
                }
                else if (update_manager->failed())
                {
                    update_state = RS2_CALIB_STATE_FAILED;
                    enable_dismiss = true;
                }

                if (!expanded)
                {
                    if (update_manager->failed())
                    {
                        update_manager->check_error(_error_message);
                        update_state = RS2_CALIB_STATE_FAILED;
                        enable_dismiss = true;
                    }

                    draw_progress_bar(win, bar_width);
                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    int d500_autocalib_notification_model::calc_height()
    {
        // Restore default width for every non-health state; the health check row needs extra room for four buttons.
        width = (update_state == RS2_CALIB_STATE_HEALTH_CHECK) ? 460 : 320;

        // adjusting the height of the notification window
        if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS ||
            update_state == RS2_CALIB_STATE_COMPLETE ||
            update_state == RS2_CALIB_STATE_ABORT_CALLED ||
            update_state == RS2_CALIB_STATE_FAILED ||
            update_state == RS2_CALIB_STATE_COMMIT_IN_PROGRESS)
            return 90;
        if (update_state == RS2_CALIB_STATE_HEALTH_CHECK)
            return 110;  // two text lines + button row
        return 60;
    }


    void d500_autocalib_notification_model::calibration_button(ux_window& win, int x, int y, int bar_width)
    {
        using namespace std;
        using namespace chrono;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - ImGui::GetTextLineHeightWithSpacing() - 31) });

        auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

        std::string activation_cal_str = "Calibrate";
        if (update_state == RS2_CALIB_STATE_INIT_DRY_RUN)
            activation_cal_str = "Calibrate Dry Run";

        std::string calibrate_button_name = rsutils::string::from() << activation_cal_str << "##self" << index;

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 28) });
        if (ImGui::Button(calibrate_button_name.c_str(), { float(bar_width), 20.f }))
        {
            get_manager().reset();
            if (update_state == RS2_CALIB_STATE_INIT_DRY_RUN)
            {
                get_manager().action = d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_DRY_RUN;
            }

            get_manager().prepare_for_calibration();

            auto _this = shared_from_this();
            auto invoke = [_this](std::function<void()> action) {_this->invoke(action); };
            get_manager().start(invoke);
            update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
            enable_dismiss = false;
        }
        ImGui::PopStyleColor(2);
    }

    void d500_autocalib_notification_model::draw_abort(ux_window& win, int x, int y)
    {
        ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

        std::string id = rsutils::string::from() << "Abort" << "##" << index;


        ImGui::SetNextWindowPos({ float(x + width - 125), float(y + height - 25) });
        ImGui::SetNextWindowSize({ 120, 70 });

        if (ImGui::Button(id.c_str(), { 100, 20 }))
        {
            update_state = RS2_CALIB_STATE_ABORT;
        }
    }

    void d500_autocalib_notification_model::update_ui_after_abort_called(ux_window& win, int x, int y)
    {
        ImGui::SetCursorScreenPos({ float(x + 10), float(y) });
        ImGui::Text("%s", "Calibration Aborting");
        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 40) });
        ImGui::PushFont(win.get_large_font());
        std::string txt = rsutils::string::from() << textual_icons::stop;
        ImGui::Text("%s", txt.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos({ float(x + 40), float(y + 40) });
        if (_has_abort_succeeded)
        {
            ImGui::Text("%s", "Camera Calibration Aborted Successfully");
        }
        else
        {
            ImGui::Text("%s", "Camera Calibration Could not be Aborted");
        }
        enable_dismiss = true;
    }
    
    void d500_autocalib_notification_model::update_ui_on_failure(ux_window& win, int x, int y)
    {
        ImGui::SetCursorScreenPos({ float(x + 50), float(y + 50) });
        ImGui::Text("%s", "Calibration Failed");
        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 50) });
        ImGui::PushFont(win.get_large_font());
        std::string txt = rsutils::string::from() << textual_icons::exclamation_triangle;
        ImGui::Text("%s", txt.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos({ float(x + 40), float(y + 50) });
        
        enable_dismiss = true;
    }

    void d500_autocalib_notification_model::start_action_phase(d500_on_chip_calib_manager::calib_action a)
    {
        get_manager().reset();
        get_manager().action = a;
        auto _this = shared_from_this();
        auto invoke = [_this](std::function<void()> action) {_this->invoke(action); };
        get_manager().start(invoke);
        if (a == d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_COMMIT)
            update_state = RS2_CALIB_STATE_COMMIT_IN_PROGRESS;
        // TRY_NEW / TRY_OLD stay in RS2_CALIB_STATE_HEALTH_CHECK; ABORT (Discard) also stays until confirmed.
        enable_dismiss = false;
    }

    void d500_autocalib_notification_model::draw_health_check(ux_window& win, int x, int y, int bar_width)
    {
        using namespace std::chrono;

        // The base Dismiss button is not needed on this screen — the four action buttons (Commit/Discard/Try…) are terminal.
        enable_dismiss = false;

        const float h = get_manager().get_scalar_health();
        const bool passes = get_manager().health_passes();
        // A previous TRY / COMMIT / ABORT click may still be in flight on the manager's background thread — every
        // button on this row spawns a new process_flow, so a second click before the first phase finishes would race
        // on _done / _scalar_health and send two overlapping SET_CALIB_MODE commands. Dim + swallow while running.
        const bool in_flight = update_manager
                            && update_manager->started()
                            && !update_manager->done()
                            && !update_manager->failed();
        const bool commit_enabled = passes && !in_flight;

        // Settle a pending TRY: if the phase finished, either roll _try_side back (on failure) or clear the flag
        // (on success). Without this, RadioButton's inline write leaves the UI asserting a candidate is active
        // while FW never actually switched — same UI-lies-about-FW-state hazard as the in-flight race, reached
        // via the failure path.
        if (_pending_try_revert_to != -1 && update_manager)
        {
            if (update_manager->failed())
            {
                _try_side = _pending_try_revert_to;
                _pending_try_revert_to = -1;
            }
            else if (update_manager->done())
            {
                _pending_try_revert_to = -1;
            }
        }

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });
        ImGui::Text("%s", passes ? "Health check: PASS" : "Health check: FAIL");

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 45) });
        if (h < 0.f) ImGui::Text("Rect health: n/a");
        else         ImGui::Text("Rect health: %.3f px  (threshold %.3f)", h,
                                 d500_on_chip_calib_manager::k_rect_health_pass_threshold_px);

        // Row: [radio Try New] [radio Try Old] [Commit] [Discard]. Ignore the caller's bar_width — it reserves a 115px
        // right gutter for the base Dismiss button, which we've hidden above; radio buttons + fixed-width buttons fit
        // in the popup natively (ImGui::SameLine + ImGui default spacing).
        (void)bar_width;
        const float btn_y = float(y + height - 28);
        const float btn_w = 100.f;   // Commit/Discard fixed; radio buttons occupy the remaining space

        std::string try_new_id  = rsutils::string::from() << "Try New##"  << index;
        std::string try_old_id  = rsutils::string::from() << "Try Old##"  << index;
        std::string commit_id   = rsutils::string::from() << "Commit##"   << index;
        std::string discard_id  = rsutils::string::from() << "Discard##"  << index;

        // The notification base pushes a near-transparent ImGuiCol_Button (see notification_model::set_color_scheme),
        // so buttons on this row need their own scheme to read as clickable — matches calibration_button() and the
        // rest of on-chip-calib. Radio buttons use their own ImGui colors so no push is needed for them.
        const auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
        const float active_sat = in_flight ? 0.4f : sat;
        const float hover_sat  = in_flight ? 0.4f : 1.5f;

        // Radio pair — user selects which candidate is live. Each toggle fires the matching TRY action so FW switches
        // the RAM-active depth table; the currently-selected radio then shows which table is being previewed.
        // BeginDisabled blocks both the click AND the widget's internal _try_side write while a phase is in flight.
        // The action is deferred until AFTER EndDisabled so a throw from start_action_phase() (thread ctor,
        // allocation) cannot leak the disabled stack across frames.
        // Capture _try_side BEFORE the RadioButton widgets get a chance to overwrite it — ImGui returns pressed=true
        // on any click, including on the already-selected radio, so `1 - _try_side` (the previous version's guess)
        // is only correct when the click actually changes selection. A re-click on the same radio would otherwise
        // roll the UI to the opposite side on failure — precisely the inverse of what we want.
        const int try_side_before_click = _try_side;
        ImGui::SetCursorScreenPos({ float(x + 5), btn_y });
        ImGui::BeginDisabled(in_flight);
        const bool try_new_clicked = ImGui::RadioButton(try_new_id.c_str(), &_try_side, 0);
        ImGui::SameLine();
        const bool try_old_clicked = ImGui::RadioButton(try_old_id.c_str(), &_try_side, 1);
        ImGui::EndDisabled();
        if (try_new_clicked)
        {
            _pending_try_revert_to = try_side_before_click;
            start_action_phase(d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_NEW);
        }
        else if (try_old_clicked)
        {
            _pending_try_revert_to = try_side_before_click;
            start_action_phase(d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_OLD);
        }

        ImGui::SameLine();
        // Commit is health-gated AND in-flight-gated: dim on either condition; swallow the click accordingly.
        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, commit_enabled ? sat : 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, commit_enabled ? 1.5f : 0.4f));
        if (ImGui::Button(commit_id.c_str(), { btn_w, 20.f }) && commit_enabled)
            start_action_phase(d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_COMMIT);
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, active_sat));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, hover_sat));
        if (ImGui::Button(discard_id.c_str(), { btn_w, 20.f }) && !in_flight)
        {
            // Fire the ABORT command in the background (manager restores the workspace on completion) and
            // dismiss the popup immediately — the user has already made their decision, no further UI is needed.
            start_action_phase(d500_on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB_ABORT);
            dismissed = true;
        }
        ImGui::PopStyleColor(2);
    }

    void d500_autocalib_notification_model::update_ui_on_calibration_complete(ux_window& win, int x, int y)
    {
        ImGui::Text("%s", "Calibration Complete");

        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
        ImGui::PushFont(win.get_large_font());
        std::string txt = rsutils::string::from() << textual_icons::trophy;
        ImGui::Text("%s", txt.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });

        ImGui::Text("%s", "Camera Calibration Applied Successfully");
    }
}
