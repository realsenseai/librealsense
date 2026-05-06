// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once
#include <librealsense2/rs.hpp>
#include <rsutils/time/stopwatch.h>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace rs2
{
    struct notifications_model;
    class subdevice_model;

    // Background worker that applies option writes to the device asynchronously.
    // Coalescing: only the latest posted value between two FW round-trips is applied,
    // so spamming post() (e.g., during slider drag) never queues up stale values.
    class option_async_setter
    {
    public:
        option_async_setter( std::shared_ptr< options > endpoint, rs2_option opt );
        ~option_async_setter();

        option_async_setter( option_async_setter const & ) = delete;
        option_async_setter & operator=( option_async_setter const & ) = delete;

        void post( float value );

    private:
        void run();

        std::shared_ptr< options > _endpoint;
        rs2_option _opt;
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
        bool set_option( rs2_option opt,
            float value,
            std::string& error_message,
            std::chrono::steady_clock::duration ignore_period = std::chrono::seconds( 0 ) );
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
        void set_option_async( rs2_option opt, float value );

        std::string adjust_description( const std::string& str_in, const std::string& to_be_replaced, const std::string& to_replace );

        // Lazily created on first async dispatch; shared so option_model stays copyable.
        std::shared_ptr< option_async_setter > _async_setter;

        // Optimistic local cache so the slider doesn't visually snap back to the stale
        // `value` between dispatch (set_option_async) and the FW echo arriving via
        // options_watcher -> update_value or the post-gate update_all_fields poll.
        // Cleared when either authoritative path refreshes `value`, or after the timeout
        // below (in case FW rejects/clamps and never echoes the requested value).
        float _optimistic_value = 0.f;
        bool _has_optimistic = false;
        rsutils::time::stopwatch _optimistic_stopwatch;
    };

    option_model create_option_model(option_value const & opt,
        const std::string& opt_base_label,
        subdevice_model* model,
        std::shared_ptr<options> options,
        bool* options_invalidated,
        std::string& error_message);
}
