// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "camera-shader.h"
#include "rendering.h"
#include "option.h"
#include <src/core/device-interface.h>
#include <src/core/sensor-interface.h>

using namespace rs2;

struct short3
{
    uint16_t x, y, z;
};

#include <res/d435.h>
#include <res/d415.h>
#include <res/d455.h>

static const char* vertex_shader_text =
"#version 110\n"
"\n"
"attribute vec3 position;\n"
"uniform mat4 transformationMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"uniform mat4 cameraMatrix;\n"
"\n"
"void main(void) {\n"
"    vec4 worldPosition = transformationMatrix * vec4(position.xyz, 1.0);\n"
"    gl_Position = projectionMatrix * cameraMatrix * worldPosition;\n"
"}\n";

static const char* fragment_shader_text =
"#version 110\n"
"uniform float opacity;"
"\n"
"void main(void) {\n"
"    gl_FragColor = vec4(opacity * (36.0 / 1000.0), opacity * (44.0 / 1000.0), opacity * (51.0 / 1000.0), opacity);\n"
"}\n";

using namespace rs2;

namespace librealsense
{
    namespace gl
    {
        camera_shader::camera_shader()
        {
            _shader = shader_program::load(
                vertex_shader_text,
                fragment_shader_text);

            init();
        }

        void camera_shader::init()
        {
            _shader->bind_attribute(0, "position");

            _transformation_matrix_location = _shader->get_uniform_location("transformationMatrix");
            _projection_matrix_location = _shader->get_uniform_location("projectionMatrix");
            _camera_matrix_location = _shader->get_uniform_location("cameraMatrix");
            _opacity_location = _shader->get_uniform_location("opacity");
        }

        void camera_shader::begin() { _shader->begin(); }
        void camera_shader::end() { _shader->end(); }

        void camera_shader::set_mvp(const matrix4& model,
            const matrix4& view,
            const matrix4& projection)
        {
            _shader->load_uniform(_transformation_matrix_location, model);
            _shader->load_uniform(_camera_matrix_location, view);
            _shader->load_uniform(_projection_matrix_location, projection);
        }

        void camera_shader::set_opacity(float opacity)
        {
            _shader->load_uniform(_opacity_location, opacity);
        }

        void camera_renderer::cleanup_gpu_resources()
        {
            _shader.reset();
            _camera_model.clear();
        }

        void camera_renderer::create_gpu_resources()
        {
            if (glsl_enabled())
            {
                _shader = std::make_shared<camera_shader>(); 

                for (auto&& mesh : camera_mesh)
                {
                    _camera_model.push_back(vao::create(mesh));
                }
            }
        }

        camera_renderer::~camera_renderer()
        {
            perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            });
        }

        typedef void (*load_function)(std::vector<rs2::float3>&, 
            std::vector<rs2::float3>&, std::vector<short3>&);

        obj_mesh load_model(load_function f)
        {
            obj_mesh res;
            std::vector<short3> idx;
            f(res.positions, res.normals, idx);
            for (auto i : idx)
                res.indexes.push_back({ i.x, i.y, i.z });
            return res;
        }

        camera_renderer::camera_renderer() : stream_filter_processing_block("Camera Model Renderer")
        {
            camera_mesh.push_back(load_model(uncompress_d415_obj));
            camera_mesh.push_back(load_model(uncompress_d435_obj));
            camera_mesh.push_back(load_model(uncompress_d455_obj));

            register_option(RS2_OPTION_FILTER_MAGNITUDE, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));
            _opacity_opt = &get_option(RS2_OPTION_FILTER_MAGNITUDE);

            for (auto&& mesh : camera_mesh)
            {
                for (auto& xyz : mesh.positions)
                {
                    xyz = xyz / 1000.f;
                    xyz.x *= -1;
                    xyz.y *= -1;
                }
            }


            initialize();
        }

        bool starts_with(const std::string& s, const std::string& prefix)
        {
            auto i = s.begin(), j = prefix.begin();
            for (; i != s.end() && j != prefix.end() && *i == *j;
                i++, j++);
            return j == prefix.end();
        }

        rs2::frame camera_renderer::process_frame(const rs2::frame_source& src, const rs2::frame& f)
        {
            //scoped_timer t("camera_renderer");

            const auto& dev = ((frame_interface*)f.get())->get_sensor()->get_device();

            int index = -1;

            if (dev.supports_info(RS2_CAMERA_INFO_NAME))
            {
                std::string dev_name = dev.get_info(RS2_CAMERA_INFO_NAME);
                if (dev_name.find("D415") != std::string::npos) index = 0;
                if (dev_name.find("D435") != std::string::npos ||
                    dev_name.find("D436") != std::string::npos) index = 1;
                if (dev_name.find("D45") != std::string::npos) index = 2;
            };

            auto opacity = clamp(_opacity_opt->query(), 0.0, 1.0);

            if (index >= 0)
            {
                perform_gl_action([&]()
                {
                    //scoped_timer t("camera_renderer.gl");

                    if (glsl_enabled())
                    {
                        _shader->begin();
                        _shader->set_mvp(get_matrix(RS2_GL_MATRIX_TRANSFORMATION), 
                            get_matrix(RS2_GL_MATRIX_CAMERA), 
                            get_matrix(RS2_GL_MATRIX_PROJECTION)
                        );
                        _shader->set_opacity(opacity);
                        _camera_model[index]->draw();
                        _shader->end();
                    }
                    else
                    {
                        glMatrixMode(GL_MODELVIEW);
                        glPushMatrix();

                        auto t = get_matrix(RS2_GL_MATRIX_TRANSFORMATION);
                        auto v = get_matrix(RS2_GL_MATRIX_CAMERA);

                        glLoadMatrixf(v * t);

                        glBegin(GL_TRIANGLES);
                        auto& mesh = camera_mesh[index];
                        for (auto& i : mesh.indexes)
                        {
                            auto v0 = mesh.positions[i.x];
                            auto v1 = mesh.positions[i.y];
                            auto v2 = mesh.positions[i.z];
                            glVertex3fv(&v0.x);
                            glVertex3fv(&v1.x);
                            glVertex3fv(&v2.x);
                            glColor4f(opacity * 0.036f, opacity * 0.044f, opacity * 0.051f, opacity);
                        }
                        glEnd();

                        glPopMatrix();
                    }
                }); 
            }

            return f;
        }
    }
}
