// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "sync.h"
#include "core/frame-holder.h"
#include "core/synthetic-source-interface.h"
#include "core/stream-profile-interface.h"
#include "core/device-interface.h"
#include "core/sensor-interface.h"
#include "composite-frame.h"
#include "core/time-service.h"

#include <rsutils/string/from.h>


namespace librealsense
{
    const int MAX_GAP = 1000;

#define LOG_IF_ENABLE( OSTREAM, ENV ) \
    while( ENV.log ) \
    { \
        LOG_DEBUG( OSTREAM ); \
        break; \
    }


    matcher::matcher(std::vector<stream_id> streams_id)
        : _streams_id(streams_id){}

    void matcher::sync(frame_holder f, const syncronization_environment& env)
    {
        auto cb = begin_callback();
        _callback(std::move(f), env);
    }

    void matcher::set_callback(sync_callback f)
    {
        _callback = f;
    }

    callback_invocation_holder matcher::begin_callback()
    {
        return{ _callback_inflight.allocate(), &_callback_inflight };
    }

    std::string matcher::get_name() const
    {
        return _name;
    }

    bool matcher::get_active() const
    {
        return _active;
    }

    void matcher::set_active(const bool active)
    {
        _active = active;
    }

    const std::vector<stream_id>& matcher::get_streams() const
    {
        return _streams_id;
    }

    const std::vector<rs2_stream>& matcher::get_streams_types() const
    {
        return _streams_type;
    }

    matcher::~matcher()
    {
        _callback_inflight.stop_allocation();

        auto callbacks_inflight = _callback_inflight.get_size();
        if (callbacks_inflight > 0)
        {
            LOG_WARNING(callbacks_inflight << " callbacks are still running on some other threads. Waiting until all callbacks return...");
        }
        // wait until user is done with all the stuff he chose to borrow
        try
        {
            _callback_inflight.wait_until_empty();
        }
        catch( const std::exception & e )
        {
            LOG_DEBUG( "Error while waiting for user callback to finish: " << e.what() );
        }
    }

    identity_matcher::identity_matcher(stream_id stream, rs2_stream stream_type)
        :matcher({stream})
    {
        _streams_type = {stream_type};

        std::ostringstream ss;
        ss << get_abbr_string( stream_type );
        ss << stream;
        _name = ss.str();
    }

    void identity_matcher::dispatch(frame_holder f, const syncronization_environment& env)
    {
        //LOG_IF_ENABLE( "--> identity_matcher: " << _name, env );

        sync(std::move(f), env);
    }

    std::string create_composite_name(const std::vector<std::shared_ptr<matcher>>& matchers, const std::string& name)
    {
        std::stringstream s;
        s<<"("<<name;

        bool first = true;
        for (auto&& matcher : matchers)
        {
            if( first )
                first = false;
            else
                s << ' ';
            s << matcher->get_name();
        }
        s << ")";
        return s.str();
    }

    composite_matcher::composite_matcher(
        std::vector< std::shared_ptr< matcher > > const & matchers, std::string const & name, matcher_factory::matching_clock clock )
        : _clock( std::move( clock ) )
    {
        for (auto&& matcher : matchers)
        {
            for (auto&& stream : matcher->get_streams())
            {
                matcher->set_callback(
                    [&]( frame_holder f, const syncronization_environment & env ) {
                        LOG_IF_ENABLE( "<-- " << *f.frame << "  " << _name, env );
                        sync( std::move( f ), env );
                    } );
                _matchers[stream] = matcher;
                _streams_id.push_back(stream);
            }
            for (auto&& stream : matcher->get_streams_types())
            {
                _streams_type.push_back(stream);
            }
        }

        _name = create_composite_name(matchers, name);
    }

    composite_matcher::matcher_queue::matcher_queue()
        : q( QUEUE_MAX_SIZE,
             []( frame_holder const & fh )
             {
                 // If queues are overrun, we'll get here
                 LOG_DEBUG( "DROPPED frame " << fh );
             } )
    {
    }


    void composite_matcher::dispatch(frame_holder f, const syncronization_environment& env)
    {
        clean_inactive_streams(f);
        auto matcher = find_matcher(f);

        //LOG_IF_ENABLE( "--> composite_matcher: " << _name, env );

        if (matcher)
        {
            update_last_arrived(f, matcher.get());
            matcher->dispatch(std::move(f), env);
        }
        else
        {
            LOG_ERROR( "didn't find any matcher; releasing unsynchronized frame " << *f.frame );
            _callback(std::move(f), env);
        }
    }

