// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include <functional>
#include <string>


namespace rs2
{
    class subdevice_model;
    class option_model;
    class viewer_model;

    class processing_block_model
    {
    public:
        processing_block_model( subdevice_model* owner,
            const std::string& name,
            std::shared_ptr<rs2::filter> block,
            std::function<rs2::frame( rs2::frame )> invoker,
            std::string& error_message,
            bool enabled = true );
        virtual ~processing_block_model() = default;

        const std::string& get_name() const { return _name; }

        rs2::frame invoke( rs2::frame f ) const { return _invoker( f ); }

        void save_to_config_file();

        void populate_options( const std::string& opt_base_label,
            subdevice_model* model,
            bool* options_invalidated,
            std::string& error_message );

        void draw_options( viewer_model & viewer,
                           bool update_read_only_options,
                           bool is_streaming,
                           std::string & error_message );

        std::shared_ptr<rs2::filter> get_block() { return _block; }

        void enable( bool e = true )
        {
            processing_block_enable_disable( _enabled = e );
        }
        bool is_enabled() const { return _enabled; }

        bool visible = true;

        // Optional predicate; null means always available.
        // When it returns false the toggle is grayed out in the UI.
        std::function<bool()> available;
        std::string unavailable_tooltip;

        bool is_available() const { return !available || available(); }

        // Callback when our state changes
        // NOTE: actual may not be same as is_enabled()! The latter is this particular pb,
        // while the former takes into account global "Post-Processing"...
        virtual void processing_block_enable_disable( bool actual ) {}

    protected:
        bool _enabled = true;
        std::shared_ptr<rs2::filter> _block;
        std::map< rs2_option, option_model > _options_id_to_model;
        std::string _name;
        std::string _full_name;
        std::function<rs2::frame( rs2::frame )> _invoker;
        subdevice_model* _owner;
    };

    bool restore_processing_block(const char* name,
        std::shared_ptr<rs2::processing_block> pb, bool enable);

    void save_processing_block_to_config_file(const char* name,
        std::shared_ptr<rs2::processing_block> pb, bool enable = true);
}
