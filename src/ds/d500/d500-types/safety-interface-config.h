// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "common.h"

namespace librealsense
{

#pragma pack(push, 1)

    class pin
    {
    public:
        enum class Direction : uint8_t
        {
            In = 0,
            Out = 1
        };

        enum class Functionality : uint8_t
        {
            pGND = 0,
            p24VDC = 1,
            pOSSD1_A = 2,
            pOSSD1_B = 3,
            pOSSD2_A = 4,
            pOSSD2_B = 5,
            pOSSD2_A_Feedback = 6,
            pOSSD2_B_Feedback = 7,
            pPresetSelect1_A = 8,
            pPresetSelect1_B = 9,
            pPresetSelect2_A = 10,
            pPresetSelect2_B = 11,
            pPresetSelect3_A = 12,
            pPresetSelect3_B = 13,
            pPresetSelect4_A = 14,
            pPresetSelect4_B = 15,
            pPresetSelect5_A = 16,
            pPresetSelect5_B = 17,
            pPresetSelect6_A = 18,
            pPresetSelect6_B = 19,
            pDeviceReady = 20,
            pError = 21,
            pReset = 22,
            pRestartInterlock = 23
        };

        pin() = default;

        pin(const json &j)
        {
            validate_json(j);
            m_direction = string_to_direction(j.at("direction"));
            m_functionality = string_to_functionality(j.at("functionality"));
        }

        json to_json() const
        {
            return {
                {"direction", direction_to_string(m_direction)},
                {"functionality", functionality_to_string(m_functionality)}};
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid pin format");
            }
            for (const auto &field : {"direction", "functionality"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid pin format: missing field: ") + field);
                }
            }
        }

        Direction string_to_direction(const std::string &dir) const
        {
            if (dir == "In")
                return Direction::In;
            else if (dir == "Out")
                return Direction::Out;
            else
                throw librealsense::invalid_value_exception("Invalid direction: " + dir);
        }

        std::string direction_to_string(Direction dir) const
        {
            switch (dir)
            {
            case Direction::In:
                return "In";
            case Direction::Out:
                return "Out";
            default:
                throw librealsense::invalid_value_exception("Invalid direction value");
            }
        }

        Functionality string_to_functionality(const std::string &func) const
        {
            if (func == "pGND")
                return Functionality::pGND;
            if (func == "p24VDC")
                return Functionality::p24VDC;
            if (func == "pOSSD1_A")
                return Functionality::pOSSD1_A;
            if (func == "pOSSD1_B")
                return Functionality::pOSSD1_B;
            if (func == "pOSSD2_A")
                return Functionality::pOSSD2_A;
            if (func == "pOSSD2_B")
                return Functionality::pOSSD2_B;
            if (func == "pOSSD2_A_Feedback")
                return Functionality::pOSSD2_A_Feedback;
            if (func == "pOSSD2_B_Feedback")
                return Functionality::pOSSD2_B_Feedback;
            if (func == "pPresetSelect1_A")
                return Functionality::pPresetSelect1_A;
            if (func == "pPresetSelect1_B")
                return Functionality::pPresetSelect1_B;
            if (func == "pPresetSelect2_A")
                return Functionality::pPresetSelect2_A;
            if (func == "pPresetSelect2_B")
                return Functionality::pPresetSelect2_B;
            if (func == "pPresetSelect3_A")
                return Functionality::pPresetSelect3_A;
            if (func == "pPresetSelect3_B")
                return Functionality::pPresetSelect3_B;
            if (func == "pPresetSelect4_A")
                return Functionality::pPresetSelect4_A;
            if (func == "pPresetSelect4_B")
                return Functionality::pPresetSelect4_B;
            if (func == "pPresetSelect5_A")
                return Functionality::pPresetSelect5_A;
            if (func == "pPresetSelect5_B")
                return Functionality::pPresetSelect5_B;
            if (func == "pPresetSelect6_A")
                return Functionality::pPresetSelect6_A;
            if (func == "pPresetSelect6_B")
                return Functionality::pPresetSelect6_B;
            if (func == "pDeviceReady")
                return Functionality::pDeviceReady;
            if (func == "pError")
                return Functionality::pError;
            if (func == "pReset")
                return Functionality::pReset;
            if (func == "pRestartInterlock")
                return Functionality::pRestartInterlock;
            throw librealsense::invalid_value_exception("Invalid functionality: " + func);
        }

