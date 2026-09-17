// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Test scaffolding - NOT a real hardware round trip. Proves the E2E behavior of the generic
// "composite option" mechanism using a minimal in-memory fake composite_option_interface
// standing in for a real device-backed composite_xu_option, driven through the real public API.
//
// What this proves, for each of the three DPP composite-option structs: (1) round-trip
// correctness - a sent payload comes back byte-identical; (2) atomicity - exactly one
// set_raw()/get_raw() call reaches the fake "wire" per logical operation, never split per-field,
// the non-negotiable requirement from the HKR/FW spec. One parameterized test body covers all
// three structs instead of a near-identical copy per struct.

#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_hdrd_control.h>

#include <src/composite-option-interface.h>
#include <src/core/options-interface.h>
#include <src/proc/synthetic-stream.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace librealsense;

namespace {

// Minimal fake composite option standing in for librealsense::composite_xu_option. Implements
// ONLY composite_option_interface, matching the real class exactly. get_raw()/set_raw() are
// this test's "wire": each is called exactly once per logical call, counters below prove it.
class fake_composite_option : public composite_option_interface
{
public:
    explicit fake_composite_option( std::string description ) : _description( std::move( description ) ) {}

    // --- call counters: this is the atomicity proof ---
    mutable int get_calls = 0;
    int set_calls = 0;

    bool is_enabled() const override { return true; }
    bool is_read_only() const override { return false; }
    const char * get_description() const override { return _description.c_str(); }

    // composite_option_interface: the fake "wire" - exactly one call per logical operation.
    std::vector< uint8_t > get_raw() const override
    {
        ++get_calls;
        return _storage;
    }

    // get_range()-style query, not exercised by this test's round-trip/atomicity assertions -
    // just needs to satisfy the interface. Real behavior lives in
    // librealsense::composite_xu_option::get_raw_range() (one get_xu_range() call).
    std::vector< uint8_t > get_raw_range() const override { return {}; }

    void set_raw( const void * data, size_t size ) override
    {
        ++set_calls;
        auto p = reinterpret_cast< const uint8_t * >( data );
        _storage.assign( p, p + size );
    }

private:
    std::vector< uint8_t > _storage;
    std::string _description;
};

// Minimal fake options container: implements librealsense::options_interface directly (no
// dependency on options_container). Holds no scalar rs2_option at all - only the one
// registered composite option, demonstrating the two registries are completely separate.
class fake_options_container : public options_interface
{
public:
    fake_options_container( rs2_composite_option_id id, std::shared_ptr< fake_composite_option > opt )
        : _id( id ), _opt( std::move( opt ) )
    {
    }

    // Scalar rs2_option side: intentionally empty - this container exposes no scalar options.
    option & get_option( rs2_option ) override { throw std::runtime_error( "fake_options_container: no scalar options" ); }
    const option & get_option( rs2_option ) const override { throw std::runtime_error( "fake_options_container: no scalar options" ); }
    bool supports_option( rs2_option ) const override { return false; }
    std::vector< rs2_option > get_supported_options() const override { return {}; }
    std::string const & get_option_name( rs2_option ) const override { return _name; }

    // Composite-option side: the one id this test exercises.
    composite_option_interface & get_composite_option( rs2_composite_option_id id ) override
    {
        return const_cast< composite_option_interface & >(
            const_cast< const fake_options_container * >( this )->get_composite_option( id ) );
    }
    const composite_option_interface & get_composite_option( rs2_composite_option_id id ) const override
    {
        if( id != _id )
            throw std::runtime_error( "fake_options_container: unsupported composite option id" );
        return *_opt;
    }
    bool supports_composite_option( rs2_composite_option_id id ) const override { return id == _id; }
    std::vector< rs2_composite_option_id > get_supported_composite_options() const override { return { _id }; }
    std::string const & get_composite_option_name( rs2_composite_option_id ) const override { return _name; }

    rsutils::subscription register_options_changed_callback( options_watcher::callback && ) override
    {
        return rsutils::subscription();
    }

