// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <functional>
#include "viewer.h"

// Runs the RealSense Viewer main loop.
//
// on_setup:    configure additional components before the viewer starts.
// keep_alive:  logic to determine whether the viewer should keep running (e.g. to wait for tests to finish in test mode).
// on_teardown: collect results and clean up before exit.
int run_viewer( int argc, const char ** argv,
                std::function< void( rs2::device_models_list &, rs2::viewer_model & ) > on_setup    = nullptr,
                std::function< bool() >                                                  keep_alive = nullptr,
                std::function< void() >                                                  on_teardown = nullptr );
