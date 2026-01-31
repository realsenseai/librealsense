// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>

#include "synthetic-stream-gl.h"
#include "environment.h"
#include "../proc/occlusion-filter.h"
#include "pointcloud-gl.h"
#include "option.h"
#include "environment.h"
#include "stream.h"
#include <src/pose.h>

#include <iostream>
#include <chrono>

#include "opengl3.h"
#include <glad/glad.h>

using namespace rs2;
using namespace librealsense;
using namespace librealsense::gl;

static const char* extract_points_text =
"#version 130\n"
"in vec2 textCoords;\n"
"out vec4 output_xyz;\n"
"uniform sampler2D textureSampler;\n"
"uniform vec2 focal;\n"
"uniform vec2 principal;\n"
"uniform float is_bc;\n"
"uniform float coeffs[5];\n"
"\n"
"uniform float depth_scale;\n"
"uniform float width;\n"
"uniform float height;\n"
"\n"
"void main(void) \n"
"{\n"
"    float px = textCoords.x * width;\n"
"    float py = (1.0 - textCoords.y) * height;\n"
"    float x = (px - principal.x) / focal.x;\n"
"    float y = (py - principal.y) / focal.y;\n"
"    float xo = x;\n"
"    float yo = y;\n"
"    if(is_bc == 2.0)\n"
"    {\n"
"       for (int i = 0; i < 10; i++)\n"
"       {\n"
"           float r2 = x * x + y * y;\n"
"           float icdist = 1.0 / (1.0 + ((coeffs[4] * r2 + coeffs[1])*r2 + coeffs[0])*r2);\n"
"           float xq = x / icdist;\n"
"           float yq = y / icdist;\n"
"           float delta_x = 2 * coeffs[2] * xq*yq + coeffs[3] * (r2 + 2 * xq*xq);\n"
"           float delta_y = 2 * coeffs[3] * xq*yq + coeffs[2] * (r2 + 2 * yq*yq);\n"
"           x = (xo - delta_x)*icdist;\n"
"           y = (yo - delta_y)*icdist;\n"
"       }\n"
"    }\n"
"    if (is_bc == 4.0)\n"
"    {\n"
"        for (int i = 0; i < 10; i++)\n"
"        {\n"
"            float r2 = x * x + y * y;\n"
"            float icdist = 1.0 / (1.0 + ((coeffs[4] * r2 + coeffs[1])*r2 + coeffs[0])*r2);\n"
"            float delta_x = 2 * coeffs[2] * x*y + coeffs[3] * (r2 + 2 * x*x);\n"
"            float delta_y = 2 * coeffs[3] * x*y + coeffs[2] * (r2 + 2 * y*y);\n"
"            x = (xo - delta_x)*icdist;\n"
"            y = (yo - delta_y)*icdist;\n"
"        }\n"
"    }\n"
"    vec2 tex = vec2(textCoords.x, 1.0 - textCoords.y);\n"
"    vec4 dp = texture(textureSampler, tex);\n"
"    float nd = (dp.x + dp.y * 256.0) * 256.0;\n"
"    float depth = depth_scale * nd;\n"
"    vec4 xyz = vec4(x * depth, y * depth, depth, 1.0);\n"
"    output_xyz = xyz;\n"
"}";

