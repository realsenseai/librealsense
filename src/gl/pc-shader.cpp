// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "pc-shader.h"
#include "synthetic-stream-gl.h"
#include "rendering.h"

#include "option.h"
#include "tiny-profiler.h"

static const char* vertex_shader_text =
"#version 130\n"
"\n"
"attribute vec3 position;\n"
"attribute vec2 textureCoords;\n"
"\n"
"out float valid;\n"
"out vec2 sampledUvs;\n"
"out vec4 outPos;\n"
"out vec3 normal;\n"
"\n"
"uniform mat4 transformationMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"uniform mat4 cameraMatrix;\n"
"\n"
"uniform sampler2D uvsSampler;\n"
"uniform sampler2D positionsSampler;\n"
"\n"
"uniform float imageWidth;\n"
"uniform float imageHeight;\n"
"uniform float minDeltaZ;\n"
"\n"
"void main(void) {\n"
"    float pixelWidth = 1.0 / imageWidth;\n"
"    float pixelHeight = 1.0 / imageHeight;\n"
"    vec2 tex = vec2(textureCoords.x, textureCoords.y);\n"
"    vec4 pos = texture2D(positionsSampler, tex);\n"
"    vec4 uvs = texture2D(uvsSampler, tex);\n"
"\n"
"    vec2 tex_left = vec2(max(textureCoords.x - pixelWidth, 0.0), textureCoords.y);\n"
"    vec2 tex_right = vec2(min(textureCoords.x + pixelWidth, 1.0), textureCoords.y);\n"
"    vec2 tex_top = vec2(textureCoords.x, max(textureCoords.y - pixelHeight, 0.0));\n"
"    vec2 tex_buttom = vec2(textureCoords.x, min(textureCoords.y + pixelHeight, 1.0));\n"
"\n"
"    vec4 pos_left = texture2D(positionsSampler, tex_left);\n"
"    vec4 pos_right = texture2D(positionsSampler, tex_right);\n"
"    vec4 pos_top = texture2D(positionsSampler, tex_top);\n"
"    vec4 pos_buttom = texture2D(positionsSampler, tex_buttom);\n"
"\n"
"    vec3 axis1 = vec3(normalize(mix(pos_right - pos, pos - pos_left, 0.5)));\n"
"    vec3 axis2 = vec3(normalize(mix(pos_top - pos, pos - pos_buttom, 0.5)));\n"
"    normal = cross(axis1, axis2);\n"
"\n"
"    valid = 0.0;\n"
"    if (uvs.x < 0.0) valid = 1.0;\n"
"    if (uvs.y < 0.0) valid = 1.0;\n"
"    if (uvs.x >= 1.0) valid = 1.0;\n"
"    if (uvs.y >= 1.0) valid = 1.0;\n"
"    if (abs(pos_left.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos_right.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos_top.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos_buttom.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos.z) < 0.01) valid = 1.0;\n"
"    if (valid > 0.0) pos = vec4(1.0, 1.0, 1.0, 0.0);\n"
"    else pos = vec4(pos.xyz, 1.0);\n"
"    vec4 worldPosition = transformationMatrix * pos;\n"
"    gl_Position = projectionMatrix * cameraMatrix * worldPosition;\n"
"\n"
"    sampledUvs = uvs.xy;\n"
"    outPos = pos;\n"
"}\n";

