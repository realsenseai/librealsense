// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:static!

#include <unit-tests/catch.h>
#include <src/hid-sensor.h>
#include <src/stream.h>
#include <src/core/frame-callback.h>
#include <src/platform/stream-profile-impl.h>
#include <librealsense2/hpp/rs_sensor.hpp>

#include <condition_variable>
#include <thread>

using namespace librealsense;
using namespace std::chrono;

namespace {

struct fake_hid : platform::hid_device
{
    bool fail_open = false;
    bool fail_start = false;
    platform::hid_callback callback;

    void register_profiles( const std::vector< platform::hid_profile > & ) override {}
    void open( const std::vector< platform::hid_profile > & ) override
    {
        if( fail_open )
            throw std::runtime_error( "open failed" );
    }
    void close() override {}
    void start_capture( platform::hid_callback cb ) override
    {
        if( fail_start )
            throw std::runtime_error( "start failed" );
        callback = std::move(cb);
    }
    void stop_capture() override { callback = {}; }
    std::vector< platform::hid_sensor > get_sensors() override { return {}; }
    std::vector< uint8_t > get_custom_report_data( const std::string &, const std::string &,
                                                platform::custom_sensor_report_field ) override { return {}; }
    void set_gyro_scale_factor( double ) override {}
    void set_accel_scale_factor( double ) override {}

    void report( const std::string & name )
    {
        float xyz[3]{};
        callback( { {name}, { sizeof(xyz), 0, xyz, nullptr, 0 } } );
    }
};

struct hid_fixture
{
    std::mutex mutex;
    std::condition_variable cv;
    std::vector< std::string > timeouts;
    std::shared_ptr< fake_hid > backend = std::make_shared< fake_hid >();
    std::shared_ptr< hid_sensor > sensor;
    stream_profiles profiles;
    int frames = 0;

    hid_fixture()
    {
        sensor = std::make_shared< hid_sensor >(
            backend, std::unique_ptr< frame_timestamp_reader >( new iio_hid_timestamp_reader ),
            std::unique_ptr< frame_timestamp_reader >( new iio_hid_timestamp_reader ),
            std::map< rs2_stream, std::map< unsigned, unsigned > >{},
            std::vector< std::pair< std::string, stream_profile > >{
                { "Accel", { RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL } },
                { "Gyro", { RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO } } }, nullptr );
        sensor->set_source_owner( sensor.get() );
        for( auto stream : { RS2_STREAM_ACCEL, RS2_STREAM_GYRO } )
        {
            auto profile = std::make_shared< platform::stream_profile_impl< motion_stream_profile > >(
                platform::stream_profile{ 1, 1, 200, 0 } );
            profile->set_stream_type( stream );
            profile->set_format( RS2_FORMAT_MOTION_XYZ32F );
            profile->set_framerate( 200 );
            profiles.push_back( profile );
        }
        auto on_notification = [this]( rs2::notification n )
        {
            if( n.get_category() == RS2_NOTIFICATION_CATEGORY_FRAMES_TIMEOUT )
            {
                std::lock_guard< std::mutex > lock( mutex );
                timeouts.push_back( n.get_description() );
                cv.notify_one();
            }
        };
        sensor->register_notifications_callback(
            { new rs2::notifications_callback< decltype(on_notification) >( on_notification ),
              []( rs2_notifications_callback * cb ) { cb->release(); } } );
    }

    ~hid_fixture()
    {
        if( sensor->is_streaming() )
            sensor->stop();
        sensor->register_notifications_callback( {} );
    }

    void start()
    {
        sensor->start( make_frame_callback( [this]( frame_interface * frame )
        {
            ++frames;
            rs2_release_frame( reinterpret_cast< rs2_frame * >( frame ) );
        } ) );
    }
};

}  // namespace

TEST_CASE( "HID starts asynchronously and reports each silent sensor", "[hid][timeout]" )
{
    hid_fixture f;
    f.sensor->open( f.profiles );
    auto const before = steady_clock::now();
    f.start();
    CHECK( steady_clock::now() - before < seconds(1) );
    std::unique_lock< std::mutex > lock( f.mutex );
    REQUIRE( f.cv.wait_for( lock, seconds(8), [&] { return f.timeouts.size() >= 2; } ) );
    CHECK( f.timeouts[0].find( "Accel" ) != std::string::npos );
    CHECK( f.timeouts[1].find( "Gyro" ) != std::string::npos );
    CHECK( f.frames == 0 );
}

TEST_CASE( "Active HID accel does not hide silent gyro and restarts reset timeout state", "[hid][timeout]" )
{
    hid_fixture f;
    f.sensor->open( f.profiles );
    f.start();
    for( int i = 0; i < 65; ++i )
    {
        f.backend->report( "Accel" );
        std::this_thread::sleep_for( milliseconds(100) );
    }
    f.sensor->stop();
    {
        std::lock_guard< std::mutex > lock( f.mutex );
        REQUIRE( f.timeouts.size() == 1 );
        CHECK( f.timeouts.front().find( "Gyro" ) != std::string::npos );
        CHECK( f.frames == 65 );
        f.timeouts.clear();
    }
    f.start();
    std::this_thread::sleep_for( milliseconds(1200) );
    f.sensor->stop();
    std::lock_guard< std::mutex > lock( f.mutex );
    CHECK( f.timeouts.empty() );
}

TEST_CASE( "HID open and start failures allow retry in the same sensor", "[hid][timeout]" )
{
    hid_fixture f;
    f.backend->fail_open = true;
    CHECK_THROWS( f.sensor->open( f.profiles ) );
    CHECK_FALSE( f.sensor->is_opened() );
    f.backend->fail_open = false;
    f.sensor->open( { f.profiles.front() } );
    f.backend->fail_start = true;
    CHECK_THROWS( f.start() );
    CHECK_FALSE( f.sensor->is_streaming() );
    f.backend->fail_start = false;
    f.start();
    std::unique_lock< std::mutex > lock( f.mutex );
    REQUIRE( f.cv.wait_for( lock, seconds(8), [&] { return ! f.timeouts.empty(); } ) );
    REQUIRE( f.timeouts.size() == 1 );
    CHECK( f.timeouts.front().find( "Accel" ) != std::string::npos );
}
