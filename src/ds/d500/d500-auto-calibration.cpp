// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.


#include "d500-auto-calibration.h"
#include <src/ds/ds-calib-common.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/d500-debug-protocol-calibration-engine.h>
#include <src/sensor.h>
#include <src/core/advanced_mode.h>

#include <rsutils/json.h>
#include <rsutils/string/from.h>

#include <sstream>   // std::stringstream + drags <ios> for std::hex / std::dec used in the CRC diagnostic logs

namespace librealsense
{
    static constexpr const size_t hwm_header_size = 4;

    static const std::string calibration_state_strings[] = {
        "Idle",
        "In Process",
        "Done Success",
        "Done Failure",
        "Flash Update",
        "Complete",
        "Health Check"   // interactive triggered calibration — enum value 6
    };

    static const std::string calibration_result_strings[] = {
        "Unkown",
        "Success",
        "Failed to Converge",
        "Failed to Run"
    };

    d500_auto_calibrated::d500_auto_calibrated( std::shared_ptr< d500_debug_protocol_calibration_engine > calib_engine,
                                                debug_interface * debug_dev,
                                                sensor_base * ds )
        : _calib_engine( calib_engine )
        , _mode( calibration_mode::RESERVED )
        , _state( calibration_state::IDLE )
        , _result( calibration_result::UNKNOWN )
        , _depth_sensor( ds )
        , _debug_dev( debug_dev )
        , _try_selection( try_calibration_selection::NEW )
    {
        if( ! _debug_dev )
            throw not_implemented_exception( " debug_interface must be supplied to d500_auto_calibrated" );
    }

    void d500_auto_calibrated::cancel_and_wait_for_idle()
    {
        try
        {
            _calib_engine->run_triggered_calibration( calibration_mode::ABORT );
            // SET_CALIB_MODE is one-shot; FW takes a moment to settle back to IDLE.
            for( int i = 0; i < 20; ++i )
            {
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                _calib_engine->update_triggered_calibration_status();
                if( _calib_engine->get_triggered_calibration_state() == calibration_state::IDLE )
                    break;
            }
        }
        catch( const std::exception & e )
        {
            LOG_WARNING( "Interactive TC: CANCEL / status poll failed (" << e.what()
                         << ") - FW may not be in Idle when this returns" );
        }
        catch( ... )
        {
            LOG_WARNING( "Interactive TC: CANCEL / status poll failed (non-std exception) - swallowed to keep cancel_and_wait_for_idle non-throwing" );
        }

        // Belt-and-suspenders: on some FW builds CANCEL does not restore the flashed depth table into the RAM
        // slot that the depth pipeline reads, so the stream can end up rendering a half-baked candidate that
        // looks visibly worse than the pre-RUN calibration. Write the flashed depth table back to the RAM slot
        // ourselves via SET_HKR_CONFIG_TABLE — issued directly rather than through set_calibration_table() so we
        // don't clobber _curr_calibration, which is the shared staging buffer for the manual-calibration API.
        try
        {
            std::vector< uint8_t > dummy;
            auto flash_table = _calib_engine->get_calibration_table( dummy );
            // Require an exact size match — matches set_calibration_table's own guard. A longer-than-expected HWM
            // reply must not be pushed verbatim into the RAM slot the depth pipeline reads.
            if( flash_table.size() == sizeof( ds::d500_coefficients_table ) )
            {
                auto hdr = reinterpret_cast< const ds::table_header * >( flash_table.data() );
                auto cmd = _debug_dev->build_command(
                    ds::SET_HKR_CONFIG_TABLE,
                    static_cast< int >( ds::d500_calib_location::d500_calib_ram_memory ),
                    static_cast< int >( hdr->table_type ),
                    static_cast< int >( ds::d500_calib_type::d500_calib_dynamic ),
                    0,
                    flash_table.data(),
                    flash_table.size() );
                _debug_dev->send_receive_raw_data( cmd );
                for( auto & cb : _depth_write_callbacks )
                    cb();
            }
            else if( ! flash_table.empty() )
            {
                LOG_WARNING( "Interactive TC: flash->RAM revert skipped - GET_HKR_CONFIG_TABLE returned "
                             << flash_table.size() << " bytes, expected "
                             << sizeof( ds::d500_coefficients_table ) );
            }
        }
        catch( const std::exception & e )
        {
            LOG_WARNING( "Interactive TC: flash->RAM revert failed (" << e.what()
                         << ") - depth stream may still reflect the candidate table until next stream restart" );
        }
        catch( ... )
        {
            // Preserve the pre-refactor non-throwing property of this helper — the pre-run auto-cancel path relied
            // on it. Non-std exceptions here are pathological (there is no code that throws non-std types on this
            // path today), but a bare catch is cheap insurance.
            LOG_WARNING( "Interactive TC: flash->RAM revert failed (non-std exception) - swallowed to keep cancel_and_wait_for_idle non-throwing" );
        }
    }

    bool d500_auto_calibrated::device_uses_interactive_triggered_calibration() const
    {
        auto dev = As< device >( _debug_dev );
        if( ! dev || ! dev->supports_info( RS2_CAMERA_INFO_PRODUCT_ID ) )
            return false;
        try
        {
            auto pid = static_cast< uint16_t >( std::stoi( dev->get_info( RS2_CAMERA_INFO_PRODUCT_ID ), nullptr, 16 ) );
            return ds::uses_interactive_triggered_calibration( pid );
        }
        catch( ... )
        {
            return false;
        }
    }

