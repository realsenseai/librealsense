// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-mipi-mux.h"
#include "v4l-mipi-logic.h"
#include "v4l-mipi-device.h"

#include <rsutils/string/from.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <set>

namespace librealsense
{
    namespace platform
    {
        namespace v4l_mipi_mux
        {
            namespace
            {
                constexpr uint32_t RSVL_FOURCC = 0x5253564C;  // the carrier format the muxed streams share

                constexpr uint32_t CAPTURE_TIMING_ID = 0x80000001;
                constexpr uint32_t CAPTURE_TIMING_MUX_VERSION = 2;
                constexpr uint32_t CAPTURE_TIMING_MUX_FLAGS = 0x3;  // frame counter and timestamp

#pragma pack( push, 1 )
                // Capture Timing version 2, which names the muxed stream in place of the video timing fields
                struct capture_timing
                {
                    uint32_t id;
                    uint32_t size;
                    uint32_t version;
                    uint32_t flags;
                    uint32_t frame_counter;
                    uint32_t timestamp;
                    uint32_t metadata_type_id;
                    uint32_t reserved[3];
                };

                struct native_header
                {
                    uint32_t id;
                    uint32_t size;
                };
#pragma pack( pop )

                constexpr size_t MUX_MD_SIZE = uvc_header_size + sizeof( capture_timing );

                // Bytes one RSVL record holds
                size_t capacity( const stream_profile & p ) { return size_t( p.width ) * p.height; }

                // The RSVL record of a `payload`-byte stream, every rate highest first: the camera rounds a stream up to
                // whole rows, so its record holds the payload with less than one row to spare. Empty when none does.
                std::vector< stream_profile > exact_record( const std::vector< stream_profile > & profiles, size_t payload )
                {
                    std::vector< stream_profile > record;
                    for( auto const & p : profiles )
                        if( p.format == RSVL_FOURCC && capacity( p ) >= payload && capacity( p ) - payload < p.width )
                            record.push_back( p );
                    std::sort( record.begin(), record.end(),
                               []( const stream_profile & a, const stream_profile & b ) { return a.fps > b.fps; } );
                    return record;
                }

                bool is_carrier( const uvc_device_info & info )
                {
                    try
                    {
                        return info.is_mipi && v4l_mipi_logic::is_format_supported_on_node( info.device_path, "RSVL" );
                    }
                    catch( const std::exception & )
                    {
                        return false;
                    }
                }

                // Owns the multiplexing node. Its own (IR) user and the muxed streams are mutually exclusive; the muxed
                // streams share one capture, started with the first and stopped with the last.
                class mux
                {
                public:
                    // One device object's registration of a muxed stream
                    struct slot
                    {
                        muxed_stream desc;
                        muxed_stream_enable enable;  // the registering device's channel to the camera's MUX control
                        frame_callback callback;
                        stream_profile profile;  // as committed; what the callback reports
                        bool started = false;
                        bool committed = false;
                        bool enabled = false;
                        mutable bool no_record_warned = false;
                    };

                    explicit mux( const uvc_device_info & carrier_info )
                        : _carrier( std::make_shared< v4l_mipi_device >( carrier_info ) )
                    {
                    }

                    uvc_device & carrier() const { return *_carrier; }

                    slot & add_stream( const muxed_stream & desc, muxed_stream_enable enable )
                    {
                        std::lock_guard< std::mutex > lock( _mtx );
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        for( auto const & s : _slots )
                        {
                            // Streams can register once per profile and from another device object. Two streams never share metadata id
                            bool same_stream = s->desc.stream_id == desc.stream_id;
                            if( same_stream != ( s->desc.metadata.type_id == desc.metadata.type_id ) )
                                throw invalid_value_exception( desc.name + " conflicts with " + s->desc.name + " on the GMSL MUX" );
                        }
                        _slots.push_back( std::make_unique< slot >() );
                        _slots.back()->desc = desc;
                        _slots.back()->enable = std::move( enable );
                        return *_slots.back();
                    }

                    void remove_stream( slot * s )
                    {
                        close( s, {} );  // a no-op unless the stream's user went away without closing
                        std::lock_guard< std::mutex > lock( _mtx );
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        _slots.erase( std::find_if( _slots.begin(), _slots.end(),
                                                    [s]( const std::unique_ptr< slot > & p ) { return p.get() == s; } ) );
                    }

