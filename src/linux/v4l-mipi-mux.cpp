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

namespace librealsense
{
    namespace platform
    {
        namespace v4l_mipi_mux
        {
            namespace
            {
                constexpr uint32_t RSVL_FOURCC = 0x5253564C;  // shared carrier format of the logical streams
                constexpr uint32_t GREY_FOURCC = 0x47524559;

                // HWMC opcode 0xC0 controls one logical stream: { version, action, stream id, enable }
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
                // Capture Timing version 2, which names the logical stream in place of the video timing fields
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

                struct mux_member
                {
                    uint16_t mi;                // interface number of the stream on the USB device
                    uint32_t stream_id;         // HWMC MUX control id
                    uint32_t metadata_type_id;  // also the id of the stream's own metadata block
                    uint32_t native_min_size;
                    bool native_required;
                    stream_profile profile;     // the stream's USB profile
                };

                constexpr size_t MUX_MEMBER_COUNT = 2;
                const std::array< mux_member, MUX_MEMBER_COUNT > MUX_MEMBERS = { {
                    { 9, 6, 0x80000020, 83, false, { 2347, 1, 30, GREY_FOURCC } },    // object detection
                    { 11, 7, 0x80000016, 136, true, { 320, 256, 30, GREY_FOURCC } },  // occupancy
                } };

                struct mux_state
                {
                    uint32_t requested;
                    uint32_t applied;
                    uint32_t capture_active;
                };

                constexpr int PRIMARY = -1;  // the multiplexing node's own user, not a logical stream

                // Camera index of an rs-enum IR node, or -1 for any other node
                int ir_camera_index( const uvc_device_info & info )
                {
                    if( ! info.is_mipi || info.mi != 0 )
                        return -1;
                    int index = v4l_mipi_logic::parse_video_index( info.device_path );
                    return info.device_path == "/dev/" + v4l_mipi_logic::rs_enum_video_node_name( "ir", index, false ) ? index : -1;
                }

                bool is_carrier( const uvc_device_info & info )
                {
                    try
                    {
                        return ir_camera_index( info ) >= 0
                            && v4l_mipi_logic::is_format_supported_on_node( info.device_path, "RSVL" );
                    }
                    catch( const std::exception & )
                    {
                        return false;
                    }
                }

                // Owns the multiplexing node. Its own (IR) user and the logical streams are mutually exclusive; the
                // logical streams share one capture, started with the first and stopped with the last.
                class mux
                {
                public:
                    mux( const uvc_device_info & carrier_info, const uvc_device_info & control_info )
                        : _control( std::make_shared< retry_controls_work_around >( std::make_shared< v4l_mipi_device >( control_info ) ) )
                        , _carrier( std::make_shared< v4l_mipi_device >( carrier_info ) )
                    {
                    }

                    uvc_device & carrier() const { return *_carrier; }
                    uvc_device & control() const { return *_control; }

