// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include <librealsense2-gl/rs_processing_gl.hpp>

#include "../proc/synthetic-stream.h"
#include "align-gl.h"
#include "option.h"
#include "rendering.h"


#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>

#include "synthetic-stream-gl.h"

using namespace rs2;
using namespace librealsense::gl;

rs2_extension align_gl::select_extension(const rs2::frame& input)
{
    auto ext = input.is<rs2::depth_frame>() ? RS2_EXTENSION_DEPTH_FRAME_GL : RS2_EXTENSION_VIDEO_FRAME_GL;
    return ext;
}

void align_gl::cleanup_gpu_resources()
{
    _renderer.reset();
    _pc.reset();
    _other_texture.reset();
    _upload.reset();
    _enabled = 0;
}

void align_gl::create_gpu_resources()
{
    _renderer = std::make_shared<rs2::gl::pointcloud_renderer>();
    _pc = std::make_shared<rs2::gl::pointcloud>();
    _other_texture = std::make_shared<rs2::texture_buffer>();
    _upload = std::make_shared<rs2::gl::uploader>();
    _enabled = glsl_enabled() ? 1 : 0;
}

void align_gl::align_z_to_other(rs2::video_frame& aligned, 
    const rs2::video_frame& depth, 
    const rs2::video_stream_profile& other_profile, 
    float z_scale)
{ 
    auto width = aligned.get_width();
    auto height = aligned.get_height();

    _pc->map_to(depth);
    auto p = _pc->calculate(depth);

    auto frame_ref = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)aligned.get());
    if (!frame_ref)
        throw std::runtime_error("Frame is not depth frame, cannot cast");

    auto gf = dynamic_cast<gpu_addon_interface*>(frame_ref);
    if (!gf)
        throw std::runtime_error("Frame is not gpu_addon_interface, cannot output texture");

    gf->get_gpu_section().set_size(width, height);

    // Set the depth origin of new depth frame to the old depth frame
    auto depth_ptr = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)depth.get());
    if (!depth_ptr)
        throw std::runtime_error("Frame interface is not depth frame");

    frame_ref->set_sensor(depth_ptr->get_sensor());
    depth_ptr->acquire();
    frame_holder h{ depth_ptr };
    frame_ref->set_original(std::move(h));

    uint32_t aligned_tex;
    auto format = depth.get_profile().format();
    auto tex_type = rs_format_to_gl_format(format);
    gf->get_gpu_section().output_texture(0, &aligned_tex, tex_type.type);
    glTexImage2D(GL_TEXTURE_2D, 0, tex_type.internal_format,
        width, height, 0, tex_type.gl_format, tex_type.data_type, nullptr);

    auto prof = depth.get_profile();
    auto intr = other_profile.get_intrinsics();
    auto extr = prof.get_extrinsics_to(other_profile);

    render(p, depth, intr, extr, aligned_tex);
}

// From: https://jamesgregson.blogspot.com/2011/11/matching-calibrated-cameras-with-opengl.html
void build_opengl_projection_for_intrinsics(matrix4& frustum, 
    int *viewport, double alpha, double beta, double skew, 
    double u0, double v0, int img_width, int img_height, double near_clip, double far_clip )
{
    
    // These parameters define the final viewport that is rendered into by
    // the camera.
    int L = 0;
    int R = img_width;
    int B = 0;
    int T = img_height;
    
    // near and far clipping planes, these only matter for the mapping from
    // world-space z-coordinate into the depth coordinate for OpenGL
    double N = near_clip;
    double F = far_clip;
    
    // set the viewport parameters
    viewport[0] = L;
    viewport[1] = B;
    viewport[2] = R-L;
    viewport[3] = T-B;
    
    // construct an orthographic matrix which maps from projected
    // coordinates to normalized device coordinates in the range
    // [-1, 1].  OpenGL then maps coordinates in NDC to the current
    // viewport
    matrix4 ortho;
    ortho(0,0) =  2.f/(R-L);        ortho(0,3) = float(-(R+L)/(R-L));
    ortho(1,1) =  2.f/(T-B);        ortho(1,3) = float(-(T+B)/(T-B));
    ortho(2,2) = -2.f/float(F-N);   ortho(2,3) = float(-(F+N)/(F-N));
    ortho(3,3) =  1.f;

    // construct a projection matrix, this is identical to the 
    // projection matrix computed for the intrinsicx, except an
    // additional row is inserted to map the z-coordinate to
    // OpenGL.
    matrix4 tproj;
    tproj(0,0) = float(alpha);  tproj(0,1) = float(skew);  tproj(0,2) = float(u0);
    tproj(1,1) = float(beta);   tproj(1,2) = float(v0);
    tproj(2,2) = float(-(N+F)); tproj(2,3) = float(-N*F);
    tproj(3,2) = 1.f;

    // resulting OpenGL frustum is the product of the orthographic
    // mapping to normalized device coordinates and the augmented
    // camera intrinsic matrix
    frustum = ortho*tproj;
}