static const char* project_fragment_text =
"#version 130\n"
"in vec2 textCoords;\n"
"out vec4 output_xyz;\n"
"out vec4 output_uv;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform mat4 extrinsics;\n"
""
"uniform vec2 focal1;\n"
"uniform vec2 principal1;\n"
"uniform float is_bc1;\n"
"uniform float coeffs1[5];\n"
""
"uniform vec2 focal2;\n"
"uniform vec2 principal2;\n"
"uniform float is_bc2;\n"
"uniform float coeffs2[5];\n"
""
"uniform float depth_scale;\n"
"uniform float width1;\n"
"uniform float height1;\n"
"uniform float width2;\n"
"uniform float height2;\n"
"\n"
"uniform float needs_projection;\n"
"\n"
"void main(void) {\n"
"    float px = textCoords.x * width1;\n"
"    float py = (1.0 - textCoords.y) * height1;\n"
"    float x = (px - principal1.x) / focal1.x;\n"
"    float y = (py - principal1.y) / focal1.y;\n"
"    float xo = x;\n"
"    float yo = y;\n"
"    if(is_bc1 == 2.0)\n"
"    {\n"
"       for (int i = 0; i < 10; i++)\n"
"       {\n"
"           float r2 = x * x + y * y;\n"
"           float icdist = 1.0 / (1.0 + ((coeffs1[4] * r2 + coeffs1[1])*r2 + coeffs1[0])*r2);\n"
"           float xq = x / icdist;\n"
"           float yq = y / icdist;\n"
"           float delta_x = 2 * coeffs1[2] * xq*yq + coeffs1[3] * (r2 + 2 * xq*xq);\n"
"           float delta_y = 2 * coeffs1[3] * xq*yq + coeffs1[2] * (r2 + 2 * yq*yq);\n"
"           x = (xo - delta_x)*icdist;\n"
"           y = (yo - delta_y)*icdist;\n"
"       }\n"
"    }\n"
"    if (is_bc1 == 4.0)\n"
"    {\n"
"        for (int i = 0; i < 10; i++)\n"
"        {\n"
"            float r2 = x * x + y * y;\n"
"            float icdist = 1.0 / (1.0 + ((coeffs1[4] * r2 + coeffs1[1])*r2 + coeffs1[0])*r2);\n"
"            float delta_x = 2 * coeffs1[2] * x*y + coeffs1[3] * (r2 + 2 * x*x);\n"
"            float delta_y = 2 * coeffs1[3] * x*y + coeffs1[2] * (r2 + 2 * y*y);\n"
"            x = (xo - delta_x)*icdist;\n"
"            y = (yo - delta_y)*icdist;\n"
"        }\n"
"    }\n"
"    vec2 tex = vec2(textCoords.x, 1.0 - textCoords.y);\n"
"    vec4 dp = texture(textureSampler, tex);\n"
"    float nd = (dp.x + dp.y * 256.0) * 256.0;\n"
"    float depth = depth_scale * nd;\n"
"    vec4 xyz = vec4(x * depth, y * depth, depth, 1.0);\n"
"    output_xyz = xyz;\n"
""
"    if (needs_projection > 0) {"
"    vec4 trans = extrinsics * xyz;\n"
"    x = trans.x / trans.z;\n"
"    y = trans.y / trans.z;\n"
"\n"
"    if(is_bc2 == 2.0)\n"
"    {\n"
"        float r2  = x*x + y*y;\n"
"        float f = 1.0 + coeffs2[0]*r2 + coeffs2[1]*r2*r2 + coeffs2[4]*r2*r2*r2;\n"
"        x *= f;\n"
"        y *= f;\n"
"        float dx = x + 2.0*coeffs2[2]*x*y + coeffs2[3]*(r2 + 2.0*x*x);\n"
"        float dy = y + 2.0*coeffs2[3]*x*y + coeffs2[2]*(r2 + 2.0*y*y);\n"
"        x = dx;\n"
"        y = dy;\n"
"    }\n"
"    if (is_bc2 == 4.0)\n"
"    {\n"
"        float r2 = x * x + y * y;\n"
"        float f = 1 + coeffs2[0] * r2 + coeffs2[1] * r2*r2 + coeffs2[4] * r2*r2*r2;\n"
"        float xf = x * f;\n"
"        float yf = y * f;\n"
"        float dx = xf + 2 * coeffs2[2] * x*y + coeffs2[3] * (r2 + 2 * x*x);\n"
"        float dy = yf + 2 * coeffs2[3] * x*y + coeffs2[2] * (r2 + 2 * y*y);\n"
"        x = dx;\n"
"        y = dy;\n"
"    }\n"
"    // TODO: Enable F-Thetha\n"
"    //if (intrin->model == RS2_DISTORTION_FTHETA)\n"
"    //{\n"
"    //    float r = sqrtf(x*x + y*y);\n"
"    //    float rd = (float)(1.0f / intrin->coeffs[0] * atan(2 * r* tan(intrin->coeffs[0] / 2.0f)));\n"
"    //    x *= rd / r;\n"
"    //    y *= rd / r;\n"
"    //}\n"
"\n"
"    float u = (x * focal2.x + principal2.x) / width2;\n"
"    float v = (y * focal2.y + principal2.y) / height2;\n"
"    output_uv = vec4(u, v, 0.0, 1.0);\n"
"    } else {\n"
"       output_uv = vec4(textCoords.x, 1.0 - textCoords.y, 0.0, 1.0);\n"
"    }\n"
"}";

