// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include <rsutils/json.h>
#include <rsutils/json-validator.h>
#include <stdint.h>
#include <sstream>
#include <stdexcept>

namespace librealsense
{
    using rsutils::json;
    using rsutils::json_validator::validate_json_field;

#pragma pack(push, 1)

    class table_header
    {
    public:
        table_header(uint16_t version, uint16_t table_type, uint32_t table_size, uint32_t calib_version, uint32_t crc32) : m_version(version), m_table_type(table_type), m_table_size(table_size), m_calib_version(calib_version), m_crc32(crc32)
        {
        }

        uint16_t get_table_type() const
        {
            return m_table_type;
        }

        uint32_t get_table_size() const
        {
            return m_table_size;
        }

        uint32_t get_crc32() const
        {
            return m_crc32;
        }

    private:
        uint16_t m_version;       // major.minor. Big-endian
        uint16_t m_table_type;    // type
        uint32_t m_table_size;    // full size including: header footer
        uint32_t m_calib_version; // major.minor.index
        uint32_t m_crc32;         // crc of all the data in table excluding this header/CRC
    };

    /*
    vertex class
    handles JSON representing 2D Vertex (x,y)
    JSON schema: [ uint16_t, uint16_t ]
    */
    class vertex
    {
    public:
        vertex() : m_x(0), m_y(0) {}

        vertex(const json &j)
        {
            validate_json(j);
            m_x = j[0].get<uint16_t>();
            m_y = j[1].get<uint16_t>();
        }

        json to_json() const
        {
            return json::array({m_x, m_y});
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_array() || j.size() != 2)
            {
                throw librealsense::invalid_value_exception("Invalid Vertex format: each vertex should be an array of size=2");
            }
            if (!j[0].is_number_unsigned() || !j[1].is_number_unsigned())
            {
                throw librealsense::invalid_value_exception("Invalid Vertex type: Each vertex must include only unsigned integers");
            }
        }

    private:
        uint16_t m_x;
        uint16_t m_y;
    };

    /*
    roi class
    handles JSON of 4 vertices representing region of interest
    JSON schema:
    {
        "vertex_0": [0, 0],
        "vertex_1": [0, 0],
        "vertex_2": [0, 0],
        "vertex_3": [0, 0]
    }
    */
    class roi
    {
    public:
        roi() = default;

        roi(const json &j)
        {
            validate_json(j);
            m_vertex_0 = vertex(j.at("vertex_0"));
            m_vertex_1 = vertex(j.at("vertex_1"));
            m_vertex_2 = vertex(j.at("vertex_2"));
            m_vertex_3 = vertex(j.at("vertex_3"));
        }

        json to_json() const
        {
            return {
                {"vertex_0", m_vertex_0.to_json()},
                {"vertex_1", m_vertex_1.to_json()},
                {"vertex_2", m_vertex_2.to_json()},
                {"vertex_3", m_vertex_3.to_json()}};
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object())
            {
                throw librealsense::invalid_value_exception("Invalid ROI format");
            }
            for (const auto &field : {"vertex_0", "vertex_1", "vertex_2", "vertex_3"})
            {
                if (!j.contains(field))
                {
                    throw librealsense::invalid_value_exception(std::string("Invalid ROI format: missing field: ") + field);
                }
            }
        }

    private:
        vertex m_vertex_0;
        vertex m_vertex_1;
        vertex m_vertex_2;
        vertex m_vertex_3;
    };

    /*
    camera_position class
    handles JSON of representing the camera positon: rotation row major 3x3 matrix and translation vector
    all values are floats.
    JSON schema:
    {
        "rotation":
            [
                [a, b, c],
                [d, e, f],
                [g, h, i]
            ],
        "translation": {x, y, z}
    }
    */
    class camera_position
    {

    public:
        camera_position() = default;

        camera_position(const json &j)
        {
            validate_json(j);
            for (size_t i = 0; i < 3; ++i)
            {
                for (size_t k = 0; k < 3; ++k)
                {
                    m_rotation[i][k] = j.at("rotation")[i][k].get<float>();
                }
            }
            for (size_t i = 0; i < 3; ++i)
            {
                m_translation[i] = j.at("translation")[i].get<float>();
            }
        }

        json to_json() const
        {
            json j;
            j["rotation"] = json::array();
            for (size_t i = 0; i < 3; ++i)
            {
                j["rotation"].push_back(json::array());
                for (size_t k = 0; k < 3; ++k)
                {
                    j["rotation"][i].push_back(m_rotation[i][k]);
                }
            }
            j["translation"] = m_translation;
            return j;
        }

        std::array<std::array<float, 3>, 3> get_rotation() const
        {
            return m_rotation;
        }

        std::array<float, 3> get_translation() const
        {
            return m_translation;
        }

    private:
        void validate_json(const json &j) const
        {
            if (!j.is_object() || j.size() != 2 || !j.contains("rotation") || !j.contains("translation"))
            {
                throw librealsense::invalid_value_exception("Invalid camera_position format: camera_position must include rotation and translation fields");
            }
            if (!j.at("rotation").is_array() || j.at("rotation").size() != 3)
            {
                throw librealsense::invalid_value_exception("Invalid rotation format: rotation must be a 3x3 matrix");
            }
            for (size_t i = 0; i < 3; ++i)
            {
                if (!j.at("rotation")[i].is_array() || j.at("rotation")[i].size() != 3)
                {
                    throw librealsense::invalid_value_exception("Invalid rotation row format: rotation must be a 3x3 matrix");
                }
                for (size_t k = 0; k < 3; ++k)
                {
                    if (!j.at("rotation")[i][k].is_number_float())
                    {
                        throw librealsense::invalid_value_exception("Invalid rotation type: all rotation values must be floats.");
                    }
                }
            }
            if (!j.at("translation").is_array() || j.at("translation").size() != 3)
            {
                throw librealsense::invalid_value_exception("Invalid translation format: translation vector should be an array of size=3");
            }
            for (size_t i = 0; i < 3; ++i)
            {
                if (!j.at("translation")[i].is_number_float())
                {
                    throw librealsense::invalid_value_exception("Invalid translation type: translation vector values should be floats");
                }
            }
        }

    private:
        std::array<std::array<float, 3>, 3> m_rotation; // row major order
        std::array<float, 3> m_translation;
    };


#pragma pack(pop)

    // helper function
    template <typename T, size_t N>
    std::vector<T> native_arr_to_std_vector(const T (&arr)[N])
    {
        return std::vector<T>(std::begin(arr), std::end(arr));
    }
}