void align_gl::render(const rs2::points& model,
    const rs2::video_frame& tex,
    const rs2_intrinsics& intr,
    const rs2_extrinsics& extr,
    uint32_t output_texture)
{
    perform_gl_action([&] {
        auto width = intr.width;
        auto height = intr.height;
        
        uint32_t texture;
        if (auto input_frame = tex.as<rs2::gl::gpu_frame>())
        {
            texture = input_frame.get_texture_id(0);
        }
        else
        {
            _other_texture->upload(tex, tex.get_profile().format());
            texture = _other_texture->get_gl_handle();
        }

        fbo fbo(width, height);

        glBindTexture(GL_TEXTURE_2D, output_texture);

        auto format = tex.get_profile().format();
        auto textype = rs_format_to_gl_format(format);
        glTexImage2D(GL_TEXTURE_2D, 0, textype.internal_format, 
            width, height, 0, textype.gl_format, textype.data_type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glBindTexture(GL_TEXTURE_2D, output_texture);
        fbo.createTextureAttachment(output_texture);

        fbo.bind();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        matrix4 projection;
        int viewport[4];
        build_opengl_projection_for_intrinsics(projection, viewport,
            intr.fx, intr.fy, 0.0, intr.width - intr.ppx, intr.height - intr.ppy, width, height, 0.001, 100);
        projection(3, 2) *= -1.f;
        projection(2, 3) *= -1.f;
        projection(2, 2) *= -1.f;
        float pm[16];
        projection.to_column_major(pm);
        _renderer->set_matrix(RS2_GL_MATRIX_PROJECTION, pm);
        matrix4 view = matrix4::identity();
        view(2, 2) = -1.f;
        _renderer->set_matrix(RS2_GL_MATRIX_CAMERA, (float*)&view.mat);

        matrix4 other = matrix4::identity();
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
                other(i,j) = extr.rotation[i*3 + j];
            other(3, i) = extr.translation[i];
        }
        _renderer->set_matrix(RS2_GL_MATRIX_TRANSFORMATION, (float*)&other.mat);

        glBindTexture(GL_TEXTURE_2D, texture);
        _renderer->process(model);

        fbo.unbind();

        glBindTexture(GL_TEXTURE_2D, 0);
    }, [&] {
        _enabled = false;
    });
}

void align_gl::align_other_to_z(rs2::video_frame& aligned, 
    const rs2::video_frame& depth, 
    const rs2::video_frame& other, 
    float z_scale)
{
    auto width = aligned.get_width();
    auto height = aligned.get_height();

    _pc->map_to(other);
    auto p = _pc->calculate(depth);

    auto frame_ref = (frame_interface*)aligned.get();
    auto gf = dynamic_cast<gpu_addon_interface*>(frame_ref);
    if (!gf)
        throw std::runtime_error("Frame interface is not gpu addon interface");

    uint32_t output_rgb;
    auto format = other.get_profile().format();
    auto tex_type = rs_format_to_gl_format(format);

    gf->get_gpu_section().output_texture(0, &output_rgb, tex_type.type);
    glTexImage2D(GL_TEXTURE_2D, 0, tex_type.internal_format,
        width, height, 0, tex_type.gl_format, tex_type.data_type, nullptr);

    gf->get_gpu_section().set_size(width, height);

    auto prof = depth.get_profile();
    auto intr = prof.as<video_stream_profile>().get_intrinsics();
    auto extr = prof.get_extrinsics_to(prof);
    render(p, other, intr, extr, output_rgb);
}

align_gl::align_gl(rs2_stream to_stream) : align(to_stream, "Align (GLSL)")
{
    _source.add_extension<gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);
    _source.add_extension<gpu_depth_frame>(RS2_EXTENSION_DEPTH_FRAME_GL);

    auto opt = std::make_shared<librealsense::ptr_option<int>>(
        0, 1, 0, 1, &_enabled, "GLSL enabled"); 
    register_option(RS2_OPTION_COUNT, opt);

    initialize();
}

align_gl::~align_gl()
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
