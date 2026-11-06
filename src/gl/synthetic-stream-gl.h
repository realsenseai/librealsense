// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include "../proc/synthetic-stream.h"
#include "../core/depth-frame.h"
#include "../points.h"

#include <librealsense2/hpp/rs_frame.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include <librealsense2-gl/rs_processing_gl.hpp>
#include "opengl3.h"        // common/
#include "tiny-profiler.h"  // common/
#include <glad/glad.h>

#include <functional>
#include <thread>
#include <mutex>
#include <deque>
#include <unordered_set>


#define RS2_EXTENSION_VIDEO_FRAME_GL (rs2_extension)(RS2_EXTENSION_COUNT)
#define RS2_EXTENSION_DEPTH_FRAME_GL (rs2_extension)(RS2_EXTENSION_COUNT + 1)
#define MAX_TEXTURES 2

namespace librealsense
{ 
    namespace gl
    {
        enum texture_type
        {
            TEXTYPE_RGB,
            TEXTYPE_XYZ,
            TEXTYPE_UV,
            TEXTYPE_UINT16,
            TEXTYPE_FLOAT_ASSIST,
            TEXTYPE_RGBA,
            TEXTYPE_BGR,
            TEXTYPE_BGRA,
            TEXTYPE_UINT8,
            TEXTYPE_COUNT
        };

        struct texture_mapping
        {
            texture_type type;
            rs2_format format;
            int size;
            uint32_t internal_format;
            uint32_t gl_format;
            uint32_t data_type;
        };

        template<class T, int N = 2>
        class pbo
        {
        public:
            void init(int w, int h) 
            {
                glGenBuffers(N, pboIds);
                for (int i = 0; i < N; i++)
                {
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[i]);
                    check_gl_error();
                    glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(T) * w * h, 0, GL_STREAM_READ);
                    check_gl_error();
                }
                
                glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                check_gl_error();
            }

            void query(T* res, int x0, int y0, int w, int h, uint32_t format, uint32_t type)
            {
                GLubyte* pData = NULL;
                
                int next_idx = (index + 1) % N;
                glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[next_idx]);
                check_gl_error();
                {
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);

