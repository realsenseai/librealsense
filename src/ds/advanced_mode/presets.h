// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include "../../../include/librealsense2/h/rs_advanced_mode_command.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstddef>

namespace librealsense
{
    struct laser_power_control
    {
        float laser_power;
        bool was_set = false;
    };

    struct laser_state_control
    {
        int laser_state;
        bool was_set = false;
    };

    struct exposure_control
    {
        float exposure;
        bool was_set = false;
    };

    struct auto_exposure_control
    {
        int auto_exposure;
        bool was_set = false;
    };

    struct gain_control
    {
        float gain;
        bool was_set = false;
    };

    struct backlight_compensation_control
    {
        int backlight_compensation;
        bool was_set = false;
    };

    struct brightness_control
    {
        float brightness;
        bool was_set = false;
    };

    struct contrast_control
    {
        float contrast;
        bool was_set = false;
    };

    struct gamma_control
    {
        float gamma;
        bool was_set = false;
    };

    struct hue_control
    {
        float hue;
        bool was_set = false;
    };

    struct saturation_control
    {
        float saturation;
        bool was_set = false;
    };

    struct sharpness_control
    {
        float sharpness;
        bool was_set = false;
    };

    struct white_balance_control
    {
        float white_balance;
        bool was_set = false;
    };

    struct auto_white_balance_control
    {
        int auto_white_balance;
        bool was_set = false;
    };

    struct power_line_frequency_control
    {
        int power_line_frequency;
        bool was_set = false;
    };

    namespace hdr_preset
    {
#pragma pack(push, 1)
        // header for the entire HDR control structure
        struct preset_header {
            uint8_t header_size;
            uint8_t id;
            uint16_t iterations;
            uint8_t num_of_items;
            static constexpr uint8_t size() { return sizeof(preset_header); }
        };

        // header for each item
        struct item_header {
            uint8_t header_size;
            uint16_t iterations;
            uint8_t num_of_controls;
            static constexpr uint8_t size() { return sizeof(item_header); }
        };

        // actual control data for each item
        struct sub_control {
            uint8_t control_id;
            uint32_t control_value;
            static constexpr uint8_t size() { return sizeof(sub_control); }
        };

#pragma pack(pop)
        // HDR control structure that contains multiple items, each with its own controls
        struct hdr_preset
        {
            preset_header header;
            std::vector<std::pair<item_header, std::vector<sub_control>>> items;
            bool is_auto = false; // flag for us to enable auto exposure if needed
            size_t size() const
            {
                size_t size = header.size();
                for (const auto& item : items)
                {
                    size += item.first.size();
                    size += item.second.size() * sub_control::size();
                }
                return size;
            }
        };

        typedef enum control_id
        {
            DEPTH_LASER_MODE   = 0,
            DEPTH_EXPOSURE     = 1,
            DEPTH_GAIN         = 2,
            DEPTH_AE_EXP       = 3,
            DEPTH_AE_GAIN      = 4,
            DEPTH_AE           = 5,
            MAX_NUM_OF_CONTROLS_ID,
        } control_id;

        static const std::unordered_map< control_id, std::string > control_id_string_map = {
            { DEPTH_LASER_MODE,     "depth-laser-mode"    },    // for future use
            { DEPTH_EXPOSURE,       "depth-exposure"      },
            { DEPTH_GAIN,           "depth-gain"          },
            { DEPTH_AE_EXP,         "depth-ae-exp"        },
            { DEPTH_AE_GAIN,        "depth-ae-gain"       },
            { DEPTH_AE,             "depth-ae"            },
        };
    }

    struct preset
    {
        STDepthControlGroup            depth_controls;
        STRsm                          rsm;
        STRauSupportVectorControl      rsvc;
        STColorControl                 color_control;
        STRauColorThresholdsControl    rctc;
        STSloColorThresholdsControl    sctc;
        STSloPenaltyControl            spc;
        STHdad                         hdad;
        STColorCorrection              cc;
        STDepthTableControl            depth_table;
        STAEControl                    ae;
        STCensusRadius                 census;
        STAFactor                      amplitude_factor;
        laser_state_control            laser_state;
        laser_power_control            laser_power;
        exposure_control               depth_exposure;
        auto_exposure_control          depth_auto_exposure;
        gain_control                   depth_gain;
        auto_white_balance_control     depth_auto_white_balance;
        exposure_control               color_exposure;
        auto_exposure_control          color_auto_exposure;
        backlight_compensation_control color_backlight_compensation;
        brightness_control             color_brightness;
        contrast_control               color_contrast;
        gain_control                   color_gain;
        gamma_control                  color_gamma;
        hue_control                    color_hue;
        saturation_control             color_saturation;
        sharpness_control              color_sharpness;
        white_balance_control          color_white_balance;
        auto_white_balance_control     color_auto_white_balance;
        power_line_frequency_control   color_power_line_frequency;
        hdr_preset::hdr_preset         auto_hdr;
    };

    void default_400( preset & p );
    void default_405u( preset & p );
    void default_405( preset & p );
    void default_410( preset & p );
    void default_420( preset & p );
    void default_430( preset & p );
    void default_436( preset & p );
    void default_450_high_res( preset & p );
    void default_450_mid_low_res( preset & p );
    void default_585S( preset & p );
    void high_accuracy( preset & p );
    void high_density( preset & p );
    void mid_density( preset & p );
    void hand_gesture( preset & p );
    void d415_remove_ir( preset & p );
    void d460_remove_ir( preset & p );
    void edge_enhancement( preset & p );
}  // namespace librealsense
