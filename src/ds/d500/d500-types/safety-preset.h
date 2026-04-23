// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "common.h"

namespace librealsense
{

#pragma pack(push, 1)

    class platform_config
    {
    public:
        platform_config() = default;

        platform_config(const json &j)
        {
            validate_json(j);
            m_transformation_link = camera_position(j.at("transformation_link"));
            m_robot_height = j.at("robot_height").get<float>();

            for (size_t i = 0; i < m_reserved.size(); ++i)
            {
                m_reserved[i] = j.at("reserved")[i].get<uint8_t>();
            }
        }

        rsutils::json to_json()
        {
            json j;
            j["transformation_link"] = m_transformation_link.to_json();
            j["robot_height"] = m_robot_height;
            j["reserved"] = m_reserved;
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object() || j.size() != 3)
            {
                throw librealsense::invalid_value_exception(
                    "Invalid platform_config format: platform_config must include 'transformation_link', 'reserved' " 
                    "and 'robot_height'");
            }
            if (!j.at("transformation_link").is_object())
            {
                throw librealsense::invalid_value_exception("Invalid transformation_link format");
            }
            if (!j.at("robot_height").is_number_float())
            {
                throw librealsense::invalid_value_exception("Invalid robot_height type: robot_height must be float number");
            }
            if (!j.at("reserved").is_array() || j.at("reserved").size() != 20)
            {
                throw librealsense::invalid_value_exception("Invalid reserved format: reserved must be an array of size=20");
            }
            for (size_t i = 0; i < 20; ++i)
            {
                if (!j.at("reserved")[i].is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid reserved format: all elements in reserved array must be unsigned integers");
                }
            }

        }