                    std::vector< stream_profile > get_profiles( int m ) const
                    {
                        if( m != PRIMARY )
                            return { MUX_MEMBERS[m].profile };
                        auto profiles = _carrier->get_profiles();
                        profiles.erase( std::remove_if( profiles.begin(), profiles.end(),
                                                        []( const stream_profile & p ) { return p.format == RSVL_FOURCC; } ),
                                        profiles.end() );
                        return profiles;
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

                    void commit( int m, const stream_profile & profile, frame_callback callback, int buffers )
                    {
                        {
                            std::lock_guard< std::mutex > lock( _mtx );
                            if( m == PRIMARY )
                            {
                                if( _members_committed )
                                    throw wrong_api_call_sequence_exception( "The GMSL IR channel is in use by Perception streams" );
                                _carrier->probe_and_commit( profile, std::move( callback ), buffers );
                                _primary_committed = true;
                                return;
                            }
                            if( _primary_committed )
                                throw wrong_api_call_sequence_exception( "Perception streams are unavailable while the GMSL IR channel streams" );
                            if( _slots[m].committed )
                                throw wrong_api_call_sequence_exception( "Device already streaming!" );
                            // Claiming the queue first keeps a process that cannot capture from touching camera state
                            if( ! _members_committed )
                                _carrier->probe_and_commit( carrier_profile(),
                                                            [this]( stream_profile, frame_object f, std::function< void() > release )
                                                            { dispatch( f, std::move( release ) ); },
                                                            buffers );
                            _slots[m].committed = true;
                            ++_members_committed;
                        }
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        _slots[m].callback = std::move( callback );
                    }

                    void stream_on( int m, std::function< void( const notification & ) > error_handler )
                    {
                        std::lock_guard< std::mutex > lock( _mtx );
                        if( m == PRIMARY )
                            return _carrier->stream_on( error_handler );
                        if( ! _slots[m].committed )
                            throw wrong_api_call_sequence_exception( "Perception stream started before it was configured" );
                        if( _slots[m].enabled )
                            return;
                        if( ! _streaming )
                        {
                            clear_stale_requests();
                            set_enable( m, true );
                            // The camera starts the capture here, and refuses it unless the streams' inputs are running
                            try
                            {
                                _carrier->stream_on( error_handler );
                            }
                            catch( const std::exception & e )
                            {
                                throw backend_exception( rsutils::string::from()
                                                         << "The camera refused to start Perception streams; Depth and Color must "
                                                            "already be streaming at a supported resolution (" << e.what() << ")" );
                            }
                            _streaming = true;
                        }
                        else
                            set_enable( m, true );
                    }

                    void set_started( int m, bool on )
                    {
                        if( m == PRIMARY )
                            return on ? _carrier->start_callbacks() : _carrier->stop_callbacks();
                        {
                            std::lock_guard< std::recursive_mutex > route( _route_mtx );
                            _slots[m].started = on;
                        }
                        if( on )
                            _carrier->start_callbacks();
                    }

                    void close( int m, const stream_profile & profile )
                    {
                        if( m == PRIMARY )
                        {
                            std::lock_guard< std::mutex > lock( _mtx );
                            _primary_committed = false;
                            return _carrier->close( profile );
                        }
                        {
                            // Waits for a frame in flight, so no callback follows close()
                            std::lock_guard< std::recursive_mutex > route( _route_mtx );
                            _slots[m].callback = nullptr;
                            _slots[m].started = false;
                        }
                        std::lock_guard< std::mutex > lock( _mtx );
                        if( ! _slots[m].committed )
                            return;
                        _slots[m].committed = false;
                        if( --_members_committed == 0 )
                        {
                            // Stop the capture before the last stream, so the camera never outputs with none enabled
                            _streaming = false;
                            _carrier->stop_callbacks();
                            _carrier->close( _carrier_profile );
                        }
                        if( _slots[m].enabled )
                        {
                            try
                            {
                                set_enable( m, false );
                            }
                            catch( const std::exception & e )
                            {
                                LOG_ERROR( "Perception stream " << MUX_MEMBERS[m].stream_id << " disable failed: " << e.what() );
                            }
                        }
                    }

                private:
                    struct slot
                    {
                        frame_callback callback;
                        bool started = false;
                        bool committed = false;
                        bool enabled = false;
                    };

                    stream_profile carrier_profile()
                    {
                        for( auto const & p : _carrier->get_profiles() )
                            if( p.format == RSVL_FOURCC )
                                return _carrier_profile = p;
                        throw backend_exception( "The GMSL IR node offers no RSVL format" );
                    }

                    // False, with the camera's error code, when the camera rejects the request
                    bool command( uint32_t action, uint32_t stream_id, uint32_t enable, mux_state & state, int32_t & error ) const
                    {
                        std::vector< uint8_t > buf( HWMC_BUFFER_SIZE, 0 );
                        const uint16_t header[] = { 0x14, 0xCDAB };  // size of opcode and parameters, HWMC magic
                        const uint32_t request[] = { MUX_CONTROL_OPCODE, MUX_CONTROL_VERSION, action, stream_id, enable };
                        std::memcpy( buf.data(), header, sizeof( header ) );
                        std::memcpy( buf.data() + sizeof( header ), request, sizeof( request ) );
                        {
                            // The HWMC mailbox is shared with every other HWMC user of the depth node
                            std::lock_guard< uvc_device > lock( *_control );
                            if( ! _control->set_xu( HWMC_XU, HWMC_SELECTOR, buf.data(), static_cast< int >( buf.size() ) )
                                || ! _control->get_xu( HWMC_XU, HWMC_SELECTOR, buf.data(), static_cast< int >( buf.size() ) ) )
                                throw linux_backend_exception( "Perception MUX control transfer failed" );
                        }
                        uint32_t opcode, size;
                        std::memcpy( &opcode, buf.data(), sizeof( opcode ) );
                        std::memcpy( &size, buf.data() + HWMC_DATA_SIZE_OFFSET, sizeof( size ) );
                        if( opcode != MUX_CONTROL_OPCODE )
                        {
                            error = static_cast< int32_t >( opcode );
                            return false;
                        }
                        if( size < sizeof( state ) || size > HWMC_DATA_SIZE_OFFSET )
                        {
                            error = static_cast< int16_t >( size );
                            return false;
                        }
                        std::memcpy( &state, buf.data() + sizeof( opcode ), sizeof( state ) );
                        return true;
                    }

                    void set_enable( int m, bool on )
                    {
                        mux_state state;
                        int32_t error;
                        if( ! command( MUX_SET_ENABLE, MUX_MEMBERS[m].stream_id, on, state, error ) )
                            throw backend_exception( rsutils::string::from() << "Perception stream " << MUX_MEMBERS[m].stream_id
                                                                             << " enable(" << on << ") rejected, error " << error );
                        if( state.requested != uint32_t( on ) || ( state.capture_active && state.applied != uint32_t( on ) ) )
                            throw backend_exception( rsutils::string::from()
                                                     << "Perception stream " << MUX_MEMBERS[m].stream_id << " enable(" << on
                                                     << ") returned requested " << state.requested << ", applied " << state.applied );
                        _slots[m].enabled = on;
                    }

                    // The camera keeps requests across sessions. Owning the capture, any stream not opened here is a leftover.
                    // A stream the camera does not support rejects the query and is skipped.
                    void clear_stale_requests()
                    {
                        mux_state state;
                        int32_t error;
                        for( size_t i = 0; i < MUX_MEMBERS.size(); ++i )
                        {
                            if( ! _slots[i].committed && command( MUX_GET_STATE, MUX_MEMBERS[i].stream_id, 0, state, error )
                                && state.requested )
                            {
                                LOG_INFO( "Clearing a stale request of Perception stream " << MUX_MEMBERS[i].stream_id );
                                if( ! command( MUX_SET_ENABLE, MUX_MEMBERS[i].stream_id, 0, state, error ) )
                                    LOG_WARNING( "Perception stream " << MUX_MEMBERS[i].stream_id
                                                 << " stale request not cleared, error " << error << "; its frames are dropped" );
                            }
                        }
                    }

                    int reject( const char * what, uint32_t value )
                    {
                        // Bad frames repeat at the stream rate, so log the first and every 1000th
                        if( _rejected++ % 1000 == 0 )
                            LOG_ERROR( "Perception MUX frame dropped, unexpected " << what << " 0x" << std::hex << value << std::dec
                                                                                   << " (" << _rejected << " dropped)" );
                        return -1;
                    }

                    // Validate the MUX metadata and rewrite it in the stream's USB layout: UVC header + its own block
                    int demux( frame_object & f, std::array< uint8_t, 255 > & md )
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

                        auto it = std::find_if( MUX_MEMBERS.begin(), MUX_MEMBERS.end(),
                                                [&]( const mux_member & m ) { return m.metadata_type_id == timing.metadata_type_id; } );
                        if( it == MUX_MEMBERS.end() )
                            return reject( "metadata type", timing.metadata_type_id );

                        const uint8_t native_size = header.length - MUX_MD_SIZE;
                        if( native_size || it->native_required )
                        {
                            native_header native;
                            if( native_size < sizeof( native ) )
                                return reject( "native metadata size", native_size );
                            std::memcpy( &native, raw + MUX_MD_SIZE, sizeof( native ) );
                            if( native.id != it->metadata_type_id )
                                return reject( "native metadata id", native.id );
                            if( native.size != native_size || native.size < it->native_min_size )
                                return reject( "native metadata size", native.size );
                        }

                        const size_t payload_size = size_t( it->profile.width ) * it->profile.height;
                        if( f.frame_size < payload_size )
                            return reject( "frame size", static_cast< uint32_t >( f.frame_size ) );

                        header.length = uvc_header_size + native_size;
                        std::memcpy( md.data(), &header, sizeof( header ) );
                        std::memcpy( md.data() + uvc_header_size, raw + MUX_MD_SIZE, native_size );
                        f.metadata = md.data();
                        f.metadata_size = header.length;
                        f.frame_size = payload_size;
                        return static_cast< int >( it - MUX_MEMBERS.begin() );
                    }