                    // The IR user sees the node without the carrier format
                    std::vector< stream_profile > get_profiles() const
                    {
                        auto profiles = _carrier->get_profiles();
                        profiles.erase( std::remove_if( profiles.begin(), profiles.end(),
                                                        []( const stream_profile & p ) { return p.format == RSVL_FOURCC; } ),
                                        profiles.end() );
                        return profiles;
                    }

                    // A muxed stream runs at the rates of its RSVL record, or not at all when the camera offers none
                    std::vector< stream_profile > get_profiles( const slot & s ) const
                    {
                        std::vector< stream_profile > profiles;
                        for( auto const & p : record_of( s ) )
                            profiles.push_back( { s.desc.frame.width, s.desc.frame.height, p.fps, s.desc.frame.format } );
                        if( profiles.empty() && ! s.no_record_warned )
                        {
                            s.no_record_warned = true;
                            LOG_WARNING( s.desc.name << " is not published: the GMSL IR node offers no RSVL record of "
                                                     << s.desc.frame.payload_size << " bytes" );
                        }
                        return profiles;
                    }

                    void set_power( bool on )
                    {
                        std::lock_guard< std::mutex > lock( _power_mtx );
                        if( on )
                        {
                            if( _power_users++ == 0 )
                                _carrier->set_power_state( D0 );
                        }
                        else if( _power_users > 0 && --_power_users == 0 )
                            _carrier->set_power_state( D3 );
                    }

                    void commit( slot * s, const stream_profile & profile, frame_callback callback, int buffers )
                    {
                        {
                            std::lock_guard< std::mutex > lock( _mtx );
                            if( ! s )
                            {
                                if( _streams_committed )
                                    throw wrong_api_call_sequence_exception( "The GMSL IR channel is in use by " + committed_names() );
                                _carrier->probe_and_commit( profile, std::move( callback ), buffers );
                                _carrier_committed = true;
                                return;
                            }
                            if( _carrier_committed )
                                throw wrong_api_call_sequence_exception( s->desc.name + " is unavailable while the GMSL IR channel streams" );
                            if( is_committed( s->desc.stream_id ) )
                                throw wrong_api_call_sequence_exception( s->desc.name + " is already streaming" );
                            // Claiming the queue first keeps a process that cannot capture from touching camera state.
                            // The streams arrive in bursts, one frame each, so the ring holds a burst over the usual depth.
                            if( ! _streams_committed )
                                _carrier->probe_and_commit( carrier_profile( *s ),
                                                            [this]( stream_profile, frame_object f, std::function< void() > release )
                                                            { dispatch( f, std::move( release ) ); },
                                                            buffers + registered_streams() - 1 );
                            else
                            {
                                auto record = record_of( *s );
                                if( record.empty() || capacity( record.front() ) > capacity( _carrier_profile ) )
                                    throw wrong_api_call_sequence_exception( s->desc.name + " needs a larger GMSL capture than "
                                                                             + committed_names() + " use; stop them first" );
                            }
                            s->committed = true;
                            s->profile = profile;
                            ++_streams_committed;
                        }
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        s->callback = std::move( callback );
                    }

                    void stream_on( slot * s, std::function< void( const notification & ) > error_handler )
                    {
                        std::lock_guard< std::mutex > lock( _mtx );
                        if( ! s )
                            return _carrier->stream_on( error_handler );
                        if( ! s->committed )
                            throw wrong_api_call_sequence_exception( s->desc.name + " started before it was configured" );
                        if( s->enabled )
                            return;
                        try
                        {
                            if( ! _streaming )
                            {
                                clear_stale_requests( *s );
                                set_enable( *s, true );
                                // Firmware owns the backing graph, including inputs it starts on its own
                                _carrier->stream_on( error_handler );
                                _streaming = true;
                            }
                            else
                                set_enable( *s, true );
                        }
                        catch( const std::exception & e )
                        {
                            LOG_ERROR( s->desc.name << " start failed: " << e.what() );
                            throw backend_exception( "The camera refused to start " + s->desc.name );
                        }
                    }

