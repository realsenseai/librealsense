// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once
#include <librealsense2/rs.hpp>
#include <rsutils/time/stopwatch.h>
namespace rs2
{
    struct notifications_model;
    class subdevice_model;

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

        std::string adjust_description( const std::string& str_in, const std::string& to_be_replaced, const std::string& to_replace );
    };

    option_model create_option_model(option_value const & opt,
        const std::string& opt_base_label,
        subdevice_model* model,
        std::shared_ptr<options> options,
        bool* options_invalidated,
        std::string& error_message);
}