static const char* fragment_shader_text =
"#version 130\n"
"\n"
"in float valid;\n"
"in vec4  outPos;\n"
"in vec2 sampledUvs;\n"
"in vec3 normal;\n"
"out vec4 output_rgb;\n"
"\n"
"uniform sampler2D textureSampler;\n"
"uniform float pickedID;\n"
"uniform float shaded;\n"
"\n"
"const float Epsilon = 1e-10;\n"
"\n"
"vec3 rgb2hsv(vec3 c)\n"
"{\n"
"    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);\n"
"    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n"
"    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n"
"    float d = q.x - min(q.w, q.y);\n"
"    float e = 1.0e-10;\n"
"    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n"
"}\n"
"vec3 hsv2rgb(vec3 c)\n"
"{\n"
"    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n"
"    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
"    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
"}\n"
"void main(void) {\n"
"    if (valid > 0.0) discard;\n"
"    vec4 color = texture2D(textureSampler, sampledUvs);\n"
"    if (shaded > 0.0) {\n"
"    vec3 light0 = vec3(0.0, 1.0, 0.0);"
"    vec3 light1 = vec3(0.5, -1.0, 0.0);"
"    vec3 light2 = vec3(-0.5, -1.0, 0.0);"
"    vec3 light_dir0 = light0 - vec3(outPos);\n"
"    vec3 light_dir1 = light1 - vec3(outPos);\n"
"    vec3 light_dir2 = light2 - vec3(outPos);\n"
"    float diffuse_factor0 = max(dot(normal,light_dir0), 0.0);\n"
"    float diffuse_factor1 = max(dot(normal,light_dir1), 0.0);\n"
"    float diffuse_factor2 = max(dot(normal,light_dir2), 0.0);\n"
"    float diffuse_factor = 0.6 + diffuse_factor0 * 0.2 + diffuse_factor1 * 0.2 + diffuse_factor2 * 0.2;\n"
"    color = clamp(diffuse_factor, 0.0, 1.0) * color;\n"
"    }\n"
"    output_rgb = vec4(color.xyz, 1.0);\n"
"}\n";


static const char* vertex_shader_text_picking =
"#version 130\n"
"\n"
"attribute vec3 position;\n"
"attribute vec2 textureCoords;\n"
"\n"
"out float valid;\n"
"out vec2 sampledUvs;\n"
"out vec4 outPos;\n"
"\n"
"uniform mat4 transformationMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"uniform mat4 cameraMatrix;\n"
"\n"
"uniform sampler2D uvsSampler;\n"
"uniform sampler2D positionsSampler;\n"
"\n"
"uniform float imageWidth;\n"
"uniform float imageHeight;\n"
"uniform float minDeltaZ;\n"
"\n"
"void main(void) {\n"
"    float pixelWidth = 1.0 / imageWidth;\n"
"    float pixelHeight = 1.0 / imageHeight;\n"
"    vec2 tex = vec2(textureCoords.x, textureCoords.y);\n"
"    vec4 pos = texture2D(positionsSampler, tex);\n"
"    vec4 uvs = texture2D(uvsSampler, tex);\n"
"\n"
"    vec2 tex_left = vec2(max(textureCoords.x - pixelWidth, 0.0), textureCoords.y);\n"
"    vec2 tex_right = vec2(min(textureCoords.x + pixelWidth, 1.0), textureCoords.y);\n"
"    vec2 tex_top = vec2(textureCoords.x, max(textureCoords.y - pixelHeight, 0.0));\n"
"    vec2 tex_buttom = vec2(textureCoords.x, min(textureCoords.y + pixelHeight, 1.0));\n"
"\n"
"    vec4 pos_left = texture2D(positionsSampler, tex_left);\n"
"    vec4 pos_right = texture2D(positionsSampler, tex_right);\n"
"    vec4 pos_top = texture2D(positionsSampler, tex_top);\n"
"    vec4 pos_buttom = texture2D(positionsSampler, tex_buttom);\n"
"\n"
"    valid = 0.0;\n"
"    if (uvs.x < 0.0) valid = 1.0;\n"
"    if (uvs.y < 0.0) valid = 1.0;\n"
"    if (uvs.x >= 1.0) valid = 1.0;\n"
"    if (uvs.y >= 1.0) valid = 1.0;\n"
"    if (abs(pos_left.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos_right.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos_top.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos_buttom.z - pos.z) > minDeltaZ * pos.z) valid = 1.0;\n"
"    if (abs(pos.z) < 0.01) valid = 1.0;\n"
"    if (valid > 0.0) pos = vec4(1.0, 1.0, 1.0, 0.0);\n"
"    else pos = vec4(pos.xyz, 1.0);\n"
"    vec4 worldPosition = transformationMatrix * pos;\n"
"    gl_Position = projectionMatrix * cameraMatrix * worldPosition;\n"
"\n"
"    sampledUvs = uvs.xy;\n"
"    outPos = pos;\n"
"}\n";

static const char* fragment_shader_text_picking =
"#version 130\n"
"\n"
"in float valid;\n"
"in vec4  outPos;\n"
"in vec2 sampledUvs;\n"
"in vec3 normal;\n"
"out vec4 output_rgb;\n"
"out vec3 output_xyz;\n"
"\n"
"uniform sampler2D textureSampler;\n"
"uniform float pickedID;\n"
"uniform float shaded;\n"
"\n"
"void main(void) {\n"
"    if (valid > 0.0) discard;\n"
"    output_rgb = vec4(1.0);\n"
"    output_xyz = outPos.xyz;\n"
"}\n";



