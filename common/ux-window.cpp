// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 Intel Corporation. All Rights Reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glad/glad.h>
#include <rs-config.h>
#include "ux-window.h"

#include <imgui.h>
#include <implot.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <realsense_imgui.h>

#include "device-model.h"

#include <rsutils/os/special-folder.h>
#include "os.h"

// We use STB image to load the splash-screen from memory
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// int-rs-splash.hpp contains the PNG image from res/int-rs-splash.png
#include "res/int-rs-splash.hpp"
#include "res/icon.h"

#include "ux-alignment.h"

#include <opengl3.h>

#include <iostream>

void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW Driver Error: " << description << "\n";
}

namespace rs2
{
    void GLAPIENTRY MessageCallback(GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam)
    {
        if (type == GL_DEBUG_TYPE_ERROR)
        {
            fprintf(stderr, "GL CALLBACK: ** GL ERROR ** type = 0x%x, severity = 0x%x, message = %s\n",
                    type, severity, message);
        }
    }

    void prepare_config_file()
    {
        config_file::instance().set_default(configurations::update::allow_rc_firmware, false);
        config_file::instance().set_default(configurations::update::recommend_calibration, true);
        config_file::instance().set_default(configurations::update::recommend_updates, true);
        config_file::instance().set_default(configurations::update::sw_updates_url, server_versions_db_url);
        config_file::instance().set_default(configurations::update::sw_updates_official_server, true);

        config_file::instance().set_default(configurations::window::is_fullscreen, false);
        config_file::instance().set_default(configurations::window::saved_pos, false);
        config_file::instance().set_default(configurations::window::saved_size, false);
        config_file::instance().set_default(configurations::window::font_size, 16);

        config_file::instance().set_default(configurations::viewer::is_measuring, false);
        config_file::instance().set_default(configurations::viewer::log_to_console, true);
        config_file::instance().set_default(configurations::viewer::log_to_file, false);
        config_file::instance().set_default(configurations::viewer::log_severity, 2);
        config_file::instance().set_default(configurations::viewer::metric_system, true);
        config_file::instance().set_default(configurations::viewer::ground_truth_r, 2500);
        config_file::instance().set_default(configurations::viewer::dashboard_open, true);

        config_file::instance().set_default(configurations::record::compression_mode, 2); // Let the device decide
        config_file::instance().set_default(configurations::record::file_save_mode, 0); // Auto-select name

        config_file::instance().set_default(configurations::performance::show_fps, false);
        config_file::instance().set_default(configurations::performance::vsync, true);
        config_file::instance().set_default(configurations::performance::occlusion_invalidation, true);

        config_file::instance().set_default(configurations::ply::mesh, true);
        config_file::instance().set_default(configurations::ply::use_normals, false);
        config_file::instance().set_default(configurations::ply::encoding, configurations::ply::binary);

        config_file::instance().set_default(configurations::viewer::commands_xml, "./Commands.xml");
        config_file::instance().set_default(configurations::viewer::hwlogger_xml, "./HWLoggerEvents.xml");

        std::string path;
        try
        {
            path = rsutils::os::get_special_folder( rsutils::os::special_folder::user_documents );
        }
        catch (const std::exception&)
        {
            std::string msg = "Failed to get Documents folder";
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
            path = "";
        }
        config_file::instance().set_default(configurations::viewer::log_filename, path + "librealsense.log");
        config_file::instance().set_default(configurations::record::default_path, path);

        config_file::instance().set_nested_default(configurations::dds::enable_dds, false);
        config_file::instance().set_nested_default(configurations::dds::domain_id, 0);
        
#ifdef __APPLE__

        config_file::instance().set_default(configurations::performance::font_oversample, 2);
        config_file::instance().set_default(configurations::performance::enable_msaa, true);
        config_file::instance().set_default(configurations::performance::msaa_samples, 4);
        // On Mac-OS, mixing OpenGL 2 with OpenGL 3 is not supported by the driver
        // while this can be worked-around, this will take more development time,
        // so for now Macs should not use the GLSL stuff
        config_file::instance().set_default(configurations::performance::glsl_for_processing, false);
        config_file::instance().set_default(configurations::performance::glsl_for_rendering, false);
#else
        auto vendor = (const char*)glGetString(GL_VENDOR);
        auto renderer = (const char*)glGetString(GL_RENDERER);
        auto version = (const char*)glGetString(GL_VERSION);
        auto glsl = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

        bool use_glsl = false;

        // Absolutely arbitrary list of manufacturers that are likely to benefit from GLSL optimisation
        if (starts_with(rsutils::string::to_lower(vendor), "intel") ||
            starts_with(rsutils::string::to_lower(vendor), "ati") ||
            starts_with(rsutils::string::to_lower(vendor), "nvidia"))
        {
            use_glsl = true;
        }

        // Double-check that GLSL 1.3+ is supported
        if (starts_with(rsutils::string::to_lower(vendor), "1.1") || starts_with(rsutils::string::to_lower(vendor), "1.2"))
        {
            use_glsl = false;
        }

        if (use_glsl)
        {
            config_file::instance().set_default(configurations::performance::show_skybox, true);
            config_file::instance().set_default(configurations::performance::font_oversample, 2);
            config_file::instance().set_default(configurations::performance::enable_msaa, false);
            config_file::instance().set_default(configurations::performance::msaa_samples, 2);
            config_file::instance().set_default(configurations::performance::glsl_for_processing, true);
            config_file::instance().set_default(configurations::performance::glsl_for_rendering, true);
            config_file::instance().set_default(configurations::viewer::shading_mode, 2);
        }
        else
        {
            config_file::instance().set_default(configurations::performance::show_skybox, false);
            config_file::instance().set_default(configurations::performance::font_oversample, 1);
            config_file::instance().set_default(configurations::performance::enable_msaa, false);
            config_file::instance().set_default(configurations::performance::msaa_samples, 2);
            config_file::instance().set_default(configurations::performance::glsl_for_processing, false);
            config_file::instance().set_default(configurations::performance::glsl_for_rendering, false);
            config_file::instance().set_default(configurations::viewer::shading_mode, 0);
        }
#endif

        // Since we have seen on several laptops models that using GLSL for processing cause a memory leak decided to disable it by default
        // Users can still enable it if they wish
        config_file::instance().set_default(configurations::performance::glsl_for_processing, false);
    }

