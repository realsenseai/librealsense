// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include <src/source.h>

#include <src/option.h>
#include <src/core/frame-holder.h>
#include <src/core/enum-helpers.h>

#include <rsutils/string/from.h>
#include <src/core/stream-profile-interface.h>

namespace librealsense
{
    class frame_queue_size : public option_base
    {
    public:
        frame_queue_size(std::atomic<uint32_t>* ptr, const option_range& opt_range)
            : option_base(opt_range),
              _ptr(ptr)
        {}

        void set(float value) override
        {
            if (!is_valid(value))
                throw invalid_value_exception( rsutils::string::from() << "set(frame_queue_size) failed! Given value "
                                                                       << value << " is out of range." );

            *_ptr = static_cast<uint32_t>(value);
            _recording_function(*this);
        }

        float query() const override { return static_cast<float>(_ptr->load()); }

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Max number of frames you can hold at a given time. Increasing this number will reduce frame drops but increase latency, and vice versa";
        }
    private:
        std::atomic<uint32_t>* _ptr;
    };

    std::shared_ptr<option> frame_source::get_published_size_option()
    {
        return std::make_shared<frame_queue_size>(&_max_publish_list_size, option_range{ 0, 32, 1, 16 });
    }

    frame_source::frame_source( uint32_t max_publish_list_size )
        : _callback( nullptr, []( rs2_frame_callback * ) {} )
        , _max_publish_list_size( max_publish_list_size )
    {}

    void frame_source::init(std::shared_ptr<metadata_parser_map> metadata_parsers)
    {
        std::lock_guard< std::recursive_mutex > lock( _mutex );

        _supported_extensions = { RS2_EXTENSION_VIDEO_FRAME,
                                  RS2_EXTENSION_COMPOSITE_FRAME,
                                  RS2_EXTENSION_POINTS,
                                  RS2_EXTENSION_DEPTH_FRAME,
                                  RS2_EXTENSION_DISPARITY_FRAME,
                                  RS2_EXTENSION_MOTION_FRAME,
                                  RS2_EXTENSION_POSE_FRAME,
                                  RS2_EXTENSION_LABELED_POINTS,
                                  RS2_EXTENSION_INFERENCE_FRAME,
                                  RS2_EXTENSION_OBJECT_DETECTION_FRAME };

        _metadata_parsers = metadata_parsers;
    }

    std::map< frame_source::archive_id, std::shared_ptr< archive_interface > >::iterator
    frame_source::create_archive( archive_id id )
    {
        std::lock_guard< std::recursive_mutex > lock( _mutex );

        rs2_extension & ex = std::get< rs2_extension >( id );
        auto it = std::find( _supported_extensions.begin(), _supported_extensions.end(), ex );
        if( it == _supported_extensions.end() )
            throw wrong_api_call_sequence_exception( "Requested frame type is not supported!" );

        auto ret = _archive.insert( { id, make_archive( ex, &_max_publish_list_size, _metadata_parsers ) } );
        if( ! ret.second || ! ret.first->second ) // Check insertion success and allocation success
            throw std::runtime_error( rsutils::string::from() << "Failed to create archive of type " << get_string( ex ) );

        ret.first->second->set_sensor( _sensor );

        return ret.first;
    }

    callback_invocation_holder frame_source::begin_callback( archive_id id )
    {
        // We use a special index for extensions, like GPU accelerated frames. See add_extension.
        if( std::get< rs2_extension >( id ) >= RS2_EXTENSION_COUNT )
            std::get< rs2_stream >( id ) = RS2_STREAM_COUNT;  // For added extensions like GPU accelerated frames

        std::lock_guard< std::recursive_mutex > lock( _mutex );

        auto it = _archive.find( id );
        if( it == _archive.end() )
            it = create_archive( id );

        return it->second->begin_callback();
    }

    void frame_source::reset()
    {
        std::lock_guard< std::recursive_mutex > lock( _mutex );

        _callback.reset();
        _archive.clear();
        _metadata_parsers.reset();
    }

    frame_interface * frame_source::alloc_frame( archive_id id,
                                                 size_t size,
                                                 frame_additional_data && additional_data,
                                                 bool requires_memory )
    {
        // We use a special index for extensions, like GPU accelerated frames. See add_extension.
        if( std::get< rs2_extension>( id ) >= RS2_EXTENSION_COUNT )
            std::get< rs2_stream >( id ) = RS2_STREAM_COUNT;  // For added extensions like GPU accelerated frames

        std::lock_guard< std::recursive_mutex > lock( _mutex );

        auto it = _archive.find( id );
        if( it == _archive.end() )
            it = create_archive( id );

        return it->second->alloc_and_track( size, std::move( additional_data ), requires_memory );
    }

    void frame_source::set_sensor( const std::weak_ptr< sensor_interface > & s )
    {
        _sensor = s;

        std::lock_guard< std::recursive_mutex > lock( _mutex );

        for( auto & a : _archive )
        {
            a.second->set_sensor( _sensor );
        }
    }

    void frame_source::set_callback( rs2_frame_callback_sptr callback )
    {
        std::lock_guard< std::recursive_mutex > lock( _mutex );
        _callback = callback;
    }

    rs2_frame_callback_sptr frame_source::get_callback() const
    {
        std::lock_guard< std::recursive_mutex > lock( _mutex );
        return _callback;
    }

    void frame_source::invoke_callback(frame_holder frame) const
    {
        if (frame && frame.frame && frame.frame->get_owner())
        {
            try
            {
                if (_callback)
                {
                    frame_interface* ref = nullptr;
                    std::swap(frame.frame, ref);
                    _callback->on_frame((rs2_frame*)ref);
                }
            }
            catch( const std::exception & e )
            {
                LOG_ERROR( "Exception was thrown during user callback: " + std::string( e.what() ));
            }
            catch(...)
            {
                LOG_ERROR("Exception was thrown during user callback!");
            }
        }
    }

    void frame_source::flush() const
    {
        std::lock_guard< std::recursive_mutex > lock( _mutex );

        for( auto & kvp : _archive )
        {
            if( kvp.second )
                kvp.second->flush();
        }
    }

        rs2_extension frame_source::stream_to_frame_types( rs2_stream stream )
    {
        // TODO: explicitly return video_frame for relevant streams and default to an error?
        switch( stream )
        {
        case RS2_STREAM_DEPTH:
            return RS2_EXTENSION_DEPTH_FRAME;
        case RS2_STREAM_ACCEL:
        case RS2_STREAM_GYRO:
        case RS2_STREAM_MOTION:
            return RS2_EXTENSION_MOTION_FRAME;
        case RS2_STREAM_LABELED_POINT_CLOUD:
            return RS2_EXTENSION_LABELED_POINTS;

        case RS2_STREAM_COLOR:
        case RS2_STREAM_INFRARED:
        case RS2_STREAM_FISHEYE:
        case RS2_STREAM_GPIO:
        case RS2_STREAM_POSE:
        case RS2_STREAM_CONFIDENCE:
        case RS2_STREAM_SAFETY:
        case RS2_STREAM_OCCUPANCY:
            return RS2_EXTENSION_VIDEO_FRAME;

        default:
            throw std::runtime_error("could not find matching extension with stream type '" + std::string(get_string(stream)) + "'");
        }
    }
}

