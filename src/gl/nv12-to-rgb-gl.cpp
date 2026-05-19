// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include <librealsense2-gl/rs_processing_gl.hpp>

#include "../proc/synthetic-stream.h"
#include "nv12-to-rgb-gl.h"
#include "../option.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>

#include "synthetic-stream-gl.h"

// NV12 layout (uploaded as GL_R8, width x tex_height where tex_height = height * 1.5):
//   rows 0..height-1            : Y plane (one Y per pixel, full resolution)
//   rows height..height*1.5-1   : interleaved UV plane (U0 V0 U1 V1 ... half vertical resolution)
//   Each UV pair covers a 2x2 block of Y pixels.
static const char* fragment_shader_text =
"#version 110\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform float width;\n"
"uniform float height;\n"
"void main(void) {\n"
"    float tex_h = height * 1.5;\n"
"    float px = floor(gl_FragCoord.x);\n"
"    float py = floor(gl_FragCoord.y);\n"
"    float y_row = py;\n"
"    float uv_row = height + floor(py / 2.0);\n"
"    float y = texture2D(textureSampler, vec2((px + 0.5) / width, (y_row + 0.5) / tex_h)).r;\n"
"    float u_col = floor(px / 2.0) * 2.0;\n"
"    float v_col = u_col + 1.0;\n"
"    float u = texture2D(textureSampler, vec2((u_col + 0.5) / width, (uv_row + 0.5) / tex_h)).r;\n"
"    float v = texture2D(textureSampler, vec2((v_col + 0.5) / width, (uv_row + 0.5) / tex_h)).r;\n"
"    vec3 color;\n"
"    color.r = clamp(y + 1.40200 * (v - 0.5), 0.0, 1.0);\n"
"    color.g = clamp(y - 0.34414 * (u - 0.5) - 0.71414 * (v - 0.5), 0.0, 1.0);\n"
"    color.b = clamp(y + 1.77200 * (u - 0.5), 0.0, 1.0);\n"
"    gl_FragColor = vec4(color, opacity);\n"
"}";

using namespace rs2;
using namespace librealsense::gl;

class nv12_to_rgb_shader : public texture_2d_shader
{
public:
    nv12_to_rgb_shader()
        : texture_2d_shader(shader_program::load(
            texture_2d_shader::default_vertex_shader(),
            fragment_shader_text, "position", "textureCoords"))
    {
        _width_location = _shader->get_uniform_location("width");
        _height_location = _shader->get_uniform_location("height");
    }

    void set_size(int w, int h)
    {
        _shader->load_uniform(_width_location, (float)w);
        _shader->load_uniform(_height_location, (float)h);
    }

private:
    uint32_t _width_location;
    uint32_t _height_location;
};

void nv12_to_rgb::cleanup_gpu_resources()
{
    _viz.reset();
    _fbo.reset();
    _enabled = 0;
}

void nv12_to_rgb::create_gpu_resources()
{
    _viz = std::make_shared<visualizer_2d>(std::make_shared<nv12_to_rgb_shader>());
    _fbo = std::make_shared<fbo>(_width, _height);
    _enabled = glsl_enabled() ? 1 : 0;
}

nv12_to_rgb::nv12_to_rgb()
    : stream_filter_processing_block("NV12 Converter (GLSL)")
{
    _source.add_extension<gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);

    auto opt = std::make_shared<librealsense::ptr_option<int>>(
        0, 1, 0, 1, &_enabled, "GLSL enabled");
    register_option(RS2_OPTION_COUNT, opt);

    initialize();
}

nv12_to_rgb::~nv12_to_rgb()
{
    try {
        perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            }, [] {});
    }
    catch (...)
    {
    }
}

rs2::frame nv12_to_rgb::process_frame(const rs2::frame_source& src, const rs2::frame& f)
{
    if (f.get_profile().get() != _input_profile.get())
    {
        _input_profile = f.get_profile();
        _output_profile = _input_profile.clone(_input_profile.stream_type(),
                                            _input_profile.stream_index(),
                                            RS2_FORMAT_RGB8);
        auto vp = _input_profile.as<rs2::video_stream_profile>();
        _width = vp.width(); _height = vp.height();

        perform_gl_action([&]()
        {
            _fbo = std::make_shared<fbo>(_width, _height);
        }, [this] {
            _enabled = false;
        });
    }

    rs2::frame res = f;

    perform_gl_action([&]()
    {
        res = src.allocate_video_frame(_output_profile, f, 3, _width, _height, _width * 3, RS2_EXTENSION_VIDEO_FRAME_GL);
        if (!res) return;

        auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)res.get());
        if (!gf)
            throw invalid_value_exception("dynamic_cast to gpu_addon_interface returned null.");

        // NV12 is 12bpp: upload as single-channel R8 texture, width x (height * 3/2)
        int tex_h = _height * 3 / 2;
        uint32_t input_texture;

        if (auto input_frame = f.as<rs2::gl::gpu_frame>())
        {
            input_texture = input_frame.get_texture_id(0);
        }
        else
        {
            glGenTextures(1, &input_texture);
            glBindTexture(GL_TEXTURE_2D, input_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, _width, tex_h, 0, GL_RED, GL_UNSIGNED_BYTE, f.get_data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }

        uint32_t output_rgb;
        gf->get_gpu_section().output_texture(0, &output_rgb, TEXTYPE_RGB);
        glBindTexture(GL_TEXTURE_2D, output_rgb);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        gf->get_gpu_section().set_size(_width, _height);

        glBindFramebuffer(GL_FRAMEBUFFER, _fbo->get());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glBindTexture(GL_TEXTURE_2D, output_rgb);
        _fbo->createTextureAttachment(output_rgb);

        _fbo->bind();
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto& shader = (nv12_to_rgb_shader&)_viz->get_shader();
        shader.begin();
        shader.set_size(_width, _height);
        shader.end();

        _viz->draw_texture(input_texture);

        _fbo->unbind();

        glBindTexture(GL_TEXTURE_2D, 0);

        if (!f.is<rs2::gl::gpu_frame>())
        {
            glDeleteTextures(1, &input_texture);
        }
    },
    [this]{
        _enabled = false;
    });

    return res;
}
