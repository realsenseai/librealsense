// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "control-section.h"

#include <realsense_imgui.h>

#include <algorithm>
#include <cctype>


namespace rs2
{
    namespace
    {
        // Case-insensitive substring test. The needle is already lowercase; the haystack is
        // compared a character at a time rather than lowercased into a copy.
        bool contains_ci( std::string const & haystack, std::string const & lowercase_needle )
        {
            if( lowercase_needle.empty() )
                return true;
            return std::search( haystack.begin(), haystack.end(),
                                lowercase_needle.begin(), lowercase_needle.end(),
                                []( char h, char n )
                                { return (char)std::tolower( (unsigned char)h ) == n; } )
                != haystack.end();
        }
    }

    control_section & control_section::add_section( std::string title, std::string label )
    {
        _sections.push_back( std::make_unique< control_section >( std::move( title ), std::move( label ) ) );
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

        // on_close writes to the camera and can throw; ctx must be whole either way
        struct changed_restore
        {
            control_draw_context & ctx;
            bool before;
            ~changed_restore() { ctx.changed = ctx.changed || before; }
        } restore_changed{ ctx, ctx.changed };
        ctx.changed = false;

        if( on_open )
            on_open();
        for( auto & control : _controls )
            if( control->drawable() && ( show_all || contains_ci( control->name(), ctx.filter ) ) )
                control->draw( ctx );
        if( on_close )
            on_close( ctx );

        if( content && show_all )
            content( ctx );

        // Everything under a heading the search matched, nested groups included - otherwise a
        // heading found by its own name would stand there open and empty
        struct filter_restore
        {
            control_draw_context & ctx;
            std::string saved;
            ~filter_restore() { ctx.filter = saved; }
        } restore_filter{ ctx, ctx.filter };
        if( show_all )
            ctx.filter.clear();
        for( auto & section : _sections )
            section->draw( ctx );
    }
}