    std::shared_ptr<matcher> composite_matcher::find_matcher(const frame_holder& frame)
    {
        auto stream_profile = frame.frame->get_stream();
        auto stream_id = stream_profile->get_unique_id();
        auto stream_type = stream_profile->get_stream_type();

        std::shared_ptr<matcher> matcher;
        auto it = _matchers.find( stream_id );
        if( it != _matchers.end() )
        {
            matcher = it->second;
            if( matcher )
            {
                if( ! matcher->get_active() )
                {
                    matcher->set_active( true );
                    _frames_queue[matcher.get()].q.start();
                }
                return matcher;
            }
        }
        LOG_DEBUG( "no matcher found for " << get_abbr_string( stream_type ) << stream_id
                                           << "; creating matcher from device..." );

        auto sensor = frame.frame->get_sensor().get(); //TODO: Potential deadlock if get_sensor() gets a hold of the last reference of that sensor
        auto dev_exist = false;
        if (sensor)
        {
            const device_interface* dev = nullptr;
            try
            {
                dev = sensor->get_device().shared_from_this().get();
            }
            catch (const std::bad_weak_ptr&)
            {
                LOG_WARNING("Device destroyed");
            }
            if (dev)
            {
                dev_exist = true;
                matcher = dev->create_matcher(frame);
                LOG_DEBUG( "... created " << matcher->get_name() );

                matcher->set_callback(
                    [&]( frame_holder f, syncronization_environment const & env ) {
                        LOG_IF_ENABLE( "<-- " << *f.frame << "  " << _name, env );
                        sync( std::move( f ), env );
                    } );

                for (auto stream : matcher->get_streams())
                {
                    if (_matchers[stream])
                    {
                        _frames_queue.erase(_matchers[stream].get());
                    }
                    _matchers[stream] = matcher;
                    _streams_id.push_back(stream);
                }
                for (auto stream : matcher->get_streams_types())
                {
                    _streams_type.push_back(stream);
                }

                if (std::find(_streams_type.begin(), _streams_type.end(), stream_type) == _streams_type.end())
                {
                    LOG_ERROR("Stream matcher not found! stream=" << rs2_stream_to_string(stream_type));
                }
                else
                {
                    _name = create_composite_name( { matcher },
                                                    _name.substr( 1, _name.length() - 2 ) );  // Remove the "()" around "(CI: )"
                }
            }
        }
        else
        {
            LOG_DEBUG("sensor does not exist");
        }

        if (!dev_exist)
        {
            matcher = _matchers[stream_id];
            // We don't know what device this frame came from, so just store it under device NULL with ID matcher
            if (!matcher)
            {
                _matchers[stream_id] = std::make_shared<identity_matcher>(stream_id, stream_type);
                _streams_id.push_back(stream_id);
                _streams_type.push_back(stream_type);
                matcher = _matchers[stream_id];

                matcher->set_callback(
                    [&]( frame_holder f, syncronization_environment const & env ) {
                        LOG_IF_ENABLE( "<-- " << *f.frame << "  " << _name, env );
                        sync( std::move( f ), env );
                    } );
            }
        }
        return matcher;
    }

    void composite_matcher::stop()
    {
        // We don't want to stop while dispatching!
        std::lock_guard<std::mutex> lock( _mutex );

        // Mark ourselves inactive, so we don't get new dispatches
        set_active( false );

        // Stop all our queues to wake up anyone waiting on them
        for( auto & fq : _frames_queue )
            fq.second.q.stop();

        // Trickle the stop down to any children
        for( auto m : _matchers )
            m.second->stop();
    }

    std::string
    composite_matcher::frames_to_string( std::vector< frame_holder * > const & frames )
    {
        std::ostringstream os;
        for( auto pfh : frames )
            os << *pfh;
        return os.str();
    }

    std::string
        composite_matcher::matchers_to_string( std::vector< librealsense::matcher* > const& matchers )
    {
        std::ostringstream os;
        os << '[';
        for( auto m : matchers )
        {
            auto const & q = _frames_queue[m].q;
            q.peek( [&os]( frame_holder const & fh ) {
                os << fh;
                } );
        }
        os << ']';
        return os.str();
    }

