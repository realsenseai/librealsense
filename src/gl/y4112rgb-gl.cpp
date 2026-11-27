// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include <librealsense2-gl/rs_processing_gl.hpp>

#include "../proc/synthetic-stream.h"
#include "y4112rgb-gl.h"
#include "../option.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <glad/glad.h>
#include <iostream>
#include <chrono>
#include "synthetic-stream-gl.h"

static const char* fragment_shader_text =
"#version 110\n"
"varying vec2 textCoords;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform float width;\n"
"uniform float height;\n"
"void main(void) {\n"
"    float pixel_width = 1.0 / width;\n"
"    float pixel_height = 1.0 / height;\n"
"    float y = 0.0;\n"
"    float u = 0.0;\n"
"    float v = 0.0;\n"
"    float tex_y = 1.0 - textCoords.y;\n"
"    if (mod(floor(gl_FragCoord.y), 2.0) == 0.0)\n"
"    {\n"
"       if (mod(floor(gl_FragCoord.x), 2.0) == 0.0)\n"
"       {\n"
"          vec2 tuyy1 = vec2(textCoords.x, tex_y); \n"
"          vec4 puyy1 = texture2D(textureSampler, tuyy1);\n"
"          vec2 tvyy1 = vec2(textCoords.x + pixel_width, tex_y);\n"
"          vec4 pvyy1 = texture2D(textureSampler, tvyy1);\n"
"          y = puyy1.y; u = puyy1.x; v =pvyy1.x;\n"
"       }\n"
"       else\n"
"       {\n"
"          vec2 tuyy1 = vec2(textCoords.x - pixel_width, tex_y); \n"
"          vec4 puyy1 = texture2D(textureSampler, tuyy1);\n"
"          vec2 tvyy1 = vec2(textCoords.x, tex_y);\n"
"          vec4 pvyy1 = texture2D(textureSampler, tvyy1);\n"
"          y = puyy1.z; u = puyy1.x; v = pvyy1.x; \n"
"       }\n"
"    }\n"
"    else\n"
"    {\n"
"       if (mod(floor(gl_FragCoord.x), 2.0) == 0.0)\n"
"       {\n"
"          vec2 tuyy1 = vec2(textCoords.x, tex_y-pixel_height); \n"
"          vec4 puyy1 = texture2D(textureSampler, tuyy1);\n"
"          vec2 tvyy1 = vec2(textCoords.x + pixel_width, tex_y-pixel_height);\n"
"          vec4 pvyy1 = texture2D(textureSampler, tvyy1);\n"
"          y = pvyy1.y; u = puyy1.x; v =pvyy1.x;\n"
"       }\n"
"       else\n"
"       {\n"
"          vec2 tuyy1 = vec2(textCoords.x - pixel_width, tex_y-pixel_height); \n"
"          vec4 puyy1 = texture2D(textureSampler, tuyy1);\n"
"          vec2 tvyy1 = vec2(textCoords.x, tex_y-pixel_height);\n"
"          vec4 pvyy1 = texture2D(textureSampler, tvyy1);\n"
"          y = pvyy1.z; u = puyy1.x; v = pvyy1.x; \n"
"       }\n"
"    }\n"
"    float c = y - (16.0 / 256.0);\n"
"    float d = u - 0.5;\n"
"    float e = v - 0.5;\n"
"    vec3 color = vec3(0.0);\n"
"    color.x = clamp((y + 1.40200 * (v - 0.5)), 0.0, 1.0);\n"
"    color.y = clamp((y - 0.34414 * (u - 0.5) - 0.71414 * (v - 0.5)), 0.0, 1.0);\n"
"    color.z = clamp((y + 1.77200 * (u - 0.5)), 0.0, 1.0);\n"

"    gl_FragColor = vec4(color.xyz, opacity);\n"
"}";

using namespace rs2;
using namespace librealsense::gl;

class y411_2rgb_shader : public texture_2d_shader
{
public:
    y411_2rgb_shader()
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

void y411_2rgb::cleanup_gpu_resources()
{
    _viz.reset();
    _fbo.reset();
    _enabled = 0;
}

void y411_2rgb::create_gpu_resources()
{
    _viz = std::make_shared<visualizer_2d>(std::make_shared<y411_2rgb_shader>());
    _fbo = std::make_shared<fbo>(_width, _height);
    _enabled = glsl_enabled() ? 1 : 0;
}

y411_2rgb::y411_2rgb()
    : stream_filter_processing_block("Y411 Converter (GLSL)")
{
    _source.add_extension<gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);

    auto opt = std::make_shared<librealsense::ptr_option<int>>(
        0, 1, 0, 1, &_enabled, "GLSL enabled"); 
    register_option(RS2_OPTION_COUNT, opt);

    initialize();
}

y411_2rgb::~y411_2rgb()
{
    try
    {
        perform_gl_action( [&]()
        {
            cleanup_gpu_resources();
        }, [] {} );
    }
    catch(...)
    {
        LOG_DEBUG( "Error while cleaning up gpu resources" );
    }
}

rs2::frame y411_2rgb::process_frame(const rs2::frame_source& src, const rs2::frame& f)
{
    //scoped_timer t("yuy2rgb");

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
        //scoped_timer t("yuy2rgb.gl");

        res = src.allocate_video_frame(_output_profile, f, 3, _width, _height, _width * 3, RS2_EXTENSION_VIDEO_FRAME_GL);
        if (!res) return;
        
        auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)res.get());
        if (!gf)
            throw std::runtime_error("Frame interface is not gpu addon interface");
        
        uint32_t yuy_texture;
        temp_texture uploaded;
        
        if (auto input_frame = f.as<rs2::gl::gpu_frame>())
        {
            yuy_texture = input_frame.get_texture_id(0);
        }
        else
        {
            yuy_texture = uploaded.create();
            glBindTexture(GL_TEXTURE_2D, yuy_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height/2, 0, GL_RGB, GL_UNSIGNED_BYTE, f.get_data());
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

        auto& shader = (y411_2rgb_shader&)_viz->get_shader();
        shader.begin();
        shader.set_size(_width, _height);
        shader.end();
        
        _viz->draw_texture(yuy_texture);

        _fbo->unbind();

        glBindTexture(GL_TEXTURE_2D, 0);
    }, 
    [this]{
        _enabled = false;
    }); 

    return res;
}
