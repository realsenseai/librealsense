// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-mipi-mux.h"
#include "v4l-mipi-logic.h"
#include "v4l-mipi-device.h"

#include <rsutils/string/from.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

namespace librealsense
{
    namespace platform
    {
        namespace v4l_mipi_mux
        {
            namespace
            {
                constexpr uint32_t RSVL_FOURCC = 0x5253564C;  // the carrier format the muxed streams share

                // HWMC opcode 0xC0 controls one muxed stream: { version, action, stream id, enable }
                constexpr uint32_t MUX_CONTROL_OPCODE = 0xC0;
                constexpr uint32_t MUX_CONTROL_VERSION = 1;
                constexpr uint32_t MUX_SET_ENABLE = 1;
                constexpr uint32_t MUX_GET_STATE = 2;

                // HWMC rides the depth XU (subdevice 0, unit 3) selector 1, in fixed size buffers
                const extension_unit HWMC_XU = { 0, 3, 2, {} };
                constexpr uint8_t HWMC_SELECTOR = 1;
                constexpr size_t HWMC_BUFFER_SIZE = 1024;
                constexpr size_t HWMC_DATA_SIZE_OFFSET = 1020;

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

                struct mux_state
                {
                    uint32_t requested;
                    uint32_t applied;
                    uint32_t capture_active;
                };

                // Bytes one RSVL record holds
                size_t capacity( const stream_profile & p ) { return size_t( p.width ) * p.height; }

