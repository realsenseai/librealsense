// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace rs2
{
    class viewer_model;
    class notifications_model;
    class ux_window;

    // What a control needs to draw itself, and what it reports back
    struct control_draw_context
    {
        viewer_model & viewer;
        notifications_model & notifications;
        std::string & error_message;
        ux_window & window;
        // Drawn after the panel, so a toggle can sit at its right edge
        std::vector< std::function< void() > > & draw_later;
        float windows_width = 0.f;
        bool update_read_only_options = false;
        bool is_streaming = false;
        std::string filter;      // lowercase; empty shows everything
        bool changed = false;    // set by anything the user touched
    };

    // One drawable control. name() is what the user reads and what the search matches, so it
    // never carries the "##id" suffix ImGui needs.
    class control_model
    {
    public:
        virtual ~control_model() = default;

        virtual std::string const & name() const = 0;
        // Whether the camera still offers this control. Asked every frame, because an option
        // can come and go while the panel is open - an AE toggle brings a whole group with it
        virtual bool drawable() const { return true; }
        virtual void draw( control_draw_context & ) = 0;
    };
}