    void composite_matcher::sync(frame_holder f, const syncronization_environment& env)
    {
        auto matcher = find_matcher(f);
        if (!matcher)
        {
            LOG_ERROR("didn't find any matcher for " << f << " will not be synchronized");
            _callback(std::move(f), env);
            return;
        }
        update_next_expected( matcher, f );

        // We want to keep track of a "last-arrived" frame which is our current equivalent of "now" -- it contains the
        // latest timestamp/frame-number/etc. that we can compare to.
        auto last_arrived = f->get_header();
        if( _clock )
            last_arrived.timestamp = _clock( *f.frame );

        if( ! _frames_queue[matcher.get()].q.enqueue( std::move( f ) ) )
            // If we get stopped, nothing to do!
            return;

        // We have a queue for each known stream we want to sync.
        // E.g., for (Depth Color), we need to sync two frames, one from each.
        // If we have a Color frame but not Depth, then Depth is "missing" and needs to be
        // waited-for...

        std::vector< frame_holder * > frames_arrived;
        std::vector< librealsense::matcher * > frames_arrived_matchers;
        std::vector< int > synced_frames;
        std::vector< int > unsynced_frames;
        std::vector< librealsense::matcher * > missing_streams;

        while( true )
        {
            missing_streams.clear();
            frames_arrived_matchers.clear();
            frames_arrived.clear();

            std::vector< frame_holder > match;
            {
                // We don't want to stop while syncing!
                std::lock_guard< std::mutex > lock( _mutex );

                // We want to release one frame from each matcher. If a matcher has nothing queued, it is "missing" and
                // we need to consider waiting for it:
                for( auto s = _frames_queue.begin(); s != _frames_queue.end(); s++ )
                {
                    librealsense::matcher * const m = s->first;
                    if( ! s->second.q.peek( [&]( frame_holder & fh ) {
                            LOG_IF_ENABLE( "... have " << *fh.frame, env );
                            frames_arrived.push_back( &fh );
                            frames_arrived_matchers.push_back( m );
                        } ) )
                    {
                        missing_streams.push_back( m );
                    }
                }
                if( frames_arrived.empty() )
                {
                    // LOG_IF_ENABLE( "... nothing more to do", env );
                    break;
                }

                // From what we collected, we want to release only the frames that are synchronized (based on timestamp,
                // number, etc.) -- anything else we'll leave to the next iteration. The synced frames should be the
                // earliest possible!

                frame_holder * curr_sync = frames_arrived[0];
                synced_frames.clear();
                synced_frames.push_back( 0 );

                // Sometimes we have to release newly-arrived frames even before frames we already had previously
                // queued. If we have something like this, 'have_unsynced_frames' will be true:
                unsynced_frames.clear();
                for( auto i = 1; i < frames_arrived.size(); i++ )
                {
                    if( are_equivalent( *curr_sync, *frames_arrived[i] ) )
                    {
                        synced_frames.push_back( i );
                    }
                    else if( is_smaller_than( *frames_arrived[i], *curr_sync ) )
                    {
                        unsynced_frames.insert( unsynced_frames.end(), synced_frames.begin(), synced_frames.end() );
                        synced_frames.clear();
                        synced_frames.push_back( i );
                        curr_sync = frames_arrived[i];
                    }
                    else
                    {
                        unsynced_frames.push_back( i );
                    }
                }
                bool release_synced_frames = ( synced_frames.size() != 0 );
                if( unsynced_frames.empty() )
                {
                    // Everything (could be only one!) matches together... but if we also have
                    // something missing, we can't release anything yet...
                    for( auto i : missing_streams )
                    {
                        LOG_IF_ENABLE( "... missing " << i->get_name() << ", next expected @"
                                                      << rsutils::string::from( _next_expected[i].value ) << " (from "
                                                      << rsutils::string::from( _next_expected[i].fps ) << " fps)",
                                       env );
                        if( skip_missing_stream( *curr_sync, i, last_arrived, env ) )
                        {
                            LOG_IF_ENABLE( "...     cannot be synced; not waiting for it", env );
                            continue;
                        }

                        LOG_IF_ENABLE( "...     waiting for it", env );
                        release_synced_frames = false;
                    }
                }
                else
                {
                    for( auto i : unsynced_frames )
                    {
                        LOG_IF_ENABLE( "  - " << *frames_arrived[i]->frame << " is not in sync; won't be released", env );
                    }
                }
                if( ! release_synced_frames )
                    break;

                match.reserve( synced_frames.size() );

                for( auto index : synced_frames )
                {
                    frame_holder frame;
                    int const timeout_ms = 5000;
                    librealsense::matcher * m = frames_arrived_matchers[index];
                    _frames_queue[m].q.dequeue( &frame, timeout_ms );
                    match.push_back( std::move( frame ) );
                }
            }

            // The frameset should always be with the same order of streams (the first stream carries extra
            // meaning because it decides the frameset properties) -- so we sort them...
            std::sort( match.begin(),
                       match.end(),
                       []( const frame_holder & f1, const frame_holder & f2 ) {
                           return f1.frame->get_stream()->get_unique_id()
                                > f2.frame->get_stream()->get_unique_id();
                       } );


            frame_holder composite = env.source->allocate_composite_frame(std::move(match));
            if (composite.frame)
            {
                auto cb = begin_callback();
                _callback(std::move(composite), env);
            }
        }
    }

