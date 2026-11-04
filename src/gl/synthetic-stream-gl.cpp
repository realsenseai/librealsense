// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "core/video.h"
#include "synthetic-stream-gl.h"
#include "option.h"
#include "opengl3.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <future>

namespace librealsense
{
    namespace gl
    {
        texture_mapping texture_formats[TEXTYPE_COUNT]{
            { TEXTYPE_RGB,             RS2_FORMAT_RGB8,    3,  GL_RGB,     GL_RGB,         GL_UNSIGNED_BYTE },
            { TEXTYPE_BGR,             RS2_FORMAT_BGR8,    3,  GL_BGR,     GL_BGR,         GL_UNSIGNED_BYTE },
            { TEXTYPE_RGBA,            RS2_FORMAT_RGBA8,   4,  GL_RGBA,    GL_RGBA,        GL_UNSIGNED_BYTE },
            { TEXTYPE_BGRA,            RS2_FORMAT_BGRA8,   4,  GL_BGRA,    GL_BGRA,        GL_UNSIGNED_BYTE },
            { TEXTYPE_UINT8,           RS2_FORMAT_Y8,      1,  GL_RGB,     GL_LUMINANCE,   GL_UNSIGNED_BYTE },
            { TEXTYPE_UINT16,          RS2_FORMAT_Z16,     2,  GL_RG8,     GL_RG,          GL_UNSIGNED_BYTE },
            { TEXTYPE_XYZ,             RS2_FORMAT_XYZ32F,  12, GL_RGB16F,  GL_RGB,         GL_FLOAT },
            { TEXTYPE_UV,              RS2_FORMAT_ANY,     8,  GL_RGB16F,  GL_RG,          GL_FLOAT },
            { TEXTYPE_FLOAT_ASSIST,    RS2_FORMAT_ANY,     0,  GL_R16F,    GL_RED,         GL_FLOAT },
        };

        texture_mapping& gl_format_mapping(texture_type type)
        {
            for (int i = 0; i < TEXTYPE_COUNT; i++)
                if (texture_formats[i].type == type) return texture_formats[i];
            throw std::runtime_error("Selected RealSense format cannot be converted to GL format!");
        }

        texture_mapping& rs_format_to_gl_format(rs2_format type)
        {
            for (int i = 0; i < TEXTYPE_COUNT; i++)
                if (texture_formats[i].format == type) return texture_formats[i];
            throw std::runtime_error("Selected RealSense format cannot be converted to GL format!");
        }

        void gpu_section::ensure_init()
        {
            if (!initialized)
            {
                initialize();
                initialized = true;
            }
        }

        std::thread::id rendering_lane::_rendering_thread {};

        bool rendering_lane::is_rendering_thread()
        {
            return std::this_thread::get_id() == _rendering_thread;
        }

        void rendering_lane::register_gpu_object(gpu_rendering_object* obj)
        {
            _data.register_gpu_object(obj);
        }

        void rendering_lane::unregister_gpu_object(gpu_rendering_object* obj)
        {
            _data.unregister_gpu_object(obj);
        }

        void rendering_lane::init(glfw_binding binding, bool use_glsl)
        {
            std::lock_guard<std::recursive_mutex> gl_lock(shared_gl_mutex());
            std::lock_guard<std::mutex> lock(_data.mutex);

            gladLoadGLLoader((GLADloadproc)binding.glfwGetProcAddress);
            LOG_DEBUG("Initializing rendering, GLSL=" << use_glsl);

            for (auto&& obj : _data.objs)
            {
                obj->update_gpu_resources(use_glsl);
            }
            _data.active = true;
            _data.use_glsl = use_glsl;

            LOG_DEBUG(" " << _data.objs.size() << " GPU objects initialized");

            _rendering_thread = std::this_thread::get_id();
        }

        void rendering_lane::shutdown()
        {
            std::lock_guard<std::recursive_mutex> gl_lock(shared_gl_mutex());
            std::lock_guard<std::mutex> lock(_data.mutex);
            LOG_DEBUG("Shutting down rendering");
            for (auto&& obj : _data.objs)
            {
                obj->update_gpu_resources(false);
            }
            _data.active = false;
            LOG_DEBUG(" " << _data.objs.size() << " GPU objects cleaned-up");
        }

        rendering_lane& rendering_lane::instance()
        {
            static rendering_lane instance;
            return instance;
        }

        processing_lane& processing_lane::instance()
        {
            static processing_lane instance;
            return instance;
        }

        void processing_lane::register_gpu_object(gpu_processing_object* obj)
        {
            _data.register_gpu_object(obj);
        }

        void processing_lane::unregister_gpu_object(gpu_processing_object* obj)
        {
            _data.unregister_gpu_object(obj);
        }

        void processing_lane::init(GLFWwindow* share_with, glfw_binding binding, bool use_glsl)
        {
            std::lock_guard<std::recursive_mutex> gl_lock(shared_gl_mutex());
            std::lock_guard<std::mutex> lock(_data.mutex);

            LOG_DEBUG("Initializing processing, GLSL=" << use_glsl);

            _data.active = true;
            _data.use_glsl = use_glsl;

            _ctx = std::make_shared<context>(share_with, binding);
            auto session = _ctx->begin_session();

            for (auto&& obj : _data.objs)
            {
                ((gpu_processing_object*)obj)->set_context(_ctx);
                obj->update_gpu_resources(use_glsl);
            }

            LOG_DEBUG(" " << _data.objs.size() << " GPU objects initialized");
        }

