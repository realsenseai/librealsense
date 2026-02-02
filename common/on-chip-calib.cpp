// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <glad/glad.h>
#include "on-chip-calib.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <model-views.h>
#include <realsense_imgui.h>
#include <viewer.h>
#include "calibration-model.h"
#include "os.h"
#include "../src/algo.h"
#include "../tools/depth-quality/depth-metrics.h"


static constexpr float off_value = 0.0f;
static constexpr float on_value = 1.0f;

namespace rs2
{
    on_chip_calib_manager::on_chip_calib_manager(viewer_model& viewer, std::shared_ptr<subdevice_model> sub, device_model& model, device dev, std::shared_ptr<subdevice_model> sub_color, bool uvmapping_calib_full)
        : process_manager("On-Chip Calibration"), _model(model), _dev(dev), _sub(sub), _viewer(viewer), _sub_color(sub_color), py_px_only(!uvmapping_calib_full)
    {
        device_name_string = "Unknown";
        if (dev.supports(RS2_CAMERA_INFO_NAME))
        {
            device_name_string = _dev.get_info( RS2_CAMERA_INFO_NAME );
        }

        if( dev.supports( RS2_CAMERA_INFO_CONNECTION_TYPE ) )
        {
            auto con_type = std::string( dev.get_info( RS2_CAMERA_INFO_CONNECTION_TYPE ) );
            if( con_type == "GMSL" )
                host_assistance = 1;  // To be used for MIPI SKU only
        }

        if (dev.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION))
        {
            std::string fw_version = dev.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION);
            tare_health = is_upgradeable("05.12.14.100", fw_version);
        }
    }

    on_chip_calib_manager::~on_chip_calib_manager()
    {
        turn_roi_off();
    }

    void on_chip_calib_manager::turn_roi_on()
    {
        if (_sub)
        {
            _sub->show_algo_roi = true;
            _sub->algo_roi = { librealsense::_roi_ws, librealsense::_roi_hs, librealsense::_roi_we, librealsense::_roi_he };
        }

        if (_sub_color)
        {
            _sub_color->show_algo_roi = true;
            _sub_color->algo_roi = { librealsense::_roi_ws, librealsense::_roi_hs, librealsense::_roi_we, librealsense::_roi_he };
        }
    }

    void on_chip_calib_manager::turn_roi_off()
    {
        if (_sub)
        {
            _sub->show_algo_roi = false;
            _sub->algo_roi = { 0, 0, 0, 0 };
        }

        if (_sub_color)
        {
            _sub_color->show_algo_roi = false;
            _sub_color->algo_roi = { 0, 0, 0, 0 };
        }
    }
    
    void on_chip_calib_manager::save_options_controlled_by_calib()
    {
        // Calibration might fail and be restarted, save only once, or we will keep chaged options.
        if( ! _options_saved )
        {
            save_laser_emitter_state();
            save_thermal_loop_state();
            _options_saved = true;
        }
    }

    void on_chip_calib_manager::restore_options_controlled_by_calib()
    {
        // Restore might be called several times, restore only if options where actually saved.
        if( _options_saved )
        {
            restore_laser_emitter_state();
            restore_thermal_loop_state();
            _options_saved = false;
        }
    }

    void on_chip_calib_manager::save_laser_emitter_state()
    {
        auto it = _sub->options_metadata.find( RS2_OPTION_EMITTER_ENABLED );
        if ( it != _sub->options_metadata.end() ) //Option supported
        {
            _laser_status_prev = _sub->s->get_option( RS2_OPTION_EMITTER_ENABLED );
        }
    }

    void on_chip_calib_manager::save_thermal_loop_state()
    {
        auto it = _sub->options_metadata.find( RS2_OPTION_THERMAL_COMPENSATION );
        if( it != _sub->options_metadata.end() )  // Option supported
        {
            _thermal_loop_prev = _sub->s->get_option( RS2_OPTION_THERMAL_COMPENSATION );
        }
    }

    void on_chip_calib_manager::restore_laser_emitter_state()
    {
        set_laser_emitter_state( _laser_status_prev );
    }

    void on_chip_calib_manager::restore_thermal_loop_state()
    {
        set_thermal_loop_state( _thermal_loop_prev );
    }

    void on_chip_calib_manager::set_laser_emitter_state( float value )
    {
        // Use options_model::set_option to update GUI after change
        std::string ignored_error_message{ "" };
        auto it = _sub->options_metadata.find( RS2_OPTION_EMITTER_ENABLED );
        if( it != _sub->options_metadata.end() )  // Option supported
        {
            it->second.set_option( RS2_OPTION_EMITTER_ENABLED, value, ignored_error_message );
            if( it->second.value_as_float() != value )
                throw std::runtime_error( rsutils::string::from()
                                          << "Failed to set laser " << ( value == off_value ? "off" : "on" ) );
        }
    }

    void on_chip_calib_manager::set_thermal_loop_state( float value )
    {
        // Use options_model::set_option to update GUI after change
        std::string ignored_error_message{ "" };
        auto it = _sub->options_metadata.find( RS2_OPTION_THERMAL_COMPENSATION );
        if( it != _sub->options_metadata.end() )  // Option supported
        {
            it->second.set_option( RS2_OPTION_THERMAL_COMPENSATION, value, ignored_error_message );
            if( it->second.value_as_float() != value )
                throw std::runtime_error( rsutils::string::from()
                                          << "Failed to set thermal compensation " << ( value == off_value ? "off" : "on" ) );
        }
    }

    void on_chip_calib_manager::stop_viewer(invoker invoke)
    {
        try
        {
            auto profiles = _sub->get_selected_profiles();

            invoke([&]()
                {
                    // Stop viewer UI
                    if (_sub->streaming)
                        _sub->stop(_viewer.not_model);
                    if (_sub_color.get() && _sub_color->streaming)
                        _sub_color->stop(_viewer.not_model);
                });

            // Wait until frames from all active profiles stop arriving
            bool frame_arrived = false;
            while (frame_arrived && _viewer.streams.size())
            {
                for (auto&& stream : _viewer.streams)
                {
                    if (std::find(profiles.begin(), profiles.end(),
                        stream.second.original_profile) != profiles.end())
                    {
                        auto now = std::chrono::high_resolution_clock::now();
                        if (now - stream.second.last_frame > std::chrono::milliseconds(200))
                            frame_arrived = false;
                    }
                    else frame_arrived = false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (...) {}
    }

    rs2::depth_frame on_chip_calib_manager::fetch_depth_frame(invoker invoke, int timeout_ms)
    {
        auto profiles = _sub->get_selected_profiles();
        bool frame_arrived = false;
        rs2::depth_frame res = rs2::frame{};
        auto start_time = std::chrono::high_resolution_clock::now();
        while (!frame_arrived)
        {
            for (auto&& stream : _viewer.streams)
            {
                if (std::find(profiles.begin(), profiles.end(),
                    stream.second.original_profile) != profiles.end())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now - start_time > std::chrono::milliseconds(timeout_ms))
                        throw std::runtime_error( rsutils::string::from() << "Failed to fetch depth frame within " << timeout_ms << " ms");

                    if (now - stream.second.last_frame < std::chrono::milliseconds(100))
                    {
                        if (auto f = stream.second.texture->get_last_frame(false).as<rs2::depth_frame>())
                        {
                            frame_arrived = true;
                            res = f;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return res;
    }

    void on_chip_calib_manager::stop_viewer()
    {
        try
        {
            auto profiles = _sub->get_selected_profiles();
            if (_sub->streaming)
                _sub->stop(_viewer.not_model);
            if (_sub_color.get() && _sub_color->streaming)
                _sub_color->stop(_viewer.not_model);

            // Wait until frames from all active profiles stop arriving
            bool frame_arrived = false;
            while (frame_arrived && _viewer.streams.size())
            {
                for (auto&& stream : _viewer.streams)
                {
                    if (std::find(profiles.begin(), profiles.end(),
                        stream.second.original_profile) != profiles.end())
                    {
                        auto now = std::chrono::high_resolution_clock::now();
                        if (now - stream.second.last_frame > std::chrono::milliseconds(200))
                            frame_arrived = false;
                    }
                    else frame_arrived = false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (...) {}
    }

    void on_chip_calib_manager::start_gt_viewer()
    {
        try
        {
            stop_viewer();
            _viewer.is_3d_view = false;

            _uid = 1;
            for (const auto& format : _sub->formats)
            {
                if (format.second[0] == Y8_FORMAT)
                {
                    _uid = format.first;
                    break;
                }
            }

            // Select stream
            _sub->stream_enabled.clear();
            _sub->stream_enabled[_uid] = true;

            _sub->ui.selected_format_id.clear();
            _sub->ui.selected_format_id[_uid] = 0;

            // Select FPS value
            auto fps = 30;
            if (_sub->dev.supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
            {
                std::string desc = _sub->dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
                if (!starts_with(desc, "3."))
                    fps = 6; //USB2 bandwidth limitation for 720P
            }
            _sub->ui.selected_shared_fps_id = 0; // If requested FPS is not found use lowest common FPS for USB2 compatibility (sorted lowest first)
            for (int i = 0; i < _sub->shared_fps_values.size(); i++)
            {
                if (_sub->shared_fps_values[i] == fps)
                    _sub->ui.selected_shared_fps_id = i;
            }

            // Select Resolution
            _sub->select_resolution( 1280, 720, RS2_STREAM_INFRARED );

            auto profiles = _sub->get_selected_profiles();

            if (!_model.dev_syncer)
                _model.dev_syncer = _viewer.syncer->create_syncer();

            _sub->play(profiles, _viewer, _model.dev_syncer);
            for (auto&& profile : profiles)
                _viewer.begin_stream(_sub, profile);

            set_laser_emitter_state( off_value );
        }
        catch (...) {}
    }

    void on_chip_calib_manager::start_fl_viewer()
    {
        try
        {
            stop_viewer();
            _viewer.is_3d_view = false;

            _uid = 1;
            _uid2 = 2;
            bool first_done = 0;
            for (const auto& format : _sub->formats)
            {
                if (format.second[0] == Y8_FORMAT)
                {
                    if (!first_done)
                    {
                        _uid = format.first;
                        first_done = true;
                    }
                    else
                    {
                        _uid2 = format.first;
                        break;
                    }
                }
            }

            // Select stream
            _sub->stream_enabled.clear();
            _sub->stream_enabled[_uid] = true;
            _sub->stream_enabled[_uid2] = true;

            _sub->ui.selected_format_id.clear();
            _sub->ui.selected_format_id[_uid] = 0;
            _sub->ui.selected_format_id[_uid2] = 0;

            // Select FPS value
            for (int i = 0; i < _sub->shared_fps_values.size(); i++)
            {
                if (val_in_range(_sub->shared_fps_values[i], {5,6}))
                    _sub->ui.selected_shared_fps_id = i;
            }

            // Select Resolution
            _sub->select_resolution( 1280, 720, RS2_STREAM_INFRARED );

            auto profiles = _sub->get_selected_profiles();

            if (!_model.dev_syncer)
                _model.dev_syncer = _viewer.syncer->create_syncer();

            _sub->play(profiles, _viewer, _model.dev_syncer);
            for (auto&& profile : profiles)
                _viewer.begin_stream(_sub, profile);

            set_laser_emitter_state( off_value );
        }
        catch (...) {}
    }

    void on_chip_calib_manager::start_uvmapping_viewer(bool b3D)
    {
        for (int i = 0; i < 2; ++i)
        {
            try
            {
                stop_viewer();
                _viewer.is_3d_view = b3D;

                _uid = 1;
                _uid2 = 2;
                bool first_done = 0;
                bool second_done = 0;
                for (const auto& format : _sub->formats)
                {
                    if (format.second[0] == Y8_FORMAT && !first_done)
                    {
                        _uid = format.first;
                        first_done = true;
                    }

                    if (format.second[0] == Z16_FORMAT && !second_done)
                    {
                        _uid2 = format.first;
                        second_done = true;
                    }

                    if (first_done && second_done)
                        break;
                }

                _sub_color->ui.selected_format_id.clear();
                _sub_color->ui.selected_format_id[_uid_color] = 0;
                for (const auto& format : _sub_color->formats)
                {
                    int done = false;
                    for (int i = 0; i < int(format.second.size()); ++i)
                    {
                        if (format.second[i] == RGB8_FORMAT)
                        {
                            _uid_color = format.first;
                            _sub_color->ui.selected_format_id[_uid_color] = i;
                            done = true;
                            break;
                        }
                    }
                    if (done)
                        break;
                }

                // Select stream
                _sub->stream_enabled.clear();
                _sub->stream_enabled[_uid] = true;
                _sub->stream_enabled[_uid2] = true;

                _sub->ui.selected_format_id.clear();
                _sub->ui.selected_format_id[_uid] = 0;
                _sub->ui.selected_format_id[_uid2] = 0;

                // Select FPS value
                for (int i = 0; i < int(_sub->shared_fps_values.size()); i++)
                {
                    if (_sub->shared_fps_values[i] == 30)
                        _sub->ui.selected_shared_fps_id = i;
                }

                // Select Resolution
                _sub->select_resolution( 1280, 720, RS2_STREAM_INFRARED );
                _sub->select_resolution( 1280, 720, RS2_STREAM_DEPTH );

                auto profiles = _sub->get_selected_profiles();

                std::vector<stream_profile> profiles_color;
                _sub_color->stream_enabled[_uid_color] = true;

                for (int i = 0; i < _sub_color->shared_fps_values.size(); i++)
                {
                    if (_sub_color->shared_fps_values[i] == 30)
                        _sub_color->ui.selected_shared_fps_id = i;
                }

                _sub_color->select_resolution( 1280, 720, RS2_STREAM_COLOR );

                profiles_color = _sub_color->get_selected_profiles();

                if (!_model.dev_syncer)
                    _model.dev_syncer = _viewer.syncer->create_syncer();

                _sub->play(profiles, _viewer, _model.dev_syncer);
                for (auto&& profile : profiles)
                    _viewer.begin_stream(_sub, profile);

                _sub_color->play(profiles_color, _viewer, _model.dev_syncer);
                for (auto&& profile : profiles_color)
                    _viewer.begin_stream(_sub_color, profile);
            }
            catch (...) {}
        }
    }

    bool on_chip_calib_manager::start_viewer(int w, int h, int fps, invoker invoke)
    {
        bool frame_arrived = false;
        try
        {
            bool run_fl_calib = ( (action == RS2_CALIB_ACTION_FL_CALIB) && (w == 1280) && (h == 720));
            if (action == RS2_CALIB_ACTION_TARE_GROUND_TRUTH)
            {
                _uid = 1;
                for (const auto& format : _sub->formats)
                {
                    if (format.second[0] == Y8_FORMAT)
                    {
                        _uid = format.first;
                        break;
                    }
                }
                _sub->select_resolution( w, h, RS2_STREAM_INFRARED );
            }
            else if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
            {
                _uid = 1;
                _uid2 = 0;
                bool first_done = false;
                bool second_done = false;
                for (const auto& format : _sub->formats)
                {
                    if (format.second[0] == Y8_FORMAT && !first_done)
                    {
                        _uid = format.first;
                        first_done = true;
                    }

                    if (format.second[0] == Z16_FORMAT && !second_done)
                    {
                        _uid2 = format.first;
                        second_done = true;
                    }

                    if (first_done && second_done)
                        break;
                }
                _sub->select_resolution( w, h, RS2_STREAM_INFRARED );
                _sub->select_resolution( w, h, RS2_STREAM_DEPTH);

                _sub_color->ui.selected_format_id.clear();
                _sub_color->ui.selected_format_id[_uid_color] = 0;
                for (const auto& format : _sub_color->formats)
                {
                    int done = false;
                    for (int i = 0; i < format.second.size(); ++i)
                    {
                        if (format.second[i] == RGB8_FORMAT)
                        {
                            _uid_color = format.first;
                            _sub_color->ui.selected_format_id[_uid_color] = i;
                            done = true;
                            break;
                        }
                    }
                    if (done)
                        break;
                }

                // TODO - When implementing UV mapping calibration - should remove from here and handle in process_flow()
                set_laser_emitter_state( off_value );
                set_thermal_loop_state( off_value );
            }
            else if (action == RS2_CALIB_ACTION_FL_PLUS_CALIB)
            {
                _uid = 1;
                _uid2 = 0;
                _uid_color = 2;
                bool first_done = false;
                bool second_done = false;
                bool third_done = false;
                for (const auto& format : _sub->formats)
                {
                    if (format.second[0] == "Y8")
                    {
                        if (!first_done)
                        {
                            _uid = format.first;
                            first_done = true;
                        }
                        else
                        {
                            _uid_color = format.first;
                            third_done = true;
                        }
                    }

                    if (format.second[0] == "Z16" && !second_done)
                    {
                        _uid2 = format.first;
                        second_done = true;
                    }

                    if (first_done && second_done && third_done)
                        break;
                }

                _sub->select_resolution( w, h, RS2_STREAM_INFRARED );
                _sub->select_resolution( w, h, RS2_STREAM_DEPTH );

                // TODO - When implementing FL plus calibration - should remove from here and handle in process_flow()
                set_laser_emitter_state( off_value );
            }
            else if (run_fl_calib)
            {
                _uid = 1;
                _uid2 = 2;
                bool first_done = false;
                for (const auto& format : _sub->formats)
                {
                    if (format.second[0] == Y8_FORMAT)
                    {
                        if (!first_done)
                        {
                            _uid = format.first;
                            first_done = true;
                        }
                        else
                        {
                            _uid2 = format.first;
                            break;
                        }
                    }
                }
                _sub->select_resolution( w, h, RS2_STREAM_INFRARED );
            }
            else
            {
                _uid = 0;
                for (const auto& format : _sub->formats)
                {
                    if (format.second[0] == Z16_FORMAT)
                    {
                        _uid = format.first;
                        break;
                    }
                }
                _sub->select_resolution( w, h, RS2_STREAM_DEPTH );
            }

            // Select stream
            _sub->stream_enabled.clear();
            _sub->stream_enabled[_uid] = true;
            if (run_fl_calib || action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
                _sub->stream_enabled[_uid2] = true;

            _sub->ui.selected_format_id.clear();
            _sub->ui.selected_format_id[_uid] = 0;
            if (run_fl_calib || action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
                _sub->ui.selected_format_id[_uid2] = 0;

            // Select FPS value
            for (int i = 0; i < _sub->shared_fps_values.size(); i++)
            {
                if (_sub->shared_fps_values[i] == fps)
                    _sub->ui.selected_shared_fps_id = i;
            }

            if (!_sub->is_selected_combination_supported())
            {
                return false;
            }

            auto profiles = _sub->get_selected_profiles();

            std::vector<stream_profile> profiles_color;
            if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
            {
                _sub_color->stream_enabled[_uid_color] = true;

                for (int i = 0; i < _sub_color->shared_fps_values.size(); i++)
                {
                    if (_sub_color->shared_fps_values[i] == fps)
                        _sub_color->ui.selected_shared_fps_id = i;
                }

                _sub_color->select_resolution( w, h, RS2_STREAM_COLOR );

                profiles_color = _sub_color->get_selected_profiles();
            }

            invoke([&]()
                {
                    if (!_model.dev_syncer)
                        _model.dev_syncer = _viewer.syncer->create_syncer();

                    // Start streaming
                    _sub->play(profiles, _viewer, _model.dev_syncer);
                    for (auto&& profile : profiles)
                        _viewer.begin_stream(_sub, profile);

                    if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
                    {
                        _sub_color->play(profiles_color, _viewer, _model.dev_syncer);
                        for (auto&& profile : profiles_color)
                            _viewer.begin_stream(_sub_color, profile);
                    }
                });

            // Wait for frames to arrive
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

    void on_chip_calib_manager::try_start_viewer(int w, int h, int fps, invoker invoke)
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
            log( "Failed to start streaming" );
            throw std::runtime_error( rsutils::string::from() << "Failed to start streaming (" << w << ", " << h << ", " << fps << ")!");
        }
    }

    std::vector<uint8_t> on_chip_calib_manager::safe_send_command(const std::vector<uint8_t>& cmd, const std::string& name)
    {
        auto dp = _dev.as<debug_protocol>();
        if (!dp) throw std::runtime_error("Device does not support debug protocol!");

        auto res = dp.send_and_receive_raw_data(cmd);

        if( res.size() < sizeof( int32_t ) )
            throw std::runtime_error( rsutils::string::from() << "Not enough data from " << name << "!" );
        auto return_code = *((int32_t*)res.data());
        if( return_code < 0 )
            throw std::runtime_error( rsutils::string::from()
                                      << "Firmware error (" << return_code << ") from " << name << "!" );

        return res;
    }

    void on_chip_calib_manager::update_last_used()
    {
        time_t rawtime;
        time(&rawtime);
        std::string id = rsutils::string::from() << configurations::viewer::last_calib_notice << "."
                                                 << _sub->s->get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );
        config_file::instance().set(id.c_str(), (long long)rawtime);
    }

    void on_chip_calib_manager::calibrate()
    {
        // High exposure values might limit high FPS calibrations, issue warning to the user
        auto auto_exposure = _sub->s->supports( RS2_OPTION_ENABLE_AUTO_EXPOSURE ) && _sub->s->get_option( RS2_OPTION_ENABLE_AUTO_EXPOSURE );
        auto exposure = _sub->s->get_option( RS2_OPTION_EXPOSURE ); // Currently all camera models support exposure option (D400 and D500)
        if( !auto_exposure && exposure > 15500 )
        {
            throw std::runtime_error( "Exposure value is limiting the fps,\n"
                                      "for the algorithm to be able to converge,\n"
                                      "please reduce exposure value." ); 
        }

        int occ_timeout_ms = 9000;
        if (action == RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || action == RS2_CALIB_ACTION_ON_CHIP_FL_CALIB)
        {
            if (toggle)
            {
                occ_timeout_ms = 12000;
                if (speed_fl == 0)
                    speed_fl = 1;
                else if (speed_fl == 1)
                    speed_fl = 0;
                toggle = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }

            if (speed_fl == 0)
            {
                speed = 1;
                fl_step_count = 41;
                fy_scan_range = 30;
                white_wall_mode = 0;
            }
            else if (speed_fl == 1)
            {
                speed = 3;
                fl_step_count = 51;
                fy_scan_range = 40;
                white_wall_mode = 0;
            }
            else if (speed_fl == 2)
            {
                speed = 4;
                fl_step_count = 41;
                fy_scan_range = 30;
                white_wall_mode = 1;
            }
        }

        std::stringstream ss;
        if (action == RS2_CALIB_ACTION_ON_CHIP_CALIB)
        {
            ss << "{\n \"calib type\":" << 0 <<
                  ",\n \"host assistance\":" << host_assistance <<
                  ",\n \"speed\":" << speed <<
                  ",\n \"average step count\":" << average_step_count <<
                  ",\n \"scan parameter\":" << (intrinsic_scan ? 0 : 1) <<
                  ",\n \"step count\":" << step_count <<
                  ",\n \"apply preset\":" << (apply_preset ? 1 : 0) <<
                  ",\n \"accuracy\":" << accuracy <<
                  ",\n \"scan only\":" << (host_assistance ? 1 : 0) <<
                  ",\n \"interactive scan\":" << 0 << "}";
        }
        else if (action == RS2_CALIB_ACTION_ON_CHIP_FL_CALIB)
        {
            ss << "{\n \"calib type\":" << 1 <<
                  ",\n \"host assistance\":" << host_assistance <<
                  ",\n \"fl step count\":" << fl_step_count <<
                  ",\n \"fy scan range\":" << fy_scan_range <<
                  ",\n \"keep new value after sucessful scan\":" << keep_new_value_after_sucessful_scan <<
                  ",\n \"fl data sampling\":" << fl_data_sampling <<
                  ",\n \"adjust both sides\":" << adjust_both_sides <<
                  ",\n \"fl scan location\":" << fl_scan_location <<
                  ",\n \"fy scan direction\":" << fy_scan_direction <<
                  ",\n \"white wall mode\":" << white_wall_mode <<
                  ",\n \"scan only\":" << (host_assistance ? 1 : 0) <<
                  ",\n \"interactive scan\":" << 0 << "}";
        }
        else
        {
            ss << "{\n \"calib type\":" << 2 <<
                  ",\n \"host assistance\":" << host_assistance <<
                  ",\n \"fl step count\":" << fl_step_count <<
                  ",\n \"fy scan range\":" << fy_scan_range <<
                  ",\n \"keep new value after sucessful scan\":" << keep_new_value_after_sucessful_scan <<
                  ",\n \"fl data sampling\":" << fl_data_sampling <<
                  ",\n \"adjust both sides\":" << adjust_both_sides <<
                  ",\n \"fl scan location\":" << fl_scan_location <<
                  ",\n \"fy scan direction\":" << fy_scan_direction <<
                  ",\n \"white wall mode\":" << white_wall_mode <<
                  ",\n \"speed\":" << speed <<
                  ",\n \"average step count\":" << average_step_count <<
                  ",\n \"scan parameter\":" << (intrinsic_scan ? 0 : 1) <<
                  ",\n \"step count\":" << step_count <<
                  ",\n \"apply preset\":" << (apply_preset ? 1 : 0) <<
                  ",\n \"accuracy\":" << accuracy <<
                  ",\n \"scan only\":" << (host_assistance ? 1 : 0) <<
                  ",\n \"interactive scan\":" << 0 <<
                  ",\n \"depth\":" << 0 << "}";
        }
        std::string json = ss.str();

        float health[2] = { -1.0f, -1.0f };
        auto calib_dev = _dev.as<auto_calibrated_device>();
        if (action == RS2_CALIB_ACTION_TARE_CALIB)
            _new_calib = calib_dev.run_tare_calibration(ground_truth, json, health, [&](const float progress) {_progress = progress; }, 5000);
        else if (action == RS2_CALIB_ACTION_ON_CHIP_CALIB || action == RS2_CALIB_ACTION_ON_CHIP_FL_CALIB || action == RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
            _new_calib = calib_dev.run_on_chip_calibration(json, health, [&](const float progress) {_progress = progress; }, occ_timeout_ms);

        auto invoke = [](std::function<void()>) {};
        int frame_fetch_timeout_ms = 3000;

        bool calib_done(!_new_calib.empty());

        int timeout_sec(10);
        timeout_sec *= (1 + static_cast<int>(action == RS2_CALIB_ACTION_ON_CHIP_CALIB)); // when RS2_CALIB_ACTION_ON_CHIP_CALIB is in interactive-mode the process takes longer.
        auto start = std::chrono::high_resolution_clock::now();
        bool is_timed_out(std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout_sec));

        while (!(calib_done || is_timed_out))
        {
            rs2::depth_frame f = fetch_depth_frame(invoke, frame_fetch_timeout_ms);
            _new_calib = calib_dev.process_calibration_frame(f, health, [&](const float progress) {_progress = progress; }, 5000);
            calib_done = !_new_calib.empty();
            is_timed_out = std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(timeout_sec);
        }
        if (is_timed_out)
        {
            throw std::runtime_error("Operation timed-out!\n"
                "Calibration process did not converge on time");
        }

        // finalize results:
        if (action == RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
        {
            int h_both = static_cast<int>(_health);
            int h_1 = (h_both & 0x00000FFF);
            int h_2 = (h_both & 0x00FFF000) >> 12;
            int sign = (h_both & 0x0F000000) >> 24;

            _health_1 = h_1 / 1000.0f;
            if (sign & 1)
                _health_1 = -_health_1;

            _health_2 = h_2 / 1000.0f;
            if (sign & 2)
                _health_2 = -_health_2;
        }
        else if (action == RS2_CALIB_ACTION_ON_CHIP_CALIB)
        {
            _health = health[0];
        }
        else if (action == RS2_CALIB_ACTION_TARE_CALIB)
        {
            _health_1 = health[0] * 100;
            _health_2 = health[1] * 100;
        }
    }

    void on_chip_calib_manager::calibrate_fl()
    {
        try
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // W/A that allows for USB2 exposure to settle
            constexpr int frames_required = 25;

            rs2::frame_queue left(frames_required,true);
            rs2::frame_queue right(frames_required, true);

            int counter = 0;

            float step = 50.f / frames_required; // The first stage represents 50% of the calibration process

            // Stage 1 : Gather frames from Left/Right IR sensors
            while (counter < frames_required) // TODO timeout
            {
                auto fl = _viewer.ppf.frames_queue[_uid].wait_for_frame();    // left intensity
                auto fr = _viewer.ppf.frames_queue[_uid2].wait_for_frame();   // right intensity
                if (fl && fr)
                {
                    left.enqueue(fl);
                    right.enqueue(fr);
                    _progress += step;
                    counter++;
                }
            }

            if (counter >= frames_required)
            {
                // Stage 2 : Perform focal length calibration correction routine
                auto calib_dev = _dev.as<auto_calibrated_device>();
                _new_calib = calib_dev.run_focal_length_calibration(left, right,
                                                          config_file::instance().get_or_default(configurations::viewer::target_width_r, 175.0f),
                                                          config_file::instance().get_or_default(configurations::viewer::target_height_r, 100.0f),
                                                          adjust_both_sides,
                                                          &corrected_ratio,
                                                          &tilt_angle,
                                                          [&](const float progress) {_progress = progress; });
            }
            else
                fail("Failed to capture enough frames!");
        }
        catch (const std::runtime_error& error)
        {
            fail(error.what());
        }
        catch (...)
        {
            fail("Focal length calibration failed!\nPlease adjust the camera position \nand make sure the specific target is \nin the middle of the camera image");
        }
    }

    void on_chip_calib_manager::calibrate_uv_mapping()
    {
        try
        {
            constexpr int frames_required = 25;

            rs2::frame_queue left(frames_required, true);
            rs2::frame_queue color(frames_required, true);
            rs2::frame_queue depth(frames_required, true);

            int counter = 0;
            float step = 50.f / frames_required; // The first stage represents 50% of the calibration process

            // Stage 1 : Gather frames from Depth/Left IR and RGB streams
            while (counter < frames_required)
            {
                auto fl = _viewer.ppf.frames_queue[_uid].wait_for_frame(); // left
                auto fd = _viewer.ppf.frames_queue[_uid2].wait_for_frame(); // depth
                auto fc = _viewer.ppf.frames_queue[_uid_color].wait_for_frame(); // rgb

                if (fl && fd && fc)
                {
                    left.enqueue(fl);
                    depth.enqueue(fd);
                    color.enqueue(fc);
                    counter++;
                }
                _progress += step;
            }

            if (counter >= frames_required)
            {
                auto calib_dev = _dev.as<auto_calibrated_device>();
                _new_calib = calib_dev.run_uv_map_calibration(left, color, depth, py_px_only, _health_nums, 4,
                                                            [&](const float progress) {_progress = progress; });
                if (!_new_calib.size())
                    fail("UV-Mapping calibration failed!\nPlease adjust the camera position\nand make sure the specific target is\ninside the ROI of the camera images!");
                else
                    log( "UV-Mapping recalibration - a new work point is generated" );
            }
            else
                fail("Failed to capture sufficient amount of frames to run UV-Map calibration!");
        }
        catch (const std::runtime_error& error)
        {
            fail(error.what());
        }
        catch (...)
        {
            fail("UV-Mapping calibration failed!\nPlease adjust the camera position\nand make sure the specific target is\ninside the ROI of the camera images!");
        }
    }

    void on_chip_calib_manager::get_ground_truth()
    {
        try
        {
            int counter = 0;
            int frm_idx = 0;
            int limit = 50; // input frames required to calculate the target
            float step = 50.f / limit;  // frames gathering is 50% of the process, the rest is the internal data extraction and algo processing
            
            rs2::frame_queue queue(limit*2,true);
            rs2::frame_queue queue2(limit * 2, true);
            rs2::frame_queue queue3(limit * 2, true);
            rs2::frame f;

            // Collect sufficient amount of frames (up to 50) to extract target pattern and calculate distance to it
            while ((counter < limit) && (++frm_idx < limit*2))
            {
                f = _viewer.ppf.frames_queue[_uid].wait_for_frame();
                if (f)
                {
                    queue.enqueue(f);
                    ++counter;
                    _progress += step;
                }
            }

            // Having sufficient number of frames allows to run the algorithm for target distance estimation
            if (counter >= limit)
            {
                auto calib_dev = _dev.as<auto_calibrated_device>();
                float target_z_mm = calib_dev.calculate_target_z(queue, queue2, queue3,
                                                    config_file::instance().get_or_default(configurations::viewer::target_width_r, 175.0f),
                                                    config_file::instance().get_or_default(configurations::viewer::target_height_r, 100.0f),
                                                    [&](const float progress) { _progress = std::min(100.f, _progress+step); });

                // Update the stored value with algo-calculated
                if (target_z_mm > 0.f)
                {
                    log( rsutils::string::from() << "Target Z distance calculated - " << target_z_mm << " mm");
                    config_file::instance().set(configurations::viewer::ground_truth_r, target_z_mm);
                }
                else
                    fail("Failed to calculate target ground truth");
            }
            else
                fail("Failed to capture enough frames to calculate target'z Z distance !");
        }
        catch (const std::runtime_error& error)
        {
            fail(error.what());
        }
        catch (...)
        {
            fail("Calculating target's Z distance failed");
        }
    }

    //void on_chip_calib_manager::calibrate_fl_plus()
    //{
    //    try
    //    {
    //        std::shared_ptr<dots_calculator> gt_calculator[2];
    //        bool created[3] = { false, false, false };

    //        int counter = 0;
    //        int limit = dots_calculator::_frame_num << 1;
    //        int step = 50 / dots_calculator::_frame_num;

    //        int ret = { 0 };
    //        int id[3] = { _uid, _uid_color, _uid2 }; // 0 for left, 1 for right, and 2 for depth
    //        rs2_intrinsics intrin[2];
    //        stream_profile profile[2];
    //        float dots_x[2][4] = { 0 };
    //        float dots_y[2][4] = { 0 };

    //        int idx = 0;
    //        int depth_frame_size = 0;
    //        std::vector<std::vector<uint16_t>> depth(dots_calculator::_frame_num);

    //        rs2::frame f;
    //        int width = 0;
    //        int height = 0;
    //        bool done[3] = { false, false, false };
    //        while (counter < limit)
    //        {
    //            if (!done[0])
    //            {
    //                f = _viewer.ppf.frames_queue[id[0]].wait_for_frame();

    //                if (!created[0])
    //                {
    //                    profile[0] = f.get_profile();
    //                    auto vsp = profile[0].as<video_stream_profile>();

    //                    gt_calculator[0] = std::make_shared<dots_calculator>();
    //                    intrin[0] = vsp.get_intrinsics();
    //                    created[0] = true;
    //                }

    //                ret = gt_calculator[0]->calculate(f.get(), dots_x[0], dots_y[0]);
    //                if (ret == 0)
    //                    ++counter;
    //                else if (ret == 1)
    //                    _progress += step;
    //                else if (ret == 2)
    //                {
    //                    _progress += step;
    //                    done[0] = true;
    //                }
    //            }

    //            if (!done[1])
    //            {
    //                f = _viewer.ppf.frames_queue[id[1]].wait_for_frame();

    //                if (!created[1])
    //                {
    //                    profile[1] = f.get_profile();
    //                    auto vsp = profile[1].as<video_stream_profile>();
    //                    width = vsp.width();
    //                    height = vsp.height();

    //                    gt_calculator[1] = std::make_shared<dots_calculator>();
    //                    intrin[1] = vsp.get_intrinsics();
    //                    created[1] = true;
    //                }

    //                ret = gt_calculator[1]->calculate(f.get(), dots_x[1], dots_y[1]);
    //                if (ret == 0)
    //                    ++counter;
    //                else if (ret == 1)
    //                    _progress += step;
    //                else if (ret == 2)
    //                {
    //                    _progress += step;
    //                    done[1] = true;
    //                }
    //            }

    //            if (!done[2])
    //            {
    //                f = _viewer.ppf.frames_queue[id[2]].wait_for_frame();

    //                if (!created[2])
    //                {
    //                    auto p = f.get_profile();
    //                    auto vsp = p.as<video_stream_profile>();
    //                    width = vsp.width();
    //                    depth_frame_size = vsp.width() * vsp.height() * sizeof(uint16_t);
    //                    created[2] = true;
    //                }

    //                depth[idx].resize(depth_frame_size);
    //                memmove(depth[idx++].data(), f.get_data(), depth_frame_size);

    //                if (idx == dots_calculator::_frame_num)
    //                    done[2] = true;
    //            }

    //            if (done[0] && done[1] && done[2])
    //                break;
    //        }

    //        if (done[0] && done[1] && done[2])
    //        {
    //            rs2_extrinsics extrin = profile[0].get_extrinsics_to(profile[1]);

    //            float z[4] = { 0 };
    //            find_z_at_corners(dots_x[0], dots_y[0], width, dots_calculator::_frame_num, depth, z);

    //            uvmapping_calib calib(4, dots_x[0], dots_y[0], z, dots_x[1], dots_y[1], intrin[0], intrin[1], extrin);

    //            bool ret = calib.calibrate(_health_1, _health_2, _ppx, _ppy, _fx, _fy, py_px_only);
    //            if (!ret)
    //                fail("Please adjust the camera position\nand make sure the specific target is\ninsid the ROI of the camera images!");

    //            _viewer.not_model->add_log(to_string() << "PX: " << intrin[1].ppx << " ---> " << _ppx);
    //            _viewer.not_model->add_log(to_string() << "PY: " << intrin[1].ppy << " ---> " << _ppy);
    //            _viewer.not_model->add_log(to_string() << "FX: " << intrin[1].fx << " ---> " << _fx);
    //            _viewer.not_model->add_log(to_string() << "FY: " << intrin[1].fy << " ---> " << _fy);

    //            _new_calib = _old_calib;
    //            auto table = (librealsense::ds::coefficients_table*)_new_calib.data();

    //            _health_nums[0] = _ppx / intrin[1].ppx;
    //            _health_nums[1] = _ppy / intrin[1].ppy;
    //            _health_nums[2] = _fx / intrin[1].fx;
    //            _health_nums[3] = _fy / intrin[1].fy;

    //            table->intrinsic_right.x.z *= _health_nums[0];
    //            table->intrinsic_right.y.x *= _health_nums[1];
    //            table->intrinsic_right.x.x *= _health_nums[2];
    //            table->intrinsic_right.x.y *= _health_nums[3];

    //            _health_nums[0] -= 1.0f;
    //            _health_nums[1] -= 1.0f;
    //            _health_nums[2] -= 1.0f;
    //            _health_nums[3] -= 1.0f;

    //            _health_nums[0] *= 100.0f;
    //            _health_nums[1] *= 100.0f;
    //            _health_nums[2] *= 100.0f;
    //            _health_nums[3] *= 100.0f;

    //            auto actual_data = _new_calib.data() + sizeof(librealsense::ds::table_header);
    //            auto actual_data_size = _new_calib.size() - sizeof(librealsense::ds::table_header);
    //            auto crc = helpers::calc_crc32(actual_data, actual_data_size);
    //            table->header.crc32 = crc;
    //        }
    //        else
    //            fail("Please adjust the camera position\nand make sure the specific target is\ninsid the ROI of the camera images!");
    //    }
    //    catch (const std::runtime_error& error)
    //    {
    //        fail(error.what());
    //    }
    //    catch (...)
    //    {
    //        fail("UVMapping calibration failed!");
    //    }
    //}

    void on_chip_calib_manager::process_flow(std::function<void()> cleanup, invoker invoke)
    {
        stop_viewer(invoke);

        if( action == RS2_CALIB_ACTION_ON_CHIP_CALIB || action == RS2_CALIB_ACTION_TARE_CALIB )
            if( _model.is_color_streaming() )
                throw std::runtime_error( "Turn off RGB Camera streaming before calibrating." );

        update_last_used();

        if (action == RS2_CALIB_ACTION_ON_CHIP_FL_CALIB || action == RS2_CALIB_ACTION_FL_CALIB)
            log( rsutils::string::from() << "Starting focal length calibration");
        else if (action == RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
            log( rsutils::string::from() << "Starting OCC Extended");
        else if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
            log( rsutils::string::from() << "Starting UV-Mapping calibration");
        else
            log( rsutils::string::from() << "Starting OCC calibration at speed " << speed);

        _in_3d_view = _viewer.is_3d_view;
        _viewer.is_3d_view = (action == RS2_CALIB_ACTION_TARE_GROUND_TRUTH ? false : true);

        auto calib_dev = _dev.as<auto_calibrated_device>();
        _old_calib = calib_dev.get_calibration_table();

        _was_streaming = _sub->streaming;
        _synchronized = _viewer.synchronization_enable.load();
        _post_processing = _sub->post_processing_enabled;
        _sub->post_processing_enabled = false;
        _viewer.synchronization_enable = false;

        _restored = false;

        // Save options before starting to stream as some models change options by default on stream start. e.g. D500 laser on by default for Depth Module streaming.
        save_options_controlled_by_calib(); // Restored by GUI thread on dismiss or apply.

        auto fps = 30;
        if (_sub->dev.supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
        {
            std::string desc = _sub->dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
            if (!starts_with(desc, "3."))
                fps = 6; //USB2 bandwidth limitation for 720P RGB/DI
        }

        _ui = std::make_shared<subdevice_ui_selection>(_sub->ui);
        if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB && _sub_color.get())
            _ui_color = std::make_shared<subdevice_ui_selection>(_sub_color->ui);

        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        // Switch into special Auto-Calibration mode
        if (action == RS2_CALIB_ACTION_FL_CALIB || action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
            _viewer.is_3d_view = false;


        if (action == RS2_CALIB_ACTION_FL_CALIB || action == RS2_CALIB_ACTION_TARE_GROUND_TRUTH || action == RS2_CALIB_ACTION_UVMAPPING_CALIB) // Host only calibrations
            try_start_viewer(1280, 720, fps, invoke);
        else
        {
            if (host_assistance)
                try_start_viewer(1280, 720, 0, invoke); // Host assistance uses HD resolution and crops in host
            else
                try_start_viewer(256, 144, 90, invoke); // Special calibration resolution using an internal cropping of full sensor resolution
        }

        // Change setting after streaming have started and before actual calibration
        // Emitter on by default, off for GT/FL calib and for D415 model
        float emitter_value = on_value;
        if( action == RS2_CALIB_ACTION_FL_CALIB || action == RS2_CALIB_ACTION_TARE_GROUND_TRUTH
            || device_name_string == std::string( "Intel RealSense D415" ) )
            emitter_value = off_value;
        set_laser_emitter_state( emitter_value );

        // Thermal loop should be off during calibration as to not change calibration tables during calibration
        set_thermal_loop_state( off_value );

        if ( action == RS2_CALIB_ACTION_TARE_GROUND_TRUTH )
        {
            get_ground_truth();
        }
        else
        {
            try
            {
                if (action == RS2_CALIB_ACTION_FL_CALIB)
                    calibrate_fl();
                else if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
                    calibrate_uv_mapping();
                else
                    calibrate();
            }
            catch (...)
            {
                log( "Calibration failed with exception" );
                stop_viewer(invoke);
                if (_ui.get())
                {
                    _sub->ui = *_ui;
                    _ui.reset();
                }
                if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB && _sub_color.get() && _ui_color.get())
                {
                    _sub_color->ui = *_ui_color;
                    _ui_color.reset();
                }

                if (_was_streaming)
                    start_viewer(0, 0, 0, invoke);

                throw;
            }
        }

        if (action == RS2_CALIB_ACTION_TARE_GROUND_TRUTH)
            log( rsutils::string::from() << "Tare ground truth is got: " << ground_truth);
        else if (action == RS2_CALIB_ACTION_FL_CALIB)
            log( rsutils::string::from() << "Focal length ratio is got: " << corrected_ratio);
        else if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB)
            log( "UV-Mapping calibration completed." );
        else
            log( rsutils::string::from() << "Calibration completed, health factor = " << _health);

        if (action != RS2_CALIB_ACTION_UVMAPPING_CALIB)
        {
            stop_viewer(invoke);
            if (_sub.get() && _ui.get())
            {
                _sub->ui = *_ui;
                _ui.reset();
            }
            if (_sub_color.get() && _ui_color.get())
            {
                _sub_color->ui = *_ui_color;
                _ui_color.reset();
            }
        }

        if (action != RS2_CALIB_ACTION_TARE_GROUND_TRUTH && action != RS2_CALIB_ACTION_UVMAPPING_CALIB)
        {
            if (action == RS2_CALIB_ACTION_FL_CALIB)
                _viewer.is_3d_view = true;

            try_start_viewer(0, 0, 0, invoke); // Start with default settings

            // Make new calibration active
            apply_calib(true);
        }

        _progress = 100;
        _done = true;
    }

    void on_chip_calib_manager::restore_workspace(invoker invoke)
    {
        try
        {
            if (_restored) return;

            _viewer.is_3d_view = _in_3d_view;
            _viewer.synchronization_enable = _synchronized;

            stop_viewer(invoke);

            if (_sub.get() && _ui.get())
            {
                _sub->ui = *_ui;
                _ui.reset();
            }
            if (action == RS2_CALIB_ACTION_UVMAPPING_CALIB && _sub_color.get() && _ui_color.get())
            {
                _sub_color->ui = *_ui_color;
                _ui_color.reset();
            }

            _sub->post_processing_enabled = _post_processing;

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            if (_was_streaming) start_viewer(0, 0, 0, invoke);

            _restored = true;
        }
        catch (...) {}
    }

    void on_chip_calib_manager::keep()
    {
        // Write new calibration using SETINITCAL/SETINITCALNEW command
        auto calib_dev = _dev.as<auto_calibrated_device>();
        calib_dev.write_calibration();
    }

    void on_chip_calib_manager::apply_calib(bool use_new)
    {
        auto calib_dev = _dev.as<auto_calibrated_device>();
        auto calib_table = use_new ? _new_calib : _old_calib;
        if (calib_table.size())
            calib_dev.set_calibration_table(calib_table);
    }

    void autocalib_notification_model::draw_dismiss(ux_window& win, int x, int y)
    {
        using namespace std;
        using namespace chrono;

        auto recommend_keep = false;
        if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
        {
            float health_1 = get_manager().get_health_1();
            float health_2 = get_manager().get_health_2();
            bool recommend_keep_1 = fabs(health_1) < 0.25f;
            bool recommend_keep_2 = fabs(health_2) < 0.15f;
            recommend_keep = (recommend_keep_1 && recommend_keep_2);
        }
        else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB)
            recommend_keep = fabs(get_manager().get_health()) < 0.15f;
        else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB)
            recommend_keep = fabs(get_manager().get_health()) < 0.25f;

        if (recommend_keep && update_state == RS2_CALIB_STATE_CALIB_COMPLETE && (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB))
        {
            auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

            ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
            notification_model::draw_dismiss(win, x, y);
            ImGui::PopStyleColor(2);
        }
        else
            notification_model::draw_dismiss(win, x, y);
    }

    void autocalib_notification_model::draw_intrinsic_extrinsic(int x, int y)
    {
        bool intrinsic = get_manager().intrinsic_scan;
        bool extrinsic = !intrinsic;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 35 + ImGui::GetTextLineHeightWithSpacing()) });

        std::string id = rsutils::string::from() << "##Intrinsic_" << index;
        if (ImGui::Checkbox("Intrinsic", &intrinsic))
        {
            extrinsic = !intrinsic;
        }
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", "Calibrate intrinsic parameters of the camera");
        }
        ImGui::SetCursorScreenPos({ float(x + 135), float(y + 35 + ImGui::GetTextLineHeightWithSpacing()) });

        id = rsutils::string::from() << "##Intrinsic_" << index;

        if (ImGui::Checkbox("Extrinsic", &extrinsic))
        {
            intrinsic = !extrinsic;
        }
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", "Calibrate extrinsic parameters between left and right cameras");
        }

        get_manager().intrinsic_scan = intrinsic;
    }

    void autocalib_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        using namespace std;
        using namespace chrono;

        if (update_state == RS2_CALIB_STATE_UVMAPPING_INPUT ||
            update_state == RS2_CALIB_STATE_FL_INPUT ||
            update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH ||
            update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS ||
            update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS &&
                (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB ||
                 get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB))
            get_manager().turn_roi_on();
        else
            get_manager().turn_roi_off();

        const auto bar_width = width - 115;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
        { float(x + width), float(y + 25) }, ImColor(shadow));

        if (update_state != RS2_CALIB_STATE_COMPLETE)
        {
            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
                ImGui::Text("%s", "Calibration Health-Check");
            else if (update_state == RS2_CALIB_STATE_UVMAPPING_INPUT)
                ImGui::Text("%s", "UV-Mapping Calibration");
            else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS ||
                     update_state == RS2_CALIB_STATE_CALIB_COMPLETE ||
                     update_state == RS2_CALIB_STATE_SELF_INPUT)
            {
               if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
                   ImGui::Text("%s", "On-Chip Calibration Extended");
               else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB)
                   ImGui::Text("%s", "On-Chip Focal Length Calibration");
               else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB)
                   ImGui::Text("%s", "Tare Calibration");
               else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB)
                   ImGui::Text("%s", "Focal Length Calibration");
               else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB)
                   ImGui::Text("%s", "UV-Mapping Calibration");
               else
                   ImGui::Text("%s", "On-Chip Calibration");
            }
            else if (update_state == RS2_CALIB_STATE_FL_INPUT)
                ImGui::Text("%s", "Focal Length Calibration");
            else if (update_state == RS2_CALIB_STATE_TARE_INPUT || update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
                ImGui::Text("%s", "Tare Calibration");
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH || update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS || update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_COMPLETE)
                ImGui::Text("%s", "Get Tare Calibration Ground Truth");
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_FAILED)
                ImGui::Text("%s", "Get Tare Calibration Ground Truth Failed");
            else if (update_state == RS2_CALIB_STATE_FAILED && !((get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB) && get_manager().retry_times < 3))
                ImGui::Text("%s", "Calibration Failed");

            if (update_state == RS2_CALIB_STATE_TARE_INPUT || update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
                ImGui::SetCursorScreenPos({ float(x + width - 30), float(y) });
            else if (update_state == RS2_CALIB_STATE_FAILED)
                ImGui::SetCursorScreenPos({ float(x + 2), float(y + 27) });
            else
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });

            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1.f - t));

            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

                ImGui::Text("%s", "Following devices support On-Chip Calibration:");
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 47) });

                ImGui::PushStyleColor(ImGuiCol_Text, white);
                ImGui::Text("%s", message.c_str());
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 65) });
                ImGui::Text("%s", "Run quick calibration Health-Check? (~30 sec)");
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                enable_dismiss = false;
                if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB)
                    ImGui::Text("%s", "Camera is being calibrated...\nKeep the camera stationary pointing at the target");
                else
                    ImGui::Text("%s", "Camera is being calibrated...\nKeep the camera stationary pointing at a wall");
            }
            else if (update_state == RS2_CALIB_STATE_UVMAPPING_INPUT)
            {
                ImGui::SetCursorScreenPos({ float(x + 15), float(y + 33) });
                ImGui::Text("%s", "Please make sure the target is inside yellow\nrectangle on both left and color images. Adjust\ncamera position if necessary before to start.");

                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 55) });
                ImGui::Checkbox("Px/Py only", &get_manager().py_px_only);
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Calibrate: {Fx/Fy/Px/Py}/{Px/Py}");
                }

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 25) });
                std::string button_name = rsutils::string::from() << "Calibrate" << "##uvmapping" << index;
                if (ImGui::Button(button_name.c_str(), { float(bar_width - 60), 20.f }))
                {
                    get_manager().restore_workspace([this](std::function<void()> a) { a(); });
                    get_manager().reset();
                    get_manager().retry_times = 0;
                    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB;
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };
                    get_manager().start(invoke);
                    update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                    enable_dismiss = false;
                }

                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("%s", "Begin UV-Mapping calibration after adjusting camera position");
                ImGui::PopStyleColor(2);

                string id = rsutils::string::from() << "Py Px Calibration only##py_px_only" << index;
                ImGui::SetCursorScreenPos({ float(x + 15), float(y + height - ImGui::GetTextLineHeightWithSpacing() - 32) });
                ImGui::Checkbox(id.c_str(), &get_manager().py_px_only);
            }
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH)
            {
                ImGui::SetCursorScreenPos({ float(x + 3), float(y + 33) });
                ImGui::Text("%s", "Please make sure target is inside yellow rectangle.");

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 38 + ImGui::GetTextLineHeightWithSpacing()) });
                ImGui::Text("%s", "Target Width:");
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "The width of the rectangle in millimeter inside the specific target");
                }

                const int MAX_SIZE = 256;
                char buff[MAX_SIZE];

                ImGui::SetCursorScreenPos({ float(x + 135), float(y + 35 + ImGui::GetTextLineHeightWithSpacing()) });
                std::string id = rsutils::string::from() << "##target_width_" << index;
                ImGui::PushItemWidth(width - 145.0f);
                float target_width = config_file::instance().get_or_default(configurations::viewer::target_width_r, 175.0f);
                std::string tw = rsutils::string::from() << target_width;
                memcpy(buff, tw.c_str(), tw.size() + 1);
                if (ImGui::InputText(id.c_str(), buff, std::max((int)tw.size() + 1, 10)))
                {
                    std::stringstream ss;
                    ss << buff;
                    ss >> target_width;
                    config_file::instance().set(configurations::viewer::target_width_r, target_width);
                }
                ImGui::PopItemWidth();

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 43 + 2 * ImGui::GetTextLineHeightWithSpacing()) });
                ImGui::Text("%s", "Target Height:");
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "The height of the rectangle in millimeter inside the specific target");
                }

                ImGui::SetCursorScreenPos({ float(x + 135), float(y + 40 + 2 * ImGui::GetTextLineHeightWithSpacing()) });
                id = rsutils::string::from() << "##target_height_" << index;
                ImGui::PushItemWidth(width - 145.0f);
                float target_height = config_file::instance().get_or_default(configurations::viewer::target_height_r, 100.0f);
                std::string th = rsutils::string::from() << target_height;
                memcpy(buff, th.c_str(), th.size() + 1);
                if (ImGui::InputText(id.c_str(), buff, std::max((int)th.size() + 1, 10)))
                {
                    std::stringstream ss;
                    ss << buff;
                    ss >> target_height;
                    config_file::instance().set(configurations::viewer::target_height_r, target_height);
                }
                ImGui::PopItemWidth();

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 25) });
                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

                std::string back_button_name = rsutils::string::from() << "Back" << "##tare" << index;
                if (ImGui::Button(back_button_name.c_str(), { float(60), 20.f }))
                {
                    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB;
                    update_state = update_state_prev;
                    get_manager().stop_viewer();
                }

                ImGui::SetCursorScreenPos({ float(x + 85), float(y + height - 25) });
                std::string button_name = rsutils::string::from() << "Calculate" << "##tare" << index;
                if (ImGui::Button(button_name.c_str(), { float(bar_width - 70), 20.f }))
                {
                    get_manager().restore_workspace([this](std::function<void()> a) { a(); });
                    get_manager().reset();
                    get_manager().retry_times = 0;
                    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_TARE_GROUND_TRUTH;
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };
                    get_manager().start(invoke);
                    update_state = RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS;
                    enable_dismiss = false;
                }

                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Begin calculating Tare Calibration/Distance to Target");
                }
            }
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS)
            {
                enable_dismiss = false;
                ImGui::Text("%s", "Distance to Target calculation is in process...\nKeep camera stationary pointing at the target");
            }
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_COMPLETE)
            {
                get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB;
                update_state = update_state_prev;
            }
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_FAILED)
            {
                ImGui::Text("%s", _error_message.c_str());

                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                ImGui::PushStyleColor(ImGuiCol_Button, saturate(redish, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(redish, 1.5f));

                std::string button_name = rsutils::string::from() << "Retry" << "##retry" << index;

                ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                {
                    get_manager().restore_workspace([this](std::function<void()> a) { a(); });
                    get_manager().reset();
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };
                    get_manager().start(invoke);
                    update_state = RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS;
                    enable_dismiss = false;
                }

                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Retry calculating ground truth");
                }
            }
            else if (update_state == RS2_CALIB_STATE_TARE_INPUT || update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, update_state != RS2_CALIB_STATE_TARE_INPUT_ADVANCED ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, update_state != RS2_CALIB_STATE_TARE_INPUT_ADVANCED ? light_grey : light_blue);

                if (ImGui::Button(textual_icons::caret_down))
                {
                    if (update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
                        update_state = RS2_CALIB_STATE_TARE_INPUT;
                    else
                        update_state = RS2_CALIB_STATE_TARE_INPUT_ADVANCED;
                }

                if (ImGui::IsItemHovered())
                {
                    if (update_state == RS2_CALIB_STATE_TARE_INPUT)
                        RsImGui::CustomTooltip("%s", "More Options...");
                    else
                        RsImGui::CustomTooltip("%s", "Less Options...");
                }

                ImGui::PopStyleColor(2);
                if (update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
                {
                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + 33) });
                    ImGui::Text("%s", "Avg Step Count:");
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("%s", "Number of frames to average, Min = 1, Max = 30, Default = 20");
                    }
                    ImGui::SetCursorScreenPos({ float(x + 135), float(y + 30) });

                    std::string id = rsutils::string::from() << "##avg_step_count_" << index;
                    ImGui::PushItemWidth(width - 145.f);
                    ImGui::SliderInt(id.c_str(), &get_manager().average_step_count, 1, 30);
                    ImGui::PopItemWidth();

                    //-------------------------

                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + 38 + ImGui::GetTextLineHeightWithSpacing()) });
                    ImGui::Text("%s", "Step Count:");
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("%s", "Max iteration steps, Min = 5, Max = 30, Default = 20");
                    }
                    ImGui::SetCursorScreenPos({ float(x + 135), float(y + 35 + ImGui::GetTextLineHeightWithSpacing()) });

                    id = rsutils::string::from() << "##step_count_" << index;

                    ImGui::PushItemWidth(width - 145.f);
                    ImGui::SliderInt(id.c_str(), &get_manager().step_count, 1, 30);
                    ImGui::PopItemWidth();

                    //-------------------------

                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + 43 + 2 * ImGui::GetTextLineHeightWithSpacing()) });
                    ImGui::Text("%s", "Accuracy:");
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("%s", "Subpixel accuracy level, Very high = 0 (0.025%), High = 1 (0.05%), Medium = 2 (0.1%), Low = 3 (0.2%), Default = Very high (0.025%)");
                    }

                    ImGui::SetCursorScreenPos({ float(x + 135), float(y + 40 + 2 * ImGui::GetTextLineHeightWithSpacing()) });

                    id = rsutils::string::from() << "##accuracy_" << index;

                    std::vector<std::string> vals{ "Very High", "High", "Medium", "Low" };
                    std::vector<const char*> vals_cstr;
                    for (auto&& s : vals) vals_cstr.push_back(s.c_str());

                    ImGui::PushItemWidth(width - 145.f);
                    ImGui::Combo(id.c_str(), &get_manager().accuracy, vals_cstr.data(), int(vals.size()));

                    ImGui::SetCursorScreenPos({ float(x + 135), float(y + 35 + ImGui::GetTextLineHeightWithSpacing()) });

                    ImGui::PopItemWidth();

                    // Disabled according to the decision on v2.50
                    //draw_intrinsic_extrinsic(x, y + 3 * int(ImGui::GetTextLineHeightWithSpacing()) - 10);

                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + 52 + 4 * ImGui::GetTextLineHeightWithSpacing()) });
                    id = rsutils::string::from() << "Apply High-Accuracy Preset##apply_preset_" << index;
                    ImGui::Checkbox(id.c_str(), &get_manager().apply_preset);
                }

                if (update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
                {
                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + 60 + 5 * ImGui::GetTextLineHeightWithSpacing()) });
                    ImGui::Text("%s", "Ground Truth(mm):");
                    ImGui::SetCursorScreenPos({ float(x + 135), float(y + 58 + 5 * ImGui::GetTextLineHeightWithSpacing()) });
                }
                else
                {
                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + 33) });
                    ImGui::Text("%s", "Ground Truth (mm):");
                    ImGui::SetCursorScreenPos({ float(x + 135), float(y + 30) });
                }

                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("%s", "Distance in millimeter to the flat wall, between 60 and 10000.");

                std::string id = rsutils::string::from() << "##ground_truth_for_tare" << index;
                get_manager().ground_truth = config_file::instance().get_or_default(configurations::viewer::ground_truth_r, 1200.0f);

                std::string gt = rsutils::string::from() << get_manager().ground_truth;
                const int MAX_SIZE = 256;
                char buff[MAX_SIZE];
                memcpy(buff, gt.c_str(), gt.size() + 1);

                ImGui::PushItemWidth(width - 196.f);
                if (ImGui::InputText(id.c_str(), buff, std::max((int)gt.size() + 1, 10)))
                {
                    std::stringstream ss;
                    ss << buff;
                    ss >> get_manager().ground_truth;
                }
                ImGui::PopItemWidth();

                config_file::instance().set(configurations::viewer::ground_truth_r, get_manager().ground_truth);

                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

                std::string get_button_name = rsutils::string::from() << "Get" << "##tare" << index;
                if (update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED)
                    ImGui::SetCursorScreenPos({ float(x + width - 52), float(y + 58 + 5 * ImGui::GetTextLineHeightWithSpacing()) });
                else
                    ImGui::SetCursorScreenPos({ float(x + width - 52), float(y + 30) });

                if (ImGui::Button(get_button_name.c_str(), { 42.0f, 20.f }))
                {
                    update_state_prev = update_state;
                    update_state = RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH;
                    get_manager().start_gt_viewer();
                }
                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("%s", "Calculate ground truth for the specific target");

                std::string button_name = rsutils::string::from() << "Calibrate" << "##tare" << index;

                ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 28) });
                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                {
                    get_manager().restore_workspace([](std::function<void()> a) { a(); });
                    get_manager().reset();
                    get_manager().retry_times = 0;
                    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB;
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };
                    get_manager().start(invoke);
                    update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                    enable_dismiss = false;
                }

                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Begin Tare Calibration");
                }
            }
            else if (update_state == RS2_CALIB_STATE_SELF_INPUT)
            {
                // Disabled according to the decision on v2.50
                /*
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 33) });
                ImGui::Text("%s", "Speed:");

                ImGui::SetCursorScreenPos({ float(x + 135), float(y + 30) });

                std::string id = to_string() << "##speed_" << index;

                std::vector<const char*> vals_cstr;
                if (get_manager().action != on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB)
                {
                    std::vector<std::string> vals{ "Fast", "Slow", "White Wall" };
                    for (auto&& s : vals) vals_cstr.push_back(s.c_str());

                    ImGui::PushItemWidth(width - 145.f);
                    ImGui::Combo(id.c_str(), &get_manager().speed_fl, vals_cstr.data(), int(vals.size()));
                    ImGui::PopItemWidth();
                }
                else
                {
                    std::vector<std::string> vals{ "Very Fast", "Fast", "Medium", "Slow", "White Wall" };
                    for (auto&& s : vals) vals_cstr.push_back(s.c_str());

                    ImGui::PushItemWidth(width - 145.f);
                    ImGui::Combo(id.c_str(), &get_manager().speed, vals_cstr.data(), int(vals.size()));
                    ImGui::PopItemWidth();
                }

                if (get_manager().action != on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB)
                    draw_intrinsic_extrinsic(x, y);

                if (get_manager().action != on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB)
                {
                    float tmp_y = (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB ?
                        float(y + 40 + 2 * ImGui::GetTextLineHeightWithSpacing()) : float(y + 35 + ImGui::GetTextLineHeightWithSpacing()));
                    ImGui::SetCursorScreenPos({ float(x + 9), tmp_y });
                    id = to_string() << "##restore_" << index;
                    bool restore = (get_manager().adjust_both_sides == 1);
                    if (ImGui::Checkbox("Adjust both sides focal length", &restore))
                        get_manager().adjust_both_sides = (restore ? 1 : 0);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "check = adjust both sides, uncheck = adjust right side only");
                }*/

                // Deprecase OCC-Extended
                //float tmp_y = (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB ?
                //    float(y + 45 + 3 * ImGui::GetTextLineHeightWithSpacing()) : float(y + 41 + 2 * ImGui::GetTextLineHeightWithSpacing()));

                //ImGui::SetCursorScreenPos({ float(x + 9), tmp_y });
                //if (ImGui::RadioButton("OCC", (int*)&(get_manager().action), 1))
                //    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB;
                //if (ImGui::IsItemHovered())
                //    RsImGui::CustomTooltip("%s", "On-chip calibration");

                //ImGui::SetCursorScreenPos({ float(x + 135),  tmp_y });
                //if (ImGui::RadioButton("OCC Extended", (int *)&(get_manager().action), 0))
                //    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB;
                //if (ImGui::IsItemHovered())
                //    RsImGui::CustomTooltip("%s", "On-Chip Calibration Extended");


                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

                std::string button_name = rsutils::string::from() << "Calibrate" << "##self" << index;

                ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 28) });
                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                {
                    get_manager().restore_workspace([this](std::function<void()> a) { a(); });
                    get_manager().reset();
                    get_manager().retry_times = 0;
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {_this->invoke(action);};
                    get_manager().start(invoke);
                    update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                    enable_dismiss = false;
                }

                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("%s", "Begin On-Chip Calibration");
            }
            else if (update_state == RS2_CALIB_STATE_FL_INPUT)
            {
                ImGui::SetCursorScreenPos({ float(x + 15), float(y + 33) });
                ImGui::Text("%s", "Please make sure the target is inside yellow\nrectangle of both left and right images. Adjust\ncamera position if necessary before to start.");

                ImGui::SetCursorScreenPos({ float(x + 15), float(y + 70 + ImGui::GetTextLineHeightWithSpacing()) });
                ImGui::Text("%s", "Target Width (mm):");
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "The width of the rectangle in millimeters inside the specific target");
                }

                const int MAX_SIZE = 256;
                char buff[MAX_SIZE];

                ImGui::SetCursorScreenPos({ float(x + 145), float(y + 70 + ImGui::GetTextLineHeightWithSpacing()) });
                std::string id = rsutils::string::from() << "##target_width_" << index;
                ImGui::PushItemWidth(80);
                float target_width = config_file::instance().get_or_default(configurations::viewer::target_width_r, 175.0f);
                std::string tw = rsutils::string::from() << target_width;
                memcpy(buff, tw.c_str(), tw.size() + 1);
                if (ImGui::InputText(id.c_str(), buff, std::max((int)tw.size() + 1, 10)))
                {
                    std::stringstream ss;
                    ss << buff;
                    ss >> target_width;
                    config_file::instance().set(configurations::viewer::target_width_r, target_width);
                }
                ImGui::PopItemWidth();

                ImGui::SetCursorScreenPos({ float(x + 15), float(y + 80 + 2 * ImGui::GetTextLineHeightWithSpacing()) });
                ImGui::Text("%s", "Target Height (mm):");
                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "The height of the rectangle in millimeters inside the specific target");
                }

                ImGui::SetCursorScreenPos({ float(x + 145), float(y + 77 + 2 * ImGui::GetTextLineHeightWithSpacing()) });
                id = rsutils::string::from() << "##target_height_" << index;
                ImGui::PushItemWidth(80);
                float target_height = config_file::instance().get_or_default(configurations::viewer::target_height_r, 100.0f);
                std::string th = rsutils::string::from() << target_height;
                memcpy(buff, th.c_str(), th.size() + 1);
                if (ImGui::InputText(id.c_str(), buff, std::max((int)th.size() + 1, 10)))
                {
                    std::stringstream ss;
                    ss << buff;
                    ss >> target_height;
                    config_file::instance().set(configurations::viewer::target_height_r, target_height);
                }
                ImGui::PopItemWidth();

                ImGui::SetCursorScreenPos({ float(x + 20), float(y + 95) + 3 * ImGui::GetTextLineHeight() });
                bool adj_both = (get_manager().adjust_both_sides == 1);
                if (ImGui::Checkbox("Adjust both sides focal length", &adj_both))
                    get_manager().adjust_both_sides = (adj_both ? 1 : 0);
                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("%s", "check = adjust both sides, uncheck = adjust right side only");

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 25) });
                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

                std::string button_name = rsutils::string::from() << "Calibrate" << "##fl" << index;

                ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                {
                    get_manager().restore_workspace([this](std::function<void()> a) { a(); });
                    get_manager().reset();
                    get_manager().retry_times = 0;
                    get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB;
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };
                    get_manager().start(invoke);
                    update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                    enable_dismiss = false;
                }
                if (ImGui::IsItemHovered())
                    RsImGui::CustomTooltip("%s", "Start focal length calibration after setting up camera position correctly.");
                ImGui::PopStyleColor(2);
            }
            else if (update_state == RS2_CALIB_STATE_FAILED)
            {
                if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB
                    || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
                {
                    if (get_manager().retry_times < 3)
                    {
                        get_manager().restore_workspace([](std::function<void()> a){ a(); });
                        get_manager().reset();
                        ++get_manager().retry_times;
                        get_manager().toggle = true;

                        auto _this = shared_from_this();
                        auto invoke = [_this](std::function<void()> action) {_this->invoke(action);};
                        get_manager().start(invoke);
                        update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                        enable_dismiss = false;
                    }
                    else
                    {
                        ImGui::Text("%s", (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB ? "OCC FL calibraton cannot work with this camera!" : "OCC Extended calibraton cannot work with this camera!"));
                    }
                }
                else
                {
                    ImGui::Text("%s", _error_message.c_str());

                    auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                    ImGui::PushStyleColor(ImGuiCol_Button, saturate(redish, sat));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(redish, 1.5f));

                    std::string button_name = rsutils::string::from() << "Retry" << "##retry" << index;
                    ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                    if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                    {
                        get_manager().restore_workspace([](std::function<void()> a) { a(); });
                        get_manager().reset();
                        auto _this = shared_from_this();
                        auto invoke = [_this](std::function<void()> action) {
                            _this->invoke(action);
                        };
                        get_manager().start(invoke);
                        update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                        enable_dismiss = false;
                    }

                    ImGui::PopStyleColor(2);

                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "Retry on-chip calibration process");
                }
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
            {
                if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB)
                {
                    ImGui::SetCursorScreenPos({ float(x + 20), float(y + 33) });
                    ImGui::Text("%s", "Health-Check Number for PX: ");

                    ImGui::SetCursorScreenPos({ float(x + 20), float(y + 38) + ImGui::GetTextLineHeightWithSpacing() });
                    ImGui::Text("%s", "Health Check Number for PY: ");

                    ImGui::SetCursorScreenPos({ float(x + 20), float(y + 43) + 2 * ImGui::GetTextLineHeightWithSpacing() });
                    ImGui::Text("%s", "Health Check Number for FX: ");

                    ImGui::SetCursorScreenPos({ float(x + 20), float(y + 48) + 3 * ImGui::GetTextLineHeightWithSpacing() });
                    ImGui::Text("%s", "Health Check Number for FY: ");

                    ImGui::PushStyleColor(ImGuiCol_Text, white);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

                    ImGui::SetCursorScreenPos({ float(x + 220), float(y + 30) });
                    std::stringstream ss_1;
                    ss_1 << std::fixed << std::setprecision(4) << get_manager().get_health_nums(0);
                    auto health_str = ss_1.str();
                    std::string text_name_1 = rsutils::string::from() << "##notification_text_1_" << index;
                    ImGui::InputTextMultiline(text_name_1.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "Health check for PX");

                    ImGui::SetCursorScreenPos({ float(x + 220), float(y + 35) + ImGui::GetTextLineHeightWithSpacing() });
                    std::stringstream ss_2;
                    ss_2 << std::fixed << std::setprecision(4) << get_manager().get_health_nums(1);
                    health_str = ss_2.str();
                    std::string text_name_2 = rsutils::string::from() << "##notification_text_2_" << index;
                    ImGui::InputTextMultiline(text_name_2.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "Health check for PY");

                    ImGui::SetCursorScreenPos({ float(x + 220), float(y + 40) + 2 * ImGui::GetTextLineHeightWithSpacing() });
                    std::stringstream ss_3;
                    ss_3 << std::fixed << std::setprecision(4) << get_manager().get_health_nums(2);
                    health_str = ss_3.str();
                    std::string text_name_3 = rsutils::string::from() << "##notification_text_3_" << index;
                    ImGui::InputTextMultiline(text_name_3.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "Health check for FX");

                    ImGui::SetCursorScreenPos({ float(x + 220), float(y + 45) + 3 * ImGui::GetTextLineHeightWithSpacing() });
                    std::stringstream ss_4;
                    ss_4 << std::fixed << std::setprecision(4) << get_manager().get_health_nums(3);
                    health_str = ss_4.str();
                    std::string text_name_4 = rsutils::string::from() << "##notification_text_4_" << index;
                    ImGui::InputTextMultiline(text_name_4.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "Health check for FY");

                    ImGui::PopStyleColor(7);

                    auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                    ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                    ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 25) });
                    std::string button_name = rsutils::string::from() << "Apply" << "##apply" << index;
                    if (ImGui::Button(button_name.c_str(), { float(bar_width - 60), 20.f }))
                    {
                        get_manager().apply_calib(true);     // Store the new calibration internally
                        get_manager().keep();            // Flash the new calibration
                        if (RS2_CALIB_STATE_UVMAPPING_INPUT == update_state)
                            get_manager().reset_device(); // Workaround for reloading color calibration table. Other approach?

                        update_state = RS2_CALIB_STATE_COMPLETE;
                        pinned = false;
                        enable_dismiss = false;
                        _progress_bar.last_progress_time = last_interacted = system_clock::now() + milliseconds(500);

                        get_manager().restore_workspace([](std::function<void()> a) { a(); });
                    }

                    ImGui::PopStyleColor(2);
                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "New calibration values will be saved in device");
                }
                else
                {
                    auto health = get_manager().get_health();

                    auto recommend_keep = fabs(health) < 0.25f;
                    if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB)
                        recommend_keep = fabs(health) < 0.15f;

                    float health_1 = -1.0f;
                    float health_2 = -1.0f;
                    bool recommend_keep_1 = false;
                    bool recommend_keep_2 = false;
                    if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
                    {
                        health_1 = get_manager().get_health_1();
                        health_2 = get_manager().get_health_2();
                        recommend_keep_1 = fabs(health_1) < 0.25f;
                        recommend_keep_2 = fabs(health_2) < 0.15f;
                        recommend_keep = (recommend_keep_1 && recommend_keep_2);
                    }

                    ImGui::SetCursorScreenPos({ float(x + 10), float(y + 33) });

                    if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB)
                    {
                        if (get_manager().tare_health)
                        {
                            health_1 = get_manager().get_health_1();
                            health_2 = get_manager().get_health_2();

                            ImGui::Text("%s", "Health-Check Before Calibration: ");

                            std::stringstream ss_1;
                            ss_1 << std::fixed << std::setprecision(4) << health_1 << "%";
                            auto health_str = ss_1.str();

                            std::string text_name = rsutils::string::from() << "##notification_text_1_" << index;

                            ImGui::SetCursorScreenPos({ float(x + 225), float(y + 30) });
                            ImGui::PushStyleColor(ImGuiCol_Text, white);
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                            ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                            ImGui::PopStyleColor(7);

                            if (ImGui::IsItemHovered())
                                RsImGui::CustomTooltip("%s", "Health-check number before Tare Calibration");

                            ImGui::SetCursorScreenPos({ float(x + 10), float(y + 38) + ImGui::GetTextLineHeightWithSpacing() });
                            ImGui::Text("%s", "Health-Check After Calibration: ");

                            std::stringstream ss_2;
                            ss_2 << std::fixed << std::setprecision(4) << health_2 << "%";
                            health_str = ss_2.str();

                            text_name = rsutils::string::from() << "##notification_text_2_" << index;

                            ImGui::SetCursorScreenPos({ float(x + 225), float(y + 35) + ImGui::GetTextLineHeightWithSpacing() });
                            ImGui::PushStyleColor(ImGuiCol_Text, white);
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                            ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                            ImGui::PopStyleColor(7);

                            if (ImGui::IsItemHovered())
                                RsImGui::CustomTooltip("%s", "Health-check number after Tare Calibration");
                        }
                    }
                    else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB)
                    {
                        ImGui::Text("%s", "Health-Check: ");

                        std::stringstream ss_1;
                        ss_1 << std::fixed << std::setprecision(2) << health_1;
                        auto health_str = ss_1.str();

                        std::string text_name = rsutils::string::from() << "##notification_text_1_" << index;

                        ImGui::SetCursorScreenPos({ float(x + 125), float(y + 30) });
                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                        ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 66, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopStyleColor(7);

                        ImGui::SetCursorScreenPos({ float(x + 177), float(y + 33) });

                        if (recommend_keep_1)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                            ImGui::Text("%s", "(Good)");
                        }
                        else if (fabs(health_1) < 0.75f)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
                            ImGui::Text("%s", "(Can be Improved)");
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, redish);
                            ImGui::Text("%s", "(Requires Calibration)");
                        }
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("%s", "OCC Health-Check captures how far camera calibration is from the optimal one\n"
                                "[0, 0.25) - Good\n"
                                "[0.25, 0.75) - Can be Improved\n"
                                "[0.75, ) - Requires Calibration");
                        }

                        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 38) + ImGui::GetTextLineHeightWithSpacing() });
                        ImGui::Text("%s", "FL Health-Check: ");

                        std::stringstream ss_2;
                        ss_2 << std::fixed << std::setprecision(2) << health_2;
                        health_str = ss_2.str();

                        text_name = rsutils::string::from() << "##notification_text_2_" << index;

                        ImGui::SetCursorScreenPos({ float(x + 125), float(y + 35) + ImGui::GetTextLineHeightWithSpacing() });
                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                        ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 66, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopStyleColor(7);

                        ImGui::SetCursorScreenPos({ float(x + 175), float(y + 38) + ImGui::GetTextLineHeightWithSpacing() });

                        if (recommend_keep_2)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                            ImGui::Text("%s", "(Good)");
                        }
                        else if (fabs(health_2) < 0.75f)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
                            ImGui::Text("%s", "(Can be Improved)");
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, redish);
                            ImGui::Text("%s", "(Requires Calibration)");
                        }
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("%s", "OCC-FL Health-Check captures how far camera calibration is from the optimal one\n"
                                "[0, 0.15) - Good\n"
                                "[0.15, 0.75) - Can be Improved\n"
                                "[0.75, ) - Requires Calibration");
                        }
                    }
                    else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB)
                    {
                        ImGui::Text("%s", "Focal Length Imbalance: ");

                        std::stringstream ss_1;
                        ss_1 << std::fixed << std::setprecision(3) << get_manager().corrected_ratio;
                        auto ratio_str = ss_1.str();
                        ratio_str += " %";

                        std::string text_name = rsutils::string::from() << "##notification_text_1_" << index;

                        ImGui::SetCursorScreenPos({ float(x + 175), float(y + 30) });
                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                        ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(ratio_str.c_str()), strlen(ratio_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopStyleColor(7);

                        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 38) + ImGui::GetTextLineHeightWithSpacing() });
                        ImGui::Text("%s", "Estimated Tilt Angle: ");

                        std::stringstream ss_2;
                        ss_2 << std::fixed << std::setprecision(3) << get_manager().tilt_angle;
                        auto align_str = ss_2.str();
                        align_str += " deg";

                        text_name = rsutils::string::from() << "##notification_text_2_" << index;

                        ImGui::SetCursorScreenPos({ float(x + 175), float(y + 35) + ImGui::GetTextLineHeightWithSpacing() });
                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                        ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(align_str.c_str()), strlen(align_str.c_str()) + 1, { 86, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopStyleColor(7);
                    }
                    else if (get_manager().action != on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB)
                    {
                        ImGui::Text("%s", (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB ? "Health-Check: " : "FL Health-Check: "));

                        std::stringstream ss; ss << std::fixed << std::setprecision(2) << health;
                        auto health_str = ss.str();

                        std::string text_name = rsutils::string::from() << "##notification_text_" << index;

                        ImGui::SetCursorScreenPos({ float(x + 125), float(y + 30) });
                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
                        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
                        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
                        ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(health_str.c_str()), strlen(health_str.c_str()) + 1, { 66, ImGui::GetTextLineHeight() + 6 }, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopStyleColor(7);

                        ImGui::SetCursorScreenPos({ float(x + 177), float(y + 33) });

                        if (recommend_keep)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                            ImGui::Text("%s", "(Good)");
                        }
                        else if (fabs(health) < 0.75f)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
                            ImGui::Text("%s", "(Can be Improved)");
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, redish);
                            ImGui::Text("%s", "(Requires Calibration)");
                        }
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered())
                        {
                            if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB)
                            {
                                RsImGui::CustomTooltip("%s", "Calibration Health-Check captures how far camera calibration is from the optimal one\n"
                                    "[0, 0.25) - Good\n"
                                    "[0.25, 0.75) - Can be Improved\n"
                                    "[0.75, ) - Requires Calibration");
                            }
                            else
                            {
                                RsImGui::CustomTooltip("%s", "Calibration Health-Check captures how far camera calibration is from the optimal one\n"
                                    "[0, 0.15) - Good\n"
                                    "[0.15, 0.75) - Can be Improved\n"
                                    "[0.75, ) - Requires Calibration");
                            }
                        }
                    }

                    ImGui::SetCursorScreenPos({ float(x + 7), (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB ? float(y + 105) + ImGui::GetTextLineHeightWithSpacing() : (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB ? (get_manager().tare_health ? float(y + 105) : float(y + 50)) + ImGui::GetTextLineHeightWithSpacing() : float(y + 105))) });
                    ImGui::Text("%s", "Please compare new vs old calibration\nand decide if to keep or discard the result...");

                    ImGui::SetCursorScreenPos({ float(x + 20), (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB ? float(y + 70) + ImGui::GetTextLineHeightWithSpacing() : (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB ? (get_manager().tare_health ? float(y + 70) : float(y + 15)) + ImGui::GetTextLineHeightWithSpacing() : float(y + 70))) });

                    if (ImGui::RadioButton("New", use_new_calib))
                    {
                        use_new_calib = true;
                        get_manager().apply_calib(true);
                    }

                    ImGui::SetCursorScreenPos({ float(x + 150), (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB ? float(y + 70) + ImGui::GetTextLineHeightWithSpacing() : (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB ? (get_manager().tare_health ? float(y + 70) : float(y + 15)) + ImGui::GetTextLineHeightWithSpacing() : float(y + 70))) });
                    if (ImGui::RadioButton("Original", !use_new_calib))
                    {
                        use_new_calib = false;
                        get_manager().apply_calib(false);
                    }

                    auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                    if (!recommend_keep || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                    }

                    float scale = float(bar_width) / 3;
                    std::string button_name;

                    if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB)
                    {
                        scale = float(bar_width) / 7;

                        button_name = rsutils::string::from() << "Recalibrate" << "##refl" << index;

                        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                        if (ImGui::Button(button_name.c_str(), { scale * 3, 20.f }))
                        {
                            get_manager().restore_workspace([this](std::function<void()> a) { a(); });
                            get_manager().reset();
                            get_manager().retry_times = 0;
                            get_manager().action = on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB;
                            auto _this = shared_from_this();
                            auto invoke = [_this](std::function<void()> action) {
                                _this->invoke(action);
                            };
                            get_manager().start(invoke);
                            update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                            enable_dismiss = false;
                        }

                        ImGui::SetCursorScreenPos({ float(x + 5) + 4 * scale, float(y + height - 25) });
                    }
                    else
                        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

                    button_name = rsutils::string::from() << "Apply New" << "##apply" << index;
                    if (!use_new_calib) button_name = rsutils::string::from() << "Keep Original" << "##original" << index;

                    if (ImGui::Button(button_name.c_str(), { scale * 3, 20.f }))
                    {
                        if (use_new_calib)
                        {
                            get_manager().keep();
                            update_state = RS2_CALIB_STATE_COMPLETE;
                            pinned = false;
                            enable_dismiss = false;
                            _progress_bar.last_progress_time = last_interacted = system_clock::now() + milliseconds(500);
                        }
                        else
                        {
                            dismiss(false);
                        }

                        get_manager().restore_options_controlled_by_calib();
                        get_manager().restore_workspace([](std::function<void()> a) { a(); });
                    }

                    if (!recommend_keep || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB)
                        ImGui::PopStyleColor(2);

                    if (ImGui::IsItemHovered())
                        RsImGui::CustomTooltip("%s", "New calibration values will be saved in device");
                }
            }

            ImGui::PopStyleColor();
        }
        else
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

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        if (update_manager)
        {
            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            {
                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                std::string button_name = rsutils::string::from() << "Health-Check" << "##health_check" << index;

                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }) || update_manager->started())
                {
                    auto _this = shared_from_this();
                    auto invoke = [_this](std::function<void()> action) {
                        _this->invoke(action);
                    };

                    if (!update_manager->started()) update_manager->start(invoke);

                    update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                    enable_dismiss = false;
                    _progress_bar.last_progress_time = system_clock::now();
                }
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    RsImGui::CustomTooltip("%s", "Keep the camera pointing at an object or a wall");
                }
            }
            else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS)
            {
                if (update_manager->done())
                {
                    update_state = RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_COMPLETE;
                    enable_dismiss = true;
                }

                if (update_manager->failed())
                {
                    update_manager->check_error(_error_message);
                    update_state = RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_FAILED;
                    enable_dismiss = true;
                }

                draw_progress_bar(win, bar_width);
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                if (update_manager->done())
                {
                    update_state = RS2_CALIB_STATE_CALIB_COMPLETE;
                    enable_dismiss = true;
                    if (get_manager().action != on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB)
                    {
                        get_manager().apply_calib(true);
                        use_new_calib = true;
                    }
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

                    string id = rsutils::string::from() << "Expand" << "##" << index;
                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    if (ImGui::Button(id.c_str(), { 100, 20 }))
                        expanded = true;
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    void autocalib_notification_model::dismiss(bool snooze)
    {
        get_manager().update_last_used();

        if (!use_new_calib && get_manager().done())
            get_manager().apply_calib(false);

        get_manager().restore_options_controlled_by_calib();
        get_manager().restore_workspace( []( std::function< void() > a ) { a(); } );

        if (update_state != RS2_CALIB_STATE_TARE_INPUT)
            update_state = RS2_CALIB_STATE_INITIAL_PROMPT;

        get_manager().turn_roi_off();
        get_manager().reset();

        notification_model::dismiss(snooze);
    }

    void autocalib_notification_model::draw_expanded(ux_window& win, std::string& error_message)
    {
        if (update_manager->started() && update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;

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

        std::string title;
        switch (get_manager().action)
        {
            case on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB: title = "On - Chip Calibration Extended"; break;
            case on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_CALIB: title = "On-Chip Calibration"; break;
            case on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB: title = "On-Chip Focal Length Calibration"; break;
            case on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB: title = "Tare Calibration"; break;
            case on_chip_calib_manager::RS2_CALIB_ACTION_TARE_GROUND_TRUTH: title = "Ground Truth Calculation"; break;
            case on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB: title = "Focal Length Calibration"; break;
            case on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB: title = "UV - Mapping Calibration"; break;
            default: title = "Calibration";
        }

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
            ImGui::InputTextMultiline("##autocalib_log", const_cast<char*>(s.c_str()),
                s.size() + 1, { 490,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(190);
            if (visible || update_manager->done() || update_manager->failed())
            {
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (update_manager->failed())
                        update_state = RS2_CALIB_STATE_FAILED;

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

    int autocalib_notification_model::calc_height()
    {
        if (update_state == RS2_CALIB_STATE_COMPLETE) return 65;
        else if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT) return 120;
        else if (update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
        {
            if (get_manager().allow_calib_keep())
            {
                if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_FL_CALIB) return 190;
                else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_TARE_CALIB) return 140;
                else if (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_UVMAPPING_CALIB) return 160;
                else return 170;
            }
            else return 80;
        }
        else if (update_state == RS2_CALIB_STATE_SELF_INPUT) return (get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB ? 180 : 90);
        else if (update_state == RS2_CALIB_STATE_TARE_INPUT) return 105;
        else if (update_state == RS2_CALIB_STATE_TARE_INPUT_ADVANCED) return 230;
        else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH) return 135;
        else if (update_state == RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_FAILED) return 115;
        else if (update_state == RS2_CALIB_STATE_FAILED) return ((get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_OB_CALIB || get_manager().action == on_chip_calib_manager::RS2_CALIB_ACTION_ON_CHIP_FL_CALIB) ? (get_manager().retry_times < 3 ? 0 : 80) : 110);
        else if (update_state == RS2_CALIB_STATE_FL_INPUT) return 200;
        else if (update_state == RS2_CALIB_STATE_UVMAPPING_INPUT) return 140;
        else return 100;
    }

    void autocalib_notification_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);

        ImGui::PopStyleColor(1);

        ImVec4 c;

        if (update_state == RS2_CALIB_STATE_COMPLETE)
        {
            c = alpha(saturate(light_blue, 0.7f), 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else if (update_state == RS2_CALIB_STATE_FAILED)
        {
            c = alpha(dark_red, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            c = alpha(sensor_bg, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
    }

    autocalib_notification_model::autocalib_notification_model(std::string name, std::shared_ptr<process_manager> manager, bool exp)
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
}