    frame_number_composite_matcher::frame_number_composite_matcher(
        std::vector< std::shared_ptr< matcher > > const & matchers )
        : composite_matcher( matchers, "FN: " )
    {
    }

    void frame_number_composite_matcher::update_last_arrived(frame_holder& f, matcher* m)
    {
        _last_arrived[m] =f->get_frame_number();
    }

    bool frame_number_composite_matcher::are_equivalent(frame_holder& a, frame_holder& b)
    {
        return a->get_frame_number() == b->get_frame_number();
    }
    bool frame_number_composite_matcher::is_smaller_than(frame_holder & a, frame_holder & b)
    {
        return a->get_frame_number() < b->get_frame_number();
    }
    void frame_number_composite_matcher::clean_inactive_streams(frame_holder& f)
    {
        std::vector<stream_id> inactive_matchers;
        for(auto m: _matchers)
        {
            if( _last_arrived[m.second.get()]
                && ( std::abs( (long long)f->get_frame_number() - (long long)_last_arrived[m.second.get()] ) ) > 5 )
            {
                std::stringstream s;
                s << "clean inactive stream in "<<_name;
                for (auto stream : m.second->get_streams_types())
                {
                    s << stream << " ";
                }
                LOG_DEBUG(s.str());

                inactive_matchers.push_back(m.first);
                m.second->set_active(false);
            }
        }

        for(auto id: inactive_matchers)
        {
            _frames_queue[_matchers[id].get()].q.clear();
        }
    }

    bool
    frame_number_composite_matcher::skip_missing_stream( frame_interface const * const synced_frame,
                                                         matcher * missing,
                                                         frame_header const & last_arrived,
                                                         const syncronization_environment & env )
    {
         if(!missing->get_active())
             return true;

        auto const & next_expected = _next_expected[missing];

        if( synced_frame->get_frame_number() - next_expected.value > 4
            || synced_frame->get_frame_number() < next_expected.value )
        {
            return true;
        }
        return false;
    }

    void frame_number_composite_matcher::update_next_expected(
        std::shared_ptr< matcher > const & matcher, const frame_holder & f )
    {
        _next_expected[matcher.get()].value = f.frame->get_frame_number()+1.;
    }

    double composite_matcher::matching_timestamp( frame_interface const & f ) const
    {
        return _clock ? _clock( f ) : f.get_frame_timestamp();
    }

    std::pair<double, double> timestamp_composite_matcher::extract_timestamps(frame_holder & a, frame_holder & b) const
    {
        if (a->get_frame_timestamp_domain() == b->get_frame_timestamp_domain())
            return{ matching_timestamp( *a.frame ), matching_timestamp( *b.frame ) };
        else
            return{ a->get_frame_system_time(), b->get_frame_system_time() };
    }