    void d500_auto_calibrated::check_preconditions_and_set_state()
    {
        if (_mode == calibration_mode::RUN ||
            _mode == calibration_mode::DRY_RUN)
        {
            // calibration state to be IDLE or COMPLETE
            _calib_engine->update_triggered_calibration_status();

            _state = _calib_engine->get_triggered_calibration_state();
            if (!(_state == calibration_state::IDLE ||
                _state == calibration_state::COMPLETE))
            {
                LOG_ERROR("Calibration State is not Idle nor Complete - pleare restart the device");
                throw std::runtime_error("OCC triggerred when Calibration State is not Idle not Complete");
            }
        }
        
        if (_mode == calibration_mode::ABORT)
        {
            // calibration state to be IN_PROCESS
            _calib_engine->update_triggered_calibration_status();
            _state = _calib_engine->get_triggered_calibration_state();
            if (!(_state == calibration_state::PROCESS))
            {
                LOG_ERROR("Calibration State is not In Process - so it could not be aborted");
                throw std::runtime_error("OCC aborted when Calibration State is not In Process");
            }
        }
    }

    void d500_auto_calibrated::get_mode_from_json(const std::string& json)
    {
        if (json.find("calib dry run") != std::string::npos)
            _mode = calibration_mode::DRY_RUN;
        else if (json.find("calib commit") != std::string::npos)
            _mode = calibration_mode::COMMIT;
        else if (json.find("calib cancel") != std::string::npos)
            _mode = calibration_mode::ABORT;
        else if (json.find("calib try new") != std::string::npos)
        {
            _mode = calibration_mode::TRY;
            _try_selection = try_calibration_selection::NEW;
        }
        else if (json.find("calib try old") != std::string::npos)
        {
            _mode = calibration_mode::TRY;
            _try_selection = try_calibration_selection::OLD;
        }
        else if (json.find("calib run") != std::string::npos)
            _mode = calibration_mode::RUN;
        else if (json.find("calib abort") != std::string::npos)  // legacy alias, kept for D585S callers
            _mode = calibration_mode::ABORT;
        else
            throw std::runtime_error("run_on_chip_calibration called with wrong content in json file");
    }

    std::vector<uint8_t> d500_auto_calibrated::run_on_chip_calibration( int timeout_ms,
                                                                        std::string json,
                                                                        float * const health,
                                                                        rs2_update_progress_callback_sptr progress_callback )
    {
        bool is_d555 = false;
        auto dev = As< device >( _debug_dev );
        std::string dev_name = dev ? dev->get_info( RS2_CAMERA_INFO_NAME ) : "";
        if( dev_name.find( "D555" ) != std::string::npos )
            is_d555 = true;

        if( is_d555 )
            return run_occ( timeout_ms, json, health, progress_callback );

        if( device_uses_interactive_triggered_calibration() )
            return run_interactive_triggered_calibration( timeout_ms, json, health, progress_callback );

        return run_triggered_calibration( timeout_ms, json, progress_callback );
    }

