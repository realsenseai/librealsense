// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include <librealsense2-gl/rs_processing_gl.hpp>

#include "../proc/synthetic-stream.h"
#include "synthetic-stream-gl.h"
#include "../proc/disparity-transform.h"
#include "colorizer-gl.h"
#include "option.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <glad/glad.h>

#include <iostream>
#include <chrono>

static const char* fragment_shader_text =
"#version 110\n"
"varying vec2 textCoords;\n"
"uniform sampler2D textureSampler;\n"
"uniform sampler2D cmSampler;\n"
"uniform sampler2D histSampler;\n"
"uniform float opacity;\n"
"uniform float depth_units;\n"
"uniform float min_depth;\n"
"uniform float max_depth;\n"
"uniform float max_disparity;\n"
"uniform float equalize;\n"
"uniform float disparity;\n"
"void main(void) {\n"
"    vec2 tex = vec2(textCoords.x, 1.0 - textCoords.y);\n"
"    vec4 depth = texture2D(textureSampler, tex);\n"
"    float dx = depth.x;\n"
"    float dy = depth.y;\n"
"    float nd = dx + dy * 256.0;\n"
"    float d = 0.0;\n"
"    if (disparity > 0.0) {;\n"
"       d = dx;\n"
"    } else {\n"
"       d = nd * 256.0;\n"
"    }\n"
"    if (d > 0.0){\n"
"        float f = 0.0;\n"
"        if (equalize > 0.0){\n"
"            float x;\n"
"            float y;\n"
"            vec4 hist;\n"
"            if (disparity > 0.0) {;\n"
"               hist = texture2D(histSampler, vec2(d / max_disparity, 0.0));\n"
"            } else {\n"
"               x = dx * 0.99;\n"
"               y = dy + (1.0 / 256.0);\n"
"               hist = texture2D(histSampler, vec2(x, y));\n"
"            }\n"
"            f = hist.x;\n"
"        } else {\n"
"            if (disparity > 0.0) {\n"
"               f = ((d - min_depth) / (max_depth - min_depth));\n"
"            } else {\n"
"               f = (d * depth_units - min_depth) / (max_depth - min_depth);\n"
"            }\n"
"        }\n"
"        f = clamp(f, 0.0, 0.99);\n"
"        vec4 color = texture2D(cmSampler, vec2(f, 0.0));\n"
"        gl_FragColor = vec4(color.x / 256.0, color.y / 256.0, color.z / 256.0, opacity);\n"
"    } else {\n"
"        gl_FragColor = vec4(0.0, 0.0, 0.0, opacity);\n"
"    }\n"
"}";

using namespace rs2;
using namespace librealsense::gl;

class colorize_shader : public texture_2d_shader
{
public:
    colorize_shader()
        : texture_2d_shader(shader_program::load(
            texture_2d_shader::default_vertex_shader(), 
            fragment_shader_text, "position", "textureCoords"))
    {
        _depth_units_location = _shader->get_uniform_location("depth_units");
        _min_depth_location = _shader->get_uniform_location("min_depth");
        _max_depth_location = _shader->get_uniform_location("max_depth");
        _max_disparity_location = _shader->get_uniform_location("max_disparity");
        _equalize_location = _shader->get_uniform_location("equalize");
        _is_disparity_location = _shader->get_uniform_location("disparity");

        auto texture0_sampler_location = _shader->get_uniform_location("textureSampler");
        auto texture1_sampler_location = _shader->get_uniform_location("cmSampler");
        auto texture2_sampler_location = _shader->get_uniform_location("histSampler");

        _shader->begin();
        _shader->load_uniform(texture0_sampler_location, texture_slot());
        _shader->load_uniform(texture1_sampler_location, color_map_slot());
        _shader->load_uniform(texture2_sampler_location, histogram_slot());
        _shader->end();
    }

    int texture_slot() const { return 0; }
    int color_map_slot() const { return 1; }
    int histogram_slot() const { return 2; }