    timestamp_composite_matcher::timestamp_composite_matcher(
        std::vector< std::shared_ptr< matcher > > const & matchers, matcher_factory::matching_clock clock )
        : composite_matcher( matchers, "TS: ", std::move( clock ) )
    {
    }
    bool timestamp_composite_matcher::are_equivalent(frame_holder & a, frame_holder & b)
    {
        auto a_fps = get_fps(a);
        auto b_fps = get_fps(b);

        auto min_fps = std::min(a_fps, b_fps);

        auto ts = extract_timestamps(a, b);

        return  are_equivalent(ts.first, ts.second, min_fps);
    }

    bool timestamp_composite_matcher::is_smaller_than(frame_holder & a, frame_holder & b)
    {
        if (!a || !b)
        {
            return false;
        }

        auto ts = extract_timestamps(a, b);

        return ts.first < ts.second;
    }

    void timestamp_composite_matcher::update_last_arrived(frame_holder& f, matcher* m)
    {
        auto const now = time_service::get_time();
        //LOG_DEBUG( _name << ": _last_arrived[" << m->get_name() << "] = " << now );
        _last_arrived[m] = now;
    }

    double timestamp_composite_matcher::get_fps( frame_interface const * f )
    {
        double fps = 0.;
        rs2_metadata_type fps_md;
        if( f->find_metadata( RS2_FRAME_METADATA_ACTUAL_FPS, &fps_md ) )
            fps = fps_md / 1000.;
        if( fps )
        {
            //LOG_DEBUG( "fps " << rsutils::string::from( fps ) << " from metadata " << *f );
        }
        else
        {
            fps = f->get_stream()->get_framerate();
            //LOG_DEBUG( "fps " << rsutils::string::from( fps ) << " from stream framerate " << *f );
        }
        return fps;
    }

    void
    timestamp_composite_matcher::update_next_expected( std::shared_ptr< matcher > const & matcher,
                                                       const frame_holder & f )
    {
        auto fps = get_fps( f );
        auto gap = 1000. / fps;

        auto ts = matching_timestamp( *f.frame );
        auto ne = ts + gap;
        //LOG_DEBUG( "... next_expected = {timestamp}" << rsutils::string::from( ts ) << " + {gap}(1000/{fps}"
        //                                             << rsutils::string::from( fps )
        //                                             << ") = " << rsutils::string::from( ne ) );
        auto & next_expected = _next_expected[matcher.get()];
        next_expected.value = ne;
        next_expected.fps = fps;
        next_expected.domain = f.frame->get_frame_timestamp_domain();
    }

    void timestamp_composite_matcher::clean_inactive_streams(frame_holder& f)
    {
        // We let skip_missing_stream clean any inactive missing streams
    }