    std::vector< uint8_t > d500_auto_calibrated::run_interactive_triggered_calibration( int timeout_ms,
                                                                                        std::string json,
                                                                                        float * const health,
                                                                                        rs2_update_progress_callback_sptr progress_callback )
    {
        _calib_engine->set_interactive_triggered_calibration_enabled( true );

        // Diagnostic: read the currently flashed depth calibration table and log its CRC32. Called before RUN and
        // after COMMIT so users can tell whether the FW actually flashed a new table (differing CRC across the
        // two log lines) or silently no-op'd (same CRC → intrinsics won't visibly change even after COMMIT).
        auto log_flashed_depth_crc = [this]( const char * label )
        {
            try
            {
                std::vector< uint8_t > dummy;
                auto raw = _calib_engine->get_calibration_table( dummy );
                if( raw.size() >= sizeof( ds::table_header ) )
                {
                    auto hdr = reinterpret_cast< const ds::table_header * >( raw.data() );
                    LOG_DEBUG( "Interactive TC: depth calibration CRC32 "
                               << label << " = 0x" << std::hex << hdr->crc32 << std::dec );
                }
            }
            catch( ... )
            {
                // best-effort diagnostic — silence
            }
        };

        // Diagnostic: read the RAM-active depth calibration table (as opposed to the flashed one above) and log its
        // CRC32. Used around TRY_NEW / TRY_OLD to tell whether the FW is actually switching the RAM-active table on
        // TRY — CRC change confirms the switch; same CRC means FW acked the TRY but didn't swap the active table.
        auto log_ram_depth_crc = [this]( const char * label )
        {
            try
            {
                auto cmd = _debug_dev->build_command(
                    ds::GET_HKR_CONFIG_TABLE,
                    static_cast< int >( ds::d500_calib_location::d500_calib_ram_memory ),
                    static_cast< int >( ds::d500_calibration_table_id::depth_calibration_id ),
                    static_cast< int >( ds::d500_calib_type::d500_calib_dynamic ) );
                auto raw = _debug_dev->send_receive_raw_data( cmd );
                // Slice the 4-byte opcode prefix that HW-monitor commands echo back.
                if( raw.size() > 4 + sizeof( ds::table_header ) )
                {
                    raw.erase( raw.begin(), raw.begin() + 4 );
                    auto hdr = reinterpret_cast< const ds::table_header * >( raw.data() );
                    LOG_DEBUG( "Interactive TC: RAM depth calibration CRC32 "
                               << label << " = 0x" << std::hex << hdr->crc32 << std::dec );
                }
            }
            catch( ... )
            {
                // best-effort diagnostic — silence
            }
        };

        try
        {
            get_mode_from_json( json );

            // RUN / DRY_RUN require the device to be in IDLE. Some FW builds silently no-op SET_CALIB_MODE(RUN)
            // when the state is HEALTH_CHECK (stale candidate) or COMPLETE (prior successful run), leaving the
            // poll loop seeing the previous terminal state immediately and reporting a bogus "1-second RUN".
            // Send a CANCEL first — spec §5.2 says CANCEL is valid from any state except FLASH_UPDATE — so the
            // subsequent SET_CALIB_MODE(RUN) hits a clean IDLE and the algorithm actually starts.
            if( _mode == calibration_mode::RUN || _mode == calibration_mode::DRY_RUN )
            {
                _calib_engine->update_triggered_calibration_status();
                _state = _calib_engine->get_triggered_calibration_state();

                if( _state == calibration_state::HEALTH_CHECK || _state == calibration_state::COMPLETE )
                {
                    LOG_INFO( "Interactive TC: FW at "
                              << calibration_state_strings[static_cast<int>(_state)]
                              << " - sending CANCEL to return to Idle before RUN" );
                    cancel_and_wait_for_idle();
                    _state = _calib_engine->get_triggered_calibration_state();
                }

                if( _state != calibration_state::IDLE )
                {
                    LOG_ERROR( "Interactive TC RUN rejected: state = "
                               << calibration_state_strings[static_cast<int>(_state)]
                               << " (expected Idle)" );
                    throw std::runtime_error( "Interactive TC RUN requires state to be Idle" );
                }

                log_flashed_depth_crc( "before RUN" );
            }

            // TRY is a live-preview toggle at HEALTH_CHECK; does not change state and does not poll.
            if( _mode == calibration_mode::TRY )
            {
                const std::string sel = ( _try_selection == try_calibration_selection::NEW ) ? "NEW" : "OLD";
                log_ram_depth_crc( ( "before TRY " + sel ).c_str() );
                _calib_engine->run_triggered_calibration_try( _try_selection );
                // FW switched the active table in RAM — invalidate host-side calibration caches so any subsequent
                // get_intrinsics() re-reads. The viewer does NOT restart the depth stream (per FW-team guidance —
                // a USB reconfigure at this point prevents FW from applying the swap), so the running pipeline
                // keeps its baked intrinsics until the next stream restart; only a fresh query sees the switch.
                for( auto & cb : _depth_write_callbacks )
                    cb();
                log_ram_depth_crc( ( "after TRY " + sel ).c_str() );
                return {};
            }

            // For COMMIT / ABORT the SET_CALIB_MODE is a one-shot; state polling picks up FLASH_UPDATE / IDLE from there.
            _calib_engine->run_triggered_calibration( _mode );

            if( _mode == calibration_mode::ABORT )
                return update_abort_status();

            // RUN / DRY_RUN / COMMIT — poll until we hit a terminal state for this mode.
            auto res = update_interactive_calibration_status( timeout_ms, progress_callback );

            // COMMIT flashed a new depth calibration — invalidate host caches. Callers restart streams
            // (see viewer's restore_workspace) so the new table is picked up on the next intrinsics query.
            if( _mode == calibration_mode::COMMIT && _state == calibration_state::COMPLETE )
            {
                for( auto & cb : _depth_write_callbacks )
                    cb();
                log_flashed_depth_crc( "after COMMIT" );
            }

            // A RUN or COMMIT that didn't reach a clean terminal (SUCCESS + expected state) leaves FW in a non-IDLE
            // state (HEALTH_CHECK with a stale candidate on failed RUN, PROCESS on FAILED_TO_RUN, etc.). Send a
            // best-effort CANCEL so FW returns to IDLE and the next RUN's precondition guard doesn't reject the retry.
            //
            // Hard exception: never issue CANCEL while FW is in FLASH_UPDATE — spec §5.2 explicitly forbids it, and
            // interrupting a mid-flash write has bricked devices in the field. Reachable here via the
            // FAILED_TO_RUN/FAILED_TO_CONVERGE break in the poll loop, which fires before the state check.
            const bool run_failed    = ( _mode == calibration_mode::RUN || _mode == calibration_mode::DRY_RUN )
                                    && _result != calibration_result::SUCCESS;
            const bool commit_failed = _mode == calibration_mode::COMMIT
                                    && ( _state != calibration_state::COMPLETE
                                         || _result != calibration_result::SUCCESS );
            if( ( run_failed || commit_failed ) && _state != calibration_state::FLASH_UPDATE )
            {
                cancel_and_wait_for_idle();
            }

            if( health )
            {
                // Trust health only when FW reports SUCCESS and populated the HEALTH_CHECK/COMPLETE payload.
                // Anything else (UNKNOWN because FW never wrote the result byte, FAILED_TO_CONVERGE, FAILED_TO_RUN,
                // or a mid-run terminal state) → -1.f sentinel so the viewer's health_passes() locks the Commit
                // button rather than treating a zero-initialized rect_health as PASS.
                const bool have_payload = _state == calibration_state::HEALTH_CHECK
                                       || _state == calibration_state::COMPLETE;
                if( have_payload && _result == calibration_result::SUCCESS )
                {
                    auto h = _calib_engine->get_triggered_calibration_health();
                    *health = h.rect_health;   // primary pass/fail metric; full struct available via engine
                }
                else
                {
                    *health = -1.f;
                }
            }
            return res;
        }
        catch( std::runtime_error & )
        {
            throw;
        }
        catch( ... )
        {
            throw std::runtime_error( rsutils::string::from() << "Interactive triggered calibration could not be triggered" );
        }
    }