static const char* occulution_vertex_shader_text =
"#version 130\n"
"attribute vec3 position;\n"
"attribute vec2 textureCoords;\n"
"varying vec2 textCoords;\n"
"varying vec2 occuTextureCoords[10];\n"
"uniform vec2 elementPosition;\n"
"uniform vec2 elementScale;\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform int vscan;\n"
"void main(void)\n"
"{\n"
"    gl_Position = vec4(position * vec3(elementScale, 1.0) + vec3(elementPosition, 0.0), 1.0);\n"
"    textCoords = textureCoords;\n"
"    float pixelsize = 1.0 / width;\n"
"    float shift = 0.0;\n"
"    for (int i = 0; i < 10; i++)\n"
"    {\n"
"        if(vscan > 0)\n"
"        {\n"
"            occuTextureCoords[i] = textureCoords + vec2(0.0, shift);\n"
"            pixelsize = 2.0 / height;\n"
"        } else {\n"
"            occuTextureCoords[i] = textureCoords - vec2(shift, 0.0);\n"
"            pixelsize = 2.0 / width;\n"
"        }\n"
"        shift += pixelsize;\n"
"    }\n"
"}";

static const char* occulution_fragment_text =
"#version 130\n"
"varying vec2 textCoords;\n"
"varying vec2 occuTextureCoords[10];\n"
"out vec4 texture_xyz;\n"
"out vec4 texture_uv;\n"
"uniform sampler2D xyzSampler;\n"
"uniform sampler2D uvSampler;\n"
"uniform float opacity;\n"
"uniform int vscan;\n"
"void main(void) {\n"
"    vec4 xyz[10];\n"
"    vec4 uv[10];\n"
"    float uvmax = 0.0;\n"
"        if(vscan > 0)\n"
"        {\n"
"    for (int i = 0; i < 10; i++)\n"
"    {\n"
"    vec2 tex = vec2(occuTextureCoords[i].x, 1.0 - occuTextureCoords[i].y);\n"
"    xyz[i] = texture2D(xyzSampler, tex);\n"
"    uv[i] = texture2D(uvSampler, tex);\n"
"    if (uv[i].y > uvmax && xyz[i].z > 0.0)\n"
"    {\n"
"      uvmax = uv[i].y;\n"
"    }\n"
"    }\n"
"    if (xyz[0].z > 0.0)\n"
"    {\n"
"    if (uv[0].y < uvmax)\n"
"    {\n"
"    texture_xyz = vec4(0.0, 0.0, 0.0, 1.0);\n"
"    texture_uv = vec4(0.0, 0.0, 0.0, 1.0);\n"
"    } else {\n"
"    texture_xyz = xyz[0];\n"
"    texture_uv = uv[0];\n"
"    }\n"
"    }\n"
"    else {\n"
"    texture_xyz = xyz[0];\n"
"    texture_uv = uv[0];\n"
"    }\n"
"    } else {\n"
"    for (int i = 0; i < 10; i++)\n"
"    {\n"
"    vec2 tex = vec2(occuTextureCoords[i].x, 1.0 - occuTextureCoords[i].y);\n"
"    xyz[i] = texture2D(xyzSampler, tex);\n"
"    uv[i] = texture2D(uvSampler, tex);\n"
"    if (uv[i].x > uvmax && xyz[i].z > 0.0)\n"
"    {\n"
"      uvmax = uv[i].x;\n"
"    }\n"
"    }\n"
"    if (xyz[0].z > 0.0)\n"
"    {\n"
"    if (uv[0].x < uvmax)\n"
"    {\n"
"    texture_xyz = vec4(0.0, 0.0, 0.0, 1.0);\n"
"    texture_uv = vec4(0.0, 0.0, 0.0, 1.0);\n"
"    } else {\n"
"    texture_xyz = xyz[0];\n"
"    texture_uv = uv[0];\n"
"    }\n"
"    }\n"
"    else {\n"
"    texture_xyz = xyz[0];\n"
"    texture_uv = uv[0];\n"
"    }\n"
"    }\n"
"}";

