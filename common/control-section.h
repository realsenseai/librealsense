// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "control-model.h"

#include <imgui.h>
#include <rsutils/string/string-utilities.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rs2
{
    inline bool contains_ci( std::string const & haystack, std::string const & lowercase_needle )
    {
        return rsutils::string::to_lower( haystack ).find( lowercase_needle ) != std::string::npos;
    }

    // A titled group of controls, and of nested groups. A group with nothing to show under the
    // current search is not drawn at all, its heading included.
    class control_section
    {
    public:
        // title is what the user reads and what the search matches; label is what ImGui gets,
        // "##id" included. searchable_title is false for the container headings - Controls,
        // Post-Processing, Embedded-Filters, Advanced Controls - which name no control.
        control_section( std::string title, std::string label, bool searchable_title = true )
            : _title( std::move( title ) ), _label( std::move( label ) )
            , _searchable_title( searchable_title )
        {
        }

        void add( std::unique_ptr< control_model > control ) { _controls.push_back( std::move( control ) ); }
        // Sections are held by pointer: the controls inside one keep a reference to it, and a
        // vector of sections would move them out from under those references as it grows
        control_section & add_section( std::string title, std::string label, bool searchable_title = true );

        // Drawn deferred, at the panel's right edge, from the position this heading was drawn at
        std::function< void( ImVec2 ) > toggle;
        // Around this section's own controls: on_open sets the item width, on_close writes back
        // what they changed - the ctx.changed it reads covers those controls and nothing else
        std::function< void() > on_open;
        std::function< void( control_draw_context & ) > on_close;
        // A section with something to say beyond its controls - shown when nothing is being
        // searched for, or when the search found this section by its own name
        std::function< void( control_draw_context & ) > content;
        // Air above the heading, as the filter blocks have always had it; the denser lists opt out
        bool gap_above = true;

        std::string const & title() const { return _title; }
        // Nothing to draw: no controls of its own, nothing to say, and no non-empty section under it
        bool empty() const;
        bool matches( std::string const & filter ) const;
        void draw( control_draw_context & );

    private:
        std::string _title;
        std::string _label;
        bool _searchable_title;
        std::vector< std::unique_ptr< control_model > > _controls;
        std::vector< std::unique_ptr< control_section > > _sections;
    };
}
