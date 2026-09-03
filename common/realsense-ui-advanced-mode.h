// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs_advanced_mode.hpp>
#include <type_traits>
#include <rsutils/string/string-utilities.h>
#include <realsense_imgui.h>
#include "control-section.h"
#include <memory>
#include "textual-icons.h"

#define TEXT_BUFF_SIZE 1024

template<class T>
bool* draw_edit_button(const char* id, T val, std::string*& val_str)
{
    static std::map<const char*, bool> edit_mode;
    static std::map<const char*, std::string> edit_value;

    ImGui::SameLine();
    ImGui::SetCursorPosX(268);
    if (!edit_mode[id])
    {
        std::string edit_id = rsutils::string::from() 
            << rs2::textual_icons::edit << "##" << id;
        ImGui::PushStyleColor(ImGuiCol_Text,  { 0.8f, 0.8f, 0.8f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 0.8f, 0.8f, 0.8f, 1.f } );
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1.f,1.f,1.f,0.f });
        ImGui::PushStyleColor(ImGuiCol_Button, { 1.f,1.f,1.f,0.f });
        if (ImGui::Button(edit_id.c_str(), { 20, 20 }))
        {
            edit_value[id] = rsutils::string::from( val );
            edit_mode[id] = true;
        }
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("Enter text-edit mode");
        }
        ImGui::PopStyleColor(4);
    }
    else
    {
        std::string edit_id = rsutils::string::from()   
            << rs2::textual_icons::edit << "##" << id;
        ImGui::PushStyleColor(ImGuiCol_Text,  { 0.8f, 0.8f, 1.f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,  { 0.8f, 0.8f, 1.f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1.f,1.f,1.f,0.f });
        ImGui::PushStyleColor(ImGuiCol_Button, { 1.f,1.f,1.f,0.f });
        if (ImGui::Button(edit_id.c_str(), { 20, 20 }))
        {
            edit_mode[id] = false;
        }
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("Exit text-edit mode");
        }
        ImGui::PopStyleColor(4);
    }

    val_str = &edit_value[id];
    return &edit_mode[id];
}