    // recordable<options_interface>: not exercised by this test.
    void create_snapshot( std::shared_ptr< options_interface > & snapshot ) const override { snapshot.reset(); }
    void enable_recording( std::function< void( const options_interface & ) > ) override {}

private:
    rs2_composite_option_id _id;
    std::shared_ptr< fake_composite_option > _opt;
    std::string _name = "composite option under test";
};

// Lets this standalone test call the protected rs2::options(rs2_options*) constructor - the same
// one rs2::sensor/rs2::embedded_filter use internally - to exercise get/set_composite_option().
class fake_options_handle : public rs2::options
{
public:
    explicit fake_options_handle( rs2_options * o ) : options( o ) {}
};

// Runs the round-trip + atomicity test body for one composite-option struct type. `initial` and
// `modified` must already differ in at least one field - `equal` and `print` are the only
// per-struct knowledge this function needs, since it otherwise only moves raw bytes.
template< typename ConfigT >
bool run_round_trip_and_atomicity_test( rs2_composite_option_id id,
                                          const char * description,
                                          ConfigT initial,
                                          ConfigT modified,
                                          const std::function< bool( const ConfigT &, const ConfigT & ) > & equal,
                                          const std::function< void( std::ostream &, const ConfigT & ) > & print )
{
    std::cout << "=== " << description << " ===" << std::endl;
    auto fake_opt = std::make_shared< fake_composite_option >( description );
    fake_options_container container( id, fake_opt );
    rs2_options wrapper( &container );

    // --- 1) Exercise the raw C API path: rs2_set_composite_option / rs2_get_composite_option ---
    rs2_error * e = nullptr;
    rs2_set_composite_option( &wrapper, id, &initial, sizeof( initial ), &e );
    rs2::error::handle( e );

    auto buffer = rs2_get_composite_option( &wrapper, id, &e );
    rs2::error::handle( e );
    std::shared_ptr< const rs2_raw_data_buffer > buffer_guard( buffer, rs2_delete_raw_data );

    auto size = rs2_get_raw_data_size( buffer, &e );
    rs2::error::handle( e );
    if( (size_t)size != sizeof( ConfigT ) )
        throw std::runtime_error( "rs2_get_composite_option returned an unexpected payload size" );

    ConfigT received{};
    auto const * raw = rs2_get_raw_data( buffer, &e );
    rs2::error::handle( e );
    std::memcpy( &received, raw, sizeof( received ) );

    // --- 2) Exercise the direct C++ get_composite_option()/set_composite_option() path on the
    //        SAME underlying options object - no wrapper/handle type, no casting ---
    fake_options_handle handle( &wrapper );

    auto supported = handle.get_supported_composite_options();
    if( std::find( supported.begin(), supported.end(), id ) == supported.end() )
        throw std::runtime_error( "get_supported_composite_options() unexpectedly missing this id" );

    handle.set_composite_option( id, &modified, sizeof( modified ) );

    auto bytes2 = handle.get_composite_option( id );
    if( bytes2.size() != sizeof( ConfigT ) )
        throw std::runtime_error( "get_composite_option() returned an unexpected payload size" );

    ConfigT received2{};
    std::memcpy( &received2, bytes2.data(), sizeof( received2 ) );

    // 1) Round-trip correctness - both paths.
    bool round_trip_ok = equal( received, initial ) && equal( received2, modified );

    // 2) Atomicity - exactly one wire call per logical operation (2 sets + 2 gets total across
    // both paths above), never split per-field.
    bool atomicity_ok = ( fake_opt->set_calls == 2 ) && ( fake_opt->get_calls == 2 );

    std::cout << "[C API]      sent:     "; print( std::cout, initial );  std::cout << std::endl;
    std::cout << "[C API]      received: "; print( std::cout, received ); std::cout << std::endl;
    std::cout << "[C++ direct] sent:     "; print( std::cout, modified ); std::cout << std::endl;
    std::cout << "[C++ direct] received: "; print( std::cout, received2 ); std::cout << std::endl;
    std::cout << "set_calls=" << fake_opt->set_calls << "  get_calls=" << fake_opt->get_calls << std::endl;

    assert( round_trip_ok );
    assert( atomicity_ok );
    if( ! round_trip_ok || ! atomicity_ok )
    {
        std::cerr << description << ": FAIL" << std::endl;
        return false;
    }
    std::cout << "PASS: round-trip correctness AND atomicity (exactly 1 set + 1 get per logical "
                 "operation) both verified, through the real public API." << std::endl;
    return true;
}

bool test_decimation_filter_dpp()
{
    rs2_decimation_filter_dpp_config initial{};
    initial.enabled = 1;
    initial.magnitude = 2;
    rs2_decimation_filter_dpp_config modified = initial;
    modified.magnitude = 4;  // change the one field to prove this second round trip is independent

    return run_round_trip_and_atomicity_test< rs2_decimation_filter_dpp_config >(
        RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP, "Decimation Filter DPP (fake, for testing)", initial, modified,
        []( const rs2_decimation_filter_dpp_config & a, const rs2_decimation_filter_dpp_config & b )
        { return a.enabled == b.enabled && a.magnitude == b.magnitude; },
        []( std::ostream & os, const rs2_decimation_filter_dpp_config & v )
        { os << "enabled=" << v.enabled << " magnitude=" << v.magnitude; } );
}

bool test_temporal_filter_dpp()
{
    rs2_temporal_filter_dpp_config initial{};
    initial.enabled = 1;
    initial.smooth_alpha = 0.4F;  // normalized float in [0,1] - see rs_temporal_filter_dpp.h
    initial.smooth_delta = 20;
    initial.persistency_index = 3;
    rs2_temporal_filter_dpp_config modified = initial;
    modified.persistency_index = 7;  // change one field to prove this second round trip is independent

    return run_round_trip_and_atomicity_test< rs2_temporal_filter_dpp_config >(
        RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP, "Temporal Filter DPP (fake, for testing)", initial, modified,
        []( const rs2_temporal_filter_dpp_config & a, const rs2_temporal_filter_dpp_config & b )
        {
            return a.enabled == b.enabled && a.smooth_alpha == b.smooth_alpha
                && a.smooth_delta == b.smooth_delta && a.persistency_index == b.persistency_index;
        },
        []( std::ostream & os, const rs2_temporal_filter_dpp_config & v )
        {
            os << "enabled=" << v.enabled << " smooth_alpha=" << v.smooth_alpha
               << " smooth_delta=" << v.smooth_delta << " persistency_index=" << v.persistency_index;
        } );
}

bool test_hdrd_control()
{
    rs2_hdrd_control initial{};
    initial.enable = 1;
    initial.filter_type = 0;
    initial.downscale_ratio = 1;
    rs2_hdrd_control modified = initial;
    modified.downscale_ratio = 2;  // change one field to prove this second round trip is independent

    return run_round_trip_and_atomicity_test< rs2_hdrd_control >(
        RS2_COMPOSITE_OPTION_HDRD_CONTROL, "HDRD/Improved Close Range Control (fake, for testing)", initial, modified,
        []( const rs2_hdrd_control & a, const rs2_hdrd_control & b )
        {
            return a.enable == b.enable && a.filter_type == b.filter_type
                && a.downscale_ratio == b.downscale_ratio && a.shift_mode == b.shift_mode
                && a.shift_pixels == b.shift_pixels && a.threshold_mode == b.threshold_mode
                && a.threshold_mm == b.threshold_mm;
        },
        []( std::ostream & os, const rs2_hdrd_control & v )
        { os << "enable=" << v.enable << " filter_type=" << v.filter_type << " downscale_ratio=" << v.downscale_ratio; } );
}

}  // namespace


int main()
try
{
    bool ok = test_decimation_filter_dpp();
    ok = test_temporal_filter_dpp() && ok;
    ok = test_hdrd_control() && ok;
    return ok ? 0 : 1;
}
catch( const rs2::error & e )
{
    std::cerr << "FAIL: librealsense error: " << e.what() << std::endl;
    return 1;
}
catch( const std::exception & e )
{
    std::cerr << "FAIL: unexpected exception: " << e.what() << std::endl;
    return 1;
}
