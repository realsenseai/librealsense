// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs_advanced_mode.hpp>
#include <type_traits>
#include <rsutils/string/string-utilities.h>
#include <realsense_imgui.h>
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

inline void draw_advanced_mode_controls(rs400::advanced_mode& advanced,
    advanced_mode_control& amc, bool& get_curr_advanced_controls, bool& was_set, std::string& error_message, bool ae_setpoint_unsupported=false)
{
    if (get_curr_advanced_controls)
    {
        // Get Current Algo Control Values
        auto all = advanced.get_all_controls();
        for (int k = 0; k < 3; ++k)
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

    if (ImGui::TreeNode("Depth Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_int(error_message, "DS Second Peak Threshold", amc.depth_controls.vals, &STDepthControlGroup::deepSeaSecondPeakThreshold, to_set);
        slider_int(error_message, "DS Neighbor Threshold", amc.depth_controls.vals, &STDepthControlGroup::deepSeaNeighborThreshold, to_set);
        slider_int(error_message, "DS Median Threshold", amc.depth_controls.vals, &STDepthControlGroup::deepSeaMedianThreshold, to_set);
        slider_int(error_message, "Estimate Median Increment", amc.depth_controls.vals, &STDepthControlGroup::plusIncrement, to_set);
        slider_int(error_message, "Estimate Median Decrement", amc.depth_controls.vals, &STDepthControlGroup::minusDecrement, to_set);
        slider_int(error_message, "Score Minimum Threshold", amc.depth_controls.vals, &STDepthControlGroup::scoreThreshA, to_set);
        slider_int(error_message, "Score Maximum Threshold", amc.depth_controls.vals, &STDepthControlGroup::scoreThreshB, to_set);
        slider_int(error_message, "DS LR Threshold", amc.depth_controls.vals, &STDepthControlGroup::lrAgreeThreshold, to_set);
        slider_int(error_message, "Texture Count Threshold", amc.depth_controls.vals, &STDepthControlGroup::textureCountThreshold, to_set);
        slider_int(error_message, "Texture Difference Threshold", amc.depth_controls.vals, &STDepthControlGroup::textureDifferenceThreshold, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_depth_control(amc.depth_controls.vals[0]);
            }
            catch (...)
            {
                amc.depth_controls.vals[0] = advanced.get_depth_control( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Rsm"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        checkbox("RSM Bypass", amc.rsm.vals, &STRsm::rsmBypass, to_set);
        slider_float(error_message, "Disparity Difference Threshold", amc.rsm.vals, &STRsm::diffThresh, to_set);
        slider_float(error_message, "SLO RAU Difference Threshold", amc.rsm.vals, &STRsm::sloRauDiffThresh, to_set);
        slider_int(error_message, "Remove Threshold", amc.rsm.vals, &STRsm::removeThresh, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_rsm(amc.rsm.vals[0]);
            }
            catch (...)
            {
                amc.rsm.vals[0] = advanced.get_rsm( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }


    if (ImGui::TreeNode("Rau Support Vector Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_int(error_message, "Min West", amc.rsvc.vals, &STRauSupportVectorControl::minWest, to_set);
        slider_int(error_message, "Min East", amc.rsvc.vals, &STRauSupportVectorControl::minEast, to_set);
        slider_int(error_message, "Min WE Sum", amc.rsvc.vals, &STRauSupportVectorControl::minWEsum, to_set);
        slider_int(error_message, "Min North", amc.rsvc.vals, &STRauSupportVectorControl::minNorth, to_set);
        slider_int(error_message, "Min South", amc.rsvc.vals, &STRauSupportVectorControl::minSouth, to_set);
        slider_int(error_message, "Min NS Sum", amc.rsvc.vals, &STRauSupportVectorControl::minNSsum, to_set);
        slider_int(error_message, "U Shrink", amc.rsvc.vals, &STRauSupportVectorControl::uShrink, to_set);
        slider_int(error_message, "V Shrink", amc.rsvc.vals, &STRauSupportVectorControl::vShrink, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_rau_support_vector_control(amc.rsvc.vals[0]);
            }
            catch (...)
            {
                amc.rsvc.vals[0] = advanced.get_rau_support_vector_control( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Color Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        checkbox("Disable SAD Color", amc.color_control.vals, &STColorControl::disableSADColor, to_set);
        checkbox("Disable RAU Color", amc.color_control.vals, &STColorControl::disableRAUColor, to_set);
        checkbox("Disable SLO Right Color", amc.color_control.vals, &STColorControl::disableSLORightColor, to_set);
        checkbox("Disable SLO Left Color", amc.color_control.vals, &STColorControl::disableSLOLeftColor, to_set);
        checkbox("Disable SAD Normalize", amc.color_control.vals, &STColorControl::disableSADNormalize, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_color_control(amc.color_control.vals[0]);
            }
            catch (...)
            {
                amc.color_control.vals[0] = advanced.get_color_control( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Rau Color Thresholds Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_int(error_message, "Diff Threshold Red", amc.rctc.vals, &STRauColorThresholdsControl::rauDiffThresholdRed, to_set);
        slider_int(error_message, "Diff Threshold Green", amc.rctc.vals, &STRauColorThresholdsControl::rauDiffThresholdGreen, to_set);
        slider_int(error_message, "Diff Threshold Blue", amc.rctc.vals, &STRauColorThresholdsControl::rauDiffThresholdBlue, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_rau_thresholds_control(amc.rctc.vals[0]);
            }
            catch (...)
            {
                amc.rctc.vals[0] = advanced.get_rau_thresholds_control( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("SLO Color Thresholds Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_int(error_message, "Diff Threshold Red", amc.sctc.vals, &STSloColorThresholdsControl::diffThresholdRed, to_set);
        slider_int(error_message, "Diff Threshold Green", amc.sctc.vals, &STSloColorThresholdsControl::diffThresholdGreen, to_set);
        slider_int(error_message, "Diff Threshold Blue", amc.sctc.vals, &STSloColorThresholdsControl::diffThresholdBlue, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_slo_color_thresholds_control(amc.sctc.vals[0]);
            }
            catch (...)
            {
                amc.sctc.vals[0] = advanced.get_slo_color_thresholds_control( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("SLO Penalty Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_int(error_message, "K1 Penalty", amc.spc.vals, &STSloPenaltyControl::sloK1Penalty, to_set);
        slider_int(error_message, "K2 Penalty", amc.spc.vals, &STSloPenaltyControl::sloK2Penalty, to_set);
        slider_int(error_message, "K1 Penalty Mod1", amc.spc.vals, &STSloPenaltyControl::sloK1PenaltyMod1, to_set);
        slider_int(error_message, "K1 Penalty Mod2", amc.spc.vals, &STSloPenaltyControl::sloK1PenaltyMod2, to_set);
        slider_int(error_message, "K2 Penalty Mod1", amc.spc.vals, &STSloPenaltyControl::sloK2PenaltyMod1, to_set);
        slider_int(error_message, "K2 Penalty Mod2", amc.spc.vals, &STSloPenaltyControl::sloK2PenaltyMod2, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_slo_penalty_control(amc.spc.vals[0]);
            }
            catch (...)
            {
                amc.spc.vals[0] = advanced.get_slo_penalty_control( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("HDAD"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        checkbox("Ignore SAD", amc.hdad.vals, &STHdad::ignoreSAD, to_set);

        // TODO: Not clear from documents what is the valid range:
        slider_float(error_message, "AD Lambda", amc.hdad.vals, &STHdad::lambdaAD, to_set);
        slider_float(error_message, "Census Lambda", amc.hdad.vals, &STHdad::lambdaCensus, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_hdad(amc.hdad.vals[0]);
            }
            catch (...)
            {
                amc.hdad.vals[0] = advanced.get_hdad();
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Color Correction"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_float(error_message, "Color Correction 1", amc.cc.vals, &STColorCorrection::colorCorrection1,  to_set);
        slider_float(error_message, "Color Correction 2", amc.cc.vals, &STColorCorrection::colorCorrection2,  to_set);
        slider_float(error_message, "Color Correction 3", amc.cc.vals, &STColorCorrection::colorCorrection3,  to_set);
        slider_float(error_message, "Color Correction 4", amc.cc.vals, &STColorCorrection::colorCorrection4,  to_set);
        slider_float(error_message, "Color Correction 5", amc.cc.vals, &STColorCorrection::colorCorrection5,  to_set);
        slider_float(error_message, "Color Correction 6", amc.cc.vals, &STColorCorrection::colorCorrection6,  to_set);
        slider_float(error_message, "Color Correction 7", amc.cc.vals, &STColorCorrection::colorCorrection7,  to_set);
        slider_float(error_message, "Color Correction 8", amc.cc.vals, &STColorCorrection::colorCorrection8,  to_set);
        slider_float(error_message, "Color Correction 9", amc.cc.vals, &STColorCorrection::colorCorrection9,  to_set);
        slider_float(error_message, "Color Correction 10",amc.cc.vals, &STColorCorrection::colorCorrection10, to_set);
        slider_float(error_message, "Color Correction 11",amc.cc.vals, &STColorCorrection::colorCorrection11, to_set);
        slider_float(error_message, "Color Correction 12",amc.cc.vals, &STColorCorrection::colorCorrection12, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_color_correction(amc.cc.vals[0]);
            }
            catch (...)
            {
                amc.cc.vals[0] = advanced.get_color_correction( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Depth Table"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_float(error_message, "Depth Units", amc.depth_table.vals, &STDepthTableControl::depthUnits, to_set);
        slider_float(error_message, "Depth Clamp Min", amc.depth_table.vals, &STDepthTableControl::depthClampMin, to_set);
        slider_float(error_message, "Depth Clamp Max", amc.depth_table.vals, &STDepthTableControl::depthClampMax, to_set);
        slider_float(error_message, "Disparity Mode", amc.depth_table.vals, &STDepthTableControl::disparityMode, to_set);
        slider_float(error_message, "Disparity Shift", amc.depth_table.vals, &STDepthTableControl::disparityShift, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_depth_table(amc.depth_table.vals[0]);
            }
            catch (...)
            {
                amc.depth_table.vals[0] = advanced.get_depth_table( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    //AE setpoint is not supported on D457 and D500-family devices
    if (!ae_setpoint_unsupported && ImGui::TreeNode("AE Control"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_float(error_message, "Mean Intensity Set Point", amc.ae.vals, &STAEControl::meanIntensitySetPoint, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_ae_control(amc.ae.vals[0]);
            }
            catch (...)
            {
                amc.ae.vals[0] = advanced.get_ae_control();
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Census Enable Reg"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_float(error_message, "u-Diameter", amc.census.vals, &STCensusRadius::uDiameter, to_set);
        slider_float(error_message, "v-Diameter", amc.census.vals, &STCensusRadius::vDiameter, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_census(amc.census.vals[0]);
            }
            catch (...)
            {
                amc.census.vals[0] = advanced.get_census( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Disparity Modulation"))
    {
        ImGui::PushItemWidth(ImGui::CalcItemWidth());

        auto to_set = false;

        slider_float(error_message, "A Factor", amc.amp_factor.vals, &STAFactor::amplitude, to_set);

        ImGui::PopItemWidth();

        if (to_set)
        {
            try
            {
                advanced.set_amp_factor(amc.amp_factor.vals[0]);
            }
            catch (...)
            {
                amc.amp_factor.vals[0] = advanced.get_amp_factor( 0 );
                ImGui::TreePop();
                throw;
            }
            was_set = true;
        }

        ImGui::TreePop();
    }
}
