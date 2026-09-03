// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include <functional>
#include <string>


namespace rs2
{
    class subdevice_model;
    class option_model;
    class viewer_model;

    class embedded_filter_model
    {
    public:
        embedded_filter_model( subdevice_model* owner,
            const rs2_embedded_filter_type& type,
            std::shared_ptr<rs2::embedded_filter> filter,
            viewer_model& viewer,
            std::string& error_message);

        virtual ~embedded_filter_model();

        const std::string& get_name() const { return _name; }

        void populate_options( const std::string& opt_base_label,
            subdevice_model* model,
            bool* options_invalidated,
            std::string& error_message );

        std::shared_ptr<rs2::embedded_filter> get_filter() { return _embedded_filter; }

        // The options this filter draws, in map order - all but the enable option, which is this
        // filter's own toggle beside its name
        std::vector< option_model * > drawable_options();

        void enable( bool e = true )
        {
            embedded_filter_enable_disable( e );
        }
        bool is_enabled() const { return _enabled; }

        // Optional predicate; null means always available.
        // When it returns false the enable toggle is grayed out in the UI.
        // Set by the owner after construction for filters with runtime constraints
        // (e.g. close range, which works on depth only and must be off while color streams).
        std::function<bool()> available_predicate;
        // Optional message shown when the toggle is unavailable; empty = none.
        std::string unavailable_tooltip;

        bool is_available() const { return !available_predicate || available_predicate(); }

        void embedded_filter_enable_disable(bool actual);

    protected:
        viewer_model& _viewer;
        std::atomic<bool> _destructing;
        bool _enabled = true;
        std::shared_ptr<rs2::embedded_filter> _embedded_filter;
        std::map< rs2_option, option_model > _options_id_to_model;
        std::string _name;
    };
}
