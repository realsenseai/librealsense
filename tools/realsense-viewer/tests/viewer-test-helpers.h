// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "viewer.h"
#include "device-model.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include <vector>
#include <memory>
#include <string>


// Thrown by helpers that need to abort the current test
struct test_exit {};

// ---------------------------------------------------------------------------
// viewer_test — wraps helpers as methods for cleaner test bodies
// ---------------------------------------------------------------------------
class viewer_test;
typedef void (*viewer_test_func)( viewer_test & );


// ---------------------------------------------------------------------------
// Auto-registration
// ---------------------------------------------------------------------------
struct viewer_test_entry
{
    const char *     category;
    const char *     name;
    viewer_test_func func;
    const char *     file;
    int              line;
};

inline std::vector< viewer_test_entry > & viewer_test_registry()
{
    static std::vector< viewer_test_entry > entries;
    return entries;
}

struct viewer_test_registrar
{
    viewer_test_registrar( const char * category, const char * name,
                           viewer_test_func fn, const char * file, int line )
    {
        viewer_test_registry().push_back( { category, name, fn, file, line } );
    }
};


// ---------------------------------------------------------------------------
// VIEWER_TEST macro — auto-registers the test at static-init time
// ---------------------------------------------------------------------------
#define _VT_CONCAT2( a, b ) a##b
#define _VT_CONCAT( a, b ) _VT_CONCAT2( a, b )

#define VIEWER_TEST( CATEGORY, NAME )                                          \
    static void _VT_CONCAT( _vt_fn_, __LINE__ )( viewer_test & );             \
    namespace {                                                                \
    static viewer_test_registrar _VT_CONCAT( _vt_reg_, __LINE__ )(            \
        CATEGORY, NAME, &_VT_CONCAT( _vt_fn_, __LINE__ ), __FILE__, __LINE__ );\
    }                                                                          \
    static void _VT_CONCAT( _vt_fn_, __LINE__ )( viewer_test & test )


// ---------------------------------------------------------------------------
// viewer_test
// ---------------------------------------------------------------------------
class viewer_test
{
public:
    ImGuiTestContext *         imgui;
    rs2::device_models_list & device_models;
    rs2::viewer_model &       viewer_model;

    // Return the first connected device; throws if none found
    rs2::device_model & find_first_device_or_exit();

    // Open a sensor's collapsible panel
    void expand_sensor_panel( rs2::device_model & model,
                              std::shared_ptr< rs2::subdevice_model > sub );
    // Close a sensor's collapsible panel
    void collapse_sensor_panel( rs2::device_model & model,
                                std::shared_ptr< rs2::subdevice_model > sub );
    // Open a sensor's controls section
    void expand_controls( rs2::device_model & model,
                          std::shared_ptr< rs2::subdevice_model > sub );
    // Close a sensor's controls section
    void collapse_controls( rs2::device_model & model,
                            std::shared_ptr< rs2::subdevice_model > sub );

    // Start streaming on a sensor; throws if already streaming
    void click_stream_toggle_on( rs2::device_model & model,
                                 std::shared_ptr< rs2::subdevice_model > sub );
    // Stop streaming on a sensor; throws if already stopped
    void click_stream_toggle_off( rs2::device_model & model,
                                  std::shared_ptr< rs2::subdevice_model > sub );

    // Wait real wall-clock time (not skipped in --auto mode)
    void sleep( float seconds ) { imgui->SleepNoSkip( seconds, 1.0f ); }

    // Poll a condition up to max_attempts times, sleeping interval seconds between checks
    template< typename Pred >
    bool wait_until( int max_attempts, float interval, Pred cond )
    {
        for( int i = 0; i < max_attempts && !cond(); ++i )
            imgui->SleepNoSkip( interval, 0.05f );
        return cond();
    }

    // Open a device's hamburger menu and click the named item
    void click_device_menu_item( rs2::device_model & model, const std::string & item );

    // Set a control option via the UI, auto-detecting the control type (slider, checkbox, or enum)
    void set_control_value( rs2::device_model & model,
                            std::shared_ptr< rs2::subdevice_model > sub,
                            rs2_option option, const std::string & value );
    // Read the current value of a control option as a string
    std::string get_control_value( rs2::device_model & model,
                                   std::shared_ptr< rs2::subdevice_model > sub,
                                   rs2_option option );

    // Open a combo dropdown by ID and select the named item
    void select_combo_item( ImGuiID combo_id, const std::string & item );
    // Select a resolution from the sensor's resolution combo box
    void select_resolution( rs2::device_model & model,
                            std::shared_ptr< rs2::subdevice_model > sub,
                            const std::string & resolution );
    // Select an FPS value from the sensor's shared FPS combo box
    void select_fps( rs2::device_model & model,
                     std::shared_ptr< rs2::subdevice_model > sub,
                     const std::string & fps );

    // Check if a sensor has a writable option
    bool has_option( std::shared_ptr< rs2::subdevice_model > sub, rs2_option option );

    // Wait until all active streams are receiving frames
    bool all_streams_alive( int max_attempts = 30, float interval = 0.5f );

    // -----------------------------------------------------------------------
    // Internal helpers — build ImGui labels and ID seeds for the control panel
    // -----------------------------------------------------------------------
private:
    std::string sensor_label( rs2::device_model & model,
                              std::shared_ptr< rs2::subdevice_model > sub );
    std::string controls_label( rs2::device_model & model,
                                std::shared_ptr< rs2::subdevice_model > sub );
    ImGuiID sensor_id_seed( rs2::device_model & model,
                            std::shared_ptr< rs2::subdevice_model > sub );
    ImGuiID controls_id_seed( rs2::device_model & model,
                              std::shared_ptr< rs2::subdevice_model > sub );
};
