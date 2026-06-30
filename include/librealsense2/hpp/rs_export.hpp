// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_EXPORT_HPP
#define LIBREALSENSE_RS2_EXPORT_HPP

#include <map>
#include <fstream>
#include <cmath>
#include <sstream>
#include <cassert>
#include "rs_processing.hpp"
#include "rs_internal.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <array>

namespace rs2
{
    struct vec3d {
        float x, y, z;
        float length() const { return sqrt(x * x + y * y + z * z); }

        vec3d normalize() const
        {
            auto len = length();
            return { x / len, y / len, z / len };
        }
    };

    inline vec3d operator + (const vec3d & a, const vec3d & b) { return{ a.x + b.x, a.y + b.y, a.z + b.z }; }
    inline vec3d operator - (const vec3d & a, const vec3d & b) { return{ a.x - b.x, a.y - b.y, a.z - b.z }; }
    inline vec3d cross(const vec3d & a, const vec3d & b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

    class save_to_ply : public filter
    {
    public:
        static const auto OPTION_IGNORE_COLOR = rs2_option(RS2_OPTION_COUNT + 10);
        static const auto OPTION_PLY_MESH = rs2_option(RS2_OPTION_COUNT + 11);
        static const auto OPTION_PLY_BINARY = rs2_option(RS2_OPTION_COUNT + 12);
        static const auto OPTION_PLY_NORMALS = rs2_option(RS2_OPTION_COUNT + 13);
        static const auto OPTION_PLY_THRESHOLD = rs2_option(RS2_OPTION_COUNT + 14);

        save_to_ply(std::string filename = "RealSense Pointcloud ", pointcloud pc = pointcloud()) : filter([this](frame f, frame_source& s) { func(f, s); }),
            _pc(std::move(pc)), fname(filename)
        {
            register_simple_option(OPTION_IGNORE_COLOR, option_range{ 0, 1, 0, 1 });
            register_simple_option(OPTION_PLY_MESH, option_range{ 0, 1, 1, 1 });
            register_simple_option(OPTION_PLY_NORMALS, option_range{ 0, 1, 0, 1 });
            register_simple_option(OPTION_PLY_BINARY, option_range{ 0, 1, 1, 1 });
            register_simple_option(OPTION_PLY_THRESHOLD, option_range{ 0, 1, 0.05f, 0 });
        }

    private:
        void func(frame data, frame_source& source)
        {
            frame depth, color;
            if (auto fs = data.as<frameset>()) {
                for (auto f : fs) {
                    if (f.is<points>()) depth = f;
                    else if (!depth && f.is<depth_frame>()) depth = f;
                    else if (!color && f.is<video_frame>()) color = f;
                }
            } else if (data.is<depth_frame>() || data.is<points>()) {
                depth = data;
            }

            if (!depth) throw std::runtime_error("Need depth data to save PLY");
            if (!depth.is<points>()) {
                if (color) _pc.map_to(color);
                depth = _pc.calculate(depth);
            }

            export_to_ply(depth, color);
            source.frame_ready(data); // passthrough filter because processing_block::process doesn't support sinks
        }

        void export_to_ply(points p, video_frame color) {
            const bool use_texcoords  = color && !get_option(OPTION_IGNORE_COLOR);
            bool mesh = get_option(OPTION_PLY_MESH) != 0;
            bool binary = get_option(OPTION_PLY_BINARY) != 0;
            bool use_normals = get_option(OPTION_PLY_NORMALS) != 0;
            const auto verts = p.get_vertices();
            const auto texcoords = p.get_texture_coordinates();
            const uint8_t* texture_data = nullptr;
            if (use_texcoords) // texture might be on the gpu, get pointer to data before for-loop to avoid repeated access
                texture_data = reinterpret_cast<const uint8_t*>(color.get_data());
            std::vector<rs2::vertex> new_verts;
            std::vector<vec3d> normals;
            std::vector<std::array<uint8_t, 3>> new_tex;
            std::map<size_t, size_t> idx_map;
            std::map<size_t, std::vector<vec3d>> index_to_normals;

            new_verts.reserve(p.size());
            if (use_texcoords) new_tex.reserve(p.size());

            static const auto min_distance = 1e-6;

            for (size_t i = 0; i < p.size(); ++i) {
                if (fabs(verts[i].x) >= min_distance || fabs(verts[i].y) >= min_distance ||
                    fabs(verts[i].z) >= min_distance)
                {
                    idx_map[int(i)] = int(new_verts.size());
                    new_verts.push_back({ verts[i].x, -1 * verts[i].y, -1 * verts[i].z });
                    if (use_texcoords)
                    {
                        auto rgb = get_texcolor(color, texture_data, texcoords[i].u, texcoords[i].v);
                        new_tex.push_back(rgb);
                    }
                }
            }

            auto profile = p.get_profile().as<video_stream_profile>();
            auto width = profile.width(), height = profile.height();
            static const auto threshold = get_option(OPTION_PLY_THRESHOLD);
            std::vector<std::array<size_t, 3>> faces;
            if (mesh)
            {
                for (size_t x = 0; x < width - 1; ++x) {
                    for (size_t y = 0; y < height - 1; ++y) {
                        auto a = y * width + x, b = y * width + x + 1, c = (y + 1)*width + x, d = (y + 1)*width + x + 1;
                        if (verts[a].z && verts[b].z && verts[c].z && verts[d].z
                            && fabs(verts[a].z - verts[b].z) < threshold && fabs(verts[a].z - verts[c].z) < threshold
                            && fabs(verts[b].z - verts[d].z) < threshold && fabs(verts[c].z - verts[d].z) < threshold)
                        {
                            if (idx_map.count(a) == 0 || idx_map.count(b) == 0 || idx_map.count(c) == 0 ||
                                idx_map.count(d) == 0)
                                continue;
                            faces.push_back({ idx_map[a], idx_map[d], idx_map[b] });
                            faces.push_back({ idx_map[d], idx_map[a], idx_map[c] });

                            if (use_normals)
                            {
                                vec3d point_a = { verts[a].x ,  -1 * verts[a].y,  -1 * verts[a].z };
                                vec3d point_b = { verts[b].x ,  -1 * verts[b].y,  -1 * verts[b].z };
                                vec3d point_c = { verts[c].x ,  -1 * verts[c].y,  -1 * verts[c].z };
                                vec3d point_d = { verts[d].x ,  -1 * verts[d].y,  -1 * verts[d].z };

                                auto n1 = cross(point_d - point_a, point_b - point_a);
                                auto n2 = cross(point_c - point_a, point_d - point_a);

                                index_to_normals[idx_map[a]].push_back(n1);
                                index_to_normals[idx_map[a]].push_back(n2);

                                index_to_normals[idx_map[b]].push_back(n1);

                                index_to_normals[idx_map[c]].push_back(n2);

                                index_to_normals[idx_map[d]].push_back(n1);
                                index_to_normals[idx_map[d]].push_back(n2);
                            }
                        }
                    }
                }
            }

            if (mesh && use_normals)
            {
                for (size_t i = 0; i < new_verts.size(); ++i)
                {
                    auto normals_vec = index_to_normals[i];
                    vec3d sum = { 0, 0, 0 };
                    for (auto& n : normals_vec)
                        sum = sum + n;
                    if (normals_vec.size() > 0)
                        normals.push_back((sum.normalize()));
                    else
                        normals.push_back({ 0, 0, 0 });
                }
            }

            std::ofstream out(fname);
            out << "ply\n";
            if (binary)
                out << "format binary_little_endian 1.0\n";
            else
                out << "format ascii 1.0\n";
            out << "comment pointcloud saved from Realsense Viewer\n";
            out << "element vertex " << new_verts.size() << "\n";
            out << "property float" << sizeof(float) * 8 << " x\n";
            out << "property float" << sizeof(float) * 8 << " y\n";
            out << "property float" << sizeof(float) * 8 << " z\n";
            if (mesh && use_normals)
            {
                out << "property float" << sizeof(float) * 8 << " nx\n";
                out << "property float" << sizeof(float) * 8 << " ny\n";
                out << "property float" << sizeof(float) * 8 << " nz\n";
            }
            if (use_texcoords)
            {
                out << "property uchar red\n";
                out << "property uchar green\n";
                out << "property uchar blue\n";
            }
            if (mesh)
            {
                out << "element face " << faces.size() << "\n";
                out << "property list uchar int vertex_indices\n";
            }
            out << "end_header\n";

            if (binary)
            {
                out.close();
                out.open(fname, std::ios_base::app | std::ios_base::binary);
                for (size_t i = 0; i < new_verts.size(); ++i)
                {
                    // we assume little endian architecture on your device
                    out.write(reinterpret_cast<const char*>(&(new_verts[i].x)), sizeof(float));
                    out.write(reinterpret_cast<const char*>(&(new_verts[i].y)), sizeof(float));
                    out.write(reinterpret_cast<const char*>(&(new_verts[i].z)), sizeof(float));

                    if (mesh && use_normals)
                    {
                        out.write(reinterpret_cast<const char*>(&(normals[i].x)), sizeof(float));
                        out.write(reinterpret_cast<const char*>(&(normals[i].y)), sizeof(float));
                        out.write(reinterpret_cast<const char*>(&(normals[i].z)), sizeof(float));
                    }

                    if (use_texcoords)
                    {
                        out.write(reinterpret_cast<const char*>(&(new_tex[i][0])), sizeof(uint8_t));
                        out.write(reinterpret_cast<const char*>(&(new_tex[i][1])), sizeof(uint8_t));
                        out.write(reinterpret_cast<const char*>(&(new_tex[i][2])), sizeof(uint8_t));
                    }
                }
                if (mesh)
                {
                    auto size = faces.size();
                    for (size_t i = 0; i < size; ++i) {
                        static const int three = 3;
                        out.write(reinterpret_cast<const char*>(&three), sizeof(uint8_t));
                        out.write(reinterpret_cast<const char*>(&(faces[i][0])), sizeof(int));
                        out.write(reinterpret_cast<const char*>(&(faces[i][1])), sizeof(int));
                        out.write(reinterpret_cast<const char*>(&(faces[i][2])), sizeof(int));
                    }
                }
            }
            else
            {
                for (size_t i = 0; i <new_verts.size(); ++i)
                {
                    out << new_verts[i].x << " ";
                    out << new_verts[i].y << " ";
                    out << new_verts[i].z << " ";

                    if (mesh && use_normals)
                    {
                        out << normals[i].x << " ";
                        out << normals[i].y << " ";
                        out << normals[i].z << " ";
                    }

                    if (use_texcoords)
                    {
                        out << unsigned(new_tex[i][0]) << " ";
                        out << unsigned(new_tex[i][1]) << " ";
                        out << unsigned(new_tex[i][2]) << " ";
                    }
                    out << "\n";

                }
                if (mesh)
                {
                    auto size = faces.size();
                    for (size_t i = 0; i < size; ++i) {
                        int three = 3;
                        out << three << " ";
                        out << std::get<0>(faces[i]) << " ";
                        out << std::get<1>(faces[i]) << " ";
                        out << std::get<2>(faces[i]) << " ";
                        out << "\n";
                    }
                }
            }
        }

        std::array<uint8_t, 3> get_texcolor(const video_frame& texture, const uint8_t* texture_data, float u, float v)
        {
            const int w = texture.get_width(), h = texture.get_height();
            int x = std::min(std::max(int(u*w + .5f), 0), w - 1);
            int y = std::min(std::max(int(v*h + .5f), 0), h - 1);
            int idx = x * texture.get_bytes_per_pixel() + y * texture.get_stride_in_bytes();
            return { texture_data[idx], texture_data[idx + 1], texture_data[idx + 2] };
        }

        std::string fname;
        pointcloud _pc;
    };

    class save_single_frameset : public filter {
    public:
        save_single_frameset(std::string filename = "RealSense Frameset ")
            : filter([this](frame f, frame_source& s) { save(f, s); }), fname(filename)
        {}

    private:
        void save(frame data, frame_source& source, bool do_signal=true)
        {
            software_device dev;

            std::vector<std::tuple<software_sensor, stream_profile, int>> sensors;
            std::vector<std::tuple<stream_profile, stream_profile>> extrinsics;

            if (auto fs = data.as<frameset>()) {
                for (int i = 0; size_t(i) < fs.size(); ++i) {
                    frame f = fs[i];
                    auto profile = f.get_profile();
                    std::stringstream sname;
                    sname << "Sensor (" << i << ")";
                    auto s = dev.add_sensor(sname.str());
                    stream_profile software_profile;

                    if (auto vf = f.as<video_frame>()) {
                        auto vp = profile.as<video_stream_profile>();
                        rs2_video_stream stream{ vp.stream_type(), vp.stream_index(), i, vp.width(), vp.height(), vp.fps(), vf.get_bytes_per_pixel(), vp.format(), vp.get_intrinsics() };
                        software_profile = s.add_video_stream(stream);
                        if (f.is<rs2::depth_frame>()) {
                            auto ds = sensor_from_frame(f)->as<rs2::depth_sensor>();
                            s.add_read_only_option(RS2_OPTION_DEPTH_UNITS, ds.get_option(RS2_OPTION_DEPTH_UNITS));
                        }
                    } else if (f.is<motion_frame>()) {
                        auto mp = profile.as<motion_stream_profile>();
                        rs2_motion_stream stream{ mp.stream_type(), mp.stream_index(), i, mp.fps(), mp.format(), mp.get_motion_intrinsics() };
                        software_profile = s.add_motion_stream(stream);
                    } else if (f.is<pose_frame>()) {
                        rs2_pose_stream stream{ profile.stream_type(), profile.stream_index(), i, profile.fps(), profile.format() };
                        software_profile = s.add_pose_stream(stream);
                    } else {
                        // TODO: How to handle other frame types? (e.g. points)
                        assert(false);
                    }
                    sensors.emplace_back(s, software_profile, i);

                    bool found_extrin = false;
                    for (auto& root : extrinsics) {
                        try {
                            std::get<0>(root).register_extrinsics_to(software_profile,
                                std::get<1>(root).get_extrinsics_to(profile)
                            );
                            found_extrin = true;
                            break;
                        } catch (...) {}
                    }
                    if (!found_extrin) {
                        extrinsics.emplace_back(software_profile, profile);
                    }
                }


                // Recorder needs sensors to already exist when its created
                std::stringstream name;
                name << fname << data.get_frame_number() << ".bag";
                recorder rec(name.str(), dev);

                for (auto group : sensors) {
                    auto s = std::get<0>(group);
                    auto profile = std::get<1>(group);
                    s.open(profile);
                    s.start([](frame) {});
                    frame f = fs[std::get<2>(group)];
                    if (auto vf = f.as<video_frame>()) {
                        s.on_video_frame({ const_cast<void*>(vf.get_data()), [](void*) {}, vf.get_stride_in_bytes(), vf.get_bytes_per_pixel(),
                                           vf.get_timestamp(), vf.get_frame_timestamp_domain(), static_cast<int>(vf.get_frame_number()), profile });
                    } else if (f.is<motion_frame>()) {
                        s.on_motion_frame({ const_cast<void*>(f.get_data()), [](void*) {}, f.get_timestamp(),
                                            f.get_frame_timestamp_domain(), static_cast<int>(f.get_frame_number()), profile });
                    } else if (f.is<pose_frame>()) {
                        s.on_pose_frame({ const_cast<void*>(f.get_data()), [](void*) {}, f.get_timestamp(),
                                          f.get_frame_timestamp_domain(), static_cast<int>(f.get_frame_number()), profile });
                    }
                    s.stop();
                    s.close();
                }
            } else {
                // single frame
                auto set = source.allocate_composite_frame({ data });
                save(set, source, false);
            }

            if (do_signal)
                source.frame_ready(data);
        }

        std::string fname;
    };
}

#endif