    void set_params(float units, float min, float max, float max_disparity, bool equalize, bool disparity)
    {
        _shader->load_uniform(_depth_units_location, units);
        _shader->load_uniform(_min_depth_location, min);
        _shader->load_uniform(_max_depth_location, max);
        _shader->load_uniform(_max_disparity_location, max_disparity);
        _shader->load_uniform(_equalize_location, equalize ? 1.f : 0.f);
        _shader->load_uniform(_is_disparity_location, disparity ? 1.f : 0.f);
    }

private:
    uint32_t _depth_units_location;
    uint32_t _min_depth_location;
    uint32_t _max_depth_location;
    uint32_t _max_disparity_location;
    uint32_t _equalize_location;
    uint32_t _is_disparity_location;
};

using namespace rs2;

namespace librealsense
{
    namespace gl
    {
        void colorizer::cleanup_gpu_resources()
        {
            _viz.reset();
            _fbo.reset();

            if (_cm_texture) glDeleteTextures(1, &_cm_texture);

            _enabled = 0;
        }

        void colorizer::create_gpu_resources()
        {
            _viz = std::make_shared<visualizer_2d>(std::make_shared<colorize_shader>());
            _fbo = std::make_shared<fbo>(_width, _height);

            glGenTextures(1, &_cm_texture);
            auto& curr_map = _maps[_map_index]->get_cache();
            _last_selected_cm = _map_index;
            auto size = static_cast<GLsizei>(curr_map.size());
            glBindTexture(GL_TEXTURE_2D, _cm_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size, 1, 0, GL_RGB, GL_FLOAT, curr_map.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            _enabled = glsl_enabled() ? 1 : 0;
        }

        colorizer::colorizer() 
            : librealsense::colorizer("Depth Visualization (GLSL)"), _cm_texture(0)
        {
            _fhist = std::vector<float>(MAX_DEPTH, 0);
            _fhist_data = _fhist.data();
            _source.add_extension<gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);

            auto opt = std::make_shared<librealsense::ptr_option<int>>(
                0, 1, 0, 1, &_enabled, "GLSL enabled"); 
            register_option(RS2_OPTION_COUNT, opt);

            initialize();
        }