                // The rates of the smallest RSVL record holding `payload` bytes, highest rate first; empty when none does
                std::vector< stream_profile > fitting_record( const std::vector< stream_profile > & profiles, size_t payload )
                {
                    std::vector< stream_profile > record;
                    for( auto const & p : profiles )
                    {
                        if( p.format != RSVL_FOURCC || capacity( p ) < payload )
                            continue;
                        if( ! record.empty() && capacity( p ) != capacity( record.front() ) )
                        {
                            if( capacity( p ) > capacity( record.front() ) )
                                continue;
                            record.clear();
                        }
                        record.push_back( p );
                    }
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

                // The MUX control is an HWMC of the depth node of the carrier's camera
                uvc_device_info control_info_of( const uvc_device_info & carrier_info )
                {
                    auto info = carrier_info;
                    int const camera_index = v4l_mipi_logic::parse_video_index( carrier_info.device_path );
                    info.id = info.device_path = "/dev/" + v4l_mipi_logic::rs_enum_video_node_name( "depth", camera_index, false );
                    info.has_metadata_node = false;
                    info.metadata_node_id.clear();
                    return info;
                }

                // Owns the multiplexing node. Its own (IR) user and the muxed streams are mutually exclusive; the muxed
                // streams share one capture, started with the first and stopped with the last.
                class mux
                {
                public:
                    struct slot
                    {
                        muxed_stream desc;
                        frame_callback callback;
                        stream_profile profile;  // as committed; what the callback reports
                        bool started = false;
                        bool committed = false;
                        bool enabled = false;
                    };

                    explicit mux( const uvc_device_info & carrier_info )
                        : _control( std::make_shared< retry_controls_work_around >(
                            std::make_shared< v4l_mipi_device >( control_info_of( carrier_info ) ) ) )
                        , _carrier( std::make_shared< v4l_mipi_device >( carrier_info ) )
                    {
                    }

                    uvc_device & carrier() const { return *_carrier; }
                    uvc_device & control() const { return *_control; }

                    slot & add_stream( const muxed_stream & desc )
                    {
                        std::lock_guard< std::mutex > lock( _mtx );
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        _slots.push_back( std::make_unique< slot >() );
                        _slots.back()->desc = desc;
                        return *_slots.back();
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

                    // A muxed stream runs at the rate of its RSVL record, or not at all when the camera offers none
                    std::vector< stream_profile > get_profiles( const slot & s ) const
                    {
                        auto record = fitting_record( _carrier->get_profiles(), s.desc.payload_size );
                        if( record.empty() )
                            return {};
                        return { { s.desc.width, s.desc.height, record.front().fps, s.desc.format } };
                    }

                    void set_power( bool on )
                    {
                        std::lock_guard< std::mutex > lock( _power_mtx );
                        if( on )
                        {
                            if( _power_users == 0 )
                            {
                                _carrier->set_power_state( D0 );
                                _control->set_power_state( D0 );
                            }
                            ++_power_users;
                        }
                        else if( _power_users > 0 && --_power_users == 0 )
                        {
                            _control->set_power_state( D3 );
                            _carrier->set_power_state( D3 );
                        }
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
                            if( s->committed )
                                throw wrong_api_call_sequence_exception( "Device already streaming!" );
                            // Claiming the queue first keeps a process that cannot capture from touching camera state
                            if( ! _streams_committed )
                                _carrier->probe_and_commit( carrier_profile(),
                                                            [this]( stream_profile, frame_object f, std::function< void() > release )
                                                            { dispatch( f, std::move( release ) ); },
                                                            buffers );
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
                        if( ! _streaming )
                        {
                            clear_stale_requests();
                            set_enable( *s, true );
                            // Firmware owns the backing graph, including inputs it starts on its own
                            try
                            {
                                _carrier->stream_on( error_handler );
                            }
                            catch( const std::exception & e )
                            {
                                throw backend_exception( rsutils::string::from()
                                                         << "The camera refused to start " << s->desc.name << " (" << e.what() << ")" );
                            }
                            _streaming = true;
                        }
                        else
                            set_enable( *s, true );
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
                    // Comma-separated names of the committed streams, for messages
                    std::string committed_names() const
                    {
                        std::string names;
                        for( auto const & s : _slots )
                            if( s->committed )
                                names += ( names.empty() ? "" : ", " ) + s->desc.name;
                        return names;
                    }

                    // The smallest RSVL record holding every registered stream, at its highest rate; a late joiner must
                    // not resize the queue
                    stream_profile carrier_profile()
                    {
                        const auto profiles = _carrier->get_profiles();
                        std::vector< stream_profile > best;
                        for( auto const & s : _slots )
                        {
                            auto record = fitting_record( profiles, s->desc.payload_size );
                            if( ! record.empty() && ( best.empty() || capacity( record.front() ) > capacity( best.front() ) ) )
                                best = std::move( record );
                        }
                        if( best.empty() )
                            throw backend_exception( "The GMSL IR node offers no RSVL record for its multiplexed streams" );
                        return _carrier_profile = best.front();
                    }

                    // False, with the camera's error code, when the camera rejects the request
                    bool command( uint32_t action, uint32_t stream_id, uint32_t enable, mux_state & state, int32_t & error ) const
                    {
                        std::vector< uint8_t > buf( HWMC_BUFFER_SIZE, 0 );
                        const uint16_t header[] = { 20, 0xCDAB };
                        const uint32_t request[] = { MUX_CONTROL_OPCODE, MUX_CONTROL_VERSION, action, stream_id, enable };
                        std::memcpy( buf.data(), header, sizeof( header ) );
                        std::memcpy( buf.data() + sizeof( header ), request, sizeof( request ) );
                        {
                            // The HWMC mailbox is shared with every other HWMC user of the depth node
                            std::lock_guard< uvc_device > lock( *_control );
                            if( ! _control->set_xu( HWMC_XU, HWMC_SELECTOR, buf.data(), static_cast< int >( buf.size() ) )
                                || ! _control->get_xu( HWMC_XU, HWMC_SELECTOR, buf.data(), static_cast< int >( buf.size() ) ) )
                                throw linux_backend_exception( "MUX control transfer failed" );
                        }
                        uint32_t opcode, size;
                        std::memcpy( &opcode, buf.data(), sizeof( opcode ) );
                        std::memcpy( &size, buf.data() + HWMC_DATA_SIZE_OFFSET, sizeof( size ) );
                        if( opcode != MUX_CONTROL_OPCODE )
                        {
                            error = static_cast< int32_t >( opcode );
                            return false;
                        }
                        // Allow appended fields, but keep the response before the size trailer.
                        if( size < sizeof( state )
                            || size > HWMC_DATA_SIZE_OFFSET - sizeof( opcode ) )
                        {
                            throw backend_exception( "MUX control response size does not match the FW contract" );
                        }
                        std::memcpy( &state, buf.data() + sizeof( opcode ), sizeof( state ) );
                        return true;
                    }

                    void set_enable( slot & s, bool on )
                    {
                        mux_state state;
                        int32_t error;
                        if( ! command( MUX_SET_ENABLE, s.desc.stream_id, on, state, error ) )
                            throw backend_exception( rsutils::string::from() << s.desc.name << " (MUX stream " << s.desc.stream_id
                                                                             << ") enable(" << on << ") rejected, error " << error );
                        // The camera applies the request asynchronously; routing by metadata makes waiting for it unnecessary
                        s.enabled = ( state.requested != 0 );
                        if( state.requested != uint32_t( on ) )
                            throw backend_exception( rsutils::string::from() << s.desc.name << " (MUX stream " << s.desc.stream_id
                                                                             << ") enable(" << on << ") returned requested "
                                                                             << state.requested );
                    }

                    // Queue ownership precedes this first enable: even this stream's armed profile may be stale.
                    // A stream the camera does not support rejects the query and is skipped.
                    void clear_stale_requests()
                    {
                        mux_state state;
                        int32_t error;
                        for( auto const & s : _slots )
                        {
                            if( ! s->enabled && command( MUX_GET_STATE, s->desc.stream_id, 0, state, error ) && state.requested )
                            {
                                LOG_INFO( "Clearing a stale request of " << s->desc.name );
                                if( ! command( MUX_SET_ENABLE, s->desc.stream_id, 0, state, error ) )
                                    LOG_WARNING( s->desc.name << " (MUX stream " << s->desc.stream_id
                                                 << ") stale request not cleared, error " << error << "; its frames are dropped" );
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

                        auto it = std::find_if( _slots.begin(), _slots.end(), [&]( const std::unique_ptr< slot > & s )
                                                { return s->desc.metadata_type_id == timing.metadata_type_id; } );
                        if( it == _slots.end() )
                            return reject( "metadata type", timing.metadata_type_id );
                        const muxed_stream & desc = ( *it )->desc;

                        const uint8_t native_size = header.length - MUX_MD_SIZE;
                        if( native_size || desc.native_required )
                        {
                            native_header native;
                            if( native_size < sizeof( native ) )
                                return reject( "native metadata size", native_size );
                            std::memcpy( &native, raw + MUX_MD_SIZE, sizeof( native ) );
                            if( native.id != desc.native_id )
                                return reject( "native metadata id", native.id );
                            if( native.size != native_size || native.size < desc.native_min_size )
                                return reject( "native metadata size", native.size );
                        }

                        if( f.frame_size < desc.payload_size )
                            return reject( "frame size", static_cast< uint32_t >( f.frame_size ) );

                        header.length = uvc_header_size + native_size;
                        std::memcpy( md.data(), &header, sizeof( header ) );
                        std::memcpy( md.data() + uvc_header_size, raw + MUX_MD_SIZE, native_size );
                        f.metadata = md.data();
                        f.metadata_size = header.length;
                        f.frame_size = desc.payload_size;
                        return it->get();
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

                    std::shared_ptr< uvc_device > _control;  // the depth node, holding the camera controls
                    std::mutex _mtx;                         // capture lifecycle and stream enables
                    std::recursive_mutex _route_mtx;         // the streams' callbacks, held while delivering
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

                // The node's own device (no slot), or the device of one of its muxed streams
                class mux_device : public uvc_device
                {
                public:
                    mux_device( std::shared_ptr< mux > owner, mux::slot * s )
                        : _mux( std::move( owner ) )
                        , _slot( s )
                    {
                    }

                    std::shared_ptr< uvc_device > create_muxed_stream_device( const muxed_stream & desc ) override
                    {
                        if( _slot )
                            return nullptr;  // a muxed stream carries none of its own
                        return std::make_shared< mux_device >( _mux, &_mux->add_stream( desc ) );
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

                    void init_xu( const extension_unit & xu ) override { controls().init_xu( xu ); }
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
                    // Muxed streams have no node of their own; their controls live on the depth node
                    uvc_device & controls() const { return _slot ? _mux->control() : _mux->carrier(); }

                    std::shared_ptr< mux > _mux;
                    mux::slot * _slot;
                    power_state _state = D3;
                };
            }  // namespace

            std::shared_ptr< uvc_device > create_device( const uvc_device_info & info )
            {
                if( ! is_carrier( info ) )
                    return nullptr;
                return std::make_shared< mux_device >( std::make_shared< mux >( info ), nullptr );
            }
        }  // namespace v4l_mipi_mux
    }  // namespace platform
}  // namespace librealsense