class extract_points_shader : public texture_2d_shader
{
public:
    extract_points_shader()
        : texture_2d_shader(shader_program::load(
            texture_2d_shader::default_vertex_shader(),
            extract_points_text,
            "position", "textureCoords",
            "output_xyz", nullptr))
    {
        _focal_location = _shader->get_uniform_location("focal");
        _principal_location = _shader->get_uniform_location("principal");
        _is_bc_location = _shader->get_uniform_location("is_bc");
        _coeffs_location = _shader->get_uniform_location("coeffs");

        _depth_scale_location = _shader->get_uniform_location("depth_scale");
        _width_location = _shader->get_uniform_location("width");
        _height_location = _shader->get_uniform_location("height");
    }

    void set_size(int w, int h)
    {
        _shader->load_uniform(_width_location, (float)w);
        _shader->load_uniform(_height_location, (float)h);
    }

    void set_intrinsics(const rs2_intrinsics& intr)
    {
        rs2::float2 focal{ intr.fx, intr.fy };
        rs2::float2 principal{ intr.ppx, intr.ppy };
        _shader->load_uniform(_focal_location, focal);
        _shader->load_uniform(_principal_location, principal);
        _shader->load_uniform(_is_bc_location, (float)(int)intr.model);
        glUniform1fv(_coeffs_location, 5, intr.coeffs);
    }

    void set_depth_scale(float depth_scale)
    {
        _shader->load_uniform(_depth_scale_location, depth_scale);
    }

private:
    int _focal_location;
    int _principal_location;
    int _is_bc_location;
    int _coeffs_location;
    int _depth_scale_location;

    int _width_location;
    int _height_location;
};

class project_shader : public texture_2d_shader
{
public:
    project_shader()
        : texture_2d_shader(shader_program::load(
            texture_2d_shader::default_vertex_shader(), 
            project_fragment_text, 
            "position", "textureCoords", 
            "output_xyz", "output_uv"))
    {
        _focal_location[0] = _shader->get_uniform_location("focal1");
        _principal_location[0] = _shader->get_uniform_location("principal1");
        _is_bc_location[0] = _shader->get_uniform_location("is_bc1");
        _coeffs_location[0] = _shader->get_uniform_location("coeffs1");

        _focal_location[1] = _shader->get_uniform_location("focal2");
        _principal_location[1] = _shader->get_uniform_location("principal2");
        _is_bc_location[1] = _shader->get_uniform_location("is_bc2");
        _coeffs_location[1] = _shader->get_uniform_location("coeffs2");

        _depth_scale_location = _shader->get_uniform_location("depth_scale");
        _width_location[0] = _shader->get_uniform_location("width1");
        _height_location[0] = _shader->get_uniform_location("height1");
        _width_location[1] = _shader->get_uniform_location("width2");
        _height_location[1] = _shader->get_uniform_location("height2");
        _extrinsics_location = _shader->get_uniform_location("extrinsics");

        _requires_projection_location = _shader->get_uniform_location("needs_projection");
    }

    void requires_projection(bool val)
    {
        _shader->load_uniform(_requires_projection_location, val ? 1.f : 0.f);
    }

