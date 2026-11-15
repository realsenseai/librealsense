// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <fstream>

#include <chrono>
#include <thread>
#include <sstream>
#include <map>
#include <string>
#include <iomanip>
#include <locale>

#include <rsutils/json.h>
#include <librealsense2/h/rs_advanced_mode_command.h>
#include "types.h"
#include "presets.h"
#include "serialized-utilities.h"

#include <rsutils/string/from.h>


namespace librealsense
{
    using json = rsutils::json;

    // Advanced-mode JSON files always store numbers with a '.' decimal separator,
    // but ::atof() follows the process C locale: under comma-decimal locales
    // (e.g. de-DE, da-DK) it stops at the '.', so "0.482143" loads as 0 and
    // "1.8125" loads as 1 (see issue #15746). Parse with the classic locale
    // instead, so the value survives on any system locale.
    inline double parse_json_float( const std::string & str )
    {
        std::istringstream ss( str );
        ss.imbue( std::locale::classic() );
        double value = 0.0;
        ss >> value;
        return value;
    }

    template<class T>
    struct param_group
    {
        using group_type = T;
        T vals[3];
        bool update = false;
    };

    struct preset_param_group{
        param_group<STDepthControlGroup>            depth_controls;
        param_group<STRsm>                          rsm;
        param_group<STRauSupportVectorControl>      rsvc;
        param_group<STColorControl>                 color_control;
        param_group<STRauColorThresholdsControl>    rctc;
        param_group<STSloColorThresholdsControl>    sctc;
        param_group<STSloPenaltyControl>            spc;
        param_group<STHdad>                         hdad;
        param_group<STColorCorrection>              cc;
        param_group<STDepthTableControl>            depth_table;
        param_group<STAEControl>                    ae;
        param_group<STCensusRadius>                 census;
        param_group<STAFactor>                      a_factor;
        param_group<laser_state_control>            laser_state;
        param_group<laser_power_control>            laser_power;
        param_group<exposure_control>               depth_exposure;
        param_group<auto_exposure_control>          depth_auto_exposure;
        param_group<gain_control>                   depth_gain;
        param_group<auto_white_balance_control>     depth_auto_white_balance;
        param_group<exposure_control>               color_exposure;
        param_group<auto_exposure_control>          color_auto_exposure;
        param_group<backlight_compensation_control> color_backlight_compensation;
        param_group<brightness_control>             color_brightness;
        param_group<contrast_control>               color_contrast;
        param_group<gain_control>                   color_gain;
        param_group<gamma_control>                  color_gamma;
        param_group<hue_control>                    color_hue;
        param_group<saturation_control>             color_saturation;
        param_group<sharpness_control>              color_sharpness;
        param_group<white_balance_control>          color_white_balance;
        param_group<auto_white_balance_control>     color_auto_white_balance;
        param_group<power_line_frequency_control>   color_power_line_frequency;

        preset_param_group(const preset& other)
        {
            copy(other);
        }

        preset_param_group& operator=(const preset& other)
        {
            copy(other);
            return *this;
        }
    private:
        void copy(const preset& other)
        {
            depth_controls.vals[0] = other.depth_controls;
            rsm.vals[0] = other.rsm;
            rsvc.vals[0] = other.rsvc;
            color_control.vals[0] = other.color_control;
            rctc.vals[0] = other.rctc;
            sctc.vals[0] = other.sctc;
            spc.vals[0] = other.spc;
            hdad.vals[0] = other.hdad;
            cc.vals[0] = other.cc;
            depth_table.vals[0] = other.depth_table;
            ae.vals[0] = other.ae;
            census.vals[0] = other.census;
            a_factor.vals[0] = other.amplitude_factor;
            laser_state.vals[0] = other.laser_state;
            laser_power.vals[0] = other.laser_power;
            depth_exposure.vals[0] = other.depth_exposure;
            depth_auto_exposure.vals[0] = other.depth_auto_exposure;
            depth_gain.vals[0] = other.depth_gain;
            depth_auto_white_balance.vals[0] = other.depth_auto_white_balance;
            color_exposure.vals[0] = other.color_exposure;
            color_auto_exposure.vals[0] = other.color_auto_exposure;
            color_backlight_compensation.vals[0] = other.color_backlight_compensation;
            color_brightness.vals[0] = other.color_brightness;
            color_contrast.vals[0] = other.color_contrast;
            color_gain.vals[0] = other.color_gain;
            color_gamma.vals[0] = other.color_gamma;
            color_hue.vals[0] = other.color_hue;
            color_saturation.vals[0] = other.color_saturation;
            color_sharpness.vals[0] = other.color_sharpness;
            color_white_balance.vals[0] = other.color_white_balance;
            color_auto_white_balance.vals[0] = other.color_auto_white_balance;
            color_power_line_frequency.vals[0] = other.color_power_line_frequency;
        }
    };