        colorizer::~colorizer()
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
                LOG_DEBUG( "Error while performing cleaning up gpu resources" );
            }
        }

        void colorizer::populate_floating_histogram(float* f, int* hist)
        {
            float total = float(hist[MAX_DEPTH-1]);
            for (int i = 0; i < MAX_DEPTH; i++)
                f[i] = hist[i] / total;
        }

        rs2::frame colorizer::process_frame(const rs2::frame_source& src, const rs2::frame& f)
        {
            if(f.as<rs2::depth_frame>())
                _depth_units = ((depth_frame*)f.get())->get_units();
            if (f.get_profile().get() != _source_stream_profile.get())
            {
                _source_stream_profile = f.get_profile();
                _target_stream_profile = _source_stream_profile.clone(
                                            _source_stream_profile.stream_type(), 
                                            _source_stream_profile.stream_index(), 
                                            RS2_FORMAT_RGB8);
                auto vp = _source_stream_profile.as<rs2::video_stream_profile>();
                _width = vp.width(); _height = vp.height();

                auto info = disparity_info::update_info_from_frame(f);
                _d2d_convert_factor = info.d2d_convert_factor;

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
                //scoped_timer t("colorizer.gl");
                auto& curr_map = _maps[_map_index]->get_cache();

                if (_last_selected_cm != _map_index)
                {
                    glBindTexture(GL_TEXTURE_2D, _cm_texture);
                    auto size = static_cast<GLsizei>(curr_map.size());
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size, 1, 0, GL_RGB, GL_FLOAT, curr_map.data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                    _last_selected_cm = _map_index;
                }

                res = src.allocate_video_frame(_target_stream_profile, f, 3, _width, _height, _width * 3, RS2_EXTENSION_VIDEO_FRAME_GL);
                
                if (!res) return;

                auto fi = (frame_interface*)f.get();
                auto df = dynamic_cast<librealsense::depth_frame*>(fi);
                if (!df)
                    throw std::runtime_error("Frame interface is not depth frame");

                _depth_units = df->get_units();
                bool disparity = f.get_profile().format() == RS2_FORMAT_DISPARITY32 ? true : false;

                uint32_t depth_texture;
                uint32_t hist_texture = _cm_texture;
                bool generate_depth_texture = false;
                bool generate_histogram_texture = false;
                const void *frame_data = nullptr;

                if (auto input_frame = f.as<rs2::gl::gpu_frame>())
                {
                    depth_texture = input_frame.get_texture_id(0);

                    try
                    {
                        hist_texture = input_frame.get_texture_id(1);
                    }
                    catch(const std::exception& e)
                    {
                        LOG_DEBUG("Histogram Texture is not available. Generating one.");
                        generate_histogram_texture = true;
                        frame_data = input_frame.get_data();
                    }
                }
                else
                {
                    generate_depth_texture = true;
                    generate_histogram_texture = true;
                    frame_data = f.get_data();
                }
                if (generate_depth_texture)
                {
                    glGenTextures(1, &depth_texture);
                    glBindTexture(GL_TEXTURE_2D, depth_texture);

                    if (disparity)
                    {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, _width, _height, 0, GL_RED, GL_FLOAT, frame_data);
                    }
                    else
                    {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, _width, _height, 0, GL_RG, GL_UNSIGNED_BYTE, frame_data);
                    }

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                }
                if (generate_histogram_texture && _equalize)
                {
                    glGenTextures(1, &hist_texture);
                    glBindTexture(GL_TEXTURE_2D, hist_texture);

                    if (disparity)
                    {
                        update_histogram(_hist_data, reinterpret_cast<const float*>(frame_data), _width, _height);
                        populate_floating_histogram(_fhist_data, _hist_data);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, MAX_DISPARITY, 1, 0, GL_RED, GL_FLOAT, _fhist_data);
                    }
                    else
                    {
                        update_histogram(_hist_data, reinterpret_cast<const uint16_t*>(frame_data), _width, _height);
                        populate_floating_histogram(_fhist_data, _hist_data);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 0xFF, 0xFF, 0, GL_RED, GL_FLOAT, _fhist_data);
                    }

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                }
                
                if( auto gf = dynamic_cast< gpu_addon_interface * >( (frame_interface *)res.get() ) )
                {
                    uint32_t output_rgb = 0;
                    gf->get_gpu_section().output_texture( 0, &output_rgb, TEXTYPE_RGB );
                    glBindTexture( GL_TEXTURE_2D, output_rgb );
                    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );
                    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
                    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

                    gf->get_gpu_section().set_size( _width, _height );

                    glBindFramebuffer(GL_FRAMEBUFFER, _fbo->get());
                    glDrawBuffer(GL_COLOR_ATTACHMENT0);

                    glBindTexture(GL_TEXTURE_2D, output_rgb);
                    _fbo->createTextureAttachment(output_rgb);

                    _fbo->bind();
                    glClearColor(1, 0, 0, 1);
                    glClear(GL_COLOR_BUFFER_BIT);

                    auto& shader = (colorize_shader&)_viz->get_shader();
                    shader.begin();
                    float max = _max;;
                    float min = _min;;
                    if (disparity)
                    {
                        auto __min = _min;
                        if (__min < 1e-6f) { __min = 1e-6f; } // Min value set to prevent zero division. only when _min is zero. 
                        max = (_d2d_convert_factor / (__min)) * _depth_units + .5f;
                        min = (_d2d_convert_factor / (_max)) * _depth_units + .5f;
                    }
                    shader.set_params(_depth_units, min, max, MAX_DISPARITY, _equalize, disparity);
                    shader.end();

                    glActiveTexture(GL_TEXTURE0 + shader.histogram_slot());
                    glBindTexture(GL_TEXTURE_2D, hist_texture);

                    glActiveTexture(GL_TEXTURE0 + shader.color_map_slot());
                    glBindTexture(GL_TEXTURE_2D, _cm_texture);

                    glActiveTexture(GL_TEXTURE0 + shader.texture_slot());
                    glBindTexture(GL_TEXTURE_2D, depth_texture);

                    _viz->draw_texture(depth_texture);

                    glActiveTexture(GL_TEXTURE0 + shader.texture_slot());

                    _fbo->unbind();

                    glBindTexture(GL_TEXTURE_2D, 0);

                    if (!f.is<rs2::gl::gpu_frame>())
                    {
                        if (_equalize)
                        {
                            glDeleteTextures(1, &hist_texture);
                        }
                        glDeleteTextures(1, &depth_texture);
                    }
                }

            }, 
            [this]{
                _enabled = false;
            }); 

            return res;
        }
    }
}
