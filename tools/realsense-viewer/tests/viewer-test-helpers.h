// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "viewer.h"
#include "device-model.h"
#include "option-model.h"
#include "processing-block-model.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include <vector>
#include <memory>
#include <string>


// Thrown by helpers that need to abort the current test
struct test_exit {};

// Clears a sensor's search box however the test ends. IM_CHECK returns from the test on
// failure and the suite shares one viewer, so a box left filtered would follow into the next
// test and hide the controls it goes looking for.
struct controls_filter_reset
{
    std::shared_ptr< rs2::subdevice_model > sub;
    ~controls_filter_reset()
    {
        if( sub )
            sub->options_filter.clear();
    }
};

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

    // Replace the text in the Controls section's search/filter box ("" clears it)
    void set_controls_filter( rs2::device_model & model,
                              std::shared_ptr< rs2::subdevice_model > sub,
                              const std::string & text );
    // Click the button beside the box that clears the search
    void click_controls_filter_clear( rs2::device_model & model,
                                      std::shared_ptr< rs2::subdevice_model > sub );
    // Options whose control widgets are currently rendered inside the Controls section
    // (single gather pass; requires the sensor panel and Controls section to be expanded)
    std::vector< rs2_option > controls_options( rs2::device_model & model,
                                                std::shared_ptr< rs2::subdevice_model > sub );
    // Lowercased display name of an option's control
    std::string control_name( std::shared_ptr< rs2::subdevice_model > sub, rs2_option option );
    // Whether an option's control is currently rendered inside the Controls section
    bool control_visible( rs2::device_model & model,
                          std::shared_ptr< rs2::subdevice_model > sub, rs2_option option );
    // Whether an option's widget is currently rendered under one post-processing filter
    bool post_processing_option_visible( rs2::device_model & model,
                                         std::shared_ptr< rs2::subdevice_model > sub,
                                         std::shared_ptr< rs2::processing_block_model > pb,
                                         rs2_option option );
    // How far from the Control Panel's right edge a filter's enable toggle is drawn. The toggles are
    // deferred to the end of the panel, so a broken deferral puts them at the left edge instead.
    float post_processing_toggle_inset( rs2::device_model & model,
                                        std::shared_ptr< rs2::subdevice_model > sub,
                                        std::shared_ptr< rs2::processing_block_model > pb );
    // The ImGui id of the node at the end of this path, each element seeding the next - pass it to
    // ItemOpen / ItemInputValue to drive a node that lives under a sensor
    ImGuiID node_id( rs2::device_model & model,
                     std::shared_ptr< rs2::subdevice_model > sub,
                     std::vector< std::string > const & path );
    // Whether the node at the end of this path is rendered - so a heading with nothing under it, or
    // a control inside a section, is one call either way
    bool node_shown( rs2::device_model & model,
                     std::shared_ptr< rs2::subdevice_model > sub,
                     std::vector< std::string > const & path );
    // The tree labels the viewer draws, for building those paths
    std::string controls_label( rs2::device_model & model,
                                std::shared_ptr< rs2::subdevice_model > sub );
    std::string post_processing_label( rs2::device_model & model );
    std::string embedded_filters_label( rs2::device_model & model );
    std::string filter_label( rs2::device_model & model, std::string const & name );

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

    // -----------------------------------------------------------------------
    // Post-processing panel
    // -----------------------------------------------------------------------
    // Find the post-processing filter exposing the given option (nullptr if none)
    std::shared_ptr< rs2::processing_block_model > find_post_processing_filter(
        std::shared_ptr< rs2::subdevice_model > sub, rs2_option option );
    // Open the sensor's "Post-Processing" section
    void expand_post_processing( rs2::device_model & model,
                                 std::shared_ptr< rs2::subdevice_model > sub );
    // Turn the master post-processing toggle on (no-op if already on)
    void enable_post_processing( rs2::device_model & model,
                                 std::shared_ptr< rs2::subdevice_model > sub );
    // Turn a single filter's toggle on (no-op if already on); requires the
    // Post-Processing section to be expanded and post-processing enabled first
    void enable_post_processing_filter( rs2::device_model & model,
                                        std::shared_ptr< rs2::subdevice_model > sub,
                                        std::shared_ptr< rs2::processing_block_model > pb );
    // Open a single filter's controls under the Post-Processing section
    void expand_post_processing_filter( rs2::device_model & model,
                                        std::shared_ptr< rs2::subdevice_model > sub,
                                        std::shared_ptr< rs2::processing_block_model > pb );
    // Set / read a post-processing filter option value via the UI
    void set_post_processing_value( rs2::device_model & model,
                                    std::shared_ptr< rs2::subdevice_model > sub,
                                    std::shared_ptr< rs2::processing_block_model > pb,
                                    rs2_option option, const std::string & value );
    std::string get_post_processing_value( rs2::device_model & model,
                                           std::shared_ptr< rs2::subdevice_model > sub,
                                           std::shared_ptr< rs2::processing_block_model > pb,
                                           rs2_option option );

    // Wait until all active streams are receiving frames
    bool all_streams_alive( int max_attempts = 30, float interval = 0.5f );

    // -----------------------------------------------------------------------
    // Internal helpers — build ImGui labels and ID seeds for the control panel
    // -----------------------------------------------------------------------
private:
    std::string sensor_label( rs2::device_model & model,
                              std::shared_ptr< rs2::subdevice_model > sub );
    ImGuiID sensor_id_seed( rs2::device_model & model,
                            std::shared_ptr< rs2::subdevice_model > sub );
    ImGuiID controls_id_seed( rs2::device_model & model,
                              std::shared_ptr< rs2::subdevice_model > sub );
    ImGuiID post_processing_filter_id_seed( rs2::device_model & model,
                                            std::shared_ptr< rs2::subdevice_model > sub,
                                            std::shared_ptr< rs2::processing_block_model > pb );
    // Drive / read a control widget given its already-resolved ImGui id seed
    void set_value_by_seed( rs2::option_model & opt, ImGuiID seed, const std::string & value );
    std::string get_value_by_seed( rs2::option_model & opt, ImGuiID seed );
};
