// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "api.h"
#include "synthetic-stream-gl.h"
#include "yuy2rgb-gl.h"
#include "m420-to-rgb-gl.h"
#include "nv12-to-rgb-gl.h"
#include "y4112rgb-gl.h"
#include "align-gl.h"
#include "pointcloud-gl.h"
#include "../include/librealsense2/h/rs_types.h"
#include "../include/librealsense2-gl/rs_processing_gl.h"
#include "camera-shader.h"
#include "upload.h"
#include "pc-shader.h"
#include "colorizer-gl.h"
#include "proc/color-formats-converter.h"
#include "proc/y411-converter.h"
#include "proc/colorizer.h"
#include "proc/align.h"
#include "log.h"
#include <assert.h>

#include <GLFW/glfw3.h>


////////////////////////
// API implementation //
////////////////////////

using namespace librealsense;

struct rs2_gl_context
{
    std::shared_ptr<gl::context> ctx;
};

namespace librealsense
{
    RS2_ENUM_HELPERS(rs2_gl_extension, GL_EXTENSION)

    const char* get_string(rs2_gl_extension value)
    {
        switch (value)
        {
        case RS2_GL_EXTENSION_VIDEO_FRAME: return "GL Video Frame";
        default: assert(!is_valid(value)); return UNKNOWN_VALUE;
        }
    }
}

const char* rs2_gl_extension_to_string(rs2_gl_extension ex) { return librealsense::get_string(ex); }

namespace librealsense
{
    RS2_ENUM_HELPERS(rs2_gl_matrix_type, GL_MATRIX)

    const char* get_string(rs2_gl_matrix_type value)
    {
        switch (value)
        {
        case RS2_GL_MATRIX_TRANSFORMATION: return "Transformation Matrix";
        case RS2_GL_MATRIX_PROJECTION: return "Projection Matrix";
        case RS2_GL_MATRIX_CAMERA: return "Camera Matrix";
        default: assert(!is_valid(value)); return UNKNOWN_VALUE;
        }
    }
}

const char* rs2_gl_matrix_type_to_string(rs2_gl_matrix_type type) { return librealsense::get_string(type); }