        camera_position m_transformation_link; // Camera extrinsics: rotation 3x3 matrix (row major) + translation vecotr
        float m_robot_height;
        std::array<uint8_t, 20> m_reserved = {0};

    };

    /*
    polygon_point class
    handles JSON representing 2D polygon_point (x,y)
    JSON schema: [ float, float ]
    */
    class polygon_point
    {
    public:
        polygon_point() : m_x(0), m_y(0) {}

        polygon_point(const json &j)
        {
            validate_json(j);
            m_x = j["x"].get<float>();
            m_y = j["y"].get<float>();
        }

        json to_json() const
        {
            return {
                {"x", m_x},
                {"y", m_y}
            };
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.contains("x"))
            {
                throw std::invalid_argument("Invalid polygon_point format: missing 'x' field");
            }
            if (!j.contains("y"))
            {
                throw std::invalid_argument("Invalid polygon_point format: missing 'y' field");
            }
        }

    private:
        float m_x;
        float m_y;
    };

    class zone_polygon
    {
    public:
        zone_polygon() = default;

        zone_polygon(const json &j)
        {
            validate_json(j);
            m_p0 = polygon_point(j.at("p0"));
            m_p1 = polygon_point(j.at("p1"));
            m_p2 = polygon_point(j.at("p2"));
            m_p3 = polygon_point(j.at("p3"));
        }

        json to_json() const
        {
            return {
                {"p0", m_p0.to_json()},
                {"p1", m_p1.to_json()},
                {"p2", m_p2.to_json()},
                {"p3", m_p3.to_json()}};
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid zone_polygon format");
            }
            for (const auto &field : {"p0", "p1", "p2", "p3"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid zone_polygon format: missing field: ") + field);
                }
            }
        }

    private:
        polygon_point m_p0;
        polygon_point m_p1;
        polygon_point m_p2;
        polygon_point m_p3;
    };

    class safety_zone
    {
    public:
        safety_zone() = default;

        safety_zone(const json &j)
        {
            validate_json(j);
            m_zone_polygon = zone_polygon(j.at("zone_polygon"));
            m_safety_trigger_confidence = j.at("safety_trigger_confidence").get<uint8_t>();

            for (size_t i = 0; i < m_reserved.size(); ++i)
            {
                m_reserved[i] = j.at("reserved")[i].get<uint8_t>();
            }

        }

        json to_json() const
        {
            json j;
            j["zone_polygon"] = m_zone_polygon.to_json();
            j["safety_trigger_confidence"] = m_safety_trigger_confidence;
            j["reserved"] = m_reserved;
            return j;
        };

    protected:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid safety zone format");
            }
            for (const auto &field : {"zone_polygon", "safety_trigger_confidence", "reserved"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid safety zone format: missing field: ") + field);
                }
            }
            if (!j.at("reserved").is_array() || j.at("reserved").size() != 7)
            {
                throw librealsense::invalid_value_exception("Invalid safety zone format: reserved must be an array of size=7");
            }
            for (size_t i = 0; i < 7; ++i)
            {
                if (!j.at("reserved")[i].is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid safety zone format: all elements in reserved array must be unsigned integers");
                }
            }

        };

        zone_polygon m_zone_polygon;
        uint8_t m_safety_trigger_confidence;
        std::array<uint8_t, 7> m_reserved = {0};

    };

    class safety_zones
    {
    public:
        safety_zones() = default;

        safety_zones(const json &j)
        {
            validate_json(j);
            m_danger_zone = safety_zone(j.at("danger_zone"));
            m_warning_zone = safety_zone(j.at("warning_zone"));
        }

        json to_json() const
        {
            json j;
            j["danger_zone"] = m_danger_zone.to_json();
            j["warning_zone"] = m_warning_zone.to_json();
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid safety zones format");
            }
            for (const auto &field : {"danger_zone", "warning_zone"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid safety zone format: missing field: ") + field);
                }
            }
        }

        safety_zone m_danger_zone;
        safety_zone m_warning_zone;
    };

    class masking_zone
    {
    public:
        masking_zone() = default;

        masking_zone(const json &j)
        {
            validate_json(j);
            m_attributes = j.at("attributes").get<uint16_t>();
            m_minimal_range = j.at("minimal_range").get<float>();
            m_roi = roi(j.at("region_of_interests"));
        }

        json to_json() const
        {
            json j;
            j["attributes"] = m_attributes;
            j["minimal_range"] = m_minimal_range;
            j["region_of_interests"] = m_roi.to_json();
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid masking zone format");
            }
            for (const auto &field : {"attributes", "minimal_range", "region_of_interests"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid masking zone format: missing field: ") + field);
                }
            }
        }

        uint16_t m_attributes; // Bitmask, enumerated: (0x1 <<0) - is_valid: specifies whether the specifc zone was defined by user in this preset
        float m_minimal_range; // in meters in forward direction (leveled CS)
        roi m_roi;             // (i,j) pairs of 4 two-dimentional quadrilateral points
    };

    class masking_zones
    {
    public:
        masking_zones() = default;

        masking_zones(const json &j)
        {
            validate_json(j);
            for (size_t i = 0; i < 8; ++i)
            {
                std::string masking_zone_id_str = std::to_string(i);
                m_masking_zones[i] = masking_zone(j.at(masking_zone_id_str));
            }
        }

        json to_json() const
        {
            json j;
            for (size_t i = 0; i < 8; ++i)
            {
                std::string masking_zone_id_str = std::to_string(i);
                j[masking_zone_id_str] = m_masking_zones[i].to_json();
            }
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid masking zones format");
            }
            for (const auto &field : {"0", "1", "2", "3", "4", "5", "6", "7"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid masking zones format: missing field: ") + field);
                }
            }
        }

        std::array<masking_zone, 8> m_masking_zones;
    };

    class safety_environment
    {
    public:
        safety_environment() = default;

        safety_environment(const json &j)
        {
            validate_json(j);
            m_safety_trigger_duration = j.at("safety_trigger_duration").get<float>();
            m_zero_safety_monitoring = j.at("zero_safety_monitoring").get<uint8_t>();
            m_hara_history_continuation = j.at("hara_history_continuation").get<uint8_t>();

            for (size_t i = 0; i < m_reserved1.size(); ++i)
            {
                m_reserved1[i] = j.at("reserved1")[i].get<uint8_t>();
            }

            m_angular_velocity = j.at("angular_velocity").get<float>();
            m_payload_weight = j.at("payload_weight").get<float>();
            m_surface_inclination = j.at("surface_inclination").get<float>();
            m_diagnostic_zone_fill_rate_threshold = j.at("diagnostic_zone_fill_rate_threshold").get<uint8_t>();
            m_floor_fill_threshold = j.at("floor_fill_threshold").get<uint8_t>();
            m_depth_fill_threshold = j.at("depth_fill_threshold").get<uint8_t>();
            m_diagnostic_zone_height_median_threshold = j.at("diagnostic_zone_height_median_threshold").get<uint8_t>();
            m_vision_hara_persistency = j.at("vision_hara_persistency").get<uint8_t>();

            for (size_t i = 0; i < 32; ++i)
            {
                m_crypto_signature[i] = j.at("crypto_signature")[i].get<uint8_t>();
            }

            for (size_t i = 0; i < m_reserved2.size(); ++i)
            {
                m_reserved2[i] = j.at("reserved2")[i].get<uint8_t>();
            }

        }

        json to_json() const
        {
            json j;
            j["safety_trigger_duration"] = m_safety_trigger_duration;
            j["zero_safety_monitoring"] = m_zero_safety_monitoring;
            j["hara_history_continuation"] = m_hara_history_continuation;
            j["reserved1"] = m_reserved1;
            j["angular_velocity"] = m_angular_velocity;
            j["payload_weight"] = m_payload_weight;
            j["surface_inclination"] = m_surface_inclination;
            j["diagnostic_zone_fill_rate_threshold"] = m_diagnostic_zone_fill_rate_threshold;
            j["floor_fill_threshold"] = m_floor_fill_threshold;
            j["depth_fill_threshold"] = m_depth_fill_threshold;
            j["diagnostic_zone_height_median_threshold"] = m_diagnostic_zone_height_median_threshold;
            j["vision_hara_persistency"] = m_vision_hara_persistency;
            j["crypto_signature"] = m_crypto_signature;
            j["reserved2"] = m_reserved2;
            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object() || j.size() != 14)
            {
                throw librealsense::invalid_value_exception(
                    "Invalid environment format: environment must include 'safety_trigger_duration', "
                    "'zero_safety_monitoring', 'hara_history_continuation', 'reserved1', 'angular_velocity', 'payload_weight', 'surface_inclination', "
                    "'diagnostic_zone_fill_rate_threshold', 'floor_fill_threshold', 'depth_fill_threshold', 'diagnostic_zone_height_median_threshold', "
                    "'vision_hara_persistency', 'crypto_signature' and 'reserved2'");
            }
            if (!j.at("safety_trigger_duration").is_number_float())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: safety_trigger_duration must be float number");
            }
            if (!j.at("zero_safety_monitoring").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: roi_num_of_sezero_safety_monitoringgments must be unsigned number");
            }
            if (!j.at("hara_history_continuation").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: hara_history_continuation must be unsigned number");
            }
            if (!j.at("reserved1").is_array() || j.at("reserved1").size() != 2)
            {
                throw librealsense::invalid_value_exception("Invalid environment format: reserved1 must be an array of size=2");
            }
            for (size_t i = 0; i < 2; ++i)
            {
                if (!j.at("reserved1")[i].is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid environment type: all elements in reserved1 array must be unsigned integers");
                }
            }
            if (!j.at("angular_velocity").is_number_float())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: angular_velocity must be float number");
            }
            if (!j.at("payload_weight").is_number_float())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: payload_weight must be float number");
            }
            if (!j.at("surface_inclination").is_number_float())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: surface_inclination must be float number");
            }
            if (!j.at("diagnostic_zone_fill_rate_threshold").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: diagnostic_zone_fill_rate_threshold must be unsigned number");
            }
            if (!j.at("floor_fill_threshold").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: floor_fill_threshold must be unsigned number");
            }
            if (!j.at("depth_fill_threshold").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: depth_fill_threshold must be unsigned number");
            }
            if (!j.at("diagnostic_zone_height_median_threshold").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: diagnostic_zone_height_median_threshold must be unsigned number");
            }
            if (!j.at("vision_hara_persistency").is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid environment type: vision_hara_persistency must be unsigned number");
            }
            if (!j.at("crypto_signature").is_array() || j.at("crypto_signature").size() != 32)
            {
                throw librealsense::invalid_value_exception("Invalid environment format: crypto_signature must be an array of size=32");
            }
            for (size_t i = 0; i < 32; ++i)
            {
                if (!j.at("crypto_signature")[i].is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid environment type: all elements in crypto_signature array must be unsigned integers");
                }
            }
            if (!j.at("reserved2").is_array() || j.at("reserved2").size() != 3)
            {
                throw librealsense::invalid_value_exception("Invalid environment format: reserved2 must be an array of size=3");
            }
            for (size_t i = 0; i < 3; ++i)
            {
                if (!j.at("reserved2")[i].is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid environment type: all elements in reserved2 array must be unsigned integers");
                }
            }

        }

        float m_safety_trigger_duration; // duration in seconds to keep safety signal high after safety MCU is back to normal

        // Platform dynamics properties
        uint8_t m_zero_safety_monitoring;    // 0 - Regular (default). All Safety Mechanisms activated in nominal mode.
                                             // 1 - "Zero Safety" mode.Continue monitoring but Inhibit triggering OSSD on Vision HaRA event + Collision Detections
        uint8_t m_hara_history_continuation; // 0 - Regular or Global History. Vision HaRa Metrics are continued when switching Safety Preset. (default value)
                                             // 1 - No History.When toggling Safety Preset all Vision HaRa metrics based on multiple sampling is being reset.
                                             // 2 - Local History*, or History per Safety Preset.In this mode Vision HaRa metrics history is tracked per each preset
                                             // individually.When toggling Safety Presets S.MCU to check if that particular preset has history and if so - take that into consideration when reaching FuSa decision.
                                             // In case the particular Safety Preset had no recorded history tracking then the expected behavior is similar to #1.
        std::array<uint8_t, 2> m_reserved1 = {0};
        float m_angular_velocity; // rad/sec
        float m_payload_weight;   // a typical mass of the carriage payload in kg

        // Environmetal properties
        float m_surface_inclination;                       // Expected floor min/max inclination angle, degrees
        uint8_t m_reserved_surface_height[4] = {0};        // surface_height transferred to safety_interface_config (0xC0DC)
        uint8_t m_diagnostic_zone_fill_rate_threshold;     // Min pixel fill rate required in the diagnostic area for Safety Algo processing [0...100%]
        uint8_t m_floor_fill_threshold;                    // Depth fill rate threshold for the floor area [0...100%]
        uint8_t m_depth_fill_threshold;                    // Depth Fill rate threshold for the full image [0...100%]
        uint8_t m_diagnostic_zone_height_median_threshold; // Diagnostic Zone height median in [ 0..255 ] millimeter range. Absolute
        uint8_t m_vision_hara_persistency;                 // Represents the number of consecutive frames with alarming Vision HaRa parameters to raise safety signal [1..5]. Default = 1
        std::array<uint8_t, 32> m_crypto_signature;    // SHA2 or similar

        std::array<uint8_t, 3> m_reserved2 = {0}; // Can be modified by changing the table minor version, without breaking back-compat
    };

    class safety_preset
    {
    public:
        safety_preset(const json &j)
        {
            validate_json(j);
            m_platform_config = platform_config(j.at("platform_config"));
            m_safety_zones = safety_zones(j.at("safety_zones"));
            m_masking_zones = masking_zones(j.at("masking_zones"));

            for (size_t i = 0; i < m_reserved.size(); ++i)
            {
                m_reserved[i] = j.at("reserved")[i].get<uint8_t>();
            }

            m_environment = safety_environment(j.at("environment"));
        }

        rsutils::json to_json()
        {
            rsutils::json j;
            auto &safety_preset_json = j["safety_preset"];
            safety_preset_json["platform_config"] = m_platform_config.to_json();
            safety_preset_json["safety_zones"] = m_safety_zones.to_json();
            safety_preset_json["masking_zones"] = m_masking_zones.to_json();
            safety_preset_json["reserved"] = m_reserved;
            safety_preset_json["environment"] = m_environment.to_json();

            return j;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object() || j.size() != 5)
            {
                throw librealsense::invalid_value_exception(
                    "Invalid safety_preset format: safety_preset must include 'platform_config', "
                    "'safety_zones', 'masking_zones', 'reserved', and 'environment'");
            }
            if (!j.at("platform_config").is_object())
            {
                throw librealsense::invalid_value_exception("Invalid platform_config format");
            }
            if (!j.at("safety_zones").is_object())
            {
                throw librealsense::invalid_value_exception("Invalid safety_zones format");
            }
            if (!j.at("masking_zones").is_object())
            {
                throw librealsense::invalid_value_exception("Invalid masking_zones format");
            }
            if (!j.at("reserved").is_array() || j.at("reserved").size() != 16)
            {
                throw librealsense::invalid_value_exception("Invalid reserved format: reserved must be an array of size=16");
            }
            for (size_t i = 0; i < 16; ++i)
            {
                if (!j.at("reserved")[i].is_number_unsigned())
                {
                    throw librealsense::invalid_value_exception("Invalid reserved type: all elements in reserved array must be unsigned integers");
                }
            }
            if (!j.at("environment").is_object())
            {
                throw librealsense::invalid_value_exception("Invalid environment format");
            }
        }

        platform_config m_platform_config;
        safety_zones m_safety_zones;
        masking_zones m_masking_zones;
        std::array<uint8_t, 16> m_reserved = {0};
        safety_environment m_environment;
    };

    class safety_preset_header
    {
    public:
        safety_preset_header(uint16_t version, uint16_t table_type, uint32_t table_size, uint32_t crc32) : m_version(version), m_table_type(table_type), m_table_size(table_size), m_crc32(crc32)
        {
        }

        uint32_t get_crc32() const
        {
            return m_crc32;
        }

    private:
        uint16_t m_version;       // major.minor. Big-endian
        uint16_t m_table_type;    // type
        uint32_t m_table_size;    // full size including: header footer
        uint32_t m_crc32;         // crc of all the data in table excluding this header/CRC
    };

    /***
     *  safety_preset_with_header class
     *  Consists of safety preset table and a safety preset header
     */
    class safety_preset_with_header
    {
    public:
        safety_preset_with_header(safety_preset_header header, safety_preset sp) : m_header(header), m_safety_preset(sp)
        {
        }

        safety_preset get_safety_preset() const
        {
            return m_safety_preset;
        }

        safety_preset_header get_table_header() const
        {
            return m_header;
        }

    private:
        safety_preset_header m_header;
        safety_preset m_safety_preset;
    };

#pragma pack(pop)
}