template<class T, class S>
inline void slider_int(std::string& error_message, const char* id, T* val, S T::* field, bool& to_set)
{
    ImGui::Text("%s", id);
    int temp = val->*field;
    int min = (val + 1)->*field;
    int max = (val + 2)->*field;

    std::string* val_ptr;
    auto edit_mode = draw_edit_button(id, temp, val_ptr);

    std::string slider_id = rsutils::string::from() << "##" << id;

    if (*edit_mode)
    {
        char buff[TEXT_BUFF_SIZE];
        memset(buff, 0, TEXT_BUFF_SIZE);
        strncpy(buff, val_ptr->c_str(), TEXT_BUFF_SIZE - 1);
        if (ImGui::InputText(slider_id.c_str(), buff, TEXT_BUFF_SIZE,
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            int new_value = 0;
            if (!rsutils::string::string_to_value<int>(buff, new_value))
            {
                error_message = "Invalid integer input!";
            }
            else
            {
                if ((new_value > max) || (new_value < min))
                {
                    std::stringstream ss;
                    ss << "New value " << new_value << " must be within [" << min << ", " << max << "] range";
                    error_message = ss.str();
                }
                else
                {
                    val->*field = static_cast<S>(new_value);
                    to_set = true;
                }
            }

            *edit_mode = false;
        }
        *val_ptr = buff;
    }
    else if (ImGui::SliderInt(slider_id.c_str(), &temp, min, max))
    {
        val->*field = temp;
        to_set = true;
    }
}

template<class T, class S>
inline void checkbox(const char* id, T* val, S T::* f, bool& to_set)
{
    bool temp = (val->*f) > 0;

    if (ImGui::Checkbox(id, &temp))
    {
        val->*f = temp ? 1 : 0;
        to_set = true;
    }
}

template<class T, class S>
inline void slider_float(std::string& error_message, const char* id, T* val, S T::* field, bool& to_set)
{
    ImGui::Text("%s", id);
    float temp = float(val->*field);
    float min = float((val + 1)->*field);
    float max = float((val + 2)->*field);

    std::string* val_ptr;
    auto edit_mode = draw_edit_button(id, temp, val_ptr);


    std::string slider_id = rsutils::string::from() << "##" << id;

    if (*edit_mode)
    {
        char buff[TEXT_BUFF_SIZE];
        memset(buff, 0, TEXT_BUFF_SIZE);
        strncpy(buff, val_ptr->c_str(), TEXT_BUFF_SIZE - 1);
        if (ImGui::InputText(slider_id.c_str(), buff, TEXT_BUFF_SIZE,
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            float new_value = 0;
            if (!rsutils::string::string_to_value<float>(buff, new_value))
            {
                error_message = "Invalid numeric input!";
            }
            else
            {
                // min != max added in order to step over this check for controls 
                // for which min and max have been set equal to actual value
                if ((min != max) && ((new_value > max) || (new_value < min)))
                {
                    std::stringstream ss;
                    ss << "New value " << new_value << " must be within [" << min << ", " << max << "] range";
                    error_message = ss.str();
                }
                else
                {
                    val->*field = static_cast<S>(new_value);
                    to_set = true;
                }
            }

            *edit_mode = false;
        }
        *val_ptr = buff;
    }
    else if (ImGui::SliderFloat(slider_id.c_str(), &temp, min, max))
    {
        val->*field = static_cast<S>(temp);
        to_set = true;
    }
}

template<class T>
struct param_group
{
    using group_type = T;
    T vals[3];
    bool update = false;
};

struct advanced_mode_control
{
    param_group<STDepthControlGroup> depth_controls;
    param_group<STRsm> rsm;
    param_group<STRauSupportVectorControl> rsvc;
    param_group<STColorControl> color_control;
    param_group<STRauColorThresholdsControl> rctc;
    param_group<STSloColorThresholdsControl> sctc;
    param_group<STSloPenaltyControl> spc;
    param_group<STHdad> hdad;
    param_group<STColorCorrection> cc;
    param_group<STDepthTableControl> depth_table;
    param_group<STAEControl> ae;
    param_group<STCensusRadius> census;
    param_group<STAFactor> amp_factor;
};

// The advanced-mode values are read from FW in one bulk operation, and only when they went stale
inline void refresh_advanced_mode_controls( rs400::advanced_mode & advanced,
                                            advanced_mode_control & amc,
                                            bool & get_curr_advanced_controls )
{
    if( ! get_curr_advanced_controls )
        return;

    auto all = advanced.get_all_controls();
    for( int k = 0; k < 3; ++k )
    {
        amc.depth_controls.vals[k] = all.depth_control[k];
        amc.rsm.vals[k] = all.rsm[k];
        amc.rsvc.vals[k] = all.rsvc[k];
        amc.color_control.vals[k] = all.color_control[k];
        amc.rctc.vals[k] = all.rctc[k];
        amc.sctc.vals[k] = all.sctc[k];
        amc.spc.vals[k] = all.spc[k];
        amc.cc.vals[k] = all.cc[k];
        amc.depth_table.vals[k] = all.depth_table[k];
        amc.census.vals[k] = all.census[k];
        amc.amp_factor.vals[k] = all.amp_factor[k];
        amc.hdad.vals[k] = all.hdad[k];
        amc.ae.vals[k] = all.ae[k];
    }
    get_curr_advanced_controls = false;
}

// checkbox() takes no error message; give it the signature the others have so one adapter fits all
template< class T, class S >
inline void checkbox_control( std::string &, const char * id, T * val, S T::* f, bool & to_set )
{
    checkbox( id, val, f, to_set );
}

// One advanced-mode control: a named field of an STxxx struct, whose value, minimum and maximum
// are vals[0..2]. The widget templates draw it; this only names it so the search can find it.
template< class T, class S >
class advanced_control : public rs2::control_model
{
public:
    using drawer = void ( * )( std::string &, const char *, T *, S T::*, bool & );

    advanced_control( const char * name, drawer draw_it, param_group< T > & group, S T::* field )
        : _name( name ), _draw( draw_it ), _group( &group ), _field( field )
    {
    }

    std::string const & name() const override { return _name; }
    void draw( rs2::control_draw_context & ctx ) override
    {
        bool to_set = false;
        _draw( ctx.error_message, _name.c_str(), _group->vals, _field, to_set );
        if( to_set )
            ctx.changed = true;
    }

private:
    std::string _name;
    drawer _draw;
    param_group< T > * _group;
    S T::* _field;
};

template< class T, class S >
inline void add_slider_int( rs2::control_section & section,
                            const char * name, param_group< T > & group, S T::* field )
{
    section.add( std::unique_ptr< rs2::control_model >(
        new advanced_control< T, S >( name, &slider_int< T, S >, group, field ) ) );
}

template< class T, class S >
inline void add_slider_float( rs2::control_section & section,
                              const char * name, param_group< T > & group, S T::* field )
{
    section.add( std::unique_ptr< rs2::control_model >(
        new advanced_control< T, S >( name, &slider_float< T, S >, group, field ) ) );
}

template< class T, class S >
inline void add_checkbox( rs2::control_section & section,
                          const char * name, param_group< T > & group, S T::* field )
{
    section.add( std::unique_ptr< rs2::control_model >(
        new advanced_control< T, S >( name, &checkbox_control< T, S >, group, field ) ) );
}

// A section writes its group back to FW once, after the controls inside it have drawn; a rejected
// write puts the value the camera still holds back on screen and surfaces the error
template< class Set, class Revert >
inline rs2::control_section & add_advanced_section( rs2::control_section & parent, const char * title,
                                                    bool & was_set, Set set, Revert revert )
{
    auto & section = parent.add_section( title, title );
    section.gap_above = false;
    section.on_open = []() { ImGui::PushItemWidth( ImGui::CalcItemWidth() ); };
    section.on_close = [&was_set, set, revert]( rs2::control_draw_context & ctx )
    {
        ImGui::PopItemWidth();
        if( ! ctx.changed )
            return;
        try
        {
            set();
        }
        catch( ... )
        {
            revert();
            throw;
        }
        was_set = true;
    };
    return section;
}

// The 13 groups of advanced-mode controls, as sections of named controls. Nothing is drawn here -
// control_section::draw() decides what the current search leaves visible.
inline void build_advanced_mode_sections( rs2::control_section & root,
                                          rs400::advanced_mode & advanced,
                                          advanced_mode_control & amc,
                                          bool & was_set,
                                          bool ae_setpoint_unsupported = false )
{
    {
        auto & s = add_advanced_section( root, "Depth Control", was_set,
            [&]() { advanced.set_depth_control( amc.depth_controls.vals[0] ); },
            [&]() { amc.depth_controls.vals[0] = advanced.get_depth_control( 0 ); } );
        add_slider_int( s, "DS Second Peak Threshold", amc.depth_controls, &STDepthControlGroup::deepSeaSecondPeakThreshold );
        add_slider_int( s, "DS Neighbor Threshold", amc.depth_controls, &STDepthControlGroup::deepSeaNeighborThreshold );
        add_slider_int( s, "DS Median Threshold", amc.depth_controls, &STDepthControlGroup::deepSeaMedianThreshold );
        add_slider_int( s, "Estimate Median Increment", amc.depth_controls, &STDepthControlGroup::plusIncrement );
        add_slider_int( s, "Estimate Median Decrement", amc.depth_controls, &STDepthControlGroup::minusDecrement );
        add_slider_int( s, "Score Minimum Threshold", amc.depth_controls, &STDepthControlGroup::scoreThreshA );
        add_slider_int( s, "Score Maximum Threshold", amc.depth_controls, &STDepthControlGroup::scoreThreshB );
        add_slider_int( s, "DS LR Threshold", amc.depth_controls, &STDepthControlGroup::lrAgreeThreshold );
        add_slider_int( s, "Texture Count Threshold", amc.depth_controls, &STDepthControlGroup::textureCountThreshold );
        add_slider_int( s, "Texture Difference Threshold", amc.depth_controls, &STDepthControlGroup::textureDifferenceThreshold );
    }
    {
        auto & s = add_advanced_section( root, "Rsm", was_set,
            [&]() { advanced.set_rsm( amc.rsm.vals[0] ); },
            [&]() { amc.rsm.vals[0] = advanced.get_rsm( 0 ); } );
        add_checkbox( s, "RSM Bypass", amc.rsm, &STRsm::rsmBypass );
        add_slider_float( s, "Disparity Difference Threshold", amc.rsm, &STRsm::diffThresh );
        add_slider_float( s, "SLO RAU Difference Threshold", amc.rsm, &STRsm::sloRauDiffThresh );
        add_slider_int( s, "Remove Threshold", amc.rsm, &STRsm::removeThresh );
    }
    {
        auto & s = add_advanced_section( root, "Rau Support Vector Control", was_set,
            [&]() { advanced.set_rau_support_vector_control( amc.rsvc.vals[0] ); },
            [&]() { amc.rsvc.vals[0] = advanced.get_rau_support_vector_control( 0 ); } );
        add_slider_int( s, "Min West", amc.rsvc, &STRauSupportVectorControl::minWest );
        add_slider_int( s, "Min East", amc.rsvc, &STRauSupportVectorControl::minEast );
        add_slider_int( s, "Min WE Sum", amc.rsvc, &STRauSupportVectorControl::minWEsum );
        add_slider_int( s, "Min North", amc.rsvc, &STRauSupportVectorControl::minNorth );
        add_slider_int( s, "Min South", amc.rsvc, &STRauSupportVectorControl::minSouth );
        add_slider_int( s, "Min NS Sum", amc.rsvc, &STRauSupportVectorControl::minNSsum );
        add_slider_int( s, "U Shrink", amc.rsvc, &STRauSupportVectorControl::uShrink );
        add_slider_int( s, "V Shrink", amc.rsvc, &STRauSupportVectorControl::vShrink );
    }
    {
        auto & s = add_advanced_section( root, "Color Control", was_set,
            [&]() { advanced.set_color_control( amc.color_control.vals[0] ); },
            [&]() { amc.color_control.vals[0] = advanced.get_color_control( 0 ); } );
        add_checkbox( s, "Disable SAD Color", amc.color_control, &STColorControl::disableSADColor );
        add_checkbox( s, "Disable RAU Color", amc.color_control, &STColorControl::disableRAUColor );
        add_checkbox( s, "Disable SLO Right Color", amc.color_control, &STColorControl::disableSLORightColor );
        add_checkbox( s, "Disable SLO Left Color", amc.color_control, &STColorControl::disableSLOLeftColor );
        add_checkbox( s, "Disable SAD Normalize", amc.color_control, &STColorControl::disableSADNormalize );
    }
    {
        auto & s = add_advanced_section( root, "Rau Color Thresholds Control", was_set,
            [&]() { advanced.set_rau_thresholds_control( amc.rctc.vals[0] ); },
            [&]() { amc.rctc.vals[0] = advanced.get_rau_thresholds_control( 0 ); } );
        add_slider_int( s, "Diff Threshold Red", amc.rctc, &STRauColorThresholdsControl::rauDiffThresholdRed );
        add_slider_int( s, "Diff Threshold Green", amc.rctc, &STRauColorThresholdsControl::rauDiffThresholdGreen );
        add_slider_int( s, "Diff Threshold Blue", amc.rctc, &STRauColorThresholdsControl::rauDiffThresholdBlue );
    }
    {
        auto & s = add_advanced_section( root, "SLO Color Thresholds Control", was_set,
            [&]() { advanced.set_slo_color_thresholds_control( amc.sctc.vals[0] ); },
            [&]() { amc.sctc.vals[0] = advanced.get_slo_color_thresholds_control( 0 ); } );
        add_slider_int( s, "Diff Threshold Red", amc.sctc, &STSloColorThresholdsControl::diffThresholdRed );
        add_slider_int( s, "Diff Threshold Green", amc.sctc, &STSloColorThresholdsControl::diffThresholdGreen );
        add_slider_int( s, "Diff Threshold Blue", amc.sctc, &STSloColorThresholdsControl::diffThresholdBlue );
    }
    {
        auto & s = add_advanced_section( root, "SLO Penalty Control", was_set,
            [&]() { advanced.set_slo_penalty_control( amc.spc.vals[0] ); },
            [&]() { amc.spc.vals[0] = advanced.get_slo_penalty_control( 0 ); } );
        add_slider_int( s, "K1 Penalty", amc.spc, &STSloPenaltyControl::sloK1Penalty );
        add_slider_int( s, "K2 Penalty", amc.spc, &STSloPenaltyControl::sloK2Penalty );
        add_slider_int( s, "K1 Penalty Mod1", amc.spc, &STSloPenaltyControl::sloK1PenaltyMod1 );
        add_slider_int( s, "K1 Penalty Mod2", amc.spc, &STSloPenaltyControl::sloK1PenaltyMod2 );
        add_slider_int( s, "K2 Penalty Mod1", amc.spc, &STSloPenaltyControl::sloK2PenaltyMod1 );
        add_slider_int( s, "K2 Penalty Mod2", amc.spc, &STSloPenaltyControl::sloK2PenaltyMod2 );
    }
    {
        auto & s = add_advanced_section( root, "HDAD", was_set,
            [&]() { advanced.set_hdad( amc.hdad.vals[0] ); },
            [&]() { amc.hdad.vals[0] = advanced.get_hdad(); } );
        add_checkbox( s, "Ignore SAD", amc.hdad, &STHdad::ignoreSAD );
        add_slider_float( s, "AD Lambda", amc.hdad, &STHdad::lambdaAD );
        add_slider_float( s, "Census Lambda", amc.hdad, &STHdad::lambdaCensus );
    }
    {
        auto & s = add_advanced_section( root, "Color Correction", was_set,
            [&]() { advanced.set_color_correction( amc.cc.vals[0] ); },
            [&]() { amc.cc.vals[0] = advanced.get_color_correction( 0 ); } );
        add_slider_float( s, "Color Correction 1", amc.cc, &STColorCorrection::colorCorrection1 );
        add_slider_float( s, "Color Correction 2", amc.cc, &STColorCorrection::colorCorrection2 );
        add_slider_float( s, "Color Correction 3", amc.cc, &STColorCorrection::colorCorrection3 );
        add_slider_float( s, "Color Correction 4", amc.cc, &STColorCorrection::colorCorrection4 );
        add_slider_float( s, "Color Correction 5", amc.cc, &STColorCorrection::colorCorrection5 );
        add_slider_float( s, "Color Correction 6", amc.cc, &STColorCorrection::colorCorrection6 );
        add_slider_float( s, "Color Correction 7", amc.cc, &STColorCorrection::colorCorrection7 );
        add_slider_float( s, "Color Correction 8", amc.cc, &STColorCorrection::colorCorrection8 );
        add_slider_float( s, "Color Correction 9", amc.cc, &STColorCorrection::colorCorrection9 );
        add_slider_float( s, "Color Correction 10", amc.cc, &STColorCorrection::colorCorrection10 );
        add_slider_float( s, "Color Correction 11", amc.cc, &STColorCorrection::colorCorrection11 );
        add_slider_float( s, "Color Correction 12", amc.cc, &STColorCorrection::colorCorrection12 );
    }
    {
        auto & s = add_advanced_section( root, "Depth Table", was_set,
            [&]() { advanced.set_depth_table( amc.depth_table.vals[0] ); },
            [&]() { amc.depth_table.vals[0] = advanced.get_depth_table( 0 ); } );
        add_slider_float( s, "Depth Units", amc.depth_table, &STDepthTableControl::depthUnits );
        add_slider_float( s, "Depth Clamp Min", amc.depth_table, &STDepthTableControl::depthClampMin );
        add_slider_float( s, "Depth Clamp Max", amc.depth_table, &STDepthTableControl::depthClampMax );
        add_slider_float( s, "Disparity Mode", amc.depth_table, &STDepthTableControl::disparityMode );
        add_slider_float( s, "Disparity Shift", amc.depth_table, &STDepthTableControl::disparityShift );
    }
    if( ! ae_setpoint_unsupported )   // D457 and the D500 family have no AE set point
    {
        auto & s = add_advanced_section( root, "AE Control", was_set,
            [&]() { advanced.set_ae_control( amc.ae.vals[0] ); },
            [&]() { amc.ae.vals[0] = advanced.get_ae_control(); } );
        add_slider_float( s, "Mean Intensity Set Point", amc.ae, &STAEControl::meanIntensitySetPoint );
    }
    {
        auto & s = add_advanced_section( root, "Census Enable Reg", was_set,
            [&]() { advanced.set_census( amc.census.vals[0] ); },
            [&]() { amc.census.vals[0] = advanced.get_census( 0 ); } );
        add_slider_float( s, "u-Diameter", amc.census, &STCensusRadius::uDiameter );
        add_slider_float( s, "v-Diameter", amc.census, &STCensusRadius::vDiameter );
    }
    {
        auto & s = add_advanced_section( root, "Disparity Modulation", was_set,
            [&]() { advanced.set_amp_factor( amc.amp_factor.vals[0] ); },
            [&]() { amc.amp_factor.vals[0] = advanced.get_amp_factor( 0 ); } );
        add_slider_float( s, "A Factor", amc.amp_factor, &STAFactor::amplitude );
    }
}