#define NORMAL_WINDOW_SIZE 3

using namespace rs2;

matrix4 frustum(float left, float right, float bottom, float top, float zNear, float zFar, float ox, float oy)
{
    matrix4 res;
    auto& m = res.mat;
    float zDelta = (zFar-zNear);
    float w = (right-left);
    float h = (top-bottom);

    m[0][0]=2.0f*zNear/w;
    m[0][1]=0.0f;
    m[0][2]=2.0f*(right+left)/w;
    m[0][3]=0.0f;
    
    m[1][0]=0.0f;
    m[1][1]=2.0f*zNear/h;
    m[1][2]=2.0f*(top+bottom)/h;
    m[1][3]=0.0f;

    m[2][0]=ox;
    m[2][1]=oy;
    m[2][2]=-(zFar+zNear)/zDelta;
    m[2][3]=-(2.f * zNear*zFar)/zDelta;

    m[3][0]=0.0f;
    m[3][1]=0.0f;
    m[3][2]=-1.0f;
    m[3][3]=0.0f;

    return res;
}

namespace librealsense
{
    namespace gl
    {
        union Fp32
        {
            uint32_t u;
            float f;
        };

        float halfToNativeIeee(uint16_t value)
        {
            /*
            * https://gist.github.com/rygorous/2144712
            * Public domain, by Fabian "ryg" Giesen
            */
            const Fp32 magic = { (254U - 15U) << 23 };
            const Fp32 was_infnan = { (127U + 16U) << 23 };
            Fp32 out;

            out.u = (value & 0x7FFFU) << 13;   /* exponent/mantissa bits */
            out.f *= magic.f;                  /* exponent adjust */
            if (out.f >= was_infnan.f)         /* make sure Inf/NaN survive */
            {
                out.u |= 255U << 23;
            }
            out.u |= (value & 0x8000U) << 16;  /* sign bit */

            return out.f;
        }

        pointcloud_shader::pointcloud_shader(std::unique_ptr<shader_program> shader)
            : _shader(std::move(shader))
        {
            init();
        }

        pointcloud_shader::pointcloud_shader(const char* vertex_shader, const char* fragment_shader)
        {
            _shader = shader_program::load(
                vertex_shader,
                fragment_shader,
                "position", "textureCoords",
                "output_rgb", "output_xyz");

            init();
        }

        void pointcloud_shader::init()
        {
            _transformation_matrix_location = _shader->get_uniform_location("transformationMatrix");
            _projection_matrix_location = _shader->get_uniform_location("projectionMatrix");
            _camera_matrix_location = _shader->get_uniform_location("cameraMatrix");

            _width_location = _shader->get_uniform_location("imageWidth");
            _height_location = _shader->get_uniform_location("imageHeight");
            _min_delta_z_location = _shader->get_uniform_location("minDeltaZ");
            _picked_id_location = _shader->get_uniform_location("pickedID");
            _shaded_location = _shader->get_uniform_location("shaded");

            auto texture0_sampler_location = _shader->get_uniform_location("textureSampler");
            auto texture1_sampler_location = _shader->get_uniform_location("positionsSampler");
            auto texture2_sampler_location = _shader->get_uniform_location("uvsSampler");

            _shader->begin();
            _shader->load_uniform(_min_delta_z_location, 0.05f);
            _shader->load_uniform(texture0_sampler_location, texture_slot());
            _shader->load_uniform(texture1_sampler_location, geometry_slot());
            _shader->load_uniform(texture2_sampler_location, uvs_slot());
            _shader->end();
        }

        void pointcloud_shader::begin() { _shader->begin(); }
        void pointcloud_shader::end() { _shader->end(); }

        void pointcloud_shader::set_mvp(const matrix4& model,
            const matrix4& view,
            const matrix4& projection)
        {
            _shader->load_uniform(_transformation_matrix_location, model);
            _shader->load_uniform(_camera_matrix_location, view);
            _shader->load_uniform(_projection_matrix_location, projection);
        }

        void pointcloud_shader::set_image_size(int width, int height)
        {
            _shader->load_uniform(_width_location, (float)width);
            _shader->load_uniform(_height_location, (float)height);
        }