                    // Capture thread
                    void dispatch( frame_object f, std::function< void() > release )
                    {
                        // Valid for the callback only, which copies the metadata into the frame before returning
                        std::array< uint8_t, 255 > md;
                        int m = demux( f, md );
                        std::lock_guard< std::recursive_mutex > route( _route_mtx );
                        if( m < 0 || ! _slots[m].started || ! _slots[m].callback )
                            return release();
                        _slots[m].callback( MUX_MEMBERS[m].profile, f, std::move( release ) );
                    }

                    std::shared_ptr< uvc_device > _control;  // the depth node, holding the camera controls
                    std::mutex _mtx;                         // capture lifecycle and stream enables
                    std::recursive_mutex _route_mtx;         // callbacks of the logical streams, held while delivering
                    std::mutex _power_mtx;
                    std::array< slot, MUX_MEMBER_COUNT > _slots;
                    stream_profile _carrier_profile{};
                    int _members_committed = 0;
                    bool _primary_committed = false;
                    bool _streaming = false;
                    int _power_users = 0;
                    size_t _rejected = 0;
                    std::shared_ptr< uvc_device > _carrier;  // last, so its capture thread stops before the rest goes
                };

                // One shared multiplexer per node, whichever of its users is created first
                std::shared_ptr< mux > get_mux( uvc_device_info carrier_info, int camera_index )
                {
                    static std::mutex registry_mtx;
                    static std::map< std::string, std::weak_ptr< mux > > registry;
                    std::lock_guard< std::mutex > lock( registry_mtx );
                    auto & entry = registry[carrier_info.device_path];
                    auto shared = entry.lock();
                    if( ! shared )
                    {
                        auto control_info = carrier_info;
                        control_info.id = control_info.device_path
                            = "/dev/" + v4l_mipi_logic::rs_enum_video_node_name( "depth", camera_index, false );
                        control_info.has_metadata_node = false;
                        control_info.metadata_node_id.clear();
                        shared = std::make_shared< mux >( carrier_info, control_info );
                        entry = shared;
                    }
                    return shared;
                }

