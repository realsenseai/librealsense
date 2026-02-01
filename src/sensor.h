// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-24 RealSense, Inc. All Rights Reserved.
#pragma once

#include "core/sensor-interface.h"

#include "source.h"
#include "core/extension.h"
#include "proc/formats-converter.h"
#include <src/synthetic-options-watcher.h>
#include <src/platform/stream-profile.h>
#include <src/platform/frame-object.h>

#include <rsutils/lazy.h>
#include <rsutils/signal.h>
#include <rsutils/deferred.h>

#include <chrono>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <limits.h>
#include <atomic>
#include <functional>


namespace librealsense
{
    class device;
    class option;
    class stream_interface;
    class notifications_processor;
    enum class format_conversion;


    typedef std::function<void(std::vector<platform::stream_profile>)> on_open;
    typedef std::function<void(frame_additional_data &data)> on_frame_md;

    struct frame_timestamp_reader
    {
        virtual ~frame_timestamp_reader() {}

        virtual double get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) = 0;
        virtual unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const = 0;
        virtual rs2_timestamp_domain get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const = 0;
        virtual void reset() = 0;
    };

    class notifications_processor;
    class frame;

    class sensor_base
        : public std::enable_shared_from_this< sensor_base >
        , public virtual sensor_interface
        , public options_container
        , public virtual info_container
        , public recordable< recommended_proccesing_blocks_interface >
    {
    public:
        explicit sensor_base( std::string const & name, device * device );
        virtual ~sensor_base() override { _source.flush(); }

        void set_source_owner(sensor_base* owner); // will direct the source to the top in the source hierarchy.
        virtual stream_profiles init_stream_profiles() = 0;
        stream_profiles get_stream_profiles(int tag = profile_tag::PROFILE_TAG_ANY) const override;
        stream_profiles get_active_streams() const override;
        rs2_notifications_callback_sptr get_notifications_callback() const override;
        void register_notifications_callback( rs2_notifications_callback_sptr callback ) override;
        int register_before_streaming_changes_callback(std::function<void(bool)> callback) override;
        void unregister_before_start_callback(int token) override;
        virtual std::shared_ptr<notifications_processor> get_notifications_processor() const;
        virtual rs2_frame_callback_sptr get_frames_callback() const override;
        virtual void set_frames_callback( rs2_frame_callback_sptr callback ) override;
        bool is_streaming() const override;
        virtual bool is_opened() const;
        virtual void register_metadata(rs2_frame_metadata_value metadata, std::shared_ptr<md_attribute_parser_base> metadata_parser) const;
        virtual void register_on_open(on_open callback)
        {
            _on_open = callback;
        }
        virtual void set_frame_metadata_modifier(on_frame_md callback) { _metadata_modifier = callback; }
        device_interface& get_device() override;

        // Make sensor inherit its owning device info by default
        const std::string& get_info(rs2_camera_info info) const override;
        bool supports_info(rs2_camera_info info) const override;

        processing_blocks get_recommended_processing_blocks() const override
        {
            return {};
        }
        embedded_filters get_supported_embedded_filters() const override { return embedded_filters(); };

        // Sometimes it is more efficient to prepare for large or repeating operations. Depending on the actual sensor
        // type we might want to change power state or encapsulate small transactions into a large one.
        virtual void prepare_for_bulk_operation() {}
        virtual void finished_bulk_operation() {}
        rsutils::deferred bulk_operation()
        {
            prepare_for_bulk_operation();
            return rsutils::deferred( [this] { finished_bulk_operation(); } );
        }

        // recordable< recommended_proccesing_blocks_interface > is needed to record our recommended processing blocks
    public:
        void enable_recording( std::function< void( const recommended_proccesing_blocks_interface & ) > ) override {}
        void create_snapshot( std::shared_ptr< recommended_proccesing_blocks_interface > & snapshot ) const override
        {
            snapshot
                = std::make_shared< recommended_proccesing_blocks_snapshot >( get_recommended_processing_blocks() );
        }

        rsutils::subscription register_options_changed_callback( options_watcher::callback && cb ) override
        {
            throw not_implemented_exception( "Registering options value changed callback is not implemented for this sensor" );
        }

    protected:
        // Since _profiles is private, we need a way to get the final profiles
        stream_profiles const & initialized_profiles() const { return *_profiles; }

        void raise_on_before_streaming_changes(bool streaming);
        void set_active_streams(const stream_profiles& requests);

        void register_profile(std::shared_ptr<stream_profile_interface> target) const;

        void assign_stream(const std::shared_ptr<stream_interface>& stream,
                           std::shared_ptr<stream_profile_interface> target) const;

        format_conversion get_format_conversion() const;

        void sort_profiles( stream_profiles & );

        std::shared_ptr< frame > generate_frame_from_data( const platform::frame_object & fo,
                                                           rs2_time_t system_time,
                                                           frame_timestamp_reader * timestamp_reader,
                                                           const rs2_time_t & last_timestamp,
                                                           const unsigned long long & last_frame_number,
                                                           std::shared_ptr< stream_profile_interface > profile );

        inline int compute_frame_expected_size(int width, int height, int bpp) const
        {
            return width * height * bpp >> 3;
        }

        std::vector<uint8_t> align_width_to_64(int width, int height, int bpp, uint8_t * pix) const;

        std::atomic<bool> _is_streaming;
        std::atomic<bool> _is_opened;
        std::shared_ptr<notifications_processor> _notifications_processor;
        on_open _on_open;
        on_frame_md _metadata_modifier;
        std::shared_ptr<metadata_parser_map> _metadata_parsers = nullptr;

        sensor_base* _source_owner = nullptr;
        frame_source _source;
        device* _owner;

    private:
        rsutils::lazy< stream_profiles > _profiles;
        stream_profiles _active_profiles;
        mutable std::mutex _active_profile_mutex;
        rsutils::signal< bool > _on_before_streaming_changes;
    };

    class processing_block;

    class motion_sensor
    {
    public:
        virtual ~motion_sensor() = default;
    };

    MAP_EXTENSION(RS2_EXTENSION_MOTION_SENSOR, librealsense::motion_sensor);


    class fisheye_sensor
    {
    public:
        virtual ~fisheye_sensor() = default;
    };

    MAP_EXTENSION(RS2_EXTENSION_FISHEYE_SENSOR, librealsense::fisheye_sensor);

    // Base class for anything that is the target of a synthetic_sensor
    //
    class raw_sensor_base : public sensor_base
    {
        typedef sensor_base super;

        std::shared_ptr< std::map< uint32_t, rs2_format > > _fourcc_to_rs2_format;
        std::shared_ptr< std::map< uint32_t, rs2_stream > > _fourcc_to_rs2_stream;

    protected:
        explicit raw_sensor_base( std::string const & name,
                                  device * device )
            : super( name, device )
        {
        }

        rs2_format fourcc_to_rs2_format( uint32_t ) const;
        rs2_stream fourcc_to_rs2_stream( uint32_t ) const;

    public:
        // Raw sensor doesn't do any manipulation on the profiles from the backend
        stream_profiles const & get_raw_stream_profiles() const override { return initialized_profiles(); }

        std::shared_ptr< std::map< uint32_t, rs2_format > > & get_fourcc_to_rs2_format_map();
        std::shared_ptr< std::map< uint32_t, rs2_stream > > & get_fourcc_to_rs2_stream_map();
    };

    // A sensor pointer to another "raw sensor", usually UVC/HID
    //
    class synthetic_sensor :
        public sensor_base
    {
    public:
        explicit synthetic_sensor( std::string const & name,
                                   std::shared_ptr< raw_sensor_base > const & raw_sensor,
                                   device * device,
                                   const std::map< uint32_t, rs2_format > & fourcc_to_rs2_format_map = {},
                                   const std::map< uint32_t, rs2_stream > & fourcc_to_rs2_stream_map = {} );
        ~synthetic_sensor() override;

        virtual void register_option(rs2_option id, std::shared_ptr<option> option);
        virtual bool try_register_option(rs2_option id, std::shared_ptr<option> option);
        virtual void unregister_option(rs2_option id);
        void register_pu(rs2_option id);
        bool try_register_pu(rs2_option id);

        virtual stream_profiles init_stream_profiles() override;

        // Our raw profiles are the ones that the raw sensor gave us, before we manipulated them
        stream_profiles const & get_raw_stream_profiles() const override { return _raw_sensor->get_raw_stream_profiles(); }

        void open(const stream_profiles& requests) override;
        void close() override;
        void start( rs2_frame_callback_sptr callback ) override;
        void stop() override;

        virtual float get_preset_max_value() const;

        void register_processing_block(const std::vector<stream_profile>& from,
            const std::vector<stream_profile>& to,
            std::function<std::shared_ptr<processing_block>(void)> generate_func);
        void register_processing_block(const processing_block_factory& pbf);
        void register_processing_block(const std::vector<processing_block_factory>& pbfs);

        std::shared_ptr< raw_sensor_base > const & get_raw_sensor() const { return _raw_sensor; }
        rs2_frame_callback_sptr get_frames_callback() const override;
        void set_frames_callback( rs2_frame_callback_sptr callback ) override;
        void register_notifications_callback( rs2_notifications_callback_sptr callback ) override;
        int register_before_streaming_changes_callback(std::function<void(bool)> callback) override;
        void unregister_before_start_callback(int token) override;
        void register_metadata(rs2_frame_metadata_value metadata, std::shared_ptr<md_attribute_parser_base> metadata_parser) const override;
        bool is_streaming() const override;
        bool is_opened() const override;

        rsutils::subscription register_options_changed_callback( options_watcher::callback && cb ) override;
        virtual void register_option_to_update( rs2_option id, std::shared_ptr< option > option );
        virtual void unregister_option_from_update( rs2_option id );
        inline void pause_options_watcher() { _options_watcher.pause(); }
        inline void unpause_options_watcher() { _options_watcher.unpause(); }

        void prepare_for_bulk_operation() override { _raw_sensor->prepare_for_bulk_operation(); }
        void finished_bulk_operation() override { _raw_sensor->finished_bulk_operation(); }

        void register_on_open( on_open callback ) override { _raw_sensor->register_on_open( callback ); }

    private:
        void register_processing_block_options(const processing_block& pb);
        void unregister_processing_block_options(const processing_block& pb);

        std::mutex _synthetic_configure_lock;

        rs2_frame_callback_sptr _post_process_callback;
        std::shared_ptr<raw_sensor_base> _raw_sensor;
        formats_converter _formats_converter;
        std::vector<rs2_option> _cached_processing_blocks_options;

        synthetic_options_watcher _options_watcher;
    };


    processing_blocks get_color_recommended_proccesing_blocks();
    processing_blocks get_depth_recommended_proccesing_blocks();
}
