// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Test entry point for the realsense-viewer-tests binary.
// Delegates the entire viewer loop to run_viewer() and hooks the
// imgui_test_engine in via the three optional callbacks.

#include "realsense-viewer.h"
#include "viewer-test-helpers.h"

#include <librealsense2/rs.hpp>
#include <iostream>
#include <cstring>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui_te_ui.h"
#include "imgui_te_exporters.h"


static void register_viewer_tests( ImGuiTestEngine *       engine,
                                    rs2::device_models_list & device_models,
                                    rs2::viewer_model &       viewer_model )
{
    for( auto & e : viewer_test_registry() )
    {
        auto * t = ImGuiTestEngine_RegisterTest( engine, e.category,
                                                  e.name, e.file, e.line );
        viewer_test_func fn = e.func;
        t->TestFunc = [fn, &device_models, &viewer_model]( ImGuiTestContext * ctx )
        {
            viewer_test vtc{ ctx, device_models, viewer_model };
            fn( vtc );
        };
    }
}

int main( int argc, const char ** argv ) try
{
    // Strip test-specific flags from argv before passing to run_viewer
    // (which uses TCLAP and would reject unknown flags).
    //   --auto          Run all tests automatically and exit (headless mode)
    //   -r <filter>     Only run tests whose name contains <filter>
    bool auto_run_mode = false;
    const char * test_filter = nullptr;
    std::vector< const char * > filtered_argv;
    filtered_argv.push_back( argv[0] );
    for( int i = 1; i < argc; ++i )
    {
        if( strcmp( argv[i], "--auto" ) == 0 )
            auto_run_mode = true;
        else if( strcmp( argv[i], "-r" ) == 0 && i + 1 < argc )
            test_filter = argv[++i];
        else
            filtered_argv.push_back( argv[i] );
    }
    int filtered_argc = static_cast< int >( filtered_argv.size() );

    ImGuiTestEngine * test_engine   = nullptr;
    bool              tests_queued  = false;
    int               exit_code    = EXIT_SUCCESS;

    // for testing setup, init the test engine and register tests before the viewer starts
    auto on_setup = [&]( rs2::device_models_list & device_models,
                         rs2::viewer_model & viewer_model )
    {
        test_engine = ImGuiTestEngine_CreateContext();
        ImGuiTestEngineIO & te_io = ImGuiTestEngine_GetIO( test_engine );
        te_io.ConfigVerboseLevel  = ImGuiTestVerboseLevel_Info;
        te_io.ConfigLogToTTY      = true;
        te_io.ConfigSavedSettings = false;
        if( auto_run_mode )
        {
            te_io.ConfigRunSpeed   = ImGuiTestRunSpeed_Fast;
            te_io.ConfigNoThrottle = true;
        }
        register_viewer_tests( test_engine, device_models, viewer_model );
        ImGuiTestEngine_Start( test_engine, ImGui::GetCurrentContext() );
    };

    // loop to allow auto closing the viewer when tests are done in auto-run mode
    auto keep_alive = [&]() -> bool
    {
        // Position the test engine window at the top-right corner and give it a reasonable width.
        float win_w = ImGui::GetFontSize() * 50;
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - win_w, 0),
            ImGuiCond_FirstUseEver);
        ImGuiTestEngine_ShowTestEngineWindows( test_engine, nullptr );

        if( !auto_run_mode )
            return true;

        if( !tests_queued )
        {
            ImGuiTestEngine_QueueTests( test_engine, ImGuiTestGroup_Tests, test_filter,
                                        ImGuiTestRunFlags_RunFromCommandLine );
            tests_queued = true;
        }

        return !ImGuiTestEngine_IsTestQueueEmpty( test_engine );
    };

    auto on_teardown = [&]()
    {
        int count_tested = 0, count_success = 0;
        ImGuiTestEngine_GetResult( test_engine, count_tested, count_success );
        ImGuiTestEngine_PrintResultSummary( test_engine );
        if( auto_run_mode && ( count_tested == 0 || count_success != count_tested ) )
            exit_code = EXIT_FAILURE;
        ImGuiTestEngine_Stop( test_engine );
        ImGuiTestEngine_DestroyContext( test_engine );
    };

    run_viewer( filtered_argc, filtered_argv.data(), on_setup, keep_alive, on_teardown );

    return exit_code;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function()
              << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
