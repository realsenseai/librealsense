// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"

#include <algorithm>
#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>
#include "../src/ds/d500/d500-private.h"


namespace {

using calib_table = rs2::calibration_table;

// Puts the camera's calibration back on the way out, so a failed check (IM_CHECK returns from the
// test) never leaves a modified table in flash for the tests that follow.
struct factory_calibration_restore
{
    rs2::auto_calibrated_device dev;
    ~factory_calibration_restore() { try { dev.reset_to_factory_calibration(); } catch( ... ) {} }
};

}  // namespace


// D500 "Calibration Data" window: after a modified calibration table was written to flash, the
// "Restore Factory" button has to bring the factory table back
VIEWER_TEST( "calibration", "restore_factory" )
{
    test.find_first_device_or_exit();
    auto const is_d500 = []( rs2::device const & d ) {
        return d.supports( RS2_CAMERA_INFO_PRODUCT_LINE ) && d.is< rs2::auto_calibrated_device >()
            && std::string( d.get_info( RS2_CAMERA_INFO_PRODUCT_LINE ) ) == "D500";
    };
    auto it = std::find_if( test.device_models.begin(), test.device_models.end(),
                            [&]( auto const & m ) { return is_d500( m->dev ); } );
    if( it == test.device_models.end() )
        return;   // the restore button of this window is a D500 feature
    auto & model = **it;

    auto dev = model.dev.as< rs2::auto_calibrated_device >();

    // start from the factory table, it is what the button has to restore
    factory_calibration_restore restore{ dev };
    dev.reset_to_factory_calibration();
    calib_table const factory = dev.get_calibration_table();
    IM_CHECK( factory.size() > sizeof( librealsense::ds::table_header ) );

    // write a modified table: nudge the (read-only in the window) baseline and fix the CRC
    calib_table modified = factory;
    auto * table = reinterpret_cast< librealsense::ds::d500_coefficients_table * >( modified.data() );
    table->baseline += 1.f;
    table->header.crc32 = rsutils::number::calc_crc32(
        modified.data() + sizeof( librealsense::ds::table_header ),
        modified.size() - sizeof( librealsense::ds::table_header ) );
    dev.set_calibration_table( modified );
    dev.write_calibration();
    IM_CHECK( dev.get_calibration_table() == modified );
    IM_CHECK( modified != factory );

    // the window reads the camera's table when it opens
    test.click_device_menu_item( model, "Calibration Data" );
    test.sleep( 1.f );
    test.imgui->SetRef( "Calibration Window" );
    // restore is enabled only after the user ticked "I know what I'm doing"
    test.imgui->ItemCheck( "I know what I'm doing" );
    std::string const restore_label = rsutils::string::from()
        << rs2::textual_icons::industry << " Restore Factory";
    test.imgui->ItemClick( restore_label.c_str() );

    IM_CHECK( test.wait_until( 20, 0.25f, [&] { return dev.get_calibration_table() == factory; } ) );

    test.imgui->ItemClick( "Cancel" );
}