    std::vector< uint8_t > d500_auto_calibrated::update_interactive_calibration_status( int timeout_ms,
                                                                                        rs2_update_progress_callback_sptr progress_callback )
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector< uint8_t > res;
        do
        {
            std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
            _calib_engine->update_triggered_calibration_status();

            _state = _calib_engine->get_triggered_calibration_state();
            _result = _calib_engine->get_triggered_calibration_result();
            {
                std::stringstream ss;
                ss << "Calibration in progress - State = " << calibration_state_strings[static_cast<int>(_state)];
                if( _state == calibration_state::PROCESS )
                {
                    ss << ", progress = " << static_cast<int>( _calib_engine->get_triggered_calibration_progress() );
                    ss << ", result = " << calibration_result_strings[static_cast<int>(_result)];
                }
                else if( _state == calibration_state::HEALTH_CHECK
                      || _state == calibration_state::COMPLETE )
                {
                    ss << ", result = " << calibration_result_strings[static_cast<int>(_result)];
                    if( _state == calibration_state::HEALTH_CHECK )
                    {
                        auto h = _calib_engine->get_triggered_calibration_health();
                        ss << ", rect_health = " << h.rect_health << " px"
                           << " (pass range = [0, " << rect_health_pass_threshold_px << ") px)";
                    }
                }
                LOG_INFO( ss.str().c_str() );
            }
            if( progress_callback )
                progress_callback->on_update_progress( _calib_engine->get_triggered_calibration_progress() );

            // Either failure result should break the loop; otherwise a stuck-in-PROCESS + failed result would spin until timeout.
            // FAILED_TO_CONVERGE is a legitimate outcome at HEALTH_CHECK — surfaced to the caller via _result rather than throwing here.
            if( _result == calibration_result::FAILED_TO_RUN
                || _result == calibration_result::FAILED_TO_CONVERGE )
                break;

            // Terminal states depend on the mode:
            // - RUN / DRY_RUN: stop at HEALTH_CHECK — host inspects health then calls again with commit/cancel.
            // - COMMIT: stop at COMPLETE (or IDLE if there was nothing to persist).
            if( _state == calibration_state::IDLE )
                break;
            if( _state == calibration_state::COMPLETE )
                break;
            if( _mode != calibration_mode::COMMIT && _state == calibration_state::HEALTH_CHECK )
                break;

            if( std::chrono::high_resolution_clock::now() - start_time > std::chrono::milliseconds( timeout_ms ) )
                throw std::runtime_error( "Interactive triggered calibration timeout" );
        }
        while( true );

        if( _state == calibration_state::HEALTH_CHECK || _state == calibration_state::COMPLETE )
        {
            auto depth_calib = _calib_engine->get_depth_calibration();
            auto ptr = reinterpret_cast< uint8_t * >( &depth_calib );
            res.insert( res.begin(), ptr, ptr + sizeof( ds::d500_coefficients_table ) );
        }
        else if( _result == calibration_result::FAILED_TO_RUN )
        {
            throw std::runtime_error( "Interactive triggered calibration failed to run" );
        }