        std::string functionality_to_string(Functionality func) const
        {
            switch (func)
            {
            case Functionality::pGND:
                return "pGND";
            case Functionality::p24VDC:
                return "p24VDC";
            case Functionality::pOSSD1_A:
                return "pOSSD1_A";
            case Functionality::pOSSD1_B:
                return "pOSSD1_B";
            case Functionality::pOSSD2_A:
                return "pOSSD2_A";
            case Functionality::pOSSD2_B:
                return "pOSSD2_B";
            case Functionality::pOSSD2_A_Feedback:
                return "pOSSD2_A_Feedback";
            case Functionality::pOSSD2_B_Feedback:
                return "pOSSD2_B_Feedback";
            case Functionality::pPresetSelect1_A:
                return "pPresetSelect1_A";
            case Functionality::pPresetSelect1_B:
                return "pPresetSelect1_B";
            case Functionality::pPresetSelect2_A:
                return "pPresetSelect2_A";
            case Functionality::pPresetSelect2_B:
                return "pPresetSelect2_B";
            case Functionality::pPresetSelect3_A:
                return "pPresetSelect3_A";
            case Functionality::pPresetSelect3_B:
                return "pPresetSelect3_B";
            case Functionality::pPresetSelect4_A:
                return "pPresetSelect4_A";
            case Functionality::pPresetSelect4_B:
                return "pPresetSelect4_B";
            case Functionality::pPresetSelect5_A:
                return "pPresetSelect5_A";
            case Functionality::pPresetSelect5_B:
                return "pPresetSelect5_B";
            case Functionality::pPresetSelect6_A:
                return "pPresetSelect6_A";
            case Functionality::pPresetSelect6_B:
                return "pPresetSelect6_B";
            case Functionality::pDeviceReady:
                return "pDeviceReady";
            case Functionality::pError:
                return "pError";
            case Functionality::pReset:
                return "pReset";
            case Functionality::pRestartInterlock:
                return "pRestartInterlock";
            default:
                throw librealsense::invalid_value_exception("Invalid functionality value");
            }
        }

