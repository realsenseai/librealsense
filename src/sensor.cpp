// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-24 RealSense, Inc. All Rights Reserved.

#include "uvc-sensor.h"

#include "source.h"
#include "device.h"
#include "stream.h"
#include "context.h"
#include "image.h"
#include "proc/synthetic-stream.h"
#include "proc/decimation-filter.h"
#include "proc/rotation-filter.h"
#include "global_timestamp_reader.h"
#include "device-calibration.h"
#include "core/notification.h"
#include "platform/uvc-option.h"
#include "core/depth-frame.h"
#include "core/stream-profile-interface.h"
#include "core/frame-callback.h"
#include "core/notification.h"
#include <src/metadata-parser.h>


#include <rsutils/string/from.h>
#include <rsutils/json.h>

#include <array>
#include <set>
#include <unordered_set>
#include <iomanip>


namespace librealsense {

void log_callback_end( uint32_t fps,
                       rs2_time_t callback_start_time,
                       rs2_time_t const current_time,
                       rs2_stream stream_type,
                       unsigned long long frame_number )
{
    auto callback_warning_duration = 1000.f / ( fps + 1 );
    auto callback_duration = current_time - callback_start_time;

    LOG_DEBUG( "CallbackFinished," << librealsense::get_string( stream_type ) << ",#" << std::dec
                                   << frame_number << ",@" << std::fixed << current_time
                                   << ", callback duration: " << callback_duration << " ms" );

    if( callback_duration > callback_warning_duration )
    {
        LOG_INFO( "Frame Callback " << librealsense::get_string( stream_type ) << " #" << std::dec
                                    << frame_number << " overdue. (FPS: " << fps
                                    << ", max duration: " << callback_warning_duration << " ms)" );
    }
}

    //////////////////////////////////////////////////////
    /////////////////// Sensor Base //////////////////////
    //////////////////////////////////////////////////////

    sensor_base::sensor_base( std::string const & name, device * dev )
    : _is_streaming( false )
    , _is_opened( false )
    , _notifications_processor( std::make_shared< notifications_processor >() )
    , _on_open( nullptr )
    , _metadata_modifier( nullptr )
    , _metadata_parsers( std::make_shared< metadata_parser_map >() )
    , _owner( dev )
    , _profiles(
          [this]()
          {
              auto profiles = this->init_stream_profiles();
              _owner->tag_profiles( profiles );
              return profiles;
          } )
    {
        register_option(RS2_OPTION_FRAMES_QUEUE_SIZE, _source.get_published_size_option());

        register_metadata( RS2_FRAME_METADATA_TIME_OF_ARRIVAL,
                           make_additional_data_parser_unless( &frame_additional_data::system_time, rs2_time_t( 0 ) ) );

        register_info(RS2_CAMERA_INFO_NAME, name);
    }

    const std::string& sensor_base::get_info(rs2_camera_info info) const
    {
        if (info_container::supports_info(info)) return info_container::get_info(info);
        else return _owner->get_info(info);
    }

    bool sensor_base::supports_info(rs2_camera_info info) const
    {
        return info_container::supports_info(info) || _owner->supports_info(info);
    }

    stream_profiles sensor_base::get_active_streams() const
    {
        std::lock_guard<std::mutex> lock(_active_profile_mutex);
        return _active_profiles;
    }

    void sensor_base::register_notifications_callback( rs2_notifications_callback_sptr callback )
    {
        if (supports_option(RS2_OPTION_ERROR_POLLING_ENABLED))
        {
            auto&& opt = get_option(RS2_OPTION_ERROR_POLLING_ENABLED);
            opt.set(1.0f);
        }
        _notifications_processor->set_callback(std::move(callback));
    }

    rs2_notifications_callback_sptr sensor_base::get_notifications_callback() const
    {
        return _notifications_processor->get_callback();
    }

