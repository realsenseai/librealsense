// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once
#include <librealsense2/rs.hpp>
#include <rsutils/time/stopwatch.h>
#include <atomic>
#include <mutex>
namespace rs2
{
    struct notifications_model;
    class subdevice_model;

    // Holds the cross-thread state for async option writes: the latest FW error
    // message, plus a "completed write" notification carrying the value that was
    // actually sent to FW. Written by the dispatcher action, read+cleared by the
    // UI thread in option_model::draw_option. Held via shared_ptr by both
    // option_model and the dispatcher action so the action can outlive option_model
    // without dangling — the action captures shared_ptrs by value, never `this`,
    // so destruction of option_model is UAF-safe.
    struct option_async_state
    {
        std::mutex mutex;
        std::string last_error;     // non-empty = pending error to surface
        bool did_write = false;     // a FW write completed since the last drain
        float written_value = 0.f;  // value that was sent to FW
    };

    class option_model
    {
    public:
        bool draw( std::string& error_message, notifications_model& model, bool new_line = true, bool use_option_name = true );
        void update_supported( std::string& error_message );
        void update_read_only_status( std::string& error_message );
        void update_all_fields( std::string& error_message, notifications_model& model );
        // Synchronous option write — thin wrapper around the rs2 API set_option,
        // followed by a readback to refresh the cached `value`. Blocks the caller for
        // the FW round-trip (~200 ms on UVC), so do NOT call from the UI thread when
        // a freeze would be visible — use set_option_async() for UI-thread writes.
        bool set_option( rs2_option opt,
            float req_value,
            std::string & error_message,
            std::chrono::steady_clock::duration ignore_period = std::chrono::seconds( 0 ) );
        // Fire-and-forget option write. Dispatches via the subdevice dispatcher so
        // the UI thread is never blocked on the FW round-trip. FW errors are surfaced
        // asynchronously through the next periodic readback (update_all_fields), so
        // this method has no error-out parameter and no return value.
        void set_option_async( rs2_option opt, float value );
        // Blocking variant of set_option_async: routes through the same per-subdevice
        // dispatcher so it can't race with concurrent UI writes on the USB bus, but
        // waits for the action to run before returning. Used by on_chip_calib for
        // set+verify semantics.
        void set_option_sync( float value );
        bool draw_option( bool update_read_only_options, bool is_streaming,
            std::string& error_message, notifications_model& model );

        std::vector< const char * > get_combo_labels( int * p_selected = nullptr ) const;
        std::string value_as_string() const;
        float value_as_float() const;

        void update_value( const rs2::option_value & updated_value, notifications_model & model );

        rs2_option opt;
        option_range range;
        std::shared_ptr<options> endpoint;
        rsutils::time::stopwatch last_set_stopwatch;
        rsutils::time::stopwatch last_slider_hold_stopwatch;
        bool* invalidate_flag = nullptr;
        bool supported = false;
        bool read_only = false;
        rs2::option_value value;
        std::string label;
        std::string id;
        subdevice_model* dev;
        std::function<bool( option_model&, std::string&, notifications_model& )> custom_draw_method = nullptr;
        bool edit_mode = false;
        std::string edit_value;
        bool is_all_integers() const;
        bool is_enum() const;
        bool is_checkbox() const;
    private:
        bool draw_checkbox( notifications_model& model, std::string& error_message, const char* description );
        bool draw_combobox( notifications_model& model, std::string& error_message, const char* description, bool new_line, bool use_option_name );
        bool draw_slider( notifications_model& model, std::string& error_message, const char* description, bool use_cm_units );
        bool slider_selected( rs2_option opt,
            float value,
            std::string& error_message,
            notifications_model& model );

        bool slider_unselected( rs2_option opt,
            float value,
            std::string& error_message,
            notifications_model& model );

        // Guard for the public entry points that take an `opt` parameter
        // (set_option, set_option_async, slider_selected): an option_model is bound
        // to a single rs2_option for its lifetime, so a caller passing a different
        // opt is a bug — throw rather than silently write to the wrong option.
        // `caller` is used to make the error message tell the reader which entry
        // point detected the mismatch (pass __func__).
        void check_opt( rs2_option opt, char const * caller ) const;

        std::string adjust_description( const std::string& str_in, const std::string& to_be_replaced, const std::string& to_replace );

        // Per-option coalescing state for the subdevice dispatcher. Every UI-thread
        // set_option_async stores into _latest_pending_value, then atomically claims
        // the right to enqueue a dispatcher action via _has_pending_job (CAS false→true).
        // The action clears _has_pending_job at entry and reads back the latest value, so
        // any post() arriving during the FW call re-enqueues for that update.
        // Held via shared_ptr so the dispatcher action (which captures these by value)
        // is UAF-safe across option_model destruction, AND so option_model stays
        // copyable (std::atomic deletes its copy ctor).
        std::shared_ptr< std::atomic< float > > _latest_pending_value
            = std::make_shared< std::atomic< float > >( 0.f );
        std::shared_ptr< std::atomic< bool > > _has_pending_job
            = std::make_shared< std::atomic< bool > >( false );

        // Tracks the value the user just requested via the slider / checkbox / edit
        // field, and is displayed locally until the firmware confirms (or contradicts)
        // it. Without this mask, the slider would visually snap back to the stale
        // cached `value` for ~1 s between when set_option_async dispatches the write
        // and when the FW echo arrives back via either
        //   - sensor::on_options_changed -> option_model::update_value, or
        //   - the post-gate option_model::update_all_fields poll,
        // then jump forward again to the new value. With the mask in place, the slider
        // visually stays at the user's requested value across that interval.
        //
        // The mask is cleared as soon as either authoritative path refreshes `value`
        // above, or after a 2-second timeout in value_as_float() — the timeout covers
        // the case where the FW rejects/clamps the request and so never echoes back a
        // value matching what the user asked for.
        //
        // _has_user_request is read on the UI thread (value_as_float) and written from
        // both the UI thread (set_option_async, update_all_fields) AND the
        // sensor::on_options_changed callback thread (update_value), so it must be
        // atomic. We hold it via shared_ptr so option_model stays copyable/movable
        // (std::atomic itself is neither). _user_request_value and _user_request_stopwatch
        // are only written on the UI thread, between a false→true transition of the
        // flag, so readers that load `_has_user_request == true` first see consistent
        // values for the other two fields without further synchronization.
        std::shared_ptr< std::atomic< bool > > _has_user_request = std::make_shared< std::atomic< bool > >( false );
        float _user_request_value = 0.f;
        rsutils::time::stopwatch _user_request_stopwatch;

        // Cross-thread async-error state, see option_async_state for layout. Eagerly
        // allocated so option_model copies (e.g., map-insertion of create_option_model's
        // return value) keep the same state; the worker callback captures this
        // shared_ptr by value, so the state outlives option_model if the worker is
        // still in mid-FW-call when option_model destructs.
        std::shared_ptr< option_async_state > _async_state = std::make_shared< option_async_state >();
    };

    option_model create_option_model(option_value const & opt,
        const std::string& opt_base_label,
        subdevice_model* model,
        std::shared_ptr<options> options,
        bool* options_invalidated,
        std::string& error_message);
}