    struct json_field
    {
        virtual ~json_field() = default;

        bool was_set = false;
        // Duplicated fields will not be in the generated JSON file but will be processed as an input for backward compatibility
        bool is_duplicated = false;

        virtual void load(const std::string& value) = 0;
        virtual std::string save() const = 0;
    };

    template<class T, class S>
    struct json_struct_field : json_field
    {
        T* strct;
        S T::group_type::* field;
        float scale = 1.0f;
        bool check_ranges = true;

        void load(const std::string& str) override
        {
            float value = static_cast<float>(parse_json_float(str));
            if (std::is_integral<S>::value) //in order to convert float to int correctly we add 0.5 and then use the floor function
                value = std::floor(scale * value + 0.5f);
            else
                value = scale * value;
            strct->vals[0].*field = static_cast<S>(value);
            strct->update = true;
        }

        std::string save() const override
        {
            std::stringstream ss;
            // Always write with a '.' decimal separator, whatever locale the
            // hosting application has installed (the load side parses with the
            // classic locale; see parse_json_float above)
            ss.imbue( std::locale::classic() );
            ss << strct->vals[0].*field / scale;
            return ss.str();
        }
    };

    struct json_ignored_field : json_field
    {
        void load(const std::string& str) override
        {}

        std::string save() const override
        {
            return "";
        }
    };

    template<class T, class S>
    struct json_string_struct_field : json_field
    {
        json_string_struct_field(std::map<std::string, float> values)
            : _values(values)
        {}
        T* strct;
        S T::group_type::* field;

        std::map<std::string, float> _values;

        void load(const std::string& value) override
        {
            (strct->vals[0].*field) = static_cast<S>(_values[value]);
            strct->update = true;
        }

        std::string save() const override
        {
            std::stringstream ss;
            auto val = strct->vals[0].*field;
            auto res = std::find_if(std::begin(_values), std::end(_values), [&](const std::pair<std::string, float> &pair)
            {
                return pair.second == val;
            });

            if (res == std::end(_values))
                throw invalid_value_exception( rsutils::string::from() << "Value not found in map! value=" << val );

            ss << res->first;
            return ss.str();
        }
    };

    template<class T, class S>
    struct json_invert_struct_field : json_struct_field<T, S>
    {
        using json_struct_field<T, S>::strct;
        using json_struct_field<T, S>::field;

        void load(const std::string& str) override
        {
            auto value = parse_json_float(str);
            (strct->vals[0].*field) = (value > 0) ? 0 : 1;
            strct->update = true;
        }

        std::string save() const override
        {
            std::stringstream ss;
            ss << ((strct->vals[0].*field > 0.f) ? 0.f : 1.f);
            return ss.str();
        }
    };

    template<class T, class S>
    std::shared_ptr<json_field> make_field(T& strct, S T::group_type::* field, float scale = 1.0f, bool is_duplicated_field = false)
    {
        std::shared_ptr<json_struct_field<T, S>> f(new json_struct_field<T, S>());
        f->field = field;
        f->strct = &strct;
        f->scale = scale;
        f->is_duplicated = is_duplicated_field;
        return f;
    }

    template<class T, class S>
    std::shared_ptr<json_field> make_string_field(T& strct, S T::group_type::* field, const std::map<std::string, float>& values, bool is_duplicated_field = false)
    {
        std::shared_ptr<json_string_struct_field<T, S>> f(new json_string_struct_field<T, S>(values));
        f->field = field;
        f->strct = &strct;
        f->is_duplicated = is_duplicated_field;
        return f;
    }

    std::shared_ptr<json_field> make_ignored_field()
    {
        return std::make_shared<json_ignored_field>();
    }

    template<class T, class S>
    std::shared_ptr<json_field> make_invert_field(T& strct, S T::group_type::* field, bool is_duplicated_field = false)
    {
        std::shared_ptr<json_invert_struct_field<T, S>> f(new json_invert_struct_field<T, S>());
        f->field = field;
        f->strct = &strct;
        f->is_duplicated = is_duplicated_field;
        return f;
    }

    typedef std::map<std::string, std::shared_ptr<json_field>> parsers_map;

    template <class T, typename S>
    void insert_control_to_map(parsers_map& map, bool was_set, const std::string& name,
                               param_group<T>& control, S field)
    {
        if (was_set)
            map.insert({ name, make_field(control, field) });
    }

