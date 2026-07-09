// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "print_helpers.h"
#include "rosbag_content.h"
#include "files_container.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <regex>
#include <thread>

#include <os.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include "imgui_impl_opengl3.h"
#include <imgui_internal.h>
#include <realsense_imgui.h>
#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

using namespace rosbag_inspector;

files_container files; // Container of loaded files
std::map<std::string, uint64_t> num_topics_to_show;

class gui_window
{
public:
    gui_window(const std::string& title, uint32_t width, uint32_t height) :
        _window(glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr)),
        _first_frame(true),
        _w(0), _h(0)
    {
        if (!_window)
            throw std::runtime_error("Could not open OpenGL window, please check your graphic drivers");
        init_window();
    }
    operator bool()
    {
        int display_w, display_h;
        glfwGetFramebufferSize(_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (!_first_frame)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(_window);
        }
        bool res = !glfwWindowShouldClose(_window);

        glfwPollEvents();
        glfwGetWindowSize(_window, &_w, &_h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        RsImGui::PushNewFrame();
        _first_frame = false;
        return res;
    }
    int width() const
    {
        return _w;
    }
    int height() const
    {
        return _h;
    }
private:
    void init_window()
    {
        glfwMakeContextCurrent(_window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        glfwSetWindowUserPointer(_window, &files);

        glfwSetDropCallback(_window, [](GLFWwindow* w, int count, const char** paths)
        {
            auto f = reinterpret_cast<files_container*>(glfwGetWindowUserPointer(w));

            if (count <= 0)
                return;

            f->AddFiles(std::vector<std::string>(paths, paths + count));
        });

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(_window, true);
#ifdef __APPLE__
        // macOS gives a legacy 2.1 context (no version hints requested);
        // the backend's Apple default "#version 150" needs 3.2 core.
        ImGui_ImplOpenGL3_Init("#version 120");
#else
        ImGui_ImplOpenGL3_Init();
#endif

        glfwSetScrollCallback(_window, [](GLFWwindow * w, double xoffset, double yoffset)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseWheel += yoffset;
        });

    }

    GLFWwindow* _window;
    int _w, _h;
    bool _first_frame;
};

ImVec4 from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool consistent_color = false)
{
    auto res = ImVec4(r / (float)255, g / (float)255, b / (float)255, a / (float)255);
    return res;
}
static const ImVec4 light_blue = from_rgba(0, 174, 239, 255, true); // Light blue color for selected elements such as play button glyph when paused
static const ImVec4 regular_blue = from_rgba(0, 115, 200, 255, true); // Checkbox mark, slider grabber
static const ImVec4 light_grey = from_rgba(0xc3, 0xd5, 0xe5, 0xff, true); // Text
static const ImVec4 dark_window_background = from_rgba(9, 11, 13, 255);
static const ImVec4 almost_white_bg = from_rgba(230, 230, 230, 255, true);
static const ImVec4 black = from_rgba(0, 0, 0, 255, true);
static const ImVec4 transparent = from_rgba(0, 0, 0, 0, true);
static const ImVec4 white = from_rgba(0xff, 0xff, 0xff, 0xff, true);
static const ImVec4 scrollbar_bg = from_rgba(14, 17, 20, 255);
static const ImVec4 scrollbar_grab = from_rgba(54, 66, 67, 255);
static const ImVec4 grey{ 0.5f,0.5f,0.5f,1.f };
static const ImVec4 dark_grey = from_rgba(30, 30, 30, 255);
static const ImVec4 sensor_header_light_blue = from_rgba(80, 99, 115, 0xff);
static const ImVec4 sensor_bg = from_rgba(36, 44, 51, 0xff);
static const ImVec4 redish = from_rgba(255, 46, 54, 255, true);
static const ImVec4 dark_red = from_rgba(200, 46, 54, 255, true);
static const ImVec4 button_color = from_rgba(62, 77, 89, 0xff);
static const ImVec4 header_window_bg = from_rgba(36, 44, 54, 0xff);
static const ImVec4 header_color = from_rgba(62, 77, 89, 255);
static const ImVec4 title_color = from_rgba(27, 33, 38, 255);
static const ImVec4 device_info_color = from_rgba(33, 40, 46, 255);
static const ImVec4 yellow = from_rgba(229, 195, 101, 255, true);
static const ImVec4 green = from_rgba(0x20, 0xe0, 0x20, 0xff, true);