    bool timestamp_composite_matcher::skip_missing_stream( frame_interface const * waiting_to_be_released,
                                                           matcher * missing,
                                                           frame_header const & last_arrived,
                                                           const syncronization_environment & env )
    {
        // true : frameset is ready despite the missing stream (no use waiting) -- "skip" it
        // false: the missing stream is relevant and our frameset isn't ready yet!

        if(!missing->get_active())
            return true;

        //LOG_IF_ENABLE( "...     matcher " << synced[0]->get_name(), env );

        auto const & next_expected = _next_expected[missing];
        // LOG_IF_ENABLE( "...     next    " << std::fixed << next_expected, env );

        if( next_expected.domain != last_arrived.timestamp_domain )
        {
            // LOG_IF_ENABLE( "...     not the same domain: frameset not ready!", env );
            // D457 dev - return false removed
            // because IR has no md, so its ts domain is "system time"
            // other streams have md, and their ts domain is "hardware clock"
            //return false;
        }

        // We want to calculate a cutout for inactive stream detection: if we wait too long past
        // this cutout, then the missing stream is inactive and we no longer wait for it.
        // 
        //     cutout = next expected + threshold
        //     threshold = 7 * gap = 7 * (1000 / FPS)
        // 
        // 
        // E.g.:
        // 
        //     D: 100 fps ->  10 ms gap
        //     C: 10 fps  -> 100 ms gap
        // 
        //     D  C  @timestamp
        //     -- -- ----------
        //     1     0   -> release (we don't know about a missing stream yet, so don't get here)
        //       ...
        //     6     50  -> release (D6); next expected (NE) -> 60
        //        1  0   -> release (C1); NE -> 100                                  <----- LATENCY
        //     7  ?  60  -> release (D7) (not comparable to NE because we use fps of 100!)
        //       ...
        //     11 ?  100 -> wait for C (now comparable); cutout is 100+7*10 = 170
        //     12 ?  110 -> wait (> NE, < cutout)
        //       ...
        //     19 ?  180 > 170 -> release (D11); mark C inactive (> cutout)
        //                        release (D12) ... (D19)  (no missing streams, so don't get here)
        //     20    190 -> release (D20) (no missing streams, so don't get here)
        //       ...
        // 
        // But, if C had arrived before D19:
        // 
        //       ...
        //        2  100 -> release (D11, C2) (nothing missing); NE -> 200
        //        ?         release (D12) ... (D18) (C missing but 100 not comparable to NE 200)
        //     19 ?  180 -> release (D19)
        //     20 ?  190 -> release (D20)
        //     21 ?  200 -> wait for C (comparable to NE 200)
        // 
        // 
        // The threshold is a function of the FPS, but note we can't keep more frames than
        // our queue size and per-stream archive size allow.
        auto const fps = get_fps( waiting_to_be_released );

        rs2_time_t now = last_arrived.timestamp;
        if( now > next_expected.value )
        {
            // Wait for the missing stream frame to arrive -- up to a cutout: anything more and we
            // let the frameset be ready without it...
            auto gap = 1000. / fps;
            // NOTE: the threshold is a function of the gap; the bigger it is, the more latency
            // between the streams we're willing to live with. Each gap is a frame so we are limited
            // by the number of frames we're willing to keep (which is our queue limit)
            auto threshold = 7 * gap;  // really 7+1 because NE is already 1 away
            if( now - next_expected.value < threshold )
            {
                //LOG_IF_ENABLE( "...     still below cutout of {NE+7*gap}"
                //                   << rsutils::string::from( next_expected + threshold ),
                //               env );
                return false;
            }
            LOG_IF_ENABLE( "...     exceeded cutout of {NE+7*gap}"
                               << rsutils::string::from( next_expected.value + threshold ) << "; deactivating matcher!",
                           env );

            auto const q_it = _frames_queue.find( missing );
            if( q_it != _frames_queue.end() )
            {
                if( q_it->second.q.empty() )
                    _frames_queue.erase( q_it );
            }
            missing->set_active( false );
            return true;
        }

        return ! are_equivalent( matching_timestamp( *waiting_to_be_released ),
                                 next_expected.value,
                                 fps );
    }

    bool timestamp_composite_matcher::are_equivalent( double a, double b, double fps )
    {
        auto gap = 1000. / fps;
        if( std::abs( a - b ) < ( gap / 2 ) )
        {
            //LOG_DEBUG( "...     " << rsutils::string::from( a ) << " == " << rsutils::string::from( b ) << "  {diff}"
            //                      << std::abs( a - b ) << " < " << rsutils::string::from( gap / 2 ) << "{gap/2}" );
            return true;
        }

        //LOG_DEBUG( "...     " << rsutils::string::from( a ) << " != " << rsutils::string::from( b ) << "  {diff}"
        //                      << rsutils::string::from( std::abs( a - b ) ) << " >= " << rsutils::string::from( gap / 2 )
        //                      << "{gap/2}" );
        return false;
    }

    composite_identity_matcher::composite_identity_matcher(
        std::vector< std::shared_ptr< matcher > > const & matchers )
        : composite_matcher( matchers, "CI: " )
    {
    }

    void composite_identity_matcher::sync(frame_holder f, const syncronization_environment& env)
    {
        auto composite = dynamic_cast<const composite_frame *>(f.frame);
        // Syncer have to output composite frame 
        if (!composite)
        {
            std::vector<frame_holder> match;
            std::ostringstream frame_string_for_logging;
            frame_string_for_logging << f; // Saving frame holder string before moving frame

            match.push_back(std::move(f));
            frame_holder composite = env.source->allocate_composite_frame(std::move(match));
            if (composite.frame)
            {
                auto cb = begin_callback();
                LOG_DEBUG( "wrapped with composite: " << *composite.frame );
                _callback(std::move(composite), env);
            }
            else
            {
                LOG_ERROR( "composite_identity_matcher: "
                           << _name << " " << frame_string_for_logging.str()
                           << " faild to create composite_frame, user callback will not be called" );
            }
        }
        else
        {
            _callback( std::move( f ), env );
        }
    }
}  // namespace librealsense