    template <class T, typename S>
    void insert_string_control_to_map(parsers_map& map, bool was_set, const std::string& name,
                                      param_group<T>& control, S field,
                                      const std::map<std::string, float>& values)
    {
        if (was_set)
            map.insert({ name, make_string_field(control, field, values) });
    }

    template <typename T>
    void update_preset_control(T& preset_control, const param_group<T>& param)
    {
        if (param.update)
            preset_control = param.vals[0];
    }

    template <typename T>
    void update_preset_camera_control(T& camera_control, const param_group<T>& param)
    {
        if (param.update)
        {
            camera_control = param.vals[0];
            camera_control.was_set = true;
        }
    }

    inline parsers_map initialize_field_parsers(preset_param_group& p)
    {
        parsers_map map = {
            // Depth Control
            { "param-leftrightthreshold", make_field(p.depth_controls, &STDepthControlGroup::lrAgreeThreshold) },
            { "param-maxscorethreshb", make_field(p.depth_controls, &STDepthControlGroup::scoreThreshB) },
            { "param-medianthreshold", make_field(p.depth_controls, &STDepthControlGroup::deepSeaMedianThreshold) },
            { "param-minscorethresha", make_field(p.depth_controls, &STDepthControlGroup::scoreThreshA) },
            { "param-texturedifferencethresh", make_field(p.depth_controls, &STDepthControlGroup::textureDifferenceThreshold) },
            { "param-neighborthresh", make_field(p.depth_controls, &STDepthControlGroup::deepSeaNeighborThreshold) },
            { "param-secondpeakdelta", make_field(p.depth_controls, &STDepthControlGroup::deepSeaSecondPeakThreshold) },
            { "param-texturecountthresh", make_field(p.depth_controls, &STDepthControlGroup::textureCountThreshold) },
            { "param-robbinsmonrodecrement", make_field(p.depth_controls, &STDepthControlGroup::minusDecrement) },
            { "param-robbinsmonroincrement", make_field(p.depth_controls, &STDepthControlGroup::plusIncrement) },

            // RSM
            { "param-usersm", make_invert_field(p.rsm, &STRsm::rsmBypass) },
            { "param-rsmdiffthreshold", make_field(p.rsm, &STRsm::diffThresh) },
            { "param-rsmrauslodiffthreshold", make_field(p.rsm, &STRsm::sloRauDiffThresh) },
            { "param-rsmremovethreshold", make_field(p.rsm, &STRsm::removeThresh, 168.f) },

            // RAU Support Vector Control
            { "param-raumine", make_field(p.rsvc, &STRauSupportVectorControl::minEast) },
            { "param-rauminn", make_field(p.rsvc, &STRauSupportVectorControl::minNorth) },
            { "param-rauminnssum", make_field(p.rsvc, &STRauSupportVectorControl::minNSsum) },
            { "param-raumins", make_field(p.rsvc, &STRauSupportVectorControl::minSouth) },
            { "param-rauminw", make_field(p.rsvc, &STRauSupportVectorControl::minWest) },
            { "param-rauminwesum", make_field(p.rsvc, &STRauSupportVectorControl::minWEsum) },
            { "param-regionshrinku", make_field(p.rsvc, &STRauSupportVectorControl::uShrink) },
            { "param-regionshrinkv", make_field(p.rsvc, &STRauSupportVectorControl::vShrink) },

            // Color Controls
            { "param-disableraucolor", make_field(p.color_control, &STColorControl::disableRAUColor) },
            { "param-disablesadcolor", make_field(p.color_control, &STColorControl::disableSADColor) },
            { "param-disablesadnormalize", make_field(p.color_control, &STColorControl::disableSADNormalize) },
            { "param-disablesloleftcolor", make_field(p.color_control, &STColorControl::disableSLOLeftColor) },
            { "param-disableslorightcolor", make_field(p.color_control, &STColorControl::disableSLORightColor) },

            // RAU Color Thresholds Control
            { "param-regioncolorthresholdb", make_field(p.rctc, &STRauColorThresholdsControl::rauDiffThresholdBlue, 1022.f) },
            { "param-regioncolorthresholdg", make_field(p.rctc, &STRauColorThresholdsControl::rauDiffThresholdGreen, 1022.f) },
            { "param-regioncolorthresholdr", make_field(p.rctc, &STRauColorThresholdsControl::rauDiffThresholdRed, 1022.f) },

            // SLO Color Thresholds Control
            { "param-scanlineedgetaub", make_field(p.sctc, &STSloColorThresholdsControl::diffThresholdBlue) },
            { "param-scanlineedgetaug", make_field(p.sctc, &STSloColorThresholdsControl::diffThresholdGreen) },
            { "param-scanlineedgetaur", make_field(p.sctc, &STSloColorThresholdsControl::diffThresholdRed) },

            // SLO Penalty Control
            { "param-scanlinep1", make_field(p.spc, &STSloPenaltyControl::sloK1Penalty) },
            { "param-scanlinep1onediscon", make_field(p.spc, &STSloPenaltyControl::sloK1PenaltyMod1) },
            { "param-scanlinep1twodiscon", make_field(p.spc, &STSloPenaltyControl::sloK1PenaltyMod2) },
            { "param-scanlinep2", make_field(p.spc, &STSloPenaltyControl::sloK2Penalty) },
            { "param-scanlinep2onediscon", make_field(p.spc, &STSloPenaltyControl::sloK2PenaltyMod1) },
            { "param-scanlinep2twodiscon", make_field(p.spc, &STSloPenaltyControl::sloK2PenaltyMod2) },

            // HDAD
            { "param-lambdaad", make_field(p.hdad, &STHdad::lambdaAD) },
            { "param-lambdacensus", make_field(p.hdad, &STHdad::lambdaCensus) },
            { "ignoreSAD", make_field(p.hdad, &STHdad::ignoreSAD) },

            // SLO Penalty Control
            { "param-colorcorrection1", make_field(p.cc, &STColorCorrection::colorCorrection1, 1.f, true) },
            { "param-colorcorrection2", make_field(p.cc, &STColorCorrection::colorCorrection2, 1.f, true) },
            { "param-colorcorrection3", make_field(p.cc, &STColorCorrection::colorCorrection3, 1.f, true) },
            { "param-colorcorrection4", make_field(p.cc, &STColorCorrection::colorCorrection4, 1.f, true) },
            { "param-colorcorrection5", make_field(p.cc, &STColorCorrection::colorCorrection5, 1.f, true) },
            { "param-colorcorrection6", make_field(p.cc, &STColorCorrection::colorCorrection6, 1.f, true) },
            { "param-colorcorrection7", make_field(p.cc, &STColorCorrection::colorCorrection7, 1.f, true) },
            { "param-colorcorrection8", make_field(p.cc, &STColorCorrection::colorCorrection8, 1.f, true) },
            { "param-colorcorrection9", make_field(p.cc, &STColorCorrection::colorCorrection9, 1.f, true) },
            { "param-colorcorrection10", make_field(p.cc, &STColorCorrection::colorCorrection10, 1.f, true) },
            { "param-colorcorrection11", make_field(p.cc, &STColorCorrection::colorCorrection11, 1.f, true) },
            { "param-colorcorrection12", make_field(p.cc, &STColorCorrection::colorCorrection12, 1.f, true) },

            { "aux-param-colorcorrection1", make_field(p.cc, &STColorCorrection::colorCorrection1) },
            { "aux-param-colorcorrection2", make_field(p.cc, &STColorCorrection::colorCorrection2) },
            { "aux-param-colorcorrection3", make_field(p.cc, &STColorCorrection::colorCorrection3) },
            { "aux-param-colorcorrection4", make_field(p.cc, &STColorCorrection::colorCorrection4) },
            { "aux-param-colorcorrection5", make_field(p.cc, &STColorCorrection::colorCorrection5) },
            { "aux-param-colorcorrection6", make_field(p.cc, &STColorCorrection::colorCorrection6) },
            { "aux-param-colorcorrection7", make_field(p.cc, &STColorCorrection::colorCorrection7) },
            { "aux-param-colorcorrection8", make_field(p.cc, &STColorCorrection::colorCorrection8) },
            { "aux-param-colorcorrection9", make_field(p.cc, &STColorCorrection::colorCorrection9) },
            { "aux-param-colorcorrection10", make_field(p.cc, &STColorCorrection::colorCorrection10) },
            { "aux-param-colorcorrection11", make_field(p.cc, &STColorCorrection::colorCorrection11) },
            { "aux-param-colorcorrection12", make_field(p.cc, &STColorCorrection::colorCorrection12) },

            // Depth Table
            { "param-depthunits", make_field(p.depth_table, &STDepthTableControl::depthUnits) },
            { "param-zunits", make_field(p.depth_table, &STDepthTableControl::depthUnits) },
            { "param-depthclampmin", make_field(p.depth_table, &STDepthTableControl::depthClampMin) },
            { "param-depthclampmax", make_field(p.depth_table, &STDepthTableControl::depthClampMax) },
            { "aux-param-depthclampmin", make_field(p.depth_table, &STDepthTableControl::depthClampMin) },
            { "aux-param-depthclampmax", make_field(p.depth_table, &STDepthTableControl::depthClampMax) },
            { "param-disparitymode", make_field(p.depth_table, &STDepthTableControl::disparityMode) },
            { "param-disparityshift", make_field(p.depth_table, &STDepthTableControl::disparityShift) },
            { "aux-param-disparityshift", make_field(p.depth_table, &STDepthTableControl::disparityShift) },

            // Auto-Exposure
            { "param-autoexposure-setpoint", make_field(p.ae, &STAEControl::meanIntensitySetPoint) },
            { "aux-param-autoexposure-setpoint", make_field(p.ae, &STAEControl::meanIntensitySetPoint) },

            // Census
            { "param-censusenablereg-udiameter", make_field(p.census, &STCensusRadius::uDiameter) },
            { "param-censusenablereg-vdiameter", make_field(p.census, &STCensusRadius::vDiameter) },
            { "param-censususize", make_field(p.census, &STCensusRadius::uDiameter) },
            { "param-censusvsize", make_field(p.census, &STCensusRadius::vDiameter) },

            // Depth Linearity
            { "param-amplitude-factor", make_field(p.a_factor, &STAFactor::amplitude) },

            // Ignored fields
            { "param-regionspatialthresholdu", make_ignored_field() },
            { "param-regionspatialthresholdv", make_ignored_field() },
            { "result:", make_ignored_field() },
            { "result", make_ignored_field() },
            { "aux-param-disparitymultiplier",  make_ignored_field() },
            { "stream-depth-format",  make_ignored_field() },
            { "stream-ir-format",  make_ignored_field() },
            { "stream-width",  make_ignored_field() },
            { "stream-height",  make_ignored_field() },
            { "stream-fps",  make_ignored_field() },
        };

        static const std::map<std::string, float> auto_control_values{ { "False", 0.f }, { "True", 1.f } };
        // Controls Group
        // Depth controls
        static const std::map<std::string, float> laser_state_values{ { "off", 0.f }, { "on", 1.f }, { "auto", 2.f } };
        insert_string_control_to_map(map, p.laser_state.vals[0].was_set, "controls-laserstate", p.laser_state, &laser_state_control::laser_state, laser_state_values);
        insert_control_to_map(map, p.laser_power.vals[0].was_set, "controls-laserpower", p.laser_power, &laser_power_control::laser_power);

        insert_control_to_map(map, p.depth_exposure.vals[0].was_set, "controls-autoexposure-manual", p.depth_exposure, &exposure_control::exposure);
        insert_string_control_to_map(map, p.depth_auto_exposure.vals[0].was_set, "controls-autoexposure-auto", p.depth_auto_exposure, &auto_exposure_control::auto_exposure, auto_control_values);

        insert_control_to_map(map, p.depth_gain.vals[0].was_set, "controls-depth-gain", p.depth_gain, &gain_control::gain);
        insert_string_control_to_map(map, p.depth_auto_white_balance.vals[0].was_set, "controls-depth-white-balance-auto", p.depth_auto_white_balance, &auto_white_balance_control::auto_white_balance, auto_control_values);

        // Color controls
        insert_control_to_map(map, p.color_exposure.vals[0].was_set, "controls-color-autoexposure-manual", p.color_exposure, &exposure_control::exposure);
        insert_string_control_to_map(map, p.color_auto_exposure.vals[0].was_set, "controls-color-autoexposure-auto", p.color_auto_exposure, &auto_exposure_control::auto_exposure, auto_control_values);

        insert_control_to_map(map, p.color_backlight_compensation.vals[0].was_set, "controls-color-backlight-compensation", p.color_backlight_compensation, &backlight_compensation_control::backlight_compensation);
        insert_control_to_map(map, p.color_brightness.vals[0].was_set, "controls-color-brightness", p.color_brightness, &brightness_control::brightness);
        insert_control_to_map(map, p.color_contrast.vals[0].was_set, "controls-color-contrast", p.color_contrast, &contrast_control::contrast);
        insert_control_to_map(map, p.color_gain.vals[0].was_set, "controls-color-gain", p.color_gain, &gain_control::gain);
        insert_control_to_map(map, p.color_gamma.vals[0].was_set, "controls-color-gamma", p.color_gamma, &gamma_control::gamma);
        insert_control_to_map(map, p.color_hue.vals[0].was_set, "controls-color-hue", p.color_hue, &hue_control::hue);
        insert_control_to_map(map, p.color_saturation.vals[0].was_set, "controls-color-saturation", p.color_saturation, &saturation_control::saturation);
        insert_control_to_map(map, p.color_sharpness.vals[0].was_set, "controls-color-sharpness", p.color_sharpness, &sharpness_control::sharpness);
        insert_control_to_map(map, p.color_power_line_frequency.vals[0].was_set, "controls-color-power-line-frequency", p.color_power_line_frequency, &power_line_frequency_control::power_line_frequency);

        insert_control_to_map(map, p.color_white_balance.vals[0].was_set, "controls-color-white-balance-manual", p.color_white_balance, &white_balance_control::white_balance);

        insert_string_control_to_map(map, p.color_auto_white_balance.vals[0].was_set, "controls-color-white-balance-auto", p.color_auto_white_balance, &auto_white_balance_control::auto_white_balance, auto_control_values);
        return map;
    }