    void ux_window::reload()
    {
        _reload = true;
    }

    void ux_window::refresh()
    {
        if (_use_glsl_proc) rs2::gl::shutdown_processing();
        rs2::gl::shutdown_rendering();

        _use_glsl_render = config_file::instance().get(configurations::performance::glsl_for_rendering);
        _use_glsl_proc = config_file::instance().get(configurations::performance::glsl_for_processing);

        rs2::gl::init_rendering(_use_glsl_render);
        if (_use_glsl_proc) rs2::gl::init_processing(_win, _use_glsl_proc);
    }

    void ux_window::link_hovered()
    {
        _link_hovered = true;
    }

    void ux_window::cross_hovered()
    {
        _cross_hovered = true;
    }

    void ux_window::setup_icon()
    {
        GLFWimage icon[4];

        int x, y, comp;

        auto icon_16 = stbi_load_from_memory(icon_16_png_data, (int)icon_16_png_size, &x, &y, &comp, false);
        icon[0].width = x; icon[0].height = y;
        icon[0].pixels = icon_16;

        auto icon_24 = stbi_load_from_memory(icon_24_png_data, (int)icon_24_png_size, &x, &y, &comp, false);
        icon[1].width = x; icon[1].height = y;
        icon[1].pixels = icon_24;

        auto icon_64 = stbi_load_from_memory(icon_64_png_data, (int)icon_64_png_size, &x, &y, &comp, false);
        icon[2].width = x; icon[2].height = y;
        icon[2].pixels = icon_64;

        auto icon_256 = stbi_load_from_memory(icon_256_png_data, (int)icon_256_png_size, &x, &y, &comp, false);
        icon[3].width = x; icon[3].height = y;
        icon[3].pixels = icon_256;

        glfwSetWindowIcon(_win, 4, icon);

        stbi_image_free(icon_16);
        stbi_image_free(icon_24);
        stbi_image_free(icon_64);
        stbi_image_free(icon_256);
    }

   
    void ux_window::open_window()
    {
        if (_win)
        {
            rs2::gl::shutdown_rendering();
            if (_use_glsl_proc) rs2::gl::shutdown_processing();

            ImGui::GetIO().Fonts->ClearFonts();  // To be refactored into Viewer theme object
            ImPlot::DestroyContext();
            RsImGui::PopNewFrame();
            glfwDestroyWindow(_win);
            glfwDestroyCursor(_hand_cursor);
            glfwDestroyCursor(_cross_cursor);
            glfwTerminate();
        }

        if (!glfwInit())
            exit(1);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        #ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS
        #endif	

        glfwSetErrorCallback(glfw_error_callback);

        _hand_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
        _cross_cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

        {
            glfwWindowHint(GLFW_VISIBLE, 0);
            auto ctx = glfwCreateWindow(640, 480, "Offscreen Context", nullptr, nullptr);
            if (!ctx) throw std::runtime_error("Could not initialize offscreen context!");
            glfwMakeContextCurrent(ctx);

            gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

            // OpenGL 2.1 backward-compatibility fixes.
            // On macOS, the compatibility profile is OpenGL 2.1 + extensions.
            if (!GLAD_GL_VERSION_3_0 && !GLAD_GL_ARB_vertex_array_object) {
                if (GLAD_GL_APPLE_vertex_array_object) {
                    glBindVertexArray = glBindVertexArrayAPPLE;
                    glDeleteVertexArrays = glDeleteVertexArraysAPPLE;
                    glGenVertexArrays = glGenVertexArraysAPPLE;
                    glIsVertexArray = glIsVertexArrayAPPLE;
                } else {
                    throw std::runtime_error("OpenGL 3.0 or ARB_vertex_array_object extension required!");
                }
            }

            prepare_config_file();

            glfwDestroyWindow(ctx);
        }

        _use_glsl_render = config_file::instance().get(configurations::performance::glsl_for_rendering);
        _use_glsl_proc = config_file::instance().get(configurations::performance::glsl_for_processing);

        _enable_msaa = config_file::instance().get(configurations::performance::enable_msaa);
        _msaa_samples = config_file::instance().get(configurations::performance::msaa_samples);

        _fullscreen = config_file::instance().get(configurations::window::is_fullscreen);

        rs2_error* e = nullptr;
        _title_str = rsutils::string::from() << _title << " v" << api_version_to_string(rs2_get_api_version(&e));
        auto debug = is_debug();
        if (debug)
        {
            _title_str = _title_str + ", DEBUG";
        }

        _width = 1024;
        _height = 768;

        // Dynamically adjust new window size (by detecting monitor resolution)
        auto primary = glfwGetPrimaryMonitor();
        if (primary)
        {
            const auto mode = glfwGetVideoMode(primary);
            if (_fullscreen)
            {
                _width = mode->width;
                _height = mode->height;
            }
            else
            {
                _width = int(mode->width * 0.7f);
                _height = int(mode->height * 0.7f);
            }
        }
        
        if (_enable_msaa)
            glfwWindowHint(GLFW_SAMPLES, _msaa_samples);
        
        glfwWindowHint(GLFW_VISIBLE, 0);

        // Create GUI Windows
        _win = glfwCreateWindow(_width, _height, _title_str.c_str(),
            (_fullscreen ? primary : nullptr), nullptr);
        if (!_win)
            throw std::runtime_error("Could not open OpenGL window, please check your graphic drivers or use the textual SDK tools");

        if (config_file::instance().get(configurations::window::saved_pos))
        {
            int x = config_file::instance().get(configurations::window::position_x);
            int y = config_file::instance().get(configurations::window::position_y);

            int count;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            if (count > 0)
            {
                bool legal_position = false;
                for (int i = 0; i < count; i++)
                {
                    auto rect = get_monitor_rect(monitors[i]);
                    if (rect.contains({ (float)x, (float)y }))
                    {
                        legal_position = true;
                    }
                }
                if (legal_position) glfwSetWindowPos(_win, x, y);
            }
        }

        if (config_file::instance().get(configurations::window::saved_size))
        {
            int w = config_file::instance().get(configurations::window::width);
            int h = config_file::instance().get(configurations::window::height);
            glfwSetWindowSize(_win, w, h);
            
            if (config_file::instance().get(configurations::window::maximized))
                glfwMaximizeWindow(_win);
        }

        glfwMakeContextCurrent(_win);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

        if (glDebugMessageCallback)
        {
            // During init, enable debug output
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback(MessageCallback, 0);
        }

        glfwSetWindowPosCallback(_win, [](GLFWwindow* w, int x, int y)
        {
            config_file::instance().set(configurations::window::saved_pos, true);
            config_file::instance().set(configurations::window::position_x, x);
            config_file::instance().set(configurations::window::position_y, y);
        });

        glfwSetWindowSizeCallback( _win, []( GLFWwindow * window, int width, int height ) {
            if( width > 0 && height > 0 )
            {
                config_file::instance().set( configurations::window::saved_size, true );
                config_file::instance().set( configurations::window::width, width );
                config_file::instance().set( configurations::window::height, height );
                config_file::instance().set( configurations::window::maximized,
                                             glfwGetWindowAttrib( window, GLFW_MAXIMIZED ) );
            }
        } );

        setup_icon();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;   // added in order to prevents cursor chang when interacting with other element (when nedded remove the flag accordingly)
        ImGui_ImplGlfw_InitForOpenGL(_win, true);
        ImGui_ImplOpenGL3_Init();

        if (_use_glsl_render)
            _2d_vis = std::make_shared<visualizer_2d>(std::make_shared<splash_screen_shader>());

        // Load fonts to be used with the ImGui - TODO move to RAII
        imgui_easy_theming(_font_dynamic, _font_18, _monofont, font_size);

        // Register for UI-controller events
        glfwSetWindowUserPointer(_win, this);


        glfwSetCursorPosCallback(_win, [](GLFWwindow* w, double cx, double cy)
        {
            ImGui_ImplGlfw_CursorPosCallback(w, cx, cy); // Forward the cursor position to ImGui
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));
            data->_mouse.cursor = { (float)cx / data->_scale_factor,
                (float)cy / data->_scale_factor };
        });
        glfwSetMouseButtonCallback(_win, [](GLFWwindow* w, int button, int action, int mods)
        {
            ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);// Forward the event to ImGui's GLFW implementation
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));
            data->_mouse.mouse_down[0] = (button == GLFW_MOUSE_BUTTON_1) && (action != GLFW_RELEASE);
            data->_mouse.mouse_down[1] = (button == GLFW_MOUSE_BUTTON_2) && (action != GLFW_RELEASE);
        });
        glfwSetScrollCallback(_win, [](GLFWwindow * w, double xoffset, double yoffset)
        {
            ImGui_ImplGlfw_ScrollCallback(w, xoffset, yoffset); // Forwards scroll events to ImGui
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));
            data->_mouse.mouse_wheel = static_cast<int>(yoffset);
            data->_mouse.ui_wheel += static_cast<int>(yoffset);
        });

        glfwSetDropCallback(_win, [](GLFWwindow* w, int count, const char** paths)
        {
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));

            if (count <= 0) return;

            for (int i = 0; i < count; i++)
            {
                data->on_file_drop(paths[i]);
            }
        });

        glfwSetKeyCallback(_win, ImGui_ImplGlfw_KeyCallback);

        rs2::gl::init_rendering(_use_glsl_render);
        if (_use_glsl_proc) rs2::gl::init_processing(_win, _use_glsl_proc);

        glfwShowWindow(_win);
        glfwFocusWindow(_win);

        _show_fps = config_file::instance().get(configurations::performance::show_fps);
        _vsync = config_file::instance().get(configurations::performance::vsync);

        // Prepare the splash screen and do some initialization in the background
        int x, y, comp;
        auto r = stbi_load_from_memory(splash, (int)splash_size, &x, &y, &comp, false);
        _splash_tex.upload_image(x, y, r);
        stbi_image_free(r);
    }

    ux_window::ux_window(const char* title, context &ctx) :
        _win(nullptr), _width(0), _height(0), _output_height(0),
        _font_dynamic(nullptr), _font_18(nullptr), _monofont(nullptr), font_size(16), _app_ready(false),
        _first_frame(true), _query_devices(true), _missing_device(false),
        _hourglass_index(0), _dev_stat_message{}, _keep_alive(true), _title(title), _ctx(ctx)
    {
        open_window();

        // Apply initial UI state
        reset();
    }

    void ux_window::add_on_load_message(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(_on_load_message_mtx);
        _on_load_message.push_back(msg);
    }

    void ux_window::imgui_config_pop()
    {
        ImGui::PopFont();
        ImGui::End();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        end_frame();

        glPopMatrix();
    }

    void ux_window::imgui_config_push()
    {
        glPushMatrix();
        glViewport(0, 0, _fb_width, _fb_height);
        glClearColor(0.036f, 0.044f, 0.051f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        glLoadIdentity();
        glOrtho(0, _width, _height, 0, -1, +1);

        // Fade-in the logo
        auto opacity = smoothstep(float(_splash_timer.get_elapsed_ms()), 100.f, 2500.f);
        auto ox = 0.7f - smoothstep(float(_splash_timer.get_elapsed_ms()), 200.f, 1900.f) * 0.4f;
        auto oy = 0.5f;
        auto power = std::sin(smoothstep(float(_splash_timer.get_elapsed_ms()), 150.f, 2200.f) * 3.14f) * 0.96f;

        if (_use_glsl_render)
        {
            auto shader = ((splash_screen_shader*)&_2d_vis->get_shader());
            shader->begin();
            shader->set_power(power);
            shader->set_ray_center(float2{ ox, oy });
            shader->end();
            _2d_vis->draw_texture(_splash_tex.get_gl_handle(), opacity);
        }
        else
        {
            _splash_tex.show({ 0.f,0.f,float(_width),float(_height) }, opacity);
        }

        std::string hourglass = u8"\uf251";
        static rsutils::time::periodic_timer every_200ms(std::chrono::milliseconds(200));
        bool do_200ms = every_200ms;
        if (_query_devices && do_200ms)
        {
            _missing_device = _ctx.query_devices(RS2_PRODUCT_LINE_ANY_INTEL).size() == 0;
            _hourglass_index = (_hourglass_index + 1) % 4;

            if (!_missing_device)
            {
                _dev_stat_message = u8"\uf287 RealSense device detected.";
                _query_devices = false;
            }
        }

        hourglass[2] += _hourglass_index;

        auto flags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar;

        auto text_color = light_grey;
        text_color.w = opacity;
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5, 5 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, transparent);
        ImGui::SetNextWindowPos({ (float)_width / 2 - 150, (float)_height / 2 + 70 });

        ImGui::SetNextWindowSize({ (float)_width, (float)_height });
        ImGui::Begin("Splash Screen Banner", nullptr, flags);
        ImGui::PushFont(_font_18);

        ImGui::Text("%s   Loading %s...", hourglass.c_str(), _title_str.c_str());
    }

    // Check that the graphic subsystem is valid and start a new frame
    ux_window::operator bool()
    {
        end_frame();

        if (_show_fps)
        {
            std::stringstream temp_title;
            temp_title << _title_str;

            auto fps = ImGui::GetIO().Framerate;

            temp_title << ", FPS: " << fps;
            glfwSetWindowTitle(_win, temp_title.str().c_str());
        }

        // Yield the CPU
        if (!_vsync)
        {
            std::this_thread::yield();
            glfwSwapInterval(0);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto res = !glfwWindowShouldClose(_win);


        if (_first_frame)
        {
            assert(!_first_load.joinable()); // You must call to reset() before initiate new thread


            _first_load = std::thread([&]() {
                while (_keep_alive && !_app_ready)
                {
                    try
                    {
                        _app_ready = on_load();
                    }
                    catch (...)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for connect event and retry
                    }
                }
            });
        }

        // If we are just getting started, render the Splash Screen instead of normal UI
        while (res && (!_app_ready || _splash_timer.get_elapsed_ms() < 2000.f))
        {
            res = !glfwWindowShouldClose(_win);
            glfwPollEvents();

            begin_frame();

            if (_first_frame)
            {
                _is_ui_aligned = is_gui_aligned(_win);
                _first_frame = false;
            }

            imgui_config_push();

            {
                std::lock_guard<std::mutex> lock(_on_load_message_mtx);
                if (_on_load_message.empty())
                {
                    ImGui::Text("%s", _dev_stat_message.c_str());
                }
                else if (!_on_load_message.empty())
                {
                    ImGui::Text("%s", _dev_stat_message.c_str());
                    for (auto& msg : _on_load_message)
                    {
                        auto is_last_msg = (msg == _on_load_message.back());
                        if (is_last_msg)
                            ImGui::Text("%s", msg.c_str());
                        else if (!is_last_msg)
                            ImGui::Text("%s", msg.c_str());
                    }
                }
            }

            imgui_config_pop();


            // Yield the CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // reset graphic pipe
        begin_frame();

        if (_link_hovered)
            glfwSetCursor(_win, _hand_cursor);
        else if (_cross_hovered)
            glfwSetCursor(_win, _cross_cursor);
        else
            glfwSetCursor(_win, nullptr);
        _cross_hovered = false;
        _link_hovered = false;
        _hovers_any_input_window = false;

        return res;
    }

    ux_window::~ux_window()
    {
        if (_first_load.joinable())
        {
            _keep_alive = false;
            _first_load.join();
        }

        end_frame();

        try
        {
            rs2::gl::shutdown_rendering();
            if (_use_glsl_proc) rs2::gl::shutdown_processing();
        }
        catch( ... )
        {
        }

        ImGui::GetIO().Fonts->ClearFonts();  // To be refactored into Viewer theme object
        ImPlot::DestroyContext();
        RsImGui::PopNewFrame();
        glfwDestroyWindow(_win);

        glfwDestroyCursor(_hand_cursor);
        glfwDestroyCursor(_cross_cursor);

        glfwTerminate();
    }

    void ux_window::begin_frame()
    {
        glfwPollEvents();

        int state = glfwGetKey(_win, GLFW_KEY_F8);
        if (state == GLFW_PRESS)
        {
            _fullscreen_pressed = true;
        }
        else
        {
            if (_fullscreen_pressed)
            {
                _fullscreen = !_fullscreen;
                config_file::instance().set(configurations::window::is_fullscreen, _fullscreen);
                open_window();
            }
            _fullscreen_pressed = false;
        }

        if (_reload)
        {
            open_window();
            _reload = false;
            on_reload_complete();
        }

        int w = _width; int h = _height;

        glfwGetWindowSize(_win, &_width, &_height);

        // Set minimum size 1
        if (_width <= 0) _width = 1;
        if (_height <= 0) _height = 1;

        int fw = _fb_width;
        int fh = _fb_height;

        glfwGetFramebufferSize(_win, &_fb_width, &_fb_height);

        // Set minimum size 1
        if (_fb_width <= 0) _fb_width = 1;
        if (_fb_height <= 0) _fb_height = 1;

        if (fw != _fb_width || fh != _fb_height)
        {
            std::string msg = rsutils::string::from() << "Framebuffer size changed to " << _fb_width << " x " << _fb_height;
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
        }

        auto sf = _scale_factor;

        // Update the scale factor each frame
        // based on resolution and physical display size
        _scale_factor = static_cast<float>(pick_scale_factor(_win));
        _width = static_cast<int>(_width / _scale_factor);
        _height = static_cast<int>(_height / _scale_factor);

        if (w != _width || h != _height)
        {
            std::string msg = rsutils::string::from() << "Window size changed to " << _width << " x " << _height;
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
        }

        if (_scale_factor != sf)
        {
            std::string msg = rsutils::string::from() << "Scale Factor is now " << _scale_factor;
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
        }

        // Reset ImGui state
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        ImGui::GetIO().MouseWheel = _mouse.ui_wheel;
        _mouse.ui_wheel = 0.f;
        
        RsImGui::PushNewFrame();
    }

    void ux_window::begin_viewport()
    {
        // Rendering
        glViewport(0, 0,
            static_cast<int>(ImGui::GetIO().DisplaySize.x * _scale_factor),
            static_cast<int>(ImGui::GetIO().DisplaySize.y * _scale_factor));

        if (_enable_msaa) glEnable(GL_MULTISAMPLE);
        else glDisable(GL_MULTISAMPLE);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void ux_window::end_frame()
    {
        if (!_first_frame)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(_win);
            _mouse.mouse_wheel = 0;
        }
    }

    void ux_window::reset()
    {
        if (_first_load.joinable())
        {
            _keep_alive = false;
            _first_load.join();
            _keep_alive = true;
        }

        _query_devices = true;
        _missing_device = false;
        _hourglass_index = 0;
        _first_frame = true;
        _app_ready = false;
        _splash_timer.reset();
        _dev_stat_message = u8"\uf287 Please connect Intel RealSense device!";

        {
            std::lock_guard<std::mutex> lock(_on_load_message_mtx);
            _on_load_message.clear();
        }
    }

}