rs2_processing_block* rs2_gl_create_yuy_decoder(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    auto block = std::make_shared<librealsense::gl::yuy2rgb>();
    auto backup = std::make_shared<librealsense::yuy2_converter>(RS2_FORMAT_RGB8);
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block { dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_gl_create_m420_decoder(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    auto block = std::make_shared<librealsense::gl::m420_to_rgb>();
    auto backup = std::make_shared<librealsense::m420_converter>(RS2_FORMAT_RGB8);
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block{ dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_gl_create_nv12_decoder(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    auto block = std::make_shared<librealsense::gl::nv12_to_rgb>();
    auto backup = std::make_shared<librealsense::nv12_converter>(RS2_FORMAT_RGB8);
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block{ dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_gl_create_y411_decoder(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    auto block = std::make_shared<librealsense::gl::y411_2rgb>();
    auto backup = std::make_shared<librealsense::y411_converter>(RS2_FORMAT_RGB8);
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block{ dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

unsigned int rs2_gl_frame_get_texture_id(const rs2_frame* frame_ref, unsigned int id, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    VALIDATE_RANGE(id, 0, MAX_TEXTURES - 1);
    
    auto gpu = dynamic_cast<gl::gpu_addon_interface*>((frame_interface*)frame_ref);
    if (!gpu) throw std::runtime_error("Expected GPU frame!");

    uint32_t res;
    if (!gpu->get_gpu_section().input_texture(id, &res)) 
        throw std::runtime_error("Texture not ready!");

    return res;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

int rs2_gl_is_frame_extendable_to(const rs2_frame* f, rs2_gl_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(f);
    VALIDATE_ENUM(extension_type);

    bool res = false;

    switch (extension_type)
    {
    case RS2_GL_EXTENSION_VIDEO_FRAME: 
    {
        auto gpu = dynamic_cast<gl::gpu_addon_interface*>((frame_interface*)f);
        if (!gpu) return false;
        // If nothing was loaded to the GPU frame, abort
        if (!gpu->get_gpu_section()) return false;
        // If GPU frame was backed up to main memory, abort
        if (!gpu->get_gpu_section().on_gpu()) return false;
        return true;
    }
    default: return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, f, extension_type)

rs2_processing_block* rs2_gl_create_colorizer(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    auto block = std::make_shared<librealsense::gl::colorizer>();
    auto backup = std::make_shared<librealsense::colorizer>();
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block { dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_gl_create_align(int api_version, rs2_stream to, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    auto block = std::make_shared<librealsense::gl::align_gl>(to);
    auto backup = align::create_align(to);
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block { dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_gl_create_pointcloud(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    auto block = std::make_shared<librealsense::gl::pointcloud_gl>();
    auto backup = pointcloud::create();
    auto dual = std::make_shared<librealsense::gl::dual_processing_block>();
    dual->add(block);
    dual->add(backup);
    return new rs2_processing_block { dual };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

void rs2_gl_set_matrix(rs2_processing_block* block, rs2_gl_matrix_type type, float* m4x4, rs2_error** error) BEGIN_API_CALL
{
    auto ptr = dynamic_cast<librealsense::gl::matrix_container*>(block->block.get());
    if (!ptr) throw std::runtime_error("Processing block does not support matrix setting");
    rs2::matrix4 m;
    memcpy(&m.mat, m4x4, sizeof(rs2::matrix4));
    ptr->set_matrix(type, m);
}
HANDLE_EXCEPTIONS_AND_RETURN(, block, type, m4x4)


void rs2_gl_init_rendering(int api_version, int use_glsl, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    glfw_binding binding{
        &glfwInit,
        &glfwWindowHint,
        &glfwCreateWindow,
        &glfwDestroyWindow,
        &glfwMakeContextCurrent,
        &glfwGetCurrentContext,
        &glfwSwapInterval,
        &glfwGetProcAddress
    };
    librealsense::gl::rendering_lane::instance().init(binding, use_glsl > 0);
}
HANDLE_EXCEPTIONS_AND_RETURN(, api_version, use_glsl)

void rs2_gl_init_rendering_glfw(int api_version, glfw_binding bindings, int use_glsl, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    librealsense::gl::rendering_lane::instance().init(bindings, use_glsl > 0);
}
HANDLE_EXCEPTIONS_AND_RETURN(, api_version, use_glsl)

void rs2_gl_init_processing(int api_version, int use_glsl, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    glfw_binding binding{
        &glfwInit,
        &glfwWindowHint,
        &glfwCreateWindow,
        &glfwDestroyWindow,
        &glfwMakeContextCurrent,
        &glfwGetCurrentContext,
        &glfwSwapInterval,
        &glfwGetProcAddress
    };
    librealsense::gl::processing_lane::instance().init(nullptr, binding, use_glsl > 0);
}
HANDLE_EXCEPTIONS_AND_RETURN(, api_version, use_glsl)

void rs2_gl_init_processing_glfw(int api_version, GLFWwindow* share_with, 
                                 glfw_binding bindings, int use_glsl, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    librealsense::gl::processing_lane::instance().init(share_with, bindings, use_glsl > 0);
}
HANDLE_EXCEPTIONS_AND_RETURN(, api_version, use_glsl)

void rs2_gl_shutdown_rendering(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    librealsense::gl::rendering_lane::instance().shutdown();
}
HANDLE_EXCEPTIONS_AND_RETURN(, api_version)

void rs2_gl_shutdown_processing(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    librealsense::gl::processing_lane::instance().shutdown();
}
HANDLE_EXCEPTIONS_AND_RETURN(, api_version)

rs2_processing_block* rs2_gl_create_camera_renderer(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    auto block = std::make_shared<librealsense::gl::camera_renderer>();
    return new rs2_processing_block { block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version)

rs2_processing_block* rs2_gl_create_uploader(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    auto block = std::make_shared<librealsense::gl::upload>();
    return new rs2_processing_block{ block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version)

rs2_processing_block* rs2_gl_create_pointcloud_renderer(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    auto block = std::make_shared<librealsense::gl::pointcloud_renderer>();
    return new rs2_processing_block { block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version)

#ifdef BUILD_EASYLOGGINGPP
#ifdef SHARED_LIBS
INITIALIZE_EASYLOGGINGPP
#endif
char log_gl_name[] = LIBREALSENSE_ELPP_ID;
static logger_type<log_gl_name> logger_gl;
#endif