    void set_size(int id, int w, int h)
    {
        _shader->load_uniform(_width_location[id], (float)w);
        _shader->load_uniform(_height_location[id], (float)h);
    }

    void set_intrinsics(int idx, const rs2_intrinsics& intr)
    {
        rs2::float2 focal{ intr.fx, intr.fy };
        rs2::float2 principal{ intr.ppx, intr.ppy };
        _shader->load_uniform(_focal_location[idx], focal);
        _shader->load_uniform(_principal_location[idx], principal);
        _shader->load_uniform(_is_bc_location[idx], (float)(int)intr.model);
        glUniform1fv(_coeffs_location[idx], 5, intr.coeffs);
    }

    void set_depth_scale(float depth_scale)
    {
        _shader->load_uniform(_depth_scale_location, depth_scale);
    }

    void set_extrinsics(const rs2_extrinsics& extr)
    {
        rs2::matrix4 m;
        for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            if (i < 3 && j < 3) m.mat[i][j] = extr.rotation[i * 3 + j];
            else if (i == 3 && j < 3) m.mat[i][j] = extr.translation[j];
            else if (i < 3 && j == 3) m.mat[i][j] = 0.f;
            else m.mat[i][j] = 1.f;
        }
        _shader->load_uniform(_extrinsics_location, m);
    }
private:
    uint32_t _focal_location[2];
    uint32_t _principal_location[2];
    uint32_t _is_bc_location[2];
    uint32_t _coeffs_location[2];
    uint32_t _depth_scale_location;

    uint32_t _width_location[2];
    uint32_t _height_location[2];

    uint32_t _extrinsics_location;

    uint32_t _requires_projection_location;
};

class occulution_shader : public texture_2d_shader
{
public:
    occulution_shader()
        : texture_2d_shader(shader_program::load(
            occulution_vertex_shader_text,
            occulution_fragment_text,
            "position", "textureCoords",
            "texture_xyz", "texture_uv"))
    {
        _width_location = _shader->get_uniform_location("width");
        _height_location = _shader->get_uniform_location("height");

        _xyz_sampler_location = _shader->get_uniform_location("xyzSampler");
        _uv_sampler_location = _shader->get_uniform_location("uvSampler");

        _scanning_location = _shader->get_uniform_location("vscan");
    }

    void set_width(float width)
    {
        _shader->load_uniform(_width_location, width);
    }

    void set_height(float height)
    {
        _shader->load_uniform(_height_location, height);
    }

    void set_xyz_sampler(int xyz)
    {
        _shader->load_uniform(_xyz_sampler_location, xyz);
    }

    void set_uv_sampler(int uv)
    {
        _shader->load_uniform(_uv_sampler_location, uv);
    }

    void set_scanning(int mode)
    {
        _shader->load_uniform(_scanning_location, mode);
    }
private:
    uint32_t _width_location;
    uint32_t _height_location;

    uint32_t _xyz_sampler_location;
    uint32_t _uv_sampler_location;

    uint32_t _scanning_location;
};

void pointcloud_gl::cleanup_gpu_resources()
{
    _projection_renderer.reset();
    _occu_renderer.reset();
    _point_cloud_processor.reset();
    _enabled = 0;
}
void pointcloud_gl::create_gpu_resources()
{
    if (glsl_enabled())
    {
        _projection_renderer = std::make_shared<visualizer_2d>(std::make_shared<project_shader>());
        _occu_renderer = std::make_shared<visualizer_2d>(std::make_shared<occulution_shader>());
        _point_cloud_processor = std::make_shared<visualizer_2d>(std::make_shared<extract_points_shader>());
    }
    _enabled = glsl_enabled() ? 1 : 0;
}

pointcloud_gl::~pointcloud_gl()
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

pointcloud_gl::pointcloud_gl()
    : pointcloud("Pointcloud (GLSL)"), _depth_data(rs2::frame{})
{
    _source.add_extension<gl::gpu_points_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);

    auto opt = std::make_shared<librealsense::ptr_option<int>>(
        0, 1, 0, 1, &_enabled, "GLSL enabled"); 
    register_option(RS2_OPTION_COUNT, opt);

    initialize();
}