        void pointcloud_shader::set_picked_id(float pid)
        {
            _shader->load_uniform(_picked_id_location, pid);
        }

        void pointcloud_shader::set_shaded(bool shaded)
        {
            _shader->load_uniform(_shaded_location, shaded);
        }

        void pointcloud_shader::set_min_delta_z(float min_delta_z)
        {
            _shader->load_uniform(_min_delta_z_location, min_delta_z);
        }

        void pointcloud_renderer::cleanup_gpu_resources()
        {
            glDeleteTextures(1, &color_tex);
            glDeleteTextures(1, &xyz_tex);

            _origin_rgba_pbo.reset();
            _rgba_pbo.reset();
            _xyz_pbo.reset();

            _shader.reset();
            _pick_shader.reset();
            _model.reset();
            _vertex_texture.reset();
            _uvs_texture.reset();
            _viz.reset();
            _fbo.reset();
        }

        pointcloud_renderer::~pointcloud_renderer()
        {
            perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            });
        }

        void pointcloud_renderer::create_gpu_resources()
        {
            if (glsl_enabled())
            {
                _shader = std::make_shared<pointcloud_shader>(vertex_shader_text, fragment_shader_text);
                _pick_shader = std::make_shared<pointcloud_shader>(vertex_shader_text_picking, fragment_shader_text_picking);

                _vertex_texture = std::make_shared<rs2::texture_buffer>();
                _uvs_texture = std::make_shared<rs2::texture_buffer>();

                obj_mesh mesh = make_grid(_width, _height);
                _model = vao::create(mesh);

                _fbo = std::make_shared<fbo>(1, 1);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                _viz = std::make_shared<rs2::texture_visualizer>();

                glGenTextures(1, &color_tex);
                glGenTextures(1, &xyz_tex);

                _xyz_pbo.init(NORMAL_WINDOW_SIZE, NORMAL_WINDOW_SIZE);
                _rgba_pbo.init(1, 1);
                _origin_rgba_pbo.init(1, 1);
            }
        }

        pointcloud_renderer::pointcloud_renderer() 
            : stream_filter_processing_block("Pointcloud Renderer")
        {
            register_option(OPTION_FILLED, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));
            register_option(OPTION_SHADED, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));
            register_option(OPTION_MOUSE_X, std::make_shared<librealsense::float_option>(option_range{ 0, 10000, 0, 0 }));
            register_option(OPTION_MOUSE_Y, std::make_shared<librealsense::float_option>(option_range{ 0, 10000, 0, 0 }));
            register_option(OPTION_MOUSE_PICK, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 1, 0 }));   

            register_option(OPTION_PICKED_X, std::make_shared<librealsense::float_option>(option_range{ -1000, 1000, 0, 0 }));
            register_option(OPTION_PICKED_Y, std::make_shared<librealsense::float_option>(option_range{ -1000, 1000, 0, 0 }));
            register_option(OPTION_PICKED_Z, std::make_shared<librealsense::float_option>(option_range{ -1000, 1000, 0, 0 }));
            register_option(OPTION_PICKED_ID, std::make_shared<librealsense::float_option>(option_range{ 0, 32, 1, 0 }));

            register_option(OPTION_SELECTED, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));
            register_option(OPTION_ORIGIN_PICKED, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));

            register_option(OPTION_NORMAL_X, std::make_shared<librealsense::float_option>(option_range{ -1.f, 1.f, 0, 0 }));
            register_option(OPTION_NORMAL_Y, std::make_shared<librealsense::float_option>(option_range{ -1.f, 1.f, 0, 0 }));
            register_option(OPTION_NORMAL_Z, std::make_shared<librealsense::float_option>(option_range{ -1.f, 1.f, 0, 0 }));

            register_option(OPTION_SCALE_FACTOR, std::make_shared<librealsense::float_option>(option_range{ 1, 4, 0, 1 }));
            
            _filled_opt = &get_option(OPTION_FILLED);
            _mouse_x_opt = &get_option(OPTION_MOUSE_X);
            _mouse_y_opt = &get_option(OPTION_MOUSE_Y);
            _mouse_pick_opt = &get_option(OPTION_MOUSE_PICK);
            _picked_x_opt = &get_option(OPTION_PICKED_X);
            _picked_y_opt = &get_option(OPTION_PICKED_Y);
            _picked_z_opt = &get_option(OPTION_PICKED_Z);
            _picked_id_opt = &get_option(OPTION_PICKED_ID);
            _selected_opt = &get_option(OPTION_SELECTED);
            _shaded_opt = &get_option(OPTION_SHADED);
            _origin_picked_opt = &get_option(OPTION_ORIGIN_PICKED);
            _normal_x_opt = &get_option(OPTION_NORMAL_X);
            _normal_y_opt = &get_option(OPTION_NORMAL_Y);
            _normal_z_opt = &get_option(OPTION_NORMAL_Z);
            _scale_factor_opt = &get_option(OPTION_SCALE_FACTOR);

            initialize();
        }

        rs2::frame pointcloud_renderer::process_frame(const rs2::frame_source& src, const rs2::frame& f)
        {
            //scoped_timer t("pointcloud_renderer");
            if (auto points = f.as<rs2::points>())
            {
                perform_gl_action([&]()
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    while (_durations.size())
                    {
                        auto front = _durations.front();
                        if (now - front > std::chrono::seconds(1))
                            _durations.pop_front();
                        else break;
                    }
                    _durations.push_back(now);

                    //scoped_timer t("pointcloud_renderer.gl");

                    GLint curr_tex;
                    glGetIntegerv(GL_TEXTURE_BINDING_2D, &curr_tex);

                    clear_gl_errors();

                    auto vf_profile = f.get_profile().as<video_stream_profile>();
                    int width = vf_profile.width();
                    int height = vf_profile.height();

                    rs2::matrix4 p = get_matrix(RS2_GL_MATRIX_PROJECTION);
                    rs2::matrix4 v = get_matrix(RS2_GL_MATRIX_CAMERA);
                    rs2::matrix4 f = get_matrix(RS2_GL_MATRIX_TRANSFORMATION);

                    
                    
                    if (glsl_enabled())
                    {
                        if (_width != width || _height != height)
                        {
                            obj_mesh mesh = make_grid(width, height);
                            _model = vao::create(mesh);

                            _width = width;
                            _height = height;
                        }

                        auto points_f = (frame_interface*)points.get();

                        uint32_t vertex_tex_id = 0;
                        uint32_t uv_tex_id = 0;

                        bool error = false;
                        
                        if (auto g = dynamic_cast<gpu_points_frame*>(points_f))
                        {
                            if (!g->get_gpu_section().input_texture(0, &vertex_tex_id) ||
                                !g->get_gpu_section().input_texture(1, &uv_tex_id)
                                ) error = true;
                        }
                        else
                        {
                            _vertex_texture->upload(points, RS2_FORMAT_XYZ32F);
                            vertex_tex_id = _vertex_texture->get_gl_handle();

                            _uvs_texture->upload(points, RS2_FORMAT_Y16);
                            uv_tex_id = _uvs_texture->get_gl_handle();
                        }

                        if (!error)
                        {
                            auto render_pc = [this, &vf_profile, curr_tex, vertex_tex_id, uv_tex_id]
                                (std::shared_ptr<pointcloud_shader> shader, const rs2::matrix4& p){
                                shader->begin();
                                shader->set_mvp(get_matrix(
                                    RS2_GL_MATRIX_TRANSFORMATION),
                                    get_matrix(RS2_GL_MATRIX_CAMERA),
                                    p
                                );
                                shader->set_image_size(vf_profile.width(), vf_profile.height());

                                shader->set_picked_id(_picked_id_opt->query());
                                shader->set_shaded(_shaded_opt->query());

                                glActiveTexture(GL_TEXTURE0 + shader->texture_slot());
                                glBindTexture(GL_TEXTURE_2D, curr_tex);

                                glActiveTexture(GL_TEXTURE0 + shader->geometry_slot());
                                glBindTexture(GL_TEXTURE_2D, vertex_tex_id);

                                glActiveTexture(GL_TEXTURE0 + shader->uvs_slot());
                                glBindTexture(GL_TEXTURE_2D, uv_tex_id);

                                if (_filled_opt->query() > 0.f) _model->draw();
                                else _model->draw_points();

                                glActiveTexture(GL_TEXTURE0 + shader->texture_slot());

                                shader->end();
                            };

                            auto mouse_pick = [this, &render_pc](rs2::float2 xy, rs2::float2 wh, pbo<rgba8>& pbo,
                                bool fetch_xyz, rs2::float3* xyz, rs2::float3* normal)
                                {
                                auto gl_p = get_matrix(RS2_GL_MATRIX_PROJECTION);

                                auto near_plane = gl_p(2,3)/(gl_p(2,2)-1);
                                auto far_plae   = gl_p(2,3)/(gl_p(2,2)+1);
                                auto bottom     = near_plane * (gl_p(1,2)-1)/gl_p(1,1);
                                auto top        = near_plane * (gl_p(1,2)+1)/gl_p(1,1);
                                auto left       = near_plane * (gl_p(0,2)-1)/gl_p(0,0);
                                auto right      = near_plane * (gl_p(0,2)+1)/gl_p(0,0);
                                auto ox = 0.f;
                                auto oy = 0.f;

                                ox = (xy.x / wh.x) * 2 - 1;
                                oy = (xy.y / wh.y) * 2 - 1;

                                auto p = frustum(left/(0.5f*wh.x), right/(0.5f*wh.x),
                                    bottom / (0.5f * wh.y), top / (0.5f * wh.y), near_plane, far_plae,
                                    ox * (0.5f * wh.x), oy * (0.5f * wh.y));

                                auto fbo_width = 3;
                                auto fbo_height = 3;

                                _fbo->set_dims(fbo_width, fbo_height);

                                glBindFramebuffer(GL_FRAMEBUFFER, _fbo->get());
                                glDrawBuffer(GL_COLOR_ATTACHMENT0);

                                _fbo->createTextureAttachment(color_tex);

                                glBindTexture(GL_TEXTURE_2D, xyz_tex);
                                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, fbo_width, fbo_height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, xyz_tex, 0);

                                glBindTexture(GL_TEXTURE_2D, 0);

                                _fbo->bind();

                                GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
                                glDrawBuffers(2, attachments);

                                glClearColor(0, 0, 0, 0);
                                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                            
                                render_pc(_pick_shader, p);

                                _fbo->unbind();

                                glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo->get());
                                check_gl_error();

                                glReadBuffer(GL_COLOR_ATTACHMENT0);
                                check_gl_error();

                                rgba8 rgba { 0, 0, 0, 0 };

                                {
                                    //scoped_timer t("rgba");
                                    pbo.query(&rgba, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE);
                                }

                                const auto size = NORMAL_WINDOW_SIZE * NORMAL_WINDOW_SIZE;
                                auto center = size / 2;
                                const auto half_window = NORMAL_WINDOW_SIZE / 2;

                                std::vector<half4> pos_halfs(size);

                                if (fetch_xyz) 
                                {
                                    glReadBuffer(GL_COLOR_ATTACHMENT1);
                                    check_gl_error();

                                    //scoped_timer t("xyz");
                                    _xyz_pbo.query(pos_halfs.data(), 0, 0, 
                                        NORMAL_WINDOW_SIZE, NORMAL_WINDOW_SIZE, 
                                        GL_RGBA, GL_HALF_FLOAT);

                                    if (rgba.a > 0)
                                    {
                                        std::vector<rs2::float3> pos_floats(size);
                                        for (int i = 0; i < size; i++)
                                        {
                                            auto pos = pos_halfs[i];
                                            auto x1 = halfToNativeIeee(pos.x);
                                            auto y1 = halfToNativeIeee(pos.y);
                                            auto z1 = halfToNativeIeee(pos.z);
                                            pos_floats[i] = { x1, y1, z1 };
                                        }

                                        auto up = pos_floats[center - half_window * NORMAL_WINDOW_SIZE];
                                        auto down = pos_floats[center + half_window * NORMAL_WINDOW_SIZE];
                                        auto left = pos_floats[center - half_window];
                                        auto right = pos_floats[center + half_window];
                                        auto pos = pos_floats[center];

                                        auto right_left = lerp(pos - left, right - pos, 0.5f);
                                        auto down_up = lerp(pos - up, down - pos, 0.5f) * (-1.f);
                                        *normal = cross(right_left, down_up).normalized();
                                        *xyz = pos;
                                    }
                                }

                                glReadBuffer(GL_NONE);
                                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                                return (rgba.a > 0);
                            };

                            int32_t vp[4];
                            glGetIntegerv(GL_VIEWPORT, vp);
                            check_gl_error();

                            float scale = _scale_factor_opt->query();

                            auto fbo_width = vp[2] / scale;
                            auto fbo_height = vp[3] / scale;
                            const auto viewport_x = vp[0] / scale;
                            const auto viewport_y = vp[1] / scale;

                            const auto do_mouse_pick = _mouse_pick_opt->query() > 0.f;

                            _picked_id_opt->set(0.f);

                            if (do_mouse_pick) {

                                auto x = _mouse_x_opt->query() - viewport_x;
                                auto y = _mouse_y_opt->query() - viewport_y;
                                
                                rs2::float3 normal, pos;

                                if (mouse_pick(rs2::float2{ x, y }, 
                                        rs2::float2{ fbo_width, fbo_height }, 
                                        _rgba_pbo, true, &pos, &normal))
                                {
                                    _normal_x_opt->set(normal.x);
                                    _normal_y_opt->set(normal.y); 
                                    _normal_z_opt->set(normal.z);

                                    _picked_x_opt->set(pos.x);
                                    _picked_y_opt->set(pos.y);
                                    _picked_z_opt->set(pos.z);

                                    _picked_id_opt->set(1.f);
                                }
                                
                                _mouse_pick_opt->set(0.f);

                                glActiveTexture(GL_TEXTURE0 + _shader->texture_slot());

                                rs2::float3 org{ 0.f, 0.f, 0.f };
                                rs2::float2 origin = translate_3d_to_2d(org, p, v, f, vp);
                                origin.x = origin.x - vp[0];
                                origin.y = origin.y - vp[1];
                                _origin_picked_opt->set(0.f);

                                if (mouse_pick(origin, rs2::float2{ float(vp[2]), float(vp[3]) }, 
                                    _origin_rgba_pbo, false, nullptr, nullptr))
                                {
                                    _origin_picked_opt->set(1.f);
                                }
                            }

                            render_pc(_shader, get_matrix(RS2_GL_MATRIX_PROJECTION));
                        }
                    }
                    else
                    {
                        glMatrixMode(GL_MODELVIEW);
                        glPushMatrix();

                        auto t = get_matrix(RS2_GL_MATRIX_TRANSFORMATION);
                        auto v = get_matrix(RS2_GL_MATRIX_CAMERA);

                        glLoadMatrixf(v * t);

                        auto vertices = points.get_vertices();
                        auto tex_coords = points.get_texture_coordinates();

                        glBindTexture(GL_TEXTURE_2D, curr_tex);

                        // Fixed-function path (macOS legacy 2.1, or when GLSL is off).
                        // Texcoords must be set *before* the vertex they apply to.
                        glEnable(GL_TEXTURE_2D);
                        if (_filled_opt->query() > 0.f)
                        {
                            glBegin(GL_QUADS);

                            const auto threshold = 0.05f;
                            for (int x = 0; x < width - 1; ++x) {
                                for (int y = 0; y < height - 1; ++y) {
                                    auto a = y * width + x, b = y * width + x + 1, c = (y + 1)*width + x, d = (y + 1)*width + x + 1;
                                    if (vertices[a].z && vertices[b].z && vertices[c].z && vertices[d].z
                                        && std::abs(vertices[a].z - vertices[b].z) < threshold && std::abs(vertices[a].z - vertices[c].z) < threshold
                                        && std::abs(vertices[b].z - vertices[d].z) < threshold && std::abs(vertices[c].z - vertices[d].z) < threshold) {
                                        glTexCoord2fv(tex_coords[a]); glVertex3fv(vertices[a]);
                                        glTexCoord2fv(tex_coords[b]); glVertex3fv(vertices[b]);
                                        glTexCoord2fv(tex_coords[d]); glVertex3fv(vertices[d]);
                                        glTexCoord2fv(tex_coords[c]); glVertex3fv(vertices[c]);
                                    }
                                }
                            }
                            glEnd();
                        }
                        else
                        {
                            glBegin(GL_POINTS);
                            for (int i = 0; i < points.size(); i++)
                            {
                                if (vertices[i].z)
                                {
                                    glTexCoord2fv(tex_coords[i]);
                                    glVertex3fv(vertices[i]);
                                }
                            }
                            glEnd();
                        }

                        glPopMatrix();
                    }
                }); 
            }

            return f;
        }
    }
}
