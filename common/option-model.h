// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once
#include <librealsense2/rs.hpp>
#include <rsutils/time/stopwatch.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace rs2
{
    struct notifications_model;
    class subdevice_model;

    // Holds the cross-thread state for async option writes: the latest FW error
    // message (written by the worker thread, read+cleared by the UI thread). Held
    // via shared_ptr by both option_model and the worker callback so the worker
    // can outlive option_model without dangling — the worker captures shared_ptrs
    // by value, never `this`, so destruction of option_model is UAF-safe.
    struct option_async_state
    {
        std::mutex mutex;
        std::string last_error;  // non-empty = pending error to surface
    };

    // Background worker that applies option writes to the device asynchronously.
    // Coalescing: only the latest posted value between two FW round-trips is applied,
    // so spamming post() (e.g., during slider drag) never queues up stale values.
    class option_async_setter
    {
    public:
        // Optional callback invoked from the worker thread when set_option throws
        // (rs2::error / std::exception / unknown). Receives a human-readable message.
        // Used to surface FW rejections (e.g., "exposure can't be changed with AE
        // enabled") back to the UI thread, where they become notifications.
        using error_callback = std::function< void( std::string const & ) >;

        option_async_setter( std::shared_ptr< options > endpoint, rs2_option opt, error_callback on_error = {} );
        ~option_async_setter();

        option_async_setter( option_async_setter const & ) = delete;
        option_async_setter & operator=( option_async_setter const & ) = delete;

        void post( float value );

    private:
        void run();

        std::shared_ptr< options > _endpoint;
        rs2_option _opt;
        error_callback _on_error;
        std::mutex _mtx;
        std::condition_variable _cv;
        float _pending_value = 0.f;
        bool _has_pending = false;
        bool _stop = false;
        std::thread _worker;
    };

    class option_model
    {
    public:
        bool draw( std::string& error_message, notifications_model& model, bool new_line = true, bool use_option_name = true );
        void update_supported( std::string& error_message );
        void update_read_only_status( std::string& error_message );
        void update_all_fields( std::string& error_message, notifications_model& model );
        // Fire-and-forget option write. Always dispatches via the async worker;
        // FW errors are surfaced asynchronously through the next periodic readback
        // (option_model::update_all_fields), so this method has no error-out parameter
        // and no return value. The previous synchronous error_message / ignore_period
        // parameters are no longer applicable.
        void set_option( rs2_option opt, float value );
        bool draw_option( bool update_read_only_options, bool is_streaming,
            std::string& error_message, notifications_model& model );

        std::vector< const char * > get_combo_labels( int * p_selected = nullptr ) const;
        std::string value_as_string() const;
        float value_as_float() const;

        void update_value( const rs2::option_value & updated_value, notifications_model & model );

        rs2_option opt;
        option_range range;
        std::shared_ptr<options> endpoint;
        float unset_value = 0;
        bool have_unset_value = false;
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

        // Dispatches the option write to a background worker so the UI thread
        // (and therefore the viewer's render loop) is not blocked on the FW round-trip.
        // Always uses `this->opt` as the target — option_model is bound to a single
        // rs2_option at construction, so a separate `opt` parameter would only invite
        // mismatch bugs.
        void set_option_async( float value );

        // Guard for the public entry points that still take an `opt` parameter
        // (set_option, slider_selected): an option_model is bound to a single
        // rs2_option for its lifetime, so a caller passing a different opt is a bug
        // — throw rather than silently write to the wrong option through the cached
        // _async_setter. `caller` is used to make the error message tell the reader
        // which entry point detected the mismatch (pass __func__).
        void check_opt( rs2_option opt, char const * caller ) const;

        std::string adjust_description( const std::string& str_in, const std::string& to_be_replaced, const std::string& to_replace );

        // Lazily created on first async dispatch; shared so option_model stays copyable.
        std::shared_ptr< option_async_setter > _async_setter;

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