        void processing_lane::shutdown()
        {
            std::lock_guard<std::recursive_mutex> gl_lock(shared_gl_mutex());
            std::lock_guard<std::mutex> lock(_data.mutex);

            LOG_DEBUG("Shutting down processing");

            _data.active = false;
            auto session = _ctx->begin_session();

            for (auto&& obj : _data.objs)
            {
                ((gpu_processing_object*)obj)->set_context({});
                obj->update_gpu_resources(false);
            }

            LOG_DEBUG(" " << _data.objs.size() << " GPU objects cleaned-up");
            
            _ctx.reset();
        }

        void gpu_section::cleanup_gpu_resources()
        {
            if (backup_content)
            {
                backup = std::unique_ptr<uint8_t[]>(new uint8_t[get_frame_size()]);
                fetch_frame(backup.get());
            }
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                if (textures[i])
                {
                    glDeleteTextures(1, &textures[i]);
                    textures[i] = 0;
                }
            }
        }

        gpu_section::~gpu_section()
        {
            backup_content = false;
            try
            {
                perform_gl_action( [&]() {
                    cleanup_gpu_resources();
                    }, [] {} );
            }
            catch(...)
            {
                LOG_DEBUG( "Error while cleaning up gpu resources" );
            }
        }

        void gpu_section::create_gpu_resources()
        {
            backup.reset();
        }

        gpu_section::operator bool()
        {
            bool res = false;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (loaded[i]) res = true;
            return res;
        }

        gpu_section::gpu_section()
        {
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                textures[i] = 0;
                loaded[i] = false;
            }
            
        }

        void gpu_section::on_publish()
        {
            ensure_init();
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                loaded[i] = false;
            }
        }

        void gpu_section::on_unpublish()
        {
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                loaded[i] = false;
            }
        }

        void gpu_section::output_texture(int id, uint32_t* tex, texture_type type)
        {
            ensure_init();
            
            auto existing_tex = textures[id];
            if (existing_tex)
                *tex = existing_tex;
            else
            {
                glGenTextures(1, tex);
                textures[id] = *tex;
            }
            loaded[id] = true;
            types[id] = type;
        }

        void gpu_section::set_size(uint32_t width, uint32_t height, bool preloaded)
        {
            this->width = width; this->height = height; this->preloaded = preloaded;
        }

        bool gpu_section::input_texture(int id, uint32_t* tex)
        {
            if (loaded[id]) 
            {
                *tex = textures[id];
                return true;
            }
            return false;
        }

        int gpu_section::get_frame_size() const
        {
            int res = 0;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (textures[i] && loaded[i])
                {
                    res += width * height * gl_format_mapping(types[i]).size;
                }
            return res;
        }

        void gpu_section::fetch_frame(void* to)
        {
            if (preloaded) return;

            ensure_init();

            bool need_to_fetch = false;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (loaded[i]) need_to_fetch = true;

            if (need_to_fetch)
            {
                perform_gl_action([&]{
                    auto ptr = (uint8_t*)to;

                    for (int i = 0; i < MAX_TEXTURES; i++)
                    if (textures[i] && loaded[i])
                    {
                        auto& vis = get_texture_visualizer();
                        //rs2::visualizer_2d vis;
                        rs2::fbo fbo(width, height);
                        uint32_t res;
                        glGenTextures(1, &res);
                        glBindTexture(GL_TEXTURE_2D, res);

                        auto textype = gl_format_mapping(types[i]);
                        if (textype.size)
                            glTexImage2D(GL_TEXTURE_2D, 0, textype.internal_format, 
                                width, height, 0, textype.gl_format, textype.data_type, nullptr);

                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, res, 0);

                        fbo.bind();
                        glViewport(0, 0, width, height);
                        glClearColor(0, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                        vis.draw_texture(textures[i]);
                        glReadBuffer(GL_COLOR_ATTACHMENT0);

                        if (textype.size)
                        {
                            glReadPixels(0, 0, width, height, textype.gl_format, textype.data_type, ptr);
                            ptr += width * height * textype.size;
                        }
                        
                        glDeleteTextures(1, &res);
                        
                        fbo.unbind();

                        preloaded = true;
                    }
                }, [&]{
                    memcpy(to, backup.get(), get_frame_size());
                });
            }
        }

        context::context(GLFWwindow* share_with, glfw_binding binding) : _binding(binding)
        {
            if (binding.glfwInit) binding.glfwInit();

            binding.glfwWindowHint(GLFW_VISIBLE, 0);
            _ctx = binding.glfwCreateWindow(640, 480, "Offscreen Context", NULL, share_with);
            if (!_ctx)
            {
                throw std::runtime_error("Could not initialize offscreen context!");
            }

            auto curr = binding.glfwGetCurrentContext();
            binding.glfwMakeContextCurrent(_ctx);

            if (glShaderSource == nullptr)
            {
                gladLoadGLLoader((GLADloadproc)binding.glfwGetProcAddress);
            }

            binding.glfwSwapInterval(0);

            _vis = std::make_shared<rs2::visualizer_2d>();

            binding.glfwMakeContextCurrent(curr);
        }

        std::shared_ptr<void> context::begin_session()
        {
            auto curr = _binding.glfwGetCurrentContext();
            if (curr == _ctx) return nullptr;

            _lock.lock();

            _binding.glfwMakeContextCurrent(_ctx);
            auto me = shared_from_this();
            return std::shared_ptr<void>(nullptr, [curr, me](void*){
                me->_binding.glfwMakeContextCurrent(curr);
                me->_lock.unlock();
            });
        }

        context::~context()
        {
            _vis.reset();
            _binding.glfwDestroyWindow(_ctx);
        }
    }
}