const librealsense::float3* pointcloud_gl::depth_to_points(
        rs2::points output,
        const rs2_intrinsics &depth_intrinsics, 
        const rs2::depth_frame& depth_frame)
{
    perform_gl_action([&]{
        _depth_data = depth_frame;
        _depth_scale = depth_frame.get_units();
        _depth_intr = depth_intrinsics;

        //if map_texture is false (e.g. headless operation)
        if(!_extrinsics.has_value() || !_other_intrinsics.has_value())
        {
          auto vid_frame = depth_frame.as<rs2::video_frame>();
          auto height = vid_frame.get_height();
          auto width = vid_frame.get_width();
          get_gl_points(output, width, height);
        }
        //else, defer point extraction as it will be calculated along with texture mapping process
    }, [&]{
        _enabled = false;
    });
    return nullptr;
}

void pointcloud_gl::get_gl_points(rs2::points& output, const unsigned int width, const unsigned int height)
{
    auto viz = _point_cloud_processor;
    auto frame_ref = (frame_interface*)output.get();

    auto gf = dynamic_cast<gpu_addon_interface*>(frame_ref);

    if (!gf)
        throw std::runtime_error("Frame interface is not gpu addon interface");

    uint32_t depth_texture;

    if (auto input_frame = _depth_data.as<rs2::gl::gpu_frame>())
    {
        depth_texture = input_frame.get_texture_id(0);
    }
    else
    {
        glGenTextures(1, &depth_texture);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        auto depth_data = _depth_data.get_data();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, depth_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    // rendering output
    uint32_t output_xyz;

    gf->get_gpu_section().output_texture(0, &output_xyz, TEXTYPE_XYZ);

    // fbo1 output
    uint32_t fbo1_xyz = output_xyz;

    // fbo1 - projection rendering
    fbo fbo1(width, height);

    glBindTexture(GL_TEXTURE_2D, fbo1_xyz);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo1_xyz, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    gf->get_gpu_section().set_size(width, height);

    fbo1.bind();

    GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    auto& shader = (extract_points_shader&)viz->get_shader();
    shader.begin();
    shader.set_depth_scale(_depth_scale);
    shader.set_intrinsics(_depth_intr);
    shader.set_size(width, height);

    viz->draw_texture(depth_texture);
    shader.end();

    fbo1.unbind();

    glBindTexture(GL_TEXTURE_2D, 0);
    if (!_depth_data.is<rs2::gl::gpu_frame>())
    {
        glDeleteTextures(1, &depth_texture);
    }
}

void pointcloud_gl::get_texture_map(
    rs2::points output,
    const librealsense::float3* points,
    const unsigned int width,
    const unsigned int height,
    const rs2_intrinsics &other_intrinsics,
    const rs2_extrinsics& extr,
    librealsense::float2* pixels_ptr)
{
    perform_gl_action([&]{
        auto viz = _projection_renderer;
        auto frame_ref = (frame_interface*)output.get();

        auto gf = dynamic_cast<gpu_addon_interface*>(frame_ref);
        if (!gf)
            throw std::runtime_error("Frame interface is not gpu addon interface");

        uint32_t depth_texture;

        if (auto input_frame = _depth_data.as<rs2::gl::gpu_frame>())
        {
            depth_texture = input_frame.get_texture_id(0);
        }
        else
        {
            glGenTextures(1, &depth_texture);
            glBindTexture(GL_TEXTURE_2D, depth_texture);
            auto depth_data = _depth_data.get_data();
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, depth_data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }

        // rendering output
        uint32_t output_xyz;
        uint32_t output_uv;

        gf->get_gpu_section().output_texture(0, &output_xyz, TEXTYPE_XYZ);
        gf->get_gpu_section().output_texture(1, &output_uv, TEXTYPE_UV);

        // fbo1 output
        uint32_t fbo1_xyz;
        uint32_t fbo1_uv;

        // run glsl occlusion removal if filter is active and between different sensors
        bool run_glsl_occlusion_removal = _occlusion_filter->active() && !_occlusion_filter->is_same_sensor(extr);

        // when occlusion is turned on, output from fbo1 will be fed into fbo2 for additional processing before rendering to
        // to final output output_xyz and output_uv, otherwise, fbo1 renders directly to final output
        if (run_glsl_occlusion_removal)
        {
            glGenTextures(1, &fbo1_xyz);
            glGenTextures(1, &fbo1_uv);
        }
        else
        {
            fbo1_xyz = output_xyz;
            fbo1_uv = output_uv;
        }

        // fbo1 - projection rendering
        fbo fbo1(width, height);

        glBindTexture(GL_TEXTURE_2D, fbo1_xyz);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo1_xyz, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindTexture(GL_TEXTURE_2D, fbo1_uv);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, fbo1_uv, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        gf->get_gpu_section().set_size(width, height);

        fbo1.bind();
        
        GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto& shader = (project_shader&)viz->get_shader();
        shader.begin();

        shader.requires_projection(!(_depth_intr == other_intrinsics && 
            extr == identity_matrix()));

        shader.set_depth_scale(_depth_scale);
        shader.set_intrinsics(0, _depth_intr);
        shader.set_intrinsics(1, other_intrinsics);
        shader.set_extrinsics(extr);
        shader.set_size(0, width, height);
        shader.set_size(1, other_intrinsics.width, other_intrinsics.height);
        
        viz->draw_texture(depth_texture);
        shader.end();

        fbo1.unbind();

        // fbo2 - occlusion with glsl
        if (run_glsl_occlusion_removal)
        {
            auto oviz = _occu_renderer;

            fbo fbo2(width, height);

            glBindTexture(GL_TEXTURE_2D, output_xyz);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_xyz, 0);
            glBindTexture(GL_TEXTURE_2D, 0);

            glBindTexture(GL_TEXTURE_2D, output_uv);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, output_uv, 0);
            glBindTexture(GL_TEXTURE_2D, 0);

            glDrawBuffers(2, attachments);

            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            gf->get_gpu_section().set_size(width, height);

            fbo2.bind();

            auto& occu_shader = (occulution_shader&)oviz->get_shader();

            occu_shader.begin();

            occu_shader.set_width((float) width);
            occu_shader.set_height((float) height);

            occu_shader.set_xyz_sampler(0);
            occu_shader.set_uv_sampler(1);

            if (_occlusion_filter->find_scanning_direction(extr) == vertical)
            {
                // L500 - vertical scan
                occu_shader.set_scanning(1);
            }
            else
            {
                // D400 - horizontal scan
                occu_shader.set_scanning(0);
            }

            oviz->draw_texture(fbo1_xyz, fbo1_uv);
            occu_shader.end();

            fbo2.unbind();

            glDeleteTextures(1, &fbo1_xyz);
            glDeleteTextures(1, &fbo1_uv);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        if (!_depth_data.is<rs2::gl::gpu_frame>())
        {
            glDeleteTextures(1, &depth_texture);
        }
    }, [&]{
        _enabled = false;
    });
}

rs2::points pointcloud_gl::allocate_points(
    const rs2::frame_source& source, 
    const rs2::frame& f)
{
    auto prof = std::dynamic_pointer_cast<librealsense::stream_profile_interface>(
        _output_stream.get()->profile->shared_from_this());
    auto frame_ref = _source_wrapper.allocate_points(prof, (frame_interface*)f.get(),
        RS2_EXTENSION_VIDEO_FRAME_GL);
    rs2::frame res { (rs2_frame*)frame_ref };
    return res.as<rs2::points>();
}

bool pointcloud_gl::run__occlusion_filter(const rs2_extrinsics& extr)
{
    // skip cpu filter when occlusion removed on gpu
    return false;
}