    int sensor_base::register_before_streaming_changes_callback(std::function<void(bool)> callback)
    {
        int slot = _on_before_streaming_changes.add( std::move( callback ) );
        LOG_DEBUG("Registered token #" << slot << " to \"on_before_streaming_changes\"");
        return slot;
    }

    void sensor_base::unregister_before_start_callback( int slot )
    {
        _on_before_streaming_changes.remove( slot );
    }

    rs2_frame_callback_sptr sensor_base::get_frames_callback() const
    {
        return _source.get_callback();
    }
    void sensor_base::set_frames_callback( rs2_frame_callback_sptr callback )
    {
        return _source.set_callback(callback);
    }

    bool sensor_base::is_streaming() const
    {
        return _is_streaming;
    }

    bool sensor_base::is_opened() const
    {
        return _is_opened;
    }

    std::shared_ptr<notifications_processor> sensor_base::get_notifications_processor() const
    {
        return _notifications_processor;
    }

    void sensor_base::register_metadata(rs2_frame_metadata_value metadata, std::shared_ptr<md_attribute_parser_base> metadata_parser) const
    {
        if (_metadata_parsers.get()->end() != _metadata_parsers.get()->find(metadata))
        {
            std::string metadata_type_str(rs2_frame_metadata_to_string(metadata));
            std::string metadata_found_str = "Metadata attribute parser for " + metadata_type_str + " was previously defined";
            LOG_DEBUG(metadata_found_str.c_str());
        }
        _metadata_parsers.get()->insert(std::pair<rs2_frame_metadata_value, std::shared_ptr<md_attribute_parser_base>>(metadata, metadata_parser));
    }

    std::shared_ptr<std::map<uint32_t, rs2_format>>& raw_sensor_base::get_fourcc_to_rs2_format_map()
    {
        return _fourcc_to_rs2_format;
    }

    std::shared_ptr<std::map<uint32_t, rs2_stream>>& raw_sensor_base::get_fourcc_to_rs2_stream_map()
    {
        return _fourcc_to_rs2_stream;
    }

    rs2_format raw_sensor_base::fourcc_to_rs2_format(uint32_t fourcc_format) const
    {
        auto it = _fourcc_to_rs2_format->find( fourcc_format );
        if( it != _fourcc_to_rs2_format->end() )
            return it->second;

        return RS2_FORMAT_ANY;
    }

    rs2_stream raw_sensor_base::fourcc_to_rs2_stream(uint32_t fourcc_format) const
    {
        auto it = _fourcc_to_rs2_stream->find( fourcc_format );
        if( it != _fourcc_to_rs2_stream->end() )
            return it->second;

        return RS2_STREAM_ANY;
    }

    void sensor_base::raise_on_before_streaming_changes(bool streaming)
    {
        _on_before_streaming_changes.raise( streaming );
    }
    void sensor_base::set_active_streams(const stream_profiles& requests)
    {
        std::lock_guard<std::mutex> lock(_active_profile_mutex);
        _active_profiles = requests;
    }

    void sensor_base::register_profile(std::shared_ptr<stream_profile_interface> target) const
    {
        environment::get_instance().get_extrinsics_graph().register_profile(*target);
    }