                    void set_started( slot * s, bool on )
                    {
                        if( ! s )
                            return on ? _carrier->start_callbacks() : _carrier->stop_callbacks();
                        {
                            std::lock_guard< std::recursive_mutex > route( _route_mtx );
                            s->started = on;
                        }
                        if( on )
                            _carrier->start_callbacks();
                    }

                    void close( slot * s, const stream_profile & profile )
                    {
                        if( ! s )
                        {
                            std::lock_guard< std::mutex > lock( _mtx );
                            _carrier_committed = false;
                            return _carrier->close( profile );
                        }
                        {
                            // Waits for a frame in flight, so no callback follows close()
                            std::lock_guard< std::recursive_mutex > route( _route_mtx );
                            s->callback = nullptr;
                            s->started = false;
                        }
                        std::lock_guard< std::mutex > lock( _mtx );
                        if( ! s->committed )
                            return;
                        s->committed = false;
                        if( --_streams_committed == 0 )
                        {
                            // Stop the capture before the last stream, so the camera never outputs with none enabled
                            _streaming = false;
                            _carrier->stop_callbacks();
                            _carrier->close( _carrier_profile );
                        }
                        if( s->enabled )
                        {
                            try
                            {
                                set_enable( *s, false );
                            }
                            catch( const std::exception & e )
                            {
                                LOG_ERROR( s->desc.name << " disable failed: " << e.what() );
                            }
                        }
                    }

                private:
                    // Distinct stream ids, however many profiles or device objects registered them
                    int registered_streams() const
                    {
                        std::set< uint32_t > ids;
                        for( auto const & s : _slots )
                            ids.insert( s->desc.stream_id );
                        return int( ids.size() );
                    }

                    bool is_committed( uint32_t stream_id ) const
                    {
                        return std::any_of( _slots.begin(), _slots.end(), [&]( const std::unique_ptr< slot > & s )
                                            { return s->committed && s->desc.stream_id == stream_id; } );
                    }

                    // Comma-separated names of the committed streams, for messages
                    std::string committed_names() const
                    {
                        std::string names;
                        for( auto const & s : _slots )
                            if( s->committed )
                                names += ( names.empty() ? "" : ", " ) + s->desc.name;
                        return names;
                    }

                    std::vector< stream_profile > record_of( const slot & s ) const
                    {
                        return exact_record( _carrier->get_profiles(), s.desc.frame.payload_size );
                    }

                    // The record holding `first` and every reserved registration, at its highest rate: the capture is
                    // sized once, and a later joiner must fit it
                    stream_profile carrier_profile( const slot & first )
                    {
                        auto best = record_of( first );
                        for( auto const & s : _slots )
                        {
                            if( ! s->desc.frame.reserved )
                                continue;
                            auto record = record_of( *s );
                            if( ! record.empty() && ( best.empty() || capacity( record.front() ) > capacity( best.front() ) ) )
                                best = std::move( record );
                        }
                        if( best.empty() )
                            throw backend_exception( "The GMSL IR node offers no RSVL record for " + first.desc.name );
                        return _carrier_profile = best.front();
                    }

                    void set_enable( slot & s, bool on )
                    {
                        s.enable( s.desc.stream_id, on, s.profile );
                        s.enabled = on;
                    }

                    // Queue ownership precedes this first enable: a request another client left behind, even for this
                    // stream, is cleared through the starting device's channel. One that stays only costs dropped frames.
                    void clear_stale_requests( const slot & by )
                    {
                        std::set< uint32_t > ids;
                        for( auto const & s : _slots )
                        {
                            if( ! ids.insert( s->desc.stream_id ).second )
                                continue;
                            try
                            {
                                by.enable( s->desc.stream_id, false, {} );
                            }
                            catch( const std::exception & e )
                            {
                                LOG_WARNING( s->desc.name << " stale request not cleared: " << e.what() );
                            }
                        }
                    }

                    slot * reject( const char * what, uint32_t value )
                    {
                        // Bad frames repeat at the stream rate, so log the first and every 1000th
                        if( _rejected++ % 1000 == 0 )
                            LOG_ERROR( "MUX frame dropped, unexpected " << what << " 0x" << std::hex << value << std::dec
                                                                        << " (" << _rejected << " dropped)" );
                        return nullptr;
                    }