void draw_menu_bar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load File..."))
            {
                auto ret = file_dialog_open(rs2::file_dialog_mode::open_file, "ROS-bag\0*.bag\0", NULL, NULL);
                if (ret)
                {
                    files.AddFiles({ ret });
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

int draw_files_left_panel(int flags)
{
    ImGui::BeginChild("Loaded Files", ImVec2(150, 0), true, flags);
    static int selected = 0;

    for (int i = 0; i < files.size(); i++)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_Header, grey);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_grey);
        if (ImGui::Selectable(files[i].file_name.c_str(), selected == i, 0, ImVec2(100, 0)))
        {
            selected = i;
            num_topics_to_show.clear();
        }
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(450.0f);
            ImGui::TextUnformatted("Right click for more options");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        std::string label = tmpstringstream() << "file operations " << files[i].file_name.c_str();
        if (ImGui::BeginPopupContextItem(label.c_str()))
        {
            ImGui::Text("Choose operation");
            ImGui::Separator();
            if (ImGui::BeginMenu("Sort by"))
            {
                if (ImGui::MenuItem("Hardware Timestamp"))
                {

                }
                if (ImGui::MenuItem("Arrival Timestamp"))
                {

                }
                if (ImGui::MenuItem("System Timestamp"))
                {

                }
                if (ImGui::MenuItem("Capture Timestamp"))
                {

                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
        label = "x##" + files[i].file_name;
        ImGui::SameLine();
        ImGui::SetCursorPosX(120);
        if (ImGui::SmallButton(label.c_str()))
        {
            int next = files.remove_file(i);
            if (selected >= i)
                selected = std::max(0, selected - 1);
            i = next - 1; //since we will "i++" next
        }
    }
    ImGui::EndChild();

    return selected;
}
void draw_bag_content(rosbag_content& bag, int flags)
{
    ImGui::BeginChild("Bag Content", ImVec2(0, 0), false, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, white);
    std::ostringstream oss;
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "Path: " << bag.path).c_str());
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "Bag Version: " << bag.version).c_str());
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "Duration: " << pretty_time(bag.file_duration)).c_str());
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "Size: " << bag.size << " MB").c_str());
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "Compression: " << bag.compression_info.compression_type).c_str());
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "uncompressed: " << bag.compression_info.uncompressed).c_str());
    ImGui::Text("\t%s", std::string(tmpstringstream() << std::left << std::setw(20) << "compressed: " << bag.compression_info.compressed).c_str());
    if (ImGui::CollapsingHeader("Topics"))
    {
        for (auto&& topic_to_message_type : bag.topics_to_message_types)
        {
            std::string topic = topic_to_message_type.first;
            std::vector<std::string> messages_types = topic_to_message_type.second;
            std::ostringstream oss;
            int max_topic_len = 100;
            oss << std::left << std::setw(max_topic_len) << topic
                << " " << std::left << std::setw(10) << messages_types.size() << std::setw(6) << std::string(" msg") + (messages_types.size() > 1 ? "s" : "")
                << ": " << std::left << std::setw(40) << messages_types.front() << std::endl;
            std::string line = oss.str();
            auto pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos({ pos.x + 20, pos.y });
            if (ImGui::CollapsingHeader(line.c_str()))
            {
                rosbag::View messages(bag.bag, rosbag::TopicQuery(topic));
                uint64_t count = 0;
                constexpr uint64_t num_next_items_to_show = 10;
                num_topics_to_show[topic] = std::max(num_topics_to_show[topic], num_next_items_to_show);
                uint64_t max = num_topics_to_show[topic];
                auto win_pos = ImGui::GetWindowPos();
                ImGui::SetWindowPos({ win_pos.x + 20, win_pos.y });
                for (auto&& m : messages)
                {
                    count++;
                    ImGui::Columns(2, "Message", true);
                    ImGui::Separator();
                    ImGui::Text("Timestamp"); ImGui::NextColumn();
                    ImGui::Text("Content"); ImGui::NextColumn();
                    ImGui::Separator();
                    ImGui::Text("%s", pretty_time(std::chrono::nanoseconds(m.getTime().toNSec())).c_str()); ImGui::NextColumn();
                    ImGui::Text("%s", bag.instanciate_and_cache(m, count).c_str());
                    ImGui::Columns(1);
                    ImGui::Separator();
                    if (count >= max)
                    {
                        int left = messages.size() - max;
                        if (left > 0)
                        {
                            ImGui::Text("... %d more messages", left);
                            ImGui::SameLine();
                            std::string label = tmpstringstream() << "Show More ##" << topic;
                            if (ImGui::Button(label.c_str()))
                            {
                                num_topics_to_show[topic] += 10;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                ImGui::SetWindowPos(win_pos);
            }

            if (ImGui::IsItemHovered())
            {
                if (topic.size() > max_topic_len)
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(450.0f);
                    ImGui::Text("%s", topic.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }

        }
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();

}
void draw_error_modal()
{
    if (files.has_errors())
    {
        ImGui::PushStyleColor(ImGuiCol_TitleBg, redish);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, redish);
        ImGui::OpenPopup("Error");
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            std::string msg = tmpstringstream() << "Error: " << files.get_last_error();
            ImGui::TextWrapped("%s", msg.c_str());
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                files.clear_errors();
            }
            ImGui::EndPopup();
            ImGui::PopStyleColor(2);
        }
    }
}

enum sort_type
{
    hw_time,
    capture_time,
    system_time,
    arrival_time
};

inline void sort(sort_type m_sort_type, const std::string& in, const std::string& out)
{
    //TODO: Save sort type to file
    rosbag::Bag bag_in(in);
    rosbag::Bag bag_out(out);

    rosbag::View entire_bag_view(bag_in);
    for (auto&& m : entire_bag_view)
    {
        // 1. Write frame with new timestamp
        // 2. Get all metadata of this frame and write it

        if (m.isType<sensor_msgs::Image>())
        {

        }
        if (m.isType<sensor_msgs::Imu>())
        {

        }
        if (m.isType<geometry_msgs::Transform>())
        {

        }
        if (m.isType<geometry_msgs::Twist>() || m.isType<geometry_msgs::Accel>())
        {

        }
        if (m.getTime() == rs2rosinternal::TIME_MIN)
        {
            bag_out.write(m.getTopic(), m.getTime(), m);
            continue;
        }

        if (m.getTopic().find("/option/") != std::string::npos)
        {

        }
    }
}

int main(int argc, const char** argv) try
{
    if (!glfwInit())
    {
        return 1;
    }
    gui_window window("RealSense Rosbag Inspector", 1280, 720);

    while (window)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, grey);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, light_grey);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, light_grey);
        ImGui::PushStyleColor(ImGuiCol_Text, black);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_Header, from_rgba(0, 115, 210, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, from_rgba(20, 130, 230, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, from_rgba(0, 115, 210, 255));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, from_rgba(37, 40, 48, 255));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.286f, 0.298f, 0.549f, 1.0f));
        ImGuiStyle& style = ImGui::GetStyle();
        style.FramePadding.x = 10;
        style.FramePadding.y = 5;
        int flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

        static bool open = true;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize({ float(window.width()), float(window.height()) }, ImGuiCond_FirstUseEver);
        style.WindowRounding = 10.0f; // Set the window rounding
        if (ImGui::Begin("Rosbag Inspector", nullptr, flags | ImGuiWindowFlags_MenuBar))
        {
            draw_menu_bar();
            int selected = draw_files_left_panel(flags);

            ImGui::SameLine();
            if (files.size() > 0)
            {
                draw_bag_content(files[selected], flags);
            }

            draw_error_modal();
        }
        ImGui::End();
        ImGui::PopStyleColor(11);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // Cleanup
    RsImGui::PopNewFrame();
    glfwTerminate();
    return 0;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( ... )
{
    std::cerr << "some error" << std::endl;
    return EXIT_FAILURE;
}

#ifdef WIN32
int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow

) {
    main(0, nullptr);
}
#endif