    void sensor_base::assign_stream(const std::shared_ptr<stream_interface>& stream, std::shared_ptr<stream_profile_interface> target) const
    {
        environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*stream, *target);
        auto uid = stream->get_unique_id();
        target->set_unique_id(uid);
    }

    void sensor_base::set_source_owner(sensor_base* owner)
    {
        _source_owner = owner;
    }

    stream_profiles sensor_base::get_stream_profiles( int tag ) const
    {
        bool const need_debug = (tag & profile_tag::PROFILE_TAG_DEBUG) != 0;
        bool const need_any = (tag & profile_tag::PROFILE_TAG_ANY) != 0;
        
        auto & all_profiles = initialized_profiles();
        if( need_debug && need_any )
            return all_profiles;

        stream_profiles results;
        for( auto p : all_profiles )
        {
            auto curr_tag = p->get_tag();
            if( ! need_debug && ( curr_tag & profile_tag::PROFILE_TAG_DEBUG ) )
                continue;

            if( curr_tag & tag || need_any )
            {
                results.push_back( p );
            }
        }
        return results;
    }

    processing_blocks get_color_recommended_proccesing_blocks()
    {
        processing_blocks res;
        auto dec = std::make_shared<decimation_filter>();
        if (!dec->supports_option(RS2_OPTION_STREAM_FILTER))
            return res;
        dec->get_option(RS2_OPTION_STREAM_FILTER).set(RS2_STREAM_COLOR);
        dec->get_option(RS2_OPTION_STREAM_FORMAT_FILTER).set(RS2_FORMAT_ANY);
        res.push_back(dec);
        std::vector< rs2_stream > streams_to_rotate;
        streams_to_rotate.push_back( RS2_STREAM_COLOR );
        res.push_back( std::make_shared< rotation_filter >( streams_to_rotate ) );
        return res;
    }

    processing_blocks get_depth_recommended_proccesing_blocks()
    {
        processing_blocks res;

        auto dec = std::make_shared<decimation_filter>();
        if (dec->supports_option(RS2_OPTION_STREAM_FILTER))
        {
            dec->get_option(RS2_OPTION_STREAM_FILTER).set(RS2_STREAM_DEPTH);
            dec->get_option(RS2_OPTION_STREAM_FORMAT_FILTER).set(RS2_FORMAT_Z16);
            res.push_back(dec);
        }

        std::vector< rs2_stream > streams_to_rotate;
        streams_to_rotate.push_back( RS2_STREAM_DEPTH);
        streams_to_rotate.push_back( RS2_STREAM_INFRARED );
        res.push_back( std::make_shared< rotation_filter >( streams_to_rotate ) );
        return res;
    }

    device_interface& sensor_base::get_device()
    {
        return *_owner;
    }

    // TODO - make this method more efficient, using parralel computation, with SSE or CUDA, when available
    std::vector<uint8_t> sensor_base::align_width_to_64(int width, int height, int bpp, uint8_t * pix) const
    {
        int factor = bpp >> 3;
        int bytes_in_width = width * factor;
        int actual_input_bytes_in_width = (((bytes_in_width / 64 ) + 1) * 64);
        std::vector<uint8_t> pixels;
        for (int j = 0; j < height; ++j)
        {
            int start_index = j * actual_input_bytes_in_width;
            int end_index = (width * factor) + (j * actual_input_bytes_in_width);
            pixels.insert(pixels.end(), pix + start_index, pix + end_index);
        }
        return pixels;
    }

    std::shared_ptr< frame >
    sensor_base::generate_frame_from_data( const platform::frame_object & fo,
                                           rs2_time_t const system_time,
                                           frame_timestamp_reader * timestamp_reader,
                                           const rs2_time_t & last_timestamp,
                                           const unsigned long long & last_frame_number,
                                           std::shared_ptr< stream_profile_interface > profile )
    {
        auto fr = std::make_shared<frame>();
        
        fr->set_stream(profile);

        // D457 dev - computing relevant frame size
        const auto&& vsp = As<video_stream_profile, stream_profile_interface>(profile);
        int width = vsp ? vsp->get_width() : 0;
        int height = vsp ? vsp->get_height() : 0;
        int bpp = get_image_bpp(profile->get_format());
        auto frame_size = compute_frame_expected_size(width, height, bpp);

        frame_additional_data additional_data(0,
            0,
            system_time,
            static_cast<uint8_t>(fo.metadata_size),
            (const uint8_t*)fo.metadata,
            fo.backend_time,
            last_timestamp,
            last_frame_number,
            false,
            0,
            (uint32_t)frame_size);

        if (_metadata_modifier)
            _metadata_modifier(additional_data);
        fr->additional_data = additional_data;

        // update additional data
        additional_data.timestamp = timestamp_reader->get_frame_timestamp(fr);
        additional_data.last_frame_number = last_frame_number;
        additional_data.frame_number = timestamp_reader->get_frame_counter(fr);
        fr->additional_data = additional_data;

        return fr;
    }

    //////////////////////////////////////////////////////
    /////////////////// UVC Sensor ///////////////////////
    //////////////////////////////////////////////////////

    
    bool info_container::supports_info(rs2_camera_info info) const
    {
        auto it = _camera_info.find(info);
        return it != _camera_info.end();
    }

    void info_container::register_info(rs2_camera_info info, const std::string& val)
    {
        if (info_container::supports_info(info) && (info_container::get_info(info) != val)) // Append existing infos
        {
            _camera_info[info] += "\n" + val;
        }
        else
        {
            _camera_info[info] = val;
        }
    }

    void info_container::update_info(rs2_camera_info info, const std::string& val)
    {
        if (info_container::supports_info(info))
        {
            _camera_info[info] = val;
        }
    }
    const std::string& info_container::get_info(rs2_camera_info info) const
    {
        auto it = _camera_info.find(info);
        if (it == _camera_info.end())
            throw invalid_value_exception("Selected camera info: " + std::string(rs2_camera_info_to_string(info)) + ", is not supported for this camera!");

        return it->second;
    }
    void info_container::create_snapshot(std::shared_ptr<info_interface>& snapshot) const
    {
        snapshot = std::make_shared<info_container>(*this);
    }
    void info_container::enable_recording(std::function<void(const info_interface&)> record_action)
    {
        //info container is a read only class, nothing to record
    }

    void info_container::update(std::shared_ptr<extension_snapshot> ext)
    {
        if (auto&& info_api = As<info_interface>(ext))
        {
            for (int i = 0; i < RS2_CAMERA_INFO_COUNT; ++i)
            {
                rs2_camera_info info = static_cast<rs2_camera_info>(i);
                if (info_api->supports_info(info))
                {
                    register_info(info, info_api->get_info(info));
                }
            }
        }
    }


    //////////////////////////////////////////////////////
    ///////////////// Synthetic Sensor ///////////////////
    //////////////////////////////////////////////////////

    synthetic_sensor::synthetic_sensor( std::string const & name,
                                        std::shared_ptr< raw_sensor_base > const & raw_sensor,
                                        device * device,
                                        const std::map< uint32_t, rs2_format > & fourcc_to_rs2_format_map,
                                        const std::map< uint32_t, rs2_stream > & fourcc_to_rs2_stream_map )
        : sensor_base( name, device )
        , _raw_sensor( raw_sensor )
        , _options_watcher( _raw_sensor )
    {
        rsutils::json const & settings = device->get_context()->get_settings();
        if( auto interval_j = settings.nested( std::string( "options-update-interval", 23 ) ) )
        {
            auto interval = interval_j.get< uint32_t >();  // NOTE: can throw!
            _options_watcher.set_update_interval( std::chrono::milliseconds( interval ) );
        }

        // synthetic sensor and its raw sensor will share the formats and streams mapping
        auto& raw_fourcc_to_rs2_format_map = _raw_sensor->get_fourcc_to_rs2_format_map();
        raw_fourcc_to_rs2_format_map = std::make_shared<std::map<uint32_t, rs2_format>>(fourcc_to_rs2_format_map);

        auto& raw_fourcc_to_rs2_stream_map = _raw_sensor->get_fourcc_to_rs2_stream_map();
        raw_fourcc_to_rs2_stream_map = std::make_shared<std::map<uint32_t, rs2_stream>>(fourcc_to_rs2_stream_map);
    }

    synthetic_sensor::~synthetic_sensor()
    {
        try
        {
            if (is_streaming())
                stop();

            if (is_opened())
                close();
        }
        catch (...)
        {
            LOG_ERROR("An error has occurred while stop_streaming()!");
        }
    }

    // Register the option to both raw sensor and synthetic sensor.
    void synthetic_sensor::register_option( rs2_option id, std::shared_ptr< option > option )
    {
        _raw_sensor->register_option( id, option );
        sensor_base::register_option( id, option );
        _options_watcher.register_option( id, option );
    }

    // Used in dynamic discovery of supported controls in generic UVC devices
    bool synthetic_sensor::try_register_option(rs2_option id, std::shared_ptr<option> option)
    {
        bool res=false;
        try
        {
            auto range = option->get_range();
            bool invalid_opt = (range.max < range.min || range.step < 0 || range.def < range.min || range.def > range.max) ||
                    (range.max == range.min && range.min == range.def && range.def == range.step);
            bool readonly_opt = ((range.max == range.min ) && (0.f != range.min ) && ( 0.f == range.step));

            if (invalid_opt)
            {
                LOG_WARNING(this->get_info(RS2_CAMERA_INFO_NAME) << ": skipping " << rs2_option_to_string(id)
                            << " control. descriptor: [min/max/step/default]= ["
                            << range.min << "/" << range.max << "/" << range.step << "/" << range.def << "]");
                return res;
            }

            if (readonly_opt)
            {
                LOG_INFO(this->get_info(RS2_CAMERA_INFO_NAME) << ": " << rs2_option_to_string(id)
                        << " control was added as read-only. descriptor: [min/max/step/default]= ["
                        << range.min << "/" << range.max << "/" << range.step << "/" << range.def << "]");
            }

            // Check getter only due to options coupling (e.g. Exposure<->AutoExposure)
            auto val = option->query();
            if (val < range.min || val > range.max)
            {
                LOG_WARNING(this->get_info(RS2_CAMERA_INFO_NAME) << ": Invalid reading for " << rs2_option_to_string(id)
                            << ", val = " << val << " range [min..max] = [" << range.min << "/" << range.max << "]");
            }

            register_option(id, option);
            res = true;
        }
        catch (...)
        {
            std::string device_name = "";
            if( _owner && _owner->supports_info( RS2_CAMERA_INFO_NAME ) )
                device_name = _owner->get_info( RS2_CAMERA_INFO_NAME );
            LOG_WARNING( device_name << " failed to add " << rs2_option_to_string( id ) << " control for " << this->get_info( RS2_CAMERA_INFO_NAME ) );
        }
        return res;
    }

    void synthetic_sensor::unregister_option( rs2_option id )
    {
        _raw_sensor->unregister_option( id );
        sensor_base::unregister_option( id );
        _options_watcher.unregister_option( id );
    }

    void synthetic_sensor::register_pu(rs2_option id)
    {
        const auto raw_uvc_sensor = As<uvc_sensor, sensor_base>(_raw_sensor);
        register_option(id, std::make_shared<uvc_pu_option>(raw_uvc_sensor, id));
    }

    bool synthetic_sensor::try_register_pu(rs2_option id)
    {
        const auto raw_uvc_sensor = As<uvc_sensor, sensor_base>(_raw_sensor);
        return try_register_option(id, std::make_shared<uvc_pu_option>(raw_uvc_sensor, id));
    }

    void sensor_base::sort_profiles( stream_profiles & profiles )
    {
        std::sort(profiles.begin(), profiles.end(), [](const std::shared_ptr<stream_profile_interface>& ap,
            const std::shared_ptr<stream_profile_interface>& bp)
        {
            const auto&& a = to_profile(ap.get());
            const auto&& b = to_profile(bp.get());

            // stream == RS2_STREAM_COLOR && format == RS2_FORMAT_RGB8 element works around the fact that Y16 gets priority over RGB8 when both
            // are available for pipeline stream resolution
            // Note: Sort Stream Index decsending to make sure IR1 is chosen over IR2
            const auto&& at = std::make_tuple(a.stream, -a.index, a.width, a.height, a.stream == RS2_STREAM_COLOR && a.format == RS2_FORMAT_RGB8, a.format, a.fps);
            const auto&& bt = std::make_tuple(b.stream, -b.index, b.width, b.height, b.stream == RS2_STREAM_COLOR && b.format == RS2_FORMAT_RGB8, b.format, b.fps);

            return at > bt;
        });
    }

    void synthetic_sensor::register_processing_block_options(const processing_block & pb)
    {
        // Register the missing processing block's options to the sensor
        const auto&& options = pb.get_supported_options();

        for (auto&& opt : options)
        {
            const auto&& already_registered_predicate = [&opt](const rs2_option& o) {return o == opt; };
            if (std::none_of(begin(options), end(options), already_registered_predicate))
            {
                this->register_option(opt, std::shared_ptr<option>(const_cast<option*>(&pb.get_option(opt))));
                _cached_processing_blocks_options.push_back(opt);
            }
        }
    }

    void synthetic_sensor::unregister_processing_block_options(const processing_block & pb)
    {
        const auto&& options = pb.get_supported_options();

        // Unregister the cached options related to the processing blocks.
        for (auto&& opt : options)
        {
            const auto&& cached_option_predicate = [&opt](const rs2_option& o) {return o == opt; };
            auto&& cached_opt = std::find_if(begin(_cached_processing_blocks_options), end(_cached_processing_blocks_options), cached_option_predicate);

            if (cached_opt != end(_cached_processing_blocks_options))
            {
                this->unregister_option(*cached_opt);
                _cached_processing_blocks_options.erase(cached_opt);
            }
        }
    }

    stream_profiles synthetic_sensor::init_stream_profiles()
    {
        stream_profiles result_profiles;
        switch( get_format_conversion() )
        {
        case format_conversion::basic:
            _formats_converter.drop_non_basic_formats();
            // fall-thru
        case format_conversion::full:
            result_profiles = _formats_converter.get_all_possible_profiles( get_raw_stream_profiles() );
            break;

        case format_conversion::raw:
            result_profiles = get_raw_stream_profiles();
            // NOTE: this is not meant for actual streaming at this time -- actual behavior of the
            // formats_converter has not been implemented!
            break;
        }

        _owner->tag_profiles( result_profiles );
        sort_profiles( result_profiles );

        return result_profiles;
    }

    void synthetic_sensor::open(const stream_profiles & requests)
    {
        if( get_format_conversion() == format_conversion::raw )
            throw wrong_api_call_sequence_exception( "'raw' format-conversion is not meant for streaming" );

        std::lock_guard<std::mutex> lock(_synthetic_configure_lock);

        _formats_converter.prepare_to_convert( requests );

        const auto & resolved_req = _formats_converter.get_active_source_profiles();
        std::vector< std::shared_ptr< processing_block > > active_pbs = _formats_converter.get_active_converters();
        for( auto & pb : active_pbs )
            register_processing_block_options( *pb );

        _raw_sensor->set_source_owner(this);
        try
        {
            _raw_sensor->open( resolved_req );
        }
        catch (const std::runtime_error& e)
        {
            // Throw a more informative exception
            std::stringstream requests_info;
            for (auto&& r : requests)
            {
                auto p = to_profile(r.get());
                requests_info << "\tFormat: " + std::string(rs2_format_to_string(p.format)) << ", width: " << p.width << ", height: " << p.height << std::endl;
            }
            throw recoverable_exception("\nFailed to resolve the request: \n" + requests_info.str() + "\nInto:\n" + e.what(),
                RS2_EXCEPTION_TYPE_INVALID_VALUE);
        }

        set_active_streams(requests);
    }

    void synthetic_sensor::close()
    {
        std::lock_guard<std::mutex> lock(_synthetic_configure_lock);
        _raw_sensor->close();

        std::vector< std::shared_ptr< processing_block > > active_pbs = _formats_converter.get_active_converters();
        for( auto & pb : active_pbs )
            unregister_processing_block_options( *pb );

        _formats_converter.set_frames_callback( nullptr );
        set_active_streams({});
        _post_process_callback.reset();
    }

    void synthetic_sensor::start( rs2_frame_callback_sptr callback )
    {
        std::lock_guard<std::mutex> lock(_synthetic_configure_lock);

        // Set the post-processing callback as the user callback.
        // This callback might be modified by other object.
        set_frames_callback(callback);
        _formats_converter.set_frames_callback( callback );  // TODO duplicate?! Something fishy here!

        // Call the processing block on the frame
        _raw_sensor->start(
            make_frame_callback( [&, this]( frame_holder f ) { _formats_converter.convert_frame( f ); } ) );
    }

    void synthetic_sensor::stop()
    {
        std::lock_guard<std::mutex> lock(_synthetic_configure_lock);
        _raw_sensor->stop();
    }

    float librealsense::synthetic_sensor::get_preset_max_value() const
    {
        // to be overriden by depth sensors which need this api
        return 0.0f;
    }

    void synthetic_sensor::register_processing_block(const std::vector< stream_profile > & from,
                                                     const std::vector< stream_profile > & to,
                                                     std::function< std::shared_ptr< processing_block >( void ) > generate_func )
    {
        _formats_converter.register_converter( from, to, generate_func );
    }

    void synthetic_sensor::register_processing_block( const processing_block_factory & pbf )
    {
        _formats_converter.register_converter( pbf );
    }

    void synthetic_sensor::register_processing_block( const std::vector< processing_block_factory > & pbfs )
    {
        _formats_converter.register_converters( pbfs );
    }

    rs2_frame_callback_sptr synthetic_sensor::get_frames_callback() const
    {
        return _formats_converter.get_frames_callback();
    }

    void synthetic_sensor::set_frames_callback( rs2_frame_callback_sptr callback )
    {
        // This callback is mutable, might be modified.
        // For instance, record_sensor modifies this callback in order to hook it to record frames.
        _formats_converter.set_frames_callback( callback );
    }

    void synthetic_sensor::register_notifications_callback( rs2_notifications_callback_sptr callback )
    {
        sensor_base::register_notifications_callback(callback);
        _raw_sensor->register_notifications_callback(callback);
    }

    int synthetic_sensor::register_before_streaming_changes_callback(std::function<void(bool)> callback)
    {
        return _raw_sensor->register_before_streaming_changes_callback(callback);
    }

    void synthetic_sensor::unregister_before_start_callback(int token)
    {
        _raw_sensor->unregister_before_start_callback(token);
    }

    void synthetic_sensor::register_metadata(rs2_frame_metadata_value metadata, std::shared_ptr<md_attribute_parser_base> metadata_parser) const
    {
        sensor_base::register_metadata(metadata, metadata_parser);
        _raw_sensor->register_metadata(metadata, metadata_parser);
    }

    bool synthetic_sensor::is_streaming() const
    {
        return _raw_sensor->is_streaming();
    }

    bool synthetic_sensor::is_opened() const
    {
        return _raw_sensor->is_opened();
    }

    format_conversion sensor_base::get_format_conversion() const
    {
        return _owner->get_format_conversion();
    }

    rsutils::subscription synthetic_sensor::register_options_changed_callback( options_watcher::callback && cb )
    {
        return _options_watcher.subscribe( std::move( cb ) );
    }

    void synthetic_sensor::register_option_to_update( rs2_option id, std::shared_ptr< option > option )
    {
        _options_watcher.register_option( id, option );
    }

    void synthetic_sensor::unregister_option_from_update( rs2_option id )
    {
        _options_watcher.unregister_option( id );
    }

}
