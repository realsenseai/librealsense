// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "control-section.h"

#include <realsense_imgui.h>


namespace rs2
{
    control_section & control_section::add_section( std::string title, std::string label, bool searchable_title )
    {
        _sections.push_back( std::unique_ptr< control_section >(
            new control_section( std::move( title ), std::move( label ), searchable_title ) ) );
        return *_sections.back();
    }

    bool control_section::empty() const
    {
        // A toggle is a control in its own right: a filter with nothing but its on/off switch is
        // still something the user came here to click
        if( content || toggle )
            return false;
        for( auto const & control : _controls )
            if( control->drawable() )
                return false;
        for( auto const & section : _sections )
            if( ! section->empty() )
                return false;
        return true;
    }

    bool control_section::matches( std::string const & filter ) const
    {
        if( empty() )
            return false;
        if( filter.empty() )
            return true;
        if( _searchable_title && contains_ci( _title, filter ) )
            return true;   // a matched heading shows everything under it
        for( auto const & control : _controls )
            if( control->drawable() && contains_ci( control->name(), filter ) )
                return true;
        for( auto const & section : _sections )
            if( section->matches( filter ) )
                return true;
        return false;
    }

    void control_section::draw( control_draw_context & ctx )
    {
        if( ! matches( ctx.filter ) )
            return;

        if( gap_above )
            ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 5 );
        if( toggle )
        {
            ImVec2 const pos = ImGui::GetCursorPos();
            auto draw_toggle = toggle;
            ctx.draw_later.push_back( [draw_toggle, pos]() { draw_toggle( pos ); } );
        }

        // Shown open while searching, so the matches inside need no click; the open state ImGui
        // remembers is put back, so clearing the search returns the tree to how it was left
        if( ! RsImGui::TreeNode( _label.c_str(), ! ctx.filter.empty() ) )
            return;

        // A failed write to the camera propagates out of on_close; the tree still has to close
        struct tree_closer { ~tree_closer() { ImGui::TreePop(); } } close_tree;

        // A matched heading shows everything under it, so its name is as searchable as the names
        // of the controls inside it
        bool const show_all = ctx.filter.empty()
                           || ( _searchable_title && contains_ci( _title, ctx.filter ) );

        bool const changed_before = ctx.changed;
        ctx.changed = false;

        if( on_open )
            on_open();
        for( auto & control : _controls )
            if( control->drawable() && ( show_all || contains_ci( control->name(), ctx.filter ) ) )
                control->draw( ctx );
        if( on_close )
            on_close( ctx );

        ctx.changed = ctx.changed || changed_before;

        if( content && show_all )
            content( ctx );

        // Everything under a heading the search matched, nested groups included - otherwise a
        // heading found by its own name would stand there open and empty
        std::string const filter = ctx.filter;
        if( show_all )
            ctx.filter.clear();
        for( auto & section : _sections )
            section->draw( ctx );
        ctx.filter = filter;
    }
}