                    glReadPixels(x0, y0, w, h, format, type, 0);
                    check_gl_error();
                }

                glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[index]);
                check_gl_error();
                {
                    pData = (GLubyte*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                }
                check_gl_error();

                if (pData)
                {
                    memcpy(res, (void*)pData, w * h * sizeof(T));
                    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                    check_gl_error();
                }

                index = next_idx;
            }

            void reset()
            {
                glDeleteBuffers(N, pboIds);
            }

        private:
            uint32_t pboIds[N];
            int index = 0;
        };

        texture_mapping& rs_format_to_gl_format(rs2_format type);

        class gpu_object;
        class gpu_processing_object;
        class gpu_rendering_object;

        class context : public std::enable_shared_from_this<context>
        {
        public:
            context(GLFWwindow* share_with, glfw_binding binding);

            std::shared_ptr<void> begin_session();

            ~context();

            rs2::visualizer_2d& get_texture_visualizer() { return *_vis; }

        private:
            std::shared_ptr<rs2::visualizer_2d> _vis;
            GLFWwindow* _ctx;
            glfw_binding _binding;
            std::recursive_mutex _lock;
        };

        struct lane
        {
            std::unordered_set<gpu_object*> objs;
            std::mutex mutex;
            std::atomic_bool active { false };
            bool use_glsl = false;

            void register_gpu_object(gpu_object* obj)
            {
                std::lock_guard<std::mutex> lock(mutex);
                objs.insert(obj);
            }

            void unregister_gpu_object(gpu_object* obj)
            {
                std::lock_guard<std::mutex> lock(mutex);
                auto it = objs.find(obj);
                objs.erase(it);
            }
        };

        // This represents a persistent object that holds context and other GL stuff
        // The context within it might change, but the lane will remain
        // This is done to simplify GL objects lifetime management - 
        // Once processing blocks are created and in use, and frames have been distributed
        // to various queues and variables, it can be very hard to properly refresh 
        // them if context changes (windows closes, resolution change or any other reason)

        // Processing vs Rendering objects require slightly different handling

        class rendering_lane
        {
        public:
            void register_gpu_object(gpu_rendering_object* obj);

            void unregister_gpu_object(gpu_rendering_object* obj);

            void init(glfw_binding binding, bool use_glsl);

            void shutdown();

            static rendering_lane& instance();

            bool is_active() const { return _data.active; }
            bool glsl_enabled() const { return _data.use_glsl; }

            static bool is_rendering_thread();
        protected:
            lane _data;
            static std::thread::id _rendering_thread;
        };

        class matrix_container
        {
        public:
            matrix_container()
            {
                for (auto i = 0; i < RS2_GL_MATRIX_COUNT; i++)
                    m[i] = rs2::matrix4::identity();
            }
            const rs2::matrix4& get_matrix(rs2_gl_matrix_type type) const { return m[type]; }
            void set_matrix(rs2_gl_matrix_type type, const rs2::matrix4& val) { m[type] = val; }
            virtual ~matrix_container() {}

        private:
            rs2::matrix4 m[RS2_GL_MATRIX_COUNT];
        };

        class processing_lane
        {
        public:
            static processing_lane& instance();

            void register_gpu_object(gpu_processing_object* obj);

            void unregister_gpu_object(gpu_processing_object* obj);

            void init(GLFWwindow* share_with, glfw_binding binding, bool use_glsl);

            void shutdown();

            bool is_active() const { return _data.active; }
            std::shared_ptr<context> get_context() const 
            { 
                return _ctx; 
            }
            bool glsl_enabled() const { return _data.use_glsl; }
        private:
            lane _data;
            std::shared_ptr<context> _ctx;
        };

        // Serializes GL calls of both lanes: the driver can deadlock on its shared-context locks otherwise
        inline std::recursive_mutex& shared_gl_mutex()
        {
            static std::recursive_mutex m;
            return m;
        }

        class gpu_object
        {
        private:
            friend class processing_lane;
            friend class rendering_lane;

            void update_gpu_resources(bool use_glsl)
            {
                _use_glsl = use_glsl;
                if (_needs_cleanup.fetch_xor(1))
                    cleanup_gpu_resources();
                else
                    create_gpu_resources();
            }
        protected:
            gpu_object() = default;
            
            virtual void cleanup_gpu_resources() = 0;
            virtual void create_gpu_resources() = 0;

            bool glsl_enabled() const { return _use_glsl; }

            void need_cleanup() { _needs_cleanup = 1; }
            void use_glsl(bool val) { _use_glsl = val; }

        private:
            gpu_object(const gpu_object&) = delete;
            gpu_object& operator=(const gpu_object&) = delete;

            std::atomic_int _needs_cleanup { 0 };
            bool _use_glsl = false;
        };

        class gpu_rendering_object : public gpu_object
        {
        public:
            gpu_rendering_object()
            {
                rendering_lane::instance().register_gpu_object(this);
            }
            virtual ~gpu_rendering_object()
            {
                rendering_lane::instance().unregister_gpu_object(this);
            }

        protected:
            void initialize() {
                use_glsl(rendering_lane::instance().glsl_enabled());
                if (rendering_lane::instance().is_active())
                    create_gpu_resources();
                need_cleanup();
            }

            template<class T>
            void perform_gl_action(T action)
            {
                std::lock_guard<std::recursive_mutex> gl_lock(shared_gl_mutex());
                if (rendering_lane::instance().is_active())
                    action();
            }
        };

        class gpu_processing_object : public gpu_object
        {
        public:
            gpu_processing_object()
            {
                processing_lane::instance().register_gpu_object(this);
            }
            virtual ~gpu_processing_object()
            {
                processing_lane::instance().unregister_gpu_object(this);
            }

            void set_context(std::weak_ptr<context> ctx) { _ctx = ctx; }
        protected:
            void initialize() {
                if (processing_lane::instance().is_active())
                {
                    _ctx = processing_lane::instance().get_context();
                    use_glsl(processing_lane::instance().glsl_enabled());
                    perform_gl_action([this](){
                        create_gpu_resources();
                    }, []{});
                    need_cleanup();
                }
            }

            template<class T, class S>
            void perform_gl_action(T action, S fallback)
            {
                std::lock_guard<std::recursive_mutex> gl_lock(shared_gl_mutex());
                auto ctx = _ctx.lock();
                if (ctx)
                {
                    auto session = ctx->begin_session();
                    if (processing_lane::instance().is_active())
                        action();
                    else
                        fallback();
                }
                else fallback();
            }

            rs2::visualizer_2d& get_texture_visualizer()
            {
                if (auto ctx = _ctx.lock())
                    return ctx->get_texture_visualizer();
                else throw std::runtime_error("No context available");
            }
        private:
            std::weak_ptr<context> _ctx;
        };

        class gpu_section : public gpu_processing_object
        {
        public:
            gpu_section();
            ~gpu_section();

            void on_publish();
            void on_unpublish();
            void fetch_frame(void* to);

            bool input_texture(int id, uint32_t* tex);
            void output_texture(int id, uint32_t* tex, texture_type type);

            void set_size(uint32_t width, uint32_t height, bool preloaded = false);

            void cleanup_gpu_resources() override;
            void create_gpu_resources() override;

            int get_frame_size() const;

            bool on_gpu() const { return !backup.get(); }

            operator bool();

        private:
            uint32_t textures[MAX_TEXTURES];
            texture_type types[MAX_TEXTURES];
            bool loaded[MAX_TEXTURES];
            uint32_t width, height;
            bool backup_content = true;
            bool preloaded = false;
            bool initialized = false;
            std::unique_ptr<uint8_t[]> backup;
            void ensure_init();
        };

        class gpu_addon_interface
        {
        public:
            virtual gpu_section& get_gpu_section() = 0;
            virtual ~gpu_addon_interface() = default;
        };

        template<class T>
        class gpu_addon : public T, public gpu_addon_interface
        {
        public:
            virtual gpu_section& get_gpu_section() override { return _section; }
            frame_interface* publish(std::shared_ptr<archive_interface> new_owner) override
            {
                _section.on_publish();
                return T::publish(new_owner);
            }
            void unpublish() override
            {
                _section.on_unpublish();
                T::unpublish();
            }
            const uint8_t * get_frame_data() const override
            {
                auto res = T::get_frame_data();
                _section.fetch_frame((void*)res);
                return res;
            }
            gpu_addon() : T(), _section() {}
            gpu_addon(gpu_addon&& other)
                :T((T&&)std::move(other))
            {
            }
            gpu_addon& operator=(gpu_addon&& other)
            {
                return (gpu_addon&)T::operator=((T&&)std::move(other));
            }
        private:
            mutable gpu_section _section;
        };

        class gpu_video_frame : public gpu_addon<video_frame> {};
        class gpu_points_frame : public gpu_addon<points> {};
        class gpu_depth_frame : public gpu_addon<depth_frame> {};

        // Dual processing block is a helper class (TODO: move to core LRS)
        // that represents several blocks doing the same exact thing
        // behaving absolutely equally from API point of view,
        // but only one of them actually getting the calls from frames
        // This lets us easily create "backup" behaviour where GL block
        // can fall back on the best CPU implementation if GLSL is not available
        class dual_processing_block : public processing_block
        {
        public:
            class bypass_option : public option
            {
            public:
                bypass_option(dual_processing_block* parent, rs2_option opt) 
                    : _parent(parent), _opt(opt) {}

                void set(float value) override { 
                    // While query and other read operations
                    // will only read from the currently selected
                    // block, setting an option will propogate
                    // to all blocks in the group
                    for(size_t i = 0; i < _parent->_blocks.size(); i++)
                    {
                        if (_parent->_blocks[i]->supports_option(_opt))
                        {
                            _parent->_blocks[i]->get_option(_opt).set(value);
                        }
                    }
                }
                float query() const override { return get().query(); }
                option_range get_range() const override { return get().get_range(); }
                bool is_enabled() const override { return get().is_enabled(); }
                bool is_read_only() const override { return get().is_read_only(); }
                const char* get_description() const override { return get().get_description(); }
                const char* get_value_description(float v) const override { return get().get_value_description(v); }
                void enable_recording(std::function<void(const option &)> record_action) override {}

                option& get() { return _parent->get().get_option(_opt); }
                const option& get() const { return _parent->get().get_option(_opt); }
            private:
                dual_processing_block* _parent;
                rs2_option _opt;
            };

            dual_processing_block() : processing_block("Dual-Purpose Block") {}

            processing_block& get() 
            { 
                for(auto i = 0; i < _blocks.size(); i++)
                {
                    index = (int)i;
                    if (_blocks[i]->supports_option(RS2_OPTION_COUNT))
                    {
                        auto val = _blocks[i]->get_option(RS2_OPTION_COUNT).query();
                        if (val > 0.f) break;
                    }
                }

                update_info(RS2_CAMERA_INFO_NAME, _blocks[index]->get_info(RS2_CAMERA_INFO_NAME));

                return *_blocks[index]; 
            }

            void add(std::shared_ptr<processing_block> block)
            {
                _blocks.push_back(block);

                for (int i = 0; i < RS2_OPTION_COUNT; i++)
                {
                    auto opt = (rs2_option)i;
                    if (block->supports_option(opt))
                        register_option(opt, std::make_shared<bypass_option>(this, opt));
                }

                update_info(RS2_CAMERA_INFO_NAME, block->get_info(RS2_CAMERA_INFO_NAME));
            }

            void set_processing_callback( rs2_frame_processor_callback_sptr callback ) override
            {
                for (auto&& pb : _blocks) pb->set_processing_callback(callback);
            }
            void set_output_callback( rs2_frame_callback_sptr callback ) override
            {
                for (auto&& pb : _blocks) pb->set_output_callback(callback);
            }
            void invoke(frame_holder frames) override
            {
                get().invoke(std::move(frames));
            }
            synthetic_source_interface& get_source() override
            {
                return get().get_source();
            }
        protected:
            std::vector<std::shared_ptr<processing_block>> _blocks;
            int index = 0;
        };
   }
}