                    // Validate the MUX metadata and rewrite it in the stream's USB layout: UVC header + its own block
                    slot * demux( frame_object & f, std::array< uint8_t, 255 > & md )
                    {
                        auto raw = static_cast< const uint8_t * >( f.metadata );
                        if( ! raw || f.metadata_size < MUX_MD_SIZE )
                            return reject( "metadata size", f.metadata_size );
                        uvc_header header;
                        capture_timing timing;
                        std::memcpy( &header, raw, sizeof( header ) );
                        std::memcpy( &timing, raw + uvc_header_size, sizeof( timing ) );
                        if( header.length < MUX_MD_SIZE || header.length > f.metadata_size )
                            return reject( "metadata length", header.length );
                        if( timing.id != CAPTURE_TIMING_ID || timing.size != sizeof( timing ) )
                            return reject( "capture timing block", timing.id );
                        if( timing.version != CAPTURE_TIMING_MUX_VERSION )
                            return reject( "capture timing version", timing.version );
                        if( timing.flags & ~CAPTURE_TIMING_MUX_FLAGS )
                            return reject( "capture timing flags", timing.flags );
                        if( timing.reserved[0] || timing.reserved[1] || timing.reserved[2] )
                            return reject( "capture timing reserved field", timing.reserved[0] | timing.reserved[1] | timing.reserved[2] );

                        // The stream may be registered by several device objects; at most one of them has it started
                        slot * target = nullptr;
                        for( auto const & s : _slots )
                            if( s->desc.metadata.type_id == timing.metadata_type_id && ( ! target || s->started ) )
                                target = s.get();
                        if( ! target )
                            return reject( "metadata type", timing.metadata_type_id );
                        const muxed_stream & desc = target->desc;

                        const uint8_t native_size = header.length - MUX_MD_SIZE;
                        if( native_size || desc.metadata.native_required )
                        {
                            native_header native;
                            if( native_size < sizeof( native ) )
                                return reject( "native metadata size", native_size );
                            std::memcpy( &native, raw + MUX_MD_SIZE, sizeof( native ) );
                            if( native.id != desc.metadata.native_id )
                                return reject( "native metadata id", native.id );
                            if( native.size != native_size || native.size < desc.metadata.native_min_size )
                                return reject( "native metadata size", native.size );
                        }

                        if( f.frame_size < desc.frame.payload_size )
                            return reject( "frame size", static_cast< uint32_t >( f.frame_size ) );

                        header.length = uvc_header_size + native_size;
                        std::memcpy( md.data(), &header, sizeof( header ) );
                        std::memcpy( md.data() + uvc_header_size, raw + MUX_MD_SIZE, native_size );
                        f.metadata = md.data();
                        f.metadata_size = header.length;
                        f.frame_size = desc.frame.payload_size;
                        return target;
                    }

                    // Capture thread
                    void dispatch( frame_object f, std::function< void() > release )
                    {
                        // Valid for the callback only, which copies the metadata into the frame before returning
                        std::array< uint8_t, 255 > md;
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        slot * s = demux( f, md );
                        if( ! s || ! s->started || ! s->callback )
                            return release();
                        s->callback( s->profile, f, std::move( release ) );
                    }

                    // _mtx is taken before _route_mtx. An enable callback runs under _mtx and takes the device's HW monitor
                    // and power locks, then _power_mtx and the nodes' named mutexes; none of those paths takes _mtx back.
                    std::mutex _mtx;                  // capture lifecycle and stream enables
                    std::recursive_mutex _route_mtx;  // the streams' callbacks, held while delivering
                    std::mutex _power_mtx;
                    std::vector< std::unique_ptr< slot > > _slots;
                    stream_profile _carrier_profile{};
                    int _streams_committed = 0;
                    bool _carrier_committed = false;
                    bool _streaming = false;
                    int _power_users = 0;
                    size_t _rejected = 0;
                    std::shared_ptr< uvc_device > _carrier;  // last, so its capture thread stops before the rest goes
                };