        Direction m_direction;
        Functionality m_functionality;
    };

    class m12_safety_pins_configuration
    {
    public:
        m12_safety_pins_configuration() = default;

        m12_safety_pins_configuration(const json &j)
        {
            validate_json(j);
            m_power = pin(j.at("power"));
            m_ossd1_b = pin(j.at("ossd1_b"));
            m_ossd1_a = pin(j.at("ossd1_a"));
            m_gpio_0 = pin(j.at("gpio_0"));
            m_gpio_1 = pin(j.at("gpio_1"));
            m_gpio_2 = pin(j.at("gpio_2"));
            m_gpio_3 = pin(j.at("gpio_3"));
            m_preset3_a = pin(j.at("preset3_a"));
            m_preset3_b = pin(j.at("preset3_b"));
            m_preset4_a = pin(j.at("preset4_a"));
            m_preset1_b = pin(j.at("preset1_b"));
            m_preset1_a = pin(j.at("preset1_a"));
            m_preset2_b = pin(j.at("preset2_b"));
            m_gpio_4 = pin(j.at("gpio_4"));
            m_preset2_a = pin(j.at("preset2_a"));
            m_preset4_b = pin(j.at("preset4_b"));
            m_ground = pin(j.at("ground"));
        }

        json to_json() const
        {
            json j;
            j["power"] = m_power.to_json();
            j["ossd1_b"] = m_ossd1_b.to_json();
            j["ossd1_a"] = m_ossd1_a.to_json();
            j["gpio_0"] = m_gpio_0.to_json();
            j["gpio_1"] = m_gpio_1.to_json();
            j["gpio_2"] = m_gpio_2.to_json();
            j["gpio_3"] = m_gpio_3.to_json();
            j["preset3_a"] = m_preset3_a.to_json();
            j["preset3_b"] = m_preset3_b.to_json();
            j["preset4_a"] = m_preset4_a.to_json();
            j["preset1_b"] = m_preset1_b.to_json();
            j["preset1_a"] = m_preset1_a.to_json();
            j["preset2_b"] = m_preset2_b.to_json();
            j["gpio_4"] = m_gpio_4.to_json();
            j["preset2_a"] = m_preset2_a.to_json();
            j["preset4_b"] = m_preset4_b.to_json();
            j["ground"] = m_ground.to_json();
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid m12_safety_pins_configuration format");
            }
            for (const auto &field : {"power", "ossd1_b", "ossd1_a", "gpio_0", "gpio_1", "gpio_2", "gpio_3", "preset3_a", "preset3_b", "preset4_a", "preset1_b", "preset1_a", "preset2_b", "gpio_4", "preset2_a", "preset4_b", "ground"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid m12_safety_pins_configuration format: missing field: ") + field);
                }
            }
        }

        pin m_power;
        pin m_ossd1_b;
        pin m_ossd1_a;
        pin m_preset3_a;
        pin m_preset3_b;
        pin m_preset4_a;
        pin m_preset1_b;
        pin m_preset1_a;
        pin m_gpio_0;
        pin m_gpio_1;
        pin m_gpio_3;
        pin m_gpio_2;
        pin m_preset2_b;
        pin m_gpio_4;
        pin m_preset2_a;
        pin m_preset4_b;
        pin m_ground;
    };

    class occupancy_grid_params
    {
    public:
        occupancy_grid_params() = default;

        occupancy_grid_params(const json &j)
        {
            validate_json(j);
            m_grid_cell_size = j.at("grid_cell_size");
            m_cell_threshold_factor = j.at("cell_threshold_factor").get<float>();
            m_polynomial_bias = j.at("polynomial_bias").get<float>();
            m_surface_height = j.at("surface_height").get<float>();
        }

        json to_json() const
        {
            json j;
            j["grid_cell_size"] = m_grid_cell_size;
            j["cell_threshold_factor"] = m_cell_threshold_factor;
            j["polynomial_bias"] = m_polynomial_bias;
            j["surface_height"] = m_surface_height;
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid occupancy_grid_params format");
            }
            for (const auto &field : {"grid_cell_size", "cell_threshold_factor", "polynomial_bias", "surface_height"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid occupancy_grid_params format: missing field: ") + field);
                }
            }
        }

        uint16_t m_grid_cell_size;
        float m_cell_threshold_factor;
        float m_polynomial_bias;
        float m_surface_height;
    };

    class smcu_arbitration_params
    {
    public:
        smcu_arbitration_params() = default;

        smcu_arbitration_params(const json &j)
        {
            validate_json(j);
            m_l_0_total_threshold = j.at("l_0_total_threshold").get<uint16_t>();
            m_l_0_sustained_rate_threshold = j.at("l_0_sustained_rate_threshold").get<uint8_t>();
            m_l_1_total_threshold = j.at("l_1_total_threshold").get<uint16_t>();
            m_l_1_sustained_rate_threshold = j.at("l_1_sustained_rate_threshold").get<uint8_t>();
            m_l_2_total_threshold = j.at("l_2_total_threshold").get<uint8_t>();
            m_hkr_stl_timeout = j.at("hkr_stl_timeout").get<uint8_t>();
            m_mcu_stl_timeout = j.at("mcu_stl_timeout").get<uint8_t>();
            m_sustained_aicv_frame_drops = j.at("sustained_aicv_frame_drops").get<uint8_t>();
            m_ossd_self_test_pulse_width = j.at("ossd_self_test_pulse_width").get<uint8_t>();
        }

        json to_json() const
        {
            json j;
            j["l_0_total_threshold"] = m_l_0_total_threshold;
            j["l_0_sustained_rate_threshold"] = m_l_0_sustained_rate_threshold;
            j["l_1_total_threshold"] = m_l_1_total_threshold;
            j["l_1_sustained_rate_threshold"] = m_l_1_sustained_rate_threshold;
            j["l_2_total_threshold"] = m_l_2_total_threshold;
            j["hkr_stl_timeout"] = m_hkr_stl_timeout;
            j["mcu_stl_timeout"] = m_mcu_stl_timeout;
            j["sustained_aicv_frame_drops"] = m_sustained_aicv_frame_drops;
            j["ossd_self_test_pulse_width"] = m_ossd_self_test_pulse_width;
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid smcu_arbitration_params format");
            }
            for (const auto &field : {"l_0_total_threshold", "l_0_sustained_rate_threshold", "l_1_total_threshold", "l_1_sustained_rate_threshold", "l_2_total_threshold", "hkr_stl_timeout", "mcu_stl_timeout", "sustained_aicv_frame_drops", "ossd_self_test_pulse_width"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid smcu_arbitration_params format: missing field: ") + field);
                }
            }
        }

        uint16_t m_l_0_total_threshold;
        uint8_t m_l_0_sustained_rate_threshold;
        uint16_t m_l_1_total_threshold;
        uint8_t m_l_1_sustained_rate_threshold;
        uint8_t m_l_2_total_threshold;
        uint8_t m_hkr_stl_timeout;
        uint8_t m_mcu_stl_timeout;
        uint8_t m_sustained_aicv_frame_drops;
        uint8_t m_ossd_self_test_pulse_width;
    };

    class safety_interface_config
    {
    public:
        safety_interface_config() = default;

        safety_interface_config(const json &j)
        {
            validate_json(j);
            m_pins_configs = m12_safety_pins_configuration(j.at("m12_safety_pins_configuration"));
            m_gpio_stabilization_interval = j.at("gpio_stabilization_interval").get<uint8_t>();
            m_camera_position = camera_position(j.at("camera_position"));
            m_occupancy_grid_params = occupancy_grid_params(j.at("occupancy_grid_params"));
            m_smcu_arbitration_params = smcu_arbitration_params(j.at("smcu_arbitration_params"));
            for (size_t i = 0; i < 32; ++i)
            {
                m_crypto_signature[i] = j.at("crypto_signature")[i].get<uint8_t>();
            }
        }

        json to_json()
        {
            json j;
            auto &sic = j["safety_interface_config"];
            sic["m12_safety_pins_configuration"] = m_pins_configs.to_json();
            sic["gpio_stabilization_interval"] = m_gpio_stabilization_interval;
            sic["camera_position"] = m_camera_position.to_json();
            sic["occupancy_grid_params"] = m_occupancy_grid_params.to_json();
            sic["smcu_arbitration_params"] = m_smcu_arbitration_params.to_json();
            sic["crypto_signature"] = m_crypto_signature;
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid safety_interface_config format: not a JSON object");
            }

            // Check for all required fields
            const std::vector<std::string> required_fields = {
                "m12_safety_pins_configuration",
                "gpio_stabilization_interval",
                "camera_position",
                "occupancy_grid_params",
                "smcu_arbitration_params",
                "crypto_signature"};

            for (const auto &field : required_fields)
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception("Invalid safety_interface_config format: missing field: " + field);
                }
            }

            // Validate m12_safety_pins_configuration
            if (!j["m12_safety_pins_configuration"].is_object())
            {
                throw librealsense::invalid_value_exception("Invalid format: m12_safety_pins_configuration must be a JSON object");
            }

            // Validate gpio_stabilization_interval
            if (!j["gpio_stabilization_interval"].is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid format: gpio_stabilization_interval must be an unsigned number");
            }

            // Validate camera_position
            if (!j["camera_position"].is_object()) // Assuming camera_position is a complex object
            {
                throw librealsense::invalid_value_exception("Invalid format: camera_position must be a JSON object");
            }

            // Validate occupancy_grid_params
            if (!j["occupancy_grid_params"].is_object())
            {
                throw librealsense::invalid_value_exception("Invalid format: occupancy_grid_params must be a JSON object");
            }

            // Validate smcu_arbitration_params
            if (!j["smcu_arbitration_params"].is_object())
            {
                throw librealsense::invalid_value_exception("Invalid format: smcu_arbitration_params must be a JSON object");
            }

            // Validate crypto_signature (should be an array of 32 elements)
            if (!j["crypto_signature"].is_array() || j["crypto_signature"].size() != 32)
            {
                throw librealsense::invalid_value_exception("Invalid format: crypto_signature must be an array of 32 unsigned integers");
            }
            for (const auto &val : j["crypto_signature"])
            {
                if (!val.is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid format: crypto_signature contains non-unsigned values");
                }
            }
        }

        m12_safety_pins_configuration m_pins_configs;
        uint8_t m_gpio_stabilization_interval;
        camera_position m_camera_position;
        occupancy_grid_params m_occupancy_grid_params;
        smcu_arbitration_params m_smcu_arbitration_params;
        std::array<uint8_t, 32> m_crypto_signature; // SHA2 or similar
        std::array<uint8_t, 324> m_reserved2 = {0};
    };

    /***
     *  safety_interface_config_with_header class
     *  Consists of safety interface config table and a table header
     */
    class safety_interface_config_with_header
    {
    public:
        safety_interface_config_with_header(table_header header, safety_interface_config sic) : m_header(header), m_safety_interface_config(sic)
        {
        }

        safety_interface_config get_safety_interface_config() const
        {
            return m_safety_interface_config;
        }

        table_header get_table_header() const
        {
            return m_header;
        }

    private:
        table_header m_header;
        safety_interface_config m_safety_interface_config;
    };

#pragma pack(pop)
}