                class mux_device : public uvc_device
                {
                public:
                    mux_device( std::shared_ptr< mux > owner, int m )
                        : _mux( std::move( owner ) )
                        , _member( m )
                    {
                    }

                    void probe_and_commit( stream_profile profile, frame_callback callback, int buffers ) override
                    {
                        _mux->commit( _member, profile, std::move( callback ), buffers );
                    }
                    void stream_on( std::function< void( const notification & n ) > error_handler ) override
                    {
                        _mux->stream_on( _member, error_handler );
                    }
                    void start_callbacks() override { _mux->set_started( _member, true ); }
                    void stop_callbacks() override { _mux->set_started( _member, false ); }
                    void close( stream_profile profile ) override { _mux->close( _member, profile ); }

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

                    std::vector< stream_profile > get_profiles() const override { return _mux->get_profiles( _member ); }

                    void lock() const override { _mux->carrier().lock(); }
                    void unlock() const override { _mux->carrier().unlock(); }

                    std::string get_device_location() const override { return _mux->carrier().get_device_location(); }
                    usb_spec get_usb_specification() const override { return _mux->carrier().get_usb_specification(); }
                    bool is_platform_jetson() const override { return _mux->carrier().is_platform_jetson(); }

                private:
                    // Logical streams have no node of their own; their controls live on the depth node
                    uvc_device & controls() const { return _member == PRIMARY ? _mux->carrier() : _mux->control(); }

                    std::shared_ptr< mux > _mux;
                    int _member;
                    power_state _state = D3;
                };
            }  // namespace

            void add_mux_devices( std::vector< std::pair< uvc_device_info, std::string > > & nodes )
            {
                std::vector< std::pair< uvc_device_info, std::string > > members;
                for( auto const & node : nodes )
                {
                    if( ! is_carrier( node.first ) )
                        continue;
                    for( auto const & m : MUX_MEMBERS )
                    {
                        members.push_back( node );
                        members.back().first.mi = m.mi;
                    }
                }
                nodes.insert( nodes.end(), members.begin(), members.end() );
            }

            std::shared_ptr< uvc_device > create_device( const uvc_device_info & info )
            {
                if( ! info.is_mipi )
                    return nullptr;

                auto carrier_info = info;
                carrier_info.mi = 0;
                if( info.mi == 0 )
                {
                    if( ! is_carrier( info ) )
                        return nullptr;
                    return std::make_shared< mux_device >( get_mux( carrier_info, ir_camera_index( info ) ), PRIMARY );
                }

                auto it = std::find_if( MUX_MEMBERS.begin(), MUX_MEMBERS.end(), [&]( const mux_member & m ) { return m.mi == info.mi; } );
                if( it == MUX_MEMBERS.end() )
                    return nullptr;
                int camera_index = ir_camera_index( carrier_info );
                if( camera_index < 0 )
                    throw backend_exception( "Perception stream node " + info.device_path + " is not a GMSL IR node" );
                return std::make_shared< mux_device >( get_mux( carrier_info, camera_index ), static_cast< int >( it - MUX_MEMBERS.begin() ) );
            }
        }  // namespace v4l_mipi_mux
    }  // namespace platform
}  // namespace librealsense