                // One MUX per node, shared by every device object of the camera, so all of them see one enable table
                std::shared_ptr< mux > get_mux( const uvc_device_info & info )
                {
                    static std::mutex registry_mtx;
                    static std::map< std::string, std::weak_ptr< mux > > registry;
                    std::lock_guard< std::mutex > lock( registry_mtx );
                    auto & entry = registry[info.device_path];
                    auto shared = entry.lock();
                    if( ! shared )
                        entry = shared = std::make_shared< mux >( info );
                    return shared;
                }

                // The node's own device (no slot), or the device of one of its muxed streams
                class mux_device : public uvc_device
                {
                public:
                    mux_device( std::shared_ptr< mux > owner, mux::slot * s )
                        : _mux( std::move( owner ) )
                        , _slot( s )
                    {
                    }

                    ~mux_device() override
                    {
                        if( _slot )
                            _mux->remove_stream( _slot );
                    }

                    std::shared_ptr< uvc_device > create_muxed_stream_device( const muxed_stream & desc,
                                                                              muxed_stream_enable enable ) override
                    {
                        if( _slot )
                            return nullptr;  // a muxed stream carries none of its own
                        return std::make_shared< mux_device >( _mux, &_mux->add_stream( desc, std::move( enable ) ) );
                    }

                    void probe_and_commit( stream_profile profile, frame_callback callback, int buffers ) override
                    {
                        _mux->commit( _slot, profile, std::move( callback ), buffers );
                    }
                    void stream_on( std::function< void( const notification & n ) > error_handler ) override
                    {
                        _mux->stream_on( _slot, error_handler );
                    }
                    void start_callbacks() override { _mux->set_started( _slot, true ); }
                    void stop_callbacks() override { _mux->set_started( _slot, false ); }
                    void close( stream_profile profile ) override { _mux->close( _slot, profile ); }

                    void set_power_state( power_state state ) override
                    {
                        if( state == _state )
                            return;
                        _mux->set_power( state == D0 );
                        _state = state;
                    }
                    power_state get_power_state() const override { return _state; }

                    // A muxed stream has no node of its own, so nothing to initialize
                    void init_xu( const extension_unit & xu ) override
                    {
                        if( ! _slot )
                            _mux->carrier().init_xu( xu );
                    }
                    bool set_xu( const extension_unit & xu, uint8_t ctrl, const uint8_t * data, int len ) override
                    {
                        return controls().set_xu( xu, ctrl, data, len );
                    }
                    bool get_xu( const extension_unit & xu, uint8_t ctrl, uint8_t * data, int len ) const override
                    {
                        return controls().get_xu( xu, ctrl, data, len );
                    }
                    control_range get_xu_range( const extension_unit & xu, uint8_t ctrl, int len ) const override
                    {
                        return controls().get_xu_range( xu, ctrl, len );
                    }
                    bool get_pu( rs2_option opt, int32_t & value ) const override { return controls().get_pu( opt, value ); }
                    bool set_pu( rs2_option opt, int32_t value ) override { return controls().set_pu( opt, value ); }
                    control_range get_pu_range( rs2_option opt ) const override { return controls().get_pu_range( opt ); }

                    std::vector< stream_profile > get_profiles() const override
                    {
                        return _slot ? _mux->get_profiles( *_slot ) : _mux->get_profiles();
                    }

                    void lock() const override { _mux->carrier().lock(); }
                    void unlock() const override { _mux->carrier().unlock(); }

                    std::string get_device_location() const override { return _mux->carrier().get_device_location(); }
                    usb_spec get_usb_specification() const override { return _mux->carrier().get_usb_specification(); }
                    bool is_platform_jetson() const override { return _mux->carrier().is_platform_jetson(); }

                private:
                    // The node's controls belong to its own user; a muxed stream's live on the camera's control node
                    uvc_device & controls() const
                    {
                        if( _slot )
                            throw not_implemented_exception( _slot->desc.name + " has no controls of its own" );
                        return _mux->carrier();
                    }

                    std::shared_ptr< mux > _mux;
                    mux::slot * _slot;
                    power_state _state = D3;
                };
            }  // namespace

            std::shared_ptr< uvc_device > create_device( const uvc_device_info & info )
            {
                if( ! is_carrier( info ) )
                    return nullptr;
                return std::make_shared< mux_device >( get_mux( info ), nullptr );
            }
        }  // namespace v4l_mipi_mux
    }  // namespace platform
}  // namespace librealsense
