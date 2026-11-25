// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake: static!

#include <src/frame.h>
#include <src/core/frame-holder.h>
#include <src/frame-archive.h>
#include <unit-tests/catch.h>

using namespace librealsense;

namespace {
struct counted_frame : frame
{
    static unsigned allocations;

    void resize_storage( size_t bytes )
    {
        if( bytes > data.capacity() )
            ++allocations;
        frame::resize_storage( bytes );
    }
};
unsigned counted_frame::allocations = 0;
}

TEST_CASE( "The first owned pixel buffer survives a borrowed frame and a different frame size" )
{
    std::atomic< uint32_t > limit{ 1 };
    std::shared_ptr< archive_interface > archive = std::make_shared< frame_archive< counted_frame > >( &limit, nullptr );
    counted_frame::allocations = 0;

    archive->reserve_frame( 512 );
    archive->reserve_frame( 512 );
    REQUIRE( counted_frame::allocations == 1 );

    frame_additional_data late;
    late.timestamp = 5000;
    frame_holder borrowed = archive->alloc_and_track( 512, std::move( late ), false );
    REQUIRE( borrowed.frame );
    CHECK( static_cast< counted_frame * >( borrowed.frame )->storage_size() == 0 );
    borrowed.reset();

    frame_holder other = archive->alloc_and_track( 128, {}, true );
    REQUIRE( other.frame );
    REQUIRE( counted_frame::allocations == 2 );
    other.reset();

    frame_holder first = archive->alloc_and_track( 512, {}, true );
    REQUIRE( first.frame );
    CHECK( first->get_frame_data_size() == 512 );
    CHECK( counted_frame::allocations == 2 );
    first.reset();

    frame_additional_data idle;
    idle.timestamp = 10000;
    borrowed = archive->alloc_and_track( 512, std::move( idle ), false );
    REQUIRE( borrowed.frame );
    borrowed.reset();
    frame_holder next = archive->alloc_and_track( 512, {}, true );
    REQUIRE( next.frame );
    CHECK( counted_frame::allocations == 2 );
    next.reset();
    archive->flush();
}

TEST_CASE( "Cached mapped buffers remain bounded by the configured queue" )
{
    std::atomic< uint32_t > limit{ 2 };
    std::shared_ptr< archive_interface > archive = std::make_shared< frame_archive< counted_frame > >( &limit, nullptr );
    counted_frame::allocations = 0;
    archive->reserve_frame( 512 );
    REQUIRE( counted_frame::allocations == 2 );

    frame_holder a = archive->alloc_and_track( 512, {}, true );
    frame_holder b = archive->alloc_and_track( 512, {}, true );
    REQUIRE( a.frame );
    REQUIRE( b.frame );
    a->keep();
    frame_holder extra = archive->alloc_and_track( 512, {}, true );
    REQUIRE( extra.frame );
    REQUIRE( counted_frame::allocations == 3 );
    a.reset();
    b.reset();
    extra.reset();

    frame_additional_data idle;
    idle.timestamp = 5000;
    frame_holder borrowed = archive->alloc_and_track( 512, std::move( idle ), false );
    REQUIRE( borrowed.frame );
    borrowed.reset();

    a = archive->alloc_and_track( 512, {}, true );
    b = archive->alloc_and_track( 512, {}, true );
    REQUIRE( a.frame );
    REQUIRE( b.frame );
    CHECK( counted_frame::allocations == 3 );
    a->keep();
    extra = archive->alloc_and_track( 512, {}, true );
    REQUIRE( extra.frame );
    CHECK( counted_frame::allocations == 4 );
    a.reset();
    b.reset();
    extra.reset();
    archive->flush();
}

TEST_CASE( "Capture can fill a third frame while two consumers retain earlier outputs" )
{
    std::atomic< uint32_t > limit{ 16 };
    std::shared_ptr< archive_interface > archive = std::make_shared< frame_archive< counted_frame > >( &limit, nullptr );
    counted_frame::allocations = 0;
    archive->reserve_frame( 512 );
    REQUIRE( counted_frame::allocations == 3 );
    frame_holder delivered = archive->alloc_and_track( 512, {}, true );
    frame_holder waiting = archive->alloc_and_track( 512, {}, true );
    frame_holder next = archive->alloc_and_track( 512, {}, true );
    REQUIRE( delivered.frame );
    REQUIRE( waiting.frame );
    REQUIRE( next.frame );
    CHECK( counted_frame::allocations == 3 );
    delivered.reset();
    waiting.reset();
    next.reset();
    archive->flush();
}