    std::string control_name_from_id( hdr_preset::control_id id )
    {
        auto it = hdr_preset::control_id_string_map.find( id );
        if( it != hdr_preset::control_id_string_map.end() )
        {
            return it->second;
        }
        throw std::invalid_argument("Error: Incorrect buffer! control ID NOT found! (" + std::to_string(id) + ")");
    }

    hdr_preset::control_id get_control_id_from_string( const std::string & name )
    {
        // Build reverse map on first use
        static const std::unordered_map< std::string, hdr_preset::control_id > string_control_id_map = []()
        {
            std::unordered_map< std::string, hdr_preset::control_id > result;
            for( const auto & pair : hdr_preset::control_id_string_map)
            {
                result[pair.second] = pair.first;
            }
            return result;
        }();

        auto it = string_control_id_map.find( name );
        if( it != string_control_id_map.end() )
        {
            return it->second;
        }
        throw std::invalid_argument("Error: Incorrect control name: \"" + name + "\"");
    }

    // Helper function to append a struct to the buffer
    template<typename T>
    void append_struct(std::vector<uint8_t>& buffer, const T& st) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&st);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }

    // Given header and items, build the byte buffer
    std::vector<uint8_t> serialize_hdr_preset(
        const  hdr_preset::preset_header& header,
        const std::vector<std::pair<hdr_preset::item_header, std::vector<hdr_preset::sub_control>>>& items
    ) {
        std::vector<uint8_t> buffer;
        append_struct(buffer, header);
        for (const auto& entry : items) {
            append_struct(buffer, entry.first);
            for (const auto& ctrl : entry.second) {
                append_struct(buffer, ctrl);
            }
        }
        return buffer;
    }

    // Helper to read a struct of type T from buffer at offset
    template<typename T>
    T read_struct(const std::vector<uint8_t>& buffer, size_t& offset) {
        if (offset + sizeof(T) > buffer.size()) {
            throw std::out_of_range("Buffer too small for struct");
        }
        T obj;
        std::memcpy(&obj, buffer.data() + offset, sizeof(T));
        offset += sizeof(T);
        return obj;
    }

    // Deserialize full hdr_preset from byte buffer
    hdr_preset::hdr_preset parse_hdr_preset(const std::vector<uint8_t>& buffer) {
        size_t offset = 4;  // The command is at the beginning of the buffer
        hdr_preset::hdr_preset _hdr_preset{};
        if (buffer.size() <= offset) {
            return _hdr_preset;
        }

        // Read header
        _hdr_preset.header = read_struct<hdr_preset::preset_header>(buffer, offset);
        if (_hdr_preset.header.header_size != hdr_preset::preset_header::size()) {
            throw std::runtime_error("Unexpected header size");
        }

        // Read items
        for (uint8_t i = 0; i < _hdr_preset.header.num_of_items; ++i) {
            // Read item header
            hdr_preset::item_header ih = read_struct<hdr_preset::item_header>(buffer, offset);
            if (ih.header_size != hdr_preset::item_header::size()) {
                throw std::runtime_error("Unexpected item_header size");
            }

            // Read controls for the item
            std::vector<hdr_preset::sub_control> controls;
            controls.reserve(ih.num_of_controls);
            for (uint8_t c = 0; c < ih.num_of_controls; ++c) {
                hdr_preset::sub_control ctrl = read_struct<hdr_preset::sub_control>(buffer, offset);
                controls.push_back(ctrl);
            }

            _hdr_preset.items.emplace_back(ih, std::move(controls));
        }

        // validate that we read exactly buffer size
        if (offset != buffer.size()) {
            throw std::runtime_error("Buffer contains extra data");
        }

        return _hdr_preset;
    }


    inline std::vector<uint8_t> generate_json(const device_interface& dev, const preset& in_preset)
    {
        preset_param_group p = in_preset;
        auto fields = initialize_field_parsers(p);
        serialized_utilities::json_preset_writer preset_writer;
        
        preset_writer.set_device_info(dev);
        
        for (auto&& f : fields)
        {
            if (f.second->is_duplicated) // Skip duplicated fields
                continue;

            auto str = f.second->save();
            if (!str.empty()) // Ignored fields return empty string
                preset_writer.write_param(f.first.c_str(), str);
        }

        if (!in_preset.auto_hdr.items.empty())
        {
            json hdr_preset;
            auto & auto_hdr = in_preset.auto_hdr;
            hdr_preset["id"]         = std::to_string(auto_hdr.header.id);
            hdr_preset["iterations"] = std::to_string(auto_hdr.header.iterations);

            // build 'items' array of objects
            for (size_t i = 0; i < auto_hdr.items.size(); ++i) {
                const auto& ih = auto_hdr.items[i].first;
                json item;
                item["iterations"] = std::to_string(ih.iterations);
                item["controls"] = json::object();
                // build 'controls' array for the item
                for (int c = 0; c < ih.num_of_controls; ++c) {
                    const hdr_preset::sub_control& ctrl = auto_hdr.items[i].second[c];
                    const std::string control_name = control_name_from_id( hdr_preset::control_id( ctrl.control_id ) );
                    const std::string control_value = std::to_string( ctrl.control_value );
                    
                    item["controls"][control_name] = control_value;
                }

                hdr_preset["items"].push_back(std::move(item));
            }
            preset_writer.write_root_param("hdr-preset", hdr_preset); // in the JSON, the HDR control is stored under "hdr-preset"
        }

        auto str = preset_writer.to_string();
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    inline void update_structs(const device_interface& dev, const std::string& content, preset& in_preset)
    {
        preset_param_group p = in_preset;
        serialized_utilities::json_preset_reader preset_reader(content);
      
        // Allow cross-device compatibility (e.g., loading a D415 json to a D435) which would normally
        // be disabled. We check only the product-line:
        auto dev_info = preset_reader.get_device_info();
        dev_info.name.clear();
        dev_info.fw_version.clear();
        preset_reader.override_device_info(dev_info);

        preset_reader.check_device_info(dev);

        auto parameters = preset_reader.get_params();
        auto fields = initialize_field_parsers(p);

        for (auto it = parameters.begin(); it != parameters.end(); ++it)
        {
            auto key = it.key();
            auto value = it.value();
            auto kvp = fields.find(key);
            if (key == "hdr-preset")  // skip the hdr-preset block
                continue;
            if (kvp != fields.end())
            {
                try
                {
                    if( ! value.is_string() )
                    {
                        float val = value;
                        std::stringstream ss;
                        ss.imbue( std::locale::classic() );
                        ss << val;
                        kvp->second->load(ss.str());
                    }
                    else
                    {
                        kvp->second->load(it.value());
                    }
                    kvp->second->was_set = true;
                }
                catch (...)
                {
                    throw invalid_value_exception( rsutils::string::from() << "Couldn't set \"" << key << "\"" );
                }
            }
            else
            {
                throw invalid_value_exception( rsutils::string::from()
                                               << key << " key is not supported by the connected device!" );
            }
        }

        update_preset_control(in_preset.depth_controls                      , p.depth_controls);
        update_preset_control(in_preset.rsm                                 , p.rsm);
        update_preset_control(in_preset.rsvc                                , p.rsvc);
        update_preset_control(in_preset.color_control                       , p.color_control);
        update_preset_control(in_preset.rctc                                , p.rctc);
        update_preset_control(in_preset.sctc                                , p.sctc);
        update_preset_control(in_preset.spc                                 , p.spc);
        update_preset_control(in_preset.hdad                                , p.hdad);
        update_preset_control(in_preset.cc                                  , p.cc);
        update_preset_control(in_preset.depth_table                         , p.depth_table);
        update_preset_control(in_preset.ae                                  , p.ae);
        update_preset_control(in_preset.census                              , p.census);
        update_preset_control(in_preset.amplitude_factor                    , p.a_factor);
        update_preset_camera_control(in_preset.laser_power                  , p.laser_power);
        update_preset_camera_control(in_preset.laser_state                  , p.laser_state);
        update_preset_camera_control(in_preset.depth_exposure               , p.depth_exposure);
        update_preset_camera_control(in_preset.depth_auto_exposure          , p.depth_auto_exposure);
        update_preset_camera_control(in_preset.depth_gain                   , p.depth_gain);
        update_preset_camera_control(in_preset.depth_auto_white_balance     , p.depth_auto_white_balance);
        update_preset_camera_control(in_preset.color_exposure               , p.color_exposure);
        update_preset_camera_control(in_preset.color_auto_exposure          , p.color_auto_exposure);
        update_preset_camera_control(in_preset.color_backlight_compensation , p.color_backlight_compensation);
        update_preset_camera_control(in_preset.color_brightness             , p.color_brightness);
        update_preset_camera_control(in_preset.color_contrast               , p.color_contrast);
        update_preset_camera_control(in_preset.color_gain                   , p.color_gain);
        update_preset_camera_control(in_preset.color_gamma                  , p.color_gamma);
        update_preset_camera_control(in_preset.color_hue                    , p.color_hue);
        update_preset_camera_control(in_preset.color_saturation             , p.color_saturation);
        update_preset_camera_control(in_preset.color_sharpness              , p.color_sharpness);
        update_preset_camera_control(in_preset.color_white_balance          , p.color_white_balance);
        update_preset_camera_control(in_preset.color_auto_white_balance     , p.color_auto_white_balance);
        update_preset_camera_control(in_preset.color_power_line_frequency   , p.color_power_line_frequency);

        auto hdr_preset_from_json = preset_reader.get_hdr_preset();  // hdr-preset block
        if (!hdr_preset_from_json.is_null())
        {
            hdr_preset::hdr_preset auto_hdr;

            auto_hdr.header.header_size = hdr_preset::preset_header::size();
            auto id = std::stoi(hdr_preset_from_json.at("id").get<std::string>());
            if( id < 0 || id > 0xF )
                throw std::invalid_argument("Error: Incorrect hdr_preset id: \"" + std::to_string(id) + "\"" + ", valid range <0:15>");
            auto_hdr.header.id = static_cast<uint8_t>(id);

            auto iterations = std::stoi(hdr_preset_from_json.at( "iterations" ).get< std::string >() );
            if( iterations < 0 || iterations > 0xFFFF )
                throw std::invalid_argument("Error: Incorrect number of iterations: \"" + std::to_string(iterations) + "\"" + ", valid range <1:ffff>, 0 - for infinite");
            auto_hdr.header.iterations = static_cast<uint16_t>(iterations);

            const auto& items = hdr_preset_from_json.at("items");
            bool ae_controls = false, manual_controls = false;
            for (const auto& item : items) {
                hdr_preset::item_header item_header{};
                std::vector<hdr_preset::sub_control> item_controls;

                item_header.header_size = hdr_preset::item_header::size();
                item_header.iterations = static_cast<uint16_t>(std::stoi(item.at("iterations").get<std::string>()));
                if( item_header.iterations <= 0 || item_header.iterations > 0xFFFF )  // not allowing 0 iterations
                    throw std::invalid_argument("Error: Incorrect number of iterations: \"" + std::to_string(item_header.iterations) + "\"" + ", valid range <1:ffff>");

                const auto& controls = item.at("controls");
                for (const auto& kv : controls.items()) {
                    const std::string key = kv.key();
                    const std::string value = kv.value();

                    hdr_preset::sub_control ctrl{};
                    auto control_id = get_control_id_from_string(key);
                    ctrl.control_id = static_cast<uint8_t>(control_id); // will throw if not found
                    ctrl.control_value = static_cast<uint32_t>(std::stoul(value));
                    item_controls.push_back(ctrl);
                    if (control_id == hdr_preset::DEPTH_AE)
                    {
                        auto_hdr.is_auto = true;
                    }
                    else if( control_id == hdr_preset::DEPTH_AE_EXP || control_id == hdr_preset::DEPTH_AE_GAIN )
                    {
                        ae_controls = true;
                    }
                    else
                    {
                        manual_controls = true;
                    }
                }

                if (manual_controls && ae_controls)
                    throw std::invalid_argument("Error: HDR preset contains both manual and AE items");

                if( ae_controls && ! auto_hdr.is_auto )
                    throw std::invalid_argument( "Error: HDR contains AE items, but no depth-ae item" );

                item_header.num_of_controls = static_cast<uint8_t>(item_controls.size());

                auto_hdr.items.push_back( { item_header, item_controls } );
            }

            auto num_of_items = auto_hdr.items.size();
            auto actual_items = num_of_items - (auto_hdr.is_auto ? 1 : 0);  // we don't count the depth-ae item

            if (actual_items < 2 || actual_items > 6)
                throw std::invalid_argument( "Error: Incorrect number of items: \"" + std::to_string( actual_items ) + "\"" + ", valid range <2:6>" );
            auto_hdr.header.num_of_items = static_cast< uint8_t >( num_of_items );

            in_preset.auto_hdr = auto_hdr;
        }

    }
}