        return res;
    }

    std::vector< uint8_t > d500_auto_calibrated::run_triggered_calibration( int timeout_ms,
                                                                            std::string json,
                                                                            rs2_update_progress_callback_sptr progress_callback )
    {
        std::vector< uint8_t > res;

        try
        {
            get_mode_from_json( json );

            // checking preconditions
            check_preconditions_and_set_state();

            // sending command to start calibration
            res = _calib_engine->run_triggered_calibration( _mode );

            if( _mode == calibration_mode::RUN || _mode == calibration_mode::DRY_RUN )
            {
                res = update_calibration_status( timeout_ms, progress_callback );
            }
            else if( _mode == calibration_mode::ABORT )
            {
                res = update_abort_status();
            }
        }
        catch( std::runtime_error & )
        {
            throw;
        }
        catch( ... )
        {
            std::string error_message_prefix = "\nRUN OCC ";
            if( _mode == calibration_mode::DRY_RUN )
                error_message_prefix = "\nDRY RUN OCC ";
            else if( _mode == calibration_mode::ABORT )
                error_message_prefix = "\nABORT OCC ";

            throw std::runtime_error( rsutils::string::from() << error_message_prefix + "Could not be triggered" );
        }

        return res;
    }

    std::vector< uint8_t > d500_auto_calibrated::run_occ( int timeout_ms, std::string json, float * const health,
                                                          rs2_update_progress_callback_sptr progress_callback )
    {
        int speed = ds_calib_common::SPEED_SLOW;
        int scan_parameter = ds_calib_common::PY_SCAN;
        int data_sampling = ds_calib_common::INTERRUPT;

        volatile thermal_compensation_guard grd(this); //Enforce Thermal Compensation off during OCC

        if (json.size() > 0)
        {
            auto jsn = ds_calib_common::parse_json( json );

            ds_calib_common::update_value_if_exists( jsn, "speed", speed );
            ds_calib_common::update_value_if_exists( jsn, "scan parameter", scan_parameter );
            ds_calib_common::update_value_if_exists( jsn, "data sampling", data_sampling );

            ds_calib_common::check_params( speed, scan_parameter, data_sampling );
        }

        LOG_DEBUG("run_on_chip_calibration with parameters: speed = " << speed << " scan_parameter = " << scan_parameter << " data_sampling = " << data_sampling);

        ds_calib_common::param4 p4;
        p4.field.scan_parameter = scan_parameter;
        p4.field.data_sampling = data_sampling;

        // Begin auto-calibration
        auto cmd = _debug_dev->build_command( ds::AUTO_CALIB, ds_calib_common::PY_RX_CALIB_BEGIN, speed, 0, p4.as_uint32 );
        _debug_dev->send_receive_raw_data( cmd );

        ds_calib_common::dsc_check_status_result result = get_calibration_status(timeout_ms, [progress_callback, speed](int count)
        {
            if( progress_callback )
                progress_callback->on_update_progress( count * speed * 1.2f );  // Currently this number does not reflect the actual progress
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Handle errors from firmware
        if( result.status != ds_calib_common::STATUS_SUCCESS )
        {
            ds_calib_common::handle_calibration_error( result.status );
        }

        std::vector< uint8_t > res = get_calibration_results( health );

        if (progress_callback)
            progress_callback->on_update_progress(static_cast<float>(100));

        return res;
    }

    std::vector<uint8_t> d500_auto_calibrated::update_calibration_status(int timeout_ms,
        rs2_update_progress_callback_sptr progress_callback)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> res;
        do
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            _calib_engine->update_triggered_calibration_status();

            _state = _calib_engine->get_triggered_calibration_state();
            _result = _calib_engine->get_triggered_calibration_result();
            std::stringstream ss;
            ss << "Calibration in progress - State = " << calibration_state_strings[static_cast<int>(_state)];
            if (_state == calibration_state::PROCESS)
            {
                ss << ", progress = " << static_cast<int>(_calib_engine->get_triggered_calibration_progress());
                ss << ", result = " << calibration_result_strings[static_cast<int>(_result)];
            }
            LOG_INFO(ss.str().c_str());
            if (progress_callback)
            {
                progress_callback->on_update_progress(_calib_engine->get_triggered_calibration_progress());
            }
            
            if (_result == calibration_result::FAILED_TO_RUN)
            {
                break;
            }
            bool is_timed_out(std::chrono::high_resolution_clock::now() - start_time > std::chrono::milliseconds(timeout_ms));
            if (is_timed_out)
            {
                throw std::runtime_error("OCC Calibration Timeout");
            }
        } while (_state != calibration_state::COMPLETE &&
            // if state is back to idle, it means that Abort action has been called
            _state != calibration_state::IDLE);

        // printing new calibration to log
        if (_state == calibration_state::COMPLETE)
        {
            if (_result == calibration_result::SUCCESS)
            {
                auto depth_calib = _calib_engine->get_depth_calibration();
                LOG_INFO("Depth new Calibration = \n" + depth_calib.to_string());
                auto depth_calib_start = reinterpret_cast<uint8_t*>(&depth_calib);
                res.insert(res.begin(), depth_calib_start, depth_calib_start + sizeof(ds::d500_coefficients_table));
            }
            else if (_result == calibration_result::FAILED_TO_CONVERGE)
            {
                LOG_ERROR("Calibration completed but algorithm failed");
                throw std::runtime_error("Calibration completed but algorithm failed");
            }
        }
        else
        {
            if (_result == calibration_result::FAILED_TO_RUN)
            {
                LOG_ERROR("Calibration failed to run");
                throw std::runtime_error("Calibration failed to run");
            }
        }

        return res;
    }

    std::vector<uint8_t> d500_auto_calibrated::update_abort_status()
    {
        std::vector<uint8_t> ans;
        _calib_engine->update_triggered_calibration_status();
        if (_calib_engine->get_triggered_calibration_state() == calibration_state::PROCESS)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            _calib_engine->update_triggered_calibration_status();
        }
        if (_calib_engine->get_triggered_calibration_state() == calibration_state::IDLE)
        {
            LOG_INFO("Depth Calibration Successfully Aborted");
            // returning success
            ans.push_back(1);
        }
        else
        {
            LOG_INFO("Depth Calibration Could not be Aborted");
            // returning failure
            ans.push_back(0);
        }
        return ans;
    }

    std::vector<uint8_t> d500_auto_calibrated::run_tare_calibration(int timeout_ms, float ground_truth_mm, std::string json, float* const health, rs2_update_progress_callback_sptr progress_callback)
    {
        int average_step_count = 20;
        int step_count = 20;
        int accuracy = ds_calib_common::ACCURACY_MEDIUM;
        int speed = ds_calib_common::SPEED_SLOW;
        int scan_parameter = ds_calib_common::PY_SCAN;
        int data_sampling = ds_calib_common::POLLING;
        int apply_preset = 1;

        volatile thermal_compensation_guard grd( this ); // Enforce Thermal Compensation off during Tare calibration

        if( json.size() > 0 )
        {
            auto jsn = ds_calib_common::parse_json( json );
            ds_calib_common::update_value_if_exists( jsn, "average step count", average_step_count );
            ds_calib_common::update_value_if_exists( jsn, "step count", step_count );
            ds_calib_common::update_value_if_exists( jsn, "accuracy", accuracy );
            ds_calib_common::update_value_if_exists( jsn, "speed", speed );
            ds_calib_common::update_value_if_exists( jsn, "scan parameter", scan_parameter );
            ds_calib_common::update_value_if_exists( jsn, "data sampling", data_sampling );
            ds_calib_common::update_value_if_exists( jsn, "apply preset", apply_preset );
        }

        std::shared_ptr< option > preset_recover;
        if( apply_preset )
        {
            preset_recover = change_preset();
            std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
        }

        LOG_DEBUG( "run_tare_calibration with parameters: speed = " << speed
                   << " average_step_count = " << average_step_count << " step_count = " << step_count
                   << " accuracy = " << accuracy << " scan_parameter = " << scan_parameter
                   << " data_sampling = " << data_sampling );
        ds_calib_common::check_tare_params( speed, scan_parameter, data_sampling, average_step_count, step_count, accuracy );

        auto p2 = static_cast< uint32_t >( ground_truth_mm ) * 100;

        ds_calib_common::param3 p3;
        p3.field.average_step_count = average_step_count;
        p3.field.step_count = step_count;
        p3.field.accuracy = accuracy;

        ds_calib_common::param4 p4;
        p4.field.scan_parameter = scan_parameter;
        p4.field.data_sampling = data_sampling;

        // Log the current preset
        //auto advanced_mode = dynamic_cast< ds_advanced_mode_base * >( this );
        //if( advanced_mode )
        //{
        //    auto cur_preset = (rs2_rs400_visual_preset)(int)advanced_mode->_preset_opt->query();
        //    LOG_DEBUG(
        //        "run_tare_calibration with preset: " << rs2_rs400_visual_preset_to_string( cur_preset ) );
        //}

        auto cmd = _debug_dev->build_command( ds::AUTO_CALIB, ds_calib_common::TARE_CALIB_BEGIN, p2, p3.as_uint32, p4.as_uint32 );
        _debug_dev->send_receive_raw_data( cmd );

        ds_calib_common::TareCalibrationResult result;

        // While not ready...
        int count = 0;
        bool done = false;

        std::vector< uint8_t > res;
        auto start = std::chrono::high_resolution_clock::now();
        auto now = start;
        do
        {
            memset( &result, 0, sizeof( ds_calib_common::TareCalibrationResult ) );
            std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );

            // Check calibration status
            try
            {
                auto cmd = _debug_dev->build_command( ds::AUTO_CALIB, ds_calib_common::TARE_CALIB_CHECK_STATUS );
                res = _debug_dev->send_receive_raw_data( cmd );
                if( res.size() < sizeof( ds_calib_common::TareCalibrationResult ) + hwm_header_size )
                {
                    throw std::runtime_error( "Not enough data from CALIB_STATUS!" );
                }

                res.erase( res.begin(), res.begin() + hwm_header_size );  // Slicing HWM command header
                result = *reinterpret_cast< ds_calib_common::TareCalibrationResult * >( res.data() );
                done = result.status != ds_calib_common::STATUS_RESULT_NOT_READY;
            }
            catch( const std::exception & ex )
            {
                LOG_INFO( ex.what() );
            }

            if( progress_callback )
            {
                progress_callback->on_update_progress( count++ * ( 2.f * speed ) );  // Currently this number does not reflect the actual progress
            }

            now = std::chrono::high_resolution_clock::now();
        }
        while( now - start < std::chrono::milliseconds( timeout_ms ) && ! done );

        // If we exit due to timeout, report timeout
        if( ! done )
        {
            throw std::runtime_error( "Operation timed-out!\nCalibration did not converge on time" );
        }

        uint8_t * p = res.data() + sizeof( ds_calib_common::TareCalibrationResult ) + 2 * result.iterations * sizeof( uint32_t );
        float * ph = reinterpret_cast< float * >( p );
        health[0] = ph[0];
        health[1] = ph[1];

        LOG_INFO( "Ground truth: " << ground_truth_mm << "mm" );
        LOG_INFO( "Health check numbers from TareCalibrationResult(0x0C): before=" << ph[0] << ", after=" << ph[1] );
        LOG_INFO( "Z calculated from health check numbers : before="
                  << ( ph[0] + 1 ) * ground_truth_mm << ", after=" << ( ph[1] + 1 ) * ground_truth_mm );

        // Handle errors from firmware
        if( result.status != ds_calib_common::STATUS_SUCCESS )
            ds_calib_common::handle_calibration_error( result.status );

        res = get_calibration_results();

        if( progress_callback )
            progress_callback->on_update_progress( static_cast< float >( 100 ) );

        return res;
    }

    std::vector<uint8_t> d500_auto_calibrated::process_calibration_frame(int timeout_ms, const rs2_frame* f, float* const health, rs2_update_progress_callback_sptr progress_callback)
    {
        throw not_implemented_exception(rsutils::string::from() << "Process Calibration Frame not applicable for this device");
    }

    std::vector<uint8_t> d500_auto_calibrated::get_calibration_table() const
    {
        return _calib_engine->get_calibration_table(_curr_calibration);
    }

    void d500_auto_calibrated::write_calibration() const
    {
        _calib_engine->write_calibration(_curr_calibration);

        for( auto & cb : _depth_write_callbacks )
            cb();
        // Currently only depth calibrations are supported, call correct callbacks based on table type otherwise
        //for( auto & cb : _color_write_callbacks )
        //    cb();
    }

    void d500_auto_calibrated::set_calibration_table(const std::vector<uint8_t>& calibration)
    {
        auto table_header = reinterpret_cast< const ds::table_header * >( calibration.data() );

        size_t expected_size = sizeof( ds::d500_coefficients_table );
        if( table_header->table_type == static_cast< uint16_t >( ds::d500_calibration_table_id::rgb_calibration_id ) )
            expected_size = sizeof( ds::d500_rgb_calibration_table );

        if( calibration.size() != expected_size )
            throw std::runtime_error( rsutils::string::from()
                                      << "Setting calibration table with unexpected size" << calibration.size()
                                      << " while expecting " << expected_size );

        _curr_calibration = calibration;

        // Send updated calibration to RAM so it can be used. Will be saved only on write_calibration()
        auto cmd = _debug_dev->build_command( ds::SET_HKR_CONFIG_TABLE,
                                              static_cast< int >( ds::d500_calib_location::d500_calib_ram_memory ),
                                              static_cast< int >( table_header->table_type ),
                                              static_cast< int >( ds::d500_calib_type::d500_calib_dynamic ),
                                              0,
                                              _curr_calibration.data(),
                                              _curr_calibration.size() );
        _debug_dev->send_receive_raw_data( cmd );
    }

    void d500_auto_calibrated::reset_to_factory_calibration() const
    {
        auto cmd = _debug_dev->build_command(ds::CAL_RESTORE_DFLT);
        auto res = _debug_dev->send_receive_raw_data(cmd);

        for( auto & cb : _depth_write_callbacks )
            cb();
        for( auto & cb : _color_write_callbacks )
            cb();
    }

    std::vector< uint8_t > d500_auto_calibrated::run_focal_length_calibration( rs2_frame_queue * left,
                                                                               rs2_frame_queue * right,
                                                                               float target_w,
                                                                               float target_h,
                                                                               int adjust_both_sides,
                                                                               float * ratio,
                                                                               float * angle,
                                                                               rs2_update_progress_callback_sptr progress_callback )
    {
        float fx[2] = { -1.0f, -1.0f };
        float fy[2] = { -1.0f, -1.0f };

        float left_rect_sides[4] = { 0.f };
        ds_calib_common::get_target_rect_info( left, left_rect_sides, fx[0], fy[0], 50, progress_callback );  // Report 50% progress

        float right_rect_sides[4] = { 0.f };
        ds_calib_common::get_target_rect_info( right, right_rect_sides, fx[1], fy[1], 75, progress_callback );

        std::vector< uint8_t > ret;
        const float correction_factor = 0.5f;

        auto calib_table = get_calibration_table();
        auto table = reinterpret_cast< librealsense::ds::d500_coefficients_table * >( calib_table.data() );

        float ratio_to_apply = ds_calib_common::get_focal_length_correction_factor( left_rect_sides,
                                                                                    right_rect_sides,
                                                                                    fx,
                                                                                    fy,
                                                                                    target_w,
                                                                                    target_h,
                                                                                    table->baseline,
                                                                                    *ratio,
                                                                                    *angle );

        if( adjust_both_sides )
        {
            float ratio_to_apply_2 = sqrtf( ratio_to_apply );
            table->left_coefficients_table.base_instrinsics.fx /= ratio_to_apply_2;
            table->left_coefficients_table.base_instrinsics.fy /= ratio_to_apply_2;
            table->right_coefficients_table.base_instrinsics.fx *= ratio_to_apply_2;
            table->right_coefficients_table.base_instrinsics.fy *= ratio_to_apply_2;
        }
        else
        {
            table->right_coefficients_table.base_instrinsics.fx *= ratio_to_apply;
            table->right_coefficients_table.base_instrinsics.fy *= ratio_to_apply;
        }

        return calib_table;
    }

    std::vector<uint8_t> d500_auto_calibrated::run_uv_map_calibration(rs2_frame_queue* left, rs2_frame_queue* color, rs2_frame_queue* depth, int py_px_only,
        float* const health, int health_size, rs2_update_progress_callback_sptr progress_callback)
    {
        throw not_implemented_exception(rsutils::string::from() << "UV Map Calibration not applicable for this device");
    }

    float d500_auto_calibrated::calculate_target_z( rs2_frame_queue* queue1, rs2_frame_queue* queue2, rs2_frame_queue* queue3,
                                                    float target_w, float target_h,
                                                    rs2_update_progress_callback_sptr progress_callback )
    {
        constexpr size_t min_frames_required = 10;

        rs2_error * e = nullptr;
        int queue_size = rs2_frame_queue_size( queue1, &e );
        if( queue_size < min_frames_required )
            throw std::runtime_error( rsutils::string::from() << "Target distance calculation requires at least "
                                                                << min_frames_required << " frames, aborting" );

        float target_fw = 0;
        float target_fh = 0;
        std::array< float, 4 > rect_sides{};
        ds_calib_common::get_target_rect_info( queue1, rect_sides.data(), target_fw, target_fh, 50, progress_callback ); // Report 50% progress

        float gt[4] = { 0 };
        gt[0] = target_fw * target_w / rect_sides[0];
        gt[1] = target_fw * target_w / rect_sides[1];
        gt[2] = target_fh * target_h / rect_sides[2];
        gt[3] = target_fh * target_h / rect_sides[3];

        if( gt[0] <= 0.1f || gt[1] <= 0.1f || gt[2] <= 0.1f || gt[3] <= 0.1f )
            throw std::runtime_error( "Target distance calculation failed" );

        // Target's plane Z value is the average of the four calculated corners
        float target_z_value = 0.f;
        for( int i = 0; i < 4; ++i )
            target_z_value += gt[i];
        target_z_value /= 4.f;

        return target_z_value;
    }

    std::string d500_auto_calibrated::get_calibration_config() const
    {
        return _calib_engine->get_calibration_config();
    }

    void d500_auto_calibrated::set_calibration_config(const std::string& calibration_config_json_str) const
    {
        _calib_engine->set_calibration_config(calibration_config_json_str);
    }

    ds_calib_common::dsc_check_status_result
    d500_auto_calibrated::get_calibration_status( int timeout_ms,
                                                  std::function< void( const int count ) > progress_func,
                                                  bool wait_for_final_results ) const
    {
        ds_calib_common::dsc_check_status_result result{};

        int count = 0;
        int retries = 0;
        bool done = false;

        auto start = std::chrono::high_resolution_clock::now();
        auto now = start;

        // While not ready...
        do
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
            try
            {
                // Check calibration status
                auto cmd = _debug_dev->build_command( ds::AUTO_CALIB, ds_calib_common::PY_RX_CALIB_CHECK_STATUS );
                auto res = _debug_dev->send_receive_raw_data( cmd );
                if( res.size() < sizeof( ds_calib_common::dsc_check_status_result ) + hwm_header_size )
                {
                    if( ! ( ( retries++ ) % 5 ) ) // Add log debug once a sec
                        LOG_DEBUG( "Not enough data from CALIB_STATUS!" );
                }
                else
                {
                    res.erase( res.begin(), res.begin() + hwm_header_size ); // Slicing HWM command header
                    result = *reinterpret_cast< ds_calib_common::dsc_check_status_result * >( res.data() );
                    done = ! wait_for_final_results || result.status != ds_calib_common::STATUS_RESULT_NOT_READY;
                }
            }
            catch( const invalid_value_exception & e )
            {
                LOG_DEBUG( "error: " << e.what() ); // Asked for status while firmware is still in progress.
            }

            if( progress_func )
            {
                progress_func( count++ );
            }

            now = std::chrono::high_resolution_clock::now();
        }
        while( now - start < std::chrono::milliseconds( timeout_ms ) && ! done );


        // If we exit due to timeout, report timeout
        if( ! done )
            throw std::runtime_error( "Operation timed-out!\nCalibration state did not converge on time" );

        return result;
    }

    std::vector< uint8_t > d500_auto_calibrated::get_calibration_results( float * const health ) const
    {
        // Get new calibration from the firmware
        auto cmd = _debug_dev->build_command( ds::AUTO_CALIB, ds_calib_common::GET_CALIBRATION_RESULT );
        auto res = _debug_dev->send_receive_raw_data( cmd );
        if( res.size() < sizeof( ds_calib_common::dsc_result ) + hwm_header_size )
            throw std::runtime_error( "Not enough data from CALIB_STATUS!" );

        res.erase( res.begin(), res.begin() + hwm_header_size );  // Slicing HWM command header
        auto * header = reinterpret_cast< ds::table_header * >( res.data() + sizeof( ds_calib_common::dsc_result ) );
        if( res.size() < sizeof( ds_calib_common::dsc_result ) + sizeof( ds::table_header ) + header->table_size )
            throw std::runtime_error( "Table truncated in CALIB_STATUS!" );

        std::vector< uint8_t > calib;
        calib.resize( sizeof( ds::table_header ) + header->table_size, 0 );
        memcpy( calib.data(), header, calib.size() );  // Copy to new_calib

        auto result = reinterpret_cast< ds_calib_common::dsc_result * >( res.data() );
        if( health )
            *health = result->healthCheck;

        return calib;
    }

    std::shared_ptr< option > d500_auto_calibrated::change_preset()
    {
        if( ! _depth_sensor )
            throw not_implemented_exception( "Depth sensor must be supplied to d500_auto_calibrated" );

        if( _depth_sensor->supports_option( RS2_OPTION_VISUAL_PRESET ) )
        {
            auto & opt = _depth_sensor->get_option( RS2_OPTION_VISUAL_PRESET );
            auto old_preset = opt.get_value();
            switch( opt.get_value_type() )
            {
                case RS2_OPTION_TYPE_FLOAT: // USB visual preset type is float
                    if( old_preset == RS2_RS400_VISUAL_PRESET_CUSTOM )
                        throw not_implemented_exception( "Calibration with custom visual preset is not supported" );
                    opt.set_value( RS2_RS400_VISUAL_PRESET_HIGH_ACCURACY );
                    break;
                case RS2_OPTION_TYPE_STRING:  // DDS visual preset type is a string
                    opt.set_value( std::string( "High Accuracy") );
                    break;
                default:
                    throw invalid_value_exception( "Unsupported option type" );
            }

            std::shared_ptr< option > recover_option( &opt, [old_preset]( option * opt )
            {
                opt->set_value( old_preset );
            });

            return recover_option;
        }

        return {};
    }
}
