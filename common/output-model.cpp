// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#include <glad/glad.h>
#include "output-model.h"
#include <rs-config.h>
#include "ux-window.h"
#include "device-model.h"
#include "os.h"

#include <imgui_internal.h>
#include <librealsense2/hpp/rs_internal.hpp>

#include <fstream>
#include <iterator>

using namespace rs2;
using namespace rsutils::string;

void output_model::thread_loop()
{
    while (!to_stop)
    {
        if( enable_firmware_logs )
        {
            // User activated FW logs. Parse XML file and start collecting logs.
            std::vector<rs2::device> dev_copy;
            {
                std::lock_guard<std::mutex> lock(devices_mutex);
                dev_copy = devices;
            }
            std::vector< std::pair< rs2::firmware_logger, bool > > loggers; // Logger and is initialized successfully (can parse logs)
            for( auto & dev : dev_copy )
            {
                if( auto logger = dev.as< rs2::firmware_logger >() )
                {
                    bool can_parse = false;
                    std::string hwlogger_xml = config_file::instance().get( configurations::viewer::hwlogger_xml );
                    std::ifstream f( hwlogger_xml.c_str() );
                    if( f.good() )
                    {
                        try
                        {
                            std::string str( ( std::istreambuf_iterator< char >( f ) ),
                                             std::istreambuf_iterator< char >() );
                            can_parse = logger.init_parser( str );
                        }
                        catch( const std::exception & ex )
                        {
                            add_log( RS2_LOG_SEVERITY_WARN,
                                     __FILE__,
                                     __LINE__,
                                     rsutils::string::from()
                                         << "Invalid Hardware Logger XML at '" << hwlogger_xml << "': " << ex.what()
                                         << "\nEither configure valid XML or remove it" );
                            continue;  // Don't try to get log entries for this device
                        }
                    }

                    logger.start_collecting();
                    loggers.push_back( { logger, can_parse } );
                }
            }

            while( enable_firmware_logs )
            {
                for( auto & it : loggers )
                {
                    auto & fwlogger = it.first;
                    bool can_parse = it.second;
                    try
                    {
                        auto message = fwlogger.create_message();
                        while( fwlogger.get_firmware_log( message ) )
                        {
                            auto parsed = fwlogger.create_parsed_message();
                            auto parsed_ok = false;

                            if( can_parse )
                            {
                                if( fwlogger.parse_log( message, parsed ) )
                                {
                                    parsed_ok = true;

                                    std::string module_print = "[" + parsed.module_name() + "]";
                                    if( module_print == "[Unknown]" )
                                        module_print.clear();  // Some devices don't support FW log modules

                                    add_log( message.get_severity(),
                                             parsed.file_name(),
                                             parsed.line(),
                                             rsutils::string::from() << "FW-LOG [" << parsed.thread_name() << "]"
                                                                     << module_print << parsed.message() );
                                }
                            }

                            if( ! parsed_ok )
                            {
                                std::stringstream ss;
                                for( auto & elem : message.data() )
                                    ss << std::setfill( '0' ) << std::setw( 2 ) << std::hex
                                       << static_cast< int >( elem ) << " ";
                                add_log( message.get_severity(), __FILE__, 0, ss.str() );
                            }

                            if( !enable_firmware_logs && fwlogger.get_number_of_fw_logs() == 0 )
                                break;
                        }
                    }
                    catch( const std::exception & ex )
                    {
                        add_log( RS2_LOG_SEVERITY_WARN,
                                 __FILE__,
                                 __LINE__,
                                 rsutils::string::from() << "Failed to fetch firmware logs: " << ex.what() );
                    }
                }
                                    
                // FW define the logs polling intervals to be no less than 100msec to cope with limited resources.
                // At the same time 100 msec should guarantee no log drops
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            }

            // User dectivated FW logs. Stop collecting logs.
            for( auto & fwlogger : loggers )
                fwlogger.first.stop_collecting();
        }

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

output_model::~output_model()
{
    to_stop = 1;
    fw_logger.join();
}

output_model::output_model() : fw_logger([this](){ thread_loop(); }) , incoming_log_queue(100)
{
    is_output_open = config_file::instance().get_or_default(
            configurations::viewer::output_open, false);
    search_line = config_file::instance().get_or_default(
            configurations::viewer::search_term, std::string(""));
    is_dashboard_open = config_file::instance().get_or_default(
        configurations::viewer::dashboard_open, true );
    
    if (search_line != "") search_open = true;

    available_dashboards["Frame Drops per Second"] = [&](std::string name){
        return std::make_shared<frame_drops_dashboard>(name, &number_of_drops, &total_frames);
    };

    auto front = available_dashboards.begin();
    dashboards.push_back(front->second(front->first));
}

bool output_model::round_indicator(ux_window& win, std::string icon,
    int count, ImVec4 color, std::string tooltip, bool& highlighted, std::string suffix)
{
    std::stringstream ss;
    ss << icon;
    if (count > 0) ss << " " << count << suffix;
    auto size = ImGui::CalcTextSize(ss.str().c_str());

    if (count == 0 || (!is_output_open && !highlighted)) {
        color = dark_sensor_bg;
        ImGui::PushStyleColor(ImGuiCol_Text, header_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, header_color);
    }
    else
    {
        if (!highlighted)
        {
            color = saturate(color, 0.3f);
            ImGui::PushStyleColor(ImGuiCol_Text, white);
        }
        else
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
    }

    auto pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled({ pos.x, pos.y + 3 },
                { pos.x + size.x + 15, pos.y + 27 }, ImColor(color), 12, ImDrawFlags_RoundCornersNone);

    auto res = ImGui::Button(ss.str().c_str(), ImVec2(size.x + 15, 28));
    if (count > 0 && ImGui::IsItemHovered())
    {
        highlighted = true;
        win.link_hovered();
        RsImGui::CustomTooltip("%s", tooltip.c_str());
    }
    else highlighted = false;

    ImGui::PopStyleColor(2);

    return res;
}

void output_model::open(ux_window& win)
{
    is_output_open = true;
    config_file::instance().set(configurations::viewer::output_open, true);
    default_log_h = static_cast<int>((win.height() - 100) / 2);
    new_log = true;
}

void output_model::draw(ux_window& win, rect view_rect, device_models_list & device_models)
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, scrollbar_bg);

    auto x = view_rect.x;
    auto y = view_rect.y;
    auto w = view_rect.w;
    auto h = view_rect.h;

    auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, sensor_bg);
    ImGui::PushStyleColor(ImGuiCol_Button, transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);

    ImGui::PushFont(win.get_font());
    ImGui::SetNextWindowPos({ x, y });
    ImGui::SetNextWindowSize({ w, h });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));

    ImGui::Begin("Output", nullptr, flags);

    ImGui::SetCursorPosX(w - 31);
    if (!is_output_open)
    {
        if (ImGui::Button(textual_icons::circle_chevron_up, ImVec2(28, 28)))
        {
            open(win);
        }
        if (ImGui::IsItemHovered())
        {
            win.link_hovered();
            RsImGui::CustomTooltip("%s", "Open Debug Console Window");
        }

        if (default_log_h.value() != 36)
            default_log_h = 36;
    }
    else
    {
        if (ImGui::Button(textual_icons::circle_chevron_down, ImVec2(28, 28)))
        {
            is_output_open = false;
            config_file::instance().set(configurations::viewer::output_open, false);
            default_log_h = 36;
            search_open = false;
            set_number_open = false;
        }
        if (ImGui::IsItemHovered())
        {
            win.link_hovered();
            RsImGui::CustomTooltip("%s", "Collapse Debug Console Window");
        }

        int h_val = (int)((win.height() - 100) / 2);
        if (default_log_h.value() != h_val)
            default_log_h = h_val;
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(5);

    if (errors_selected) errors_highlighted = true;
    if (round_indicator(win, std::string(textual_icons::times_circle), number_of_errors, redish, "Instances of logged errors", errors_highlighted))
    {
        errors_selected = !errors_selected;
        open(win);
    }
    ImGui::SameLine();

    if (warnings_selected) warnings_highlighted = true;
    if (round_indicator(win, std::string(textual_icons::exclamation_triangle), number_of_warnings, orange, "Instances of logged warnings", warnings_highlighted))
    {
        warnings_selected = !warnings_selected;
        open(win);
    }
    ImGui::SameLine();

    if (info_selected) info_highlighted = true;
    if (round_indicator(win,std::string(textual_icons::info_circle), number_of_info, greenish, "Instances of logged info messages", info_highlighted))
    {
        info_selected = !info_selected;
        open(win);
    }
    ImGui::SameLine();

    if( ! is_output_open || search_open || set_number_open )
    {
        ImGui::PushStyleColor(ImGuiCol_Text, header_color);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
    }
    bool focus_search = false;
    if (ImGui::Button(textual_icons::search, ImVec2(28, 28)))
    {
        focus_search = true;
        search_open = true;
        set_number_open = false;  // Only one text box can be open at a time
        open(win);
    }
    if (ImGui::IsItemHovered())
    {
        win.link_hovered();
        RsImGui::CustomTooltip("%s", "Search through logs");
    }
    ImGui::SameLine();

    if( ImGui::Button(textual_icons::lock, ImVec2( 28, 28 ) ) )
    {
        search_open = false; // Only one text box can be open at a time
        set_number_open = true;
        open( win );
    }
    if( ImGui::IsItemHovered() )
    {
        RsImGui::CustomTooltip( "%s", "Set maximum number of log entries kept" );
    }
    ImGui::PopStyleColor(1);
    ImGui::SameLine();

    auto curr_x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(curr_x - 5);


    int percent = total_frames ? (int)(100 * ((double)number_of_drops / (total_frames))) : 0;

    std::stringstream ss;
    ss << textual_icons::tint;
    if (percent) ss << " " << percent << "%";
    auto size = ImGui::CalcTextSize(ss.str().c_str());

    char buff[1024];

    auto actual_search_width = w - size.x - 100 - curr_x;
    if (focus_search) search_width = (int)(actual_search_width);

    if (search_open && search_width.value() != actual_search_width)
        search_width = (int)(actual_search_width);

    // if (is_output_open && search_width < 1)
    // {
    //     search_open = true;
    // }

    if( search_open || set_number_open )
    {
        ImGui::PushFont(win.get_monofont());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::PushItemWidth(static_cast<float>(search_width));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);

        if( search_open )
        {
            memcpy( buff, search_line.c_str(), search_line.size() );
            buff[search_line.size()] = 0;
        }
        else if( set_number_open )
        {
            std::string tmp = std::to_string( max_notifications_kept );
            memcpy( buff, tmp.c_str(), tmp.size() );
            buff[tmp.size()] = 0;
        }

        if (ImGui::InputText("##SearchInLogs",buff, 1023))
        {
            if( search_open )
            {
                search_line = buff;
                config_file::instance().set(configurations::viewer::search_term, search_line);
            }
            else if( set_number_open )
            {
                int tmp = std::atoi( buff );
                if( tmp > 0 )
                    max_notifications_kept = tmp;
            }
        }
        if (focus_search) ImGui::SetKeyboardFocusHere();
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
        ImGui::PopFont();
    }

    ImGui::SetCursorPosX(w - size.x - 3 * 30);

    if (enable_firmware_logs)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
    }
    else
    {
        if (is_output_open)
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        else
            ImGui::PushStyleColor(ImGuiCol_Text, header_color);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
    }

    if (ImGui::Button(textual_icons::microchip, ImVec2(28, 28)))
    {
        enable_firmware_logs = !enable_firmware_logs;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
    {
        win.link_hovered();
        if (enable_firmware_logs) RsImGui::CustomTooltip("%s", "Disable Firmware Logs");
        else RsImGui::CustomTooltip("%s", "Enable Firmware Logs");
    }
    ImGui::SameLine();


    if (round_indicator(win, std::string(textual_icons::tint), percent, regular_blue, "Frame drops", drops_highlighted, "%"))
    {
        open(win);
    }

    if (is_output_open)
    {
        ImGui::SetCursorPos(ImVec2(3, 35));


        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, dark_sensor_bg);

        const float log_area_width = w - get_dashboard_width() - 2;

        ImGui::BeginChild("##LogArea",
            ImVec2(log_area_width, h - 38 - ImGui::GetTextLineHeightWithSpacing() - 1), true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar);

        bool copy_all = false;
        bool save_all = false;
        std::vector<std::string> output_strings;

        auto time_now = glfwGetTime();

        int i = 0;
        foreach_log([&](log_entry& log)
        {
            auto line = log.line;
            if (log.line_number)
            {
                line = log.filename.substr(log.filename.find_last_of("/\\") + 1) + ":";
                line += std::string( rsutils::string::from() << log.line_number) + " - ";
                line += log.line;
            }

            bool ok = false;
            if (info_selected || warnings_selected || errors_selected)
            {
                if (info_selected && log.severity <= RS2_LOG_SEVERITY_INFO) ok = true;
                if (warnings_selected && log.severity == RS2_LOG_SEVERITY_WARN) ok = true;
                if (errors_selected && log.severity >= RS2_LOG_SEVERITY_ERROR) ok = true;
            }
            else ok = true;

            if (search_line != "" && to_lower(line).find(to_lower(search_line)) == std::string::npos) ok = false;

            if (!ok) return;

            std::stringstream ss; ss << log.timestamp << " [" << rs2_log_severity_to_string(log.severity) << "] ";
            if (log.line_number) ss << log.filename << ":" << log.line_number;
            ss << " - " << log.line;
            std::string full = ss.str();

            ImGui::PushFont(win.get_monofont());
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);

            ImVec4 color = redish;
            if (log.severity >= RS2_LOG_SEVERITY_ERROR)
            {
                color = redish;
            }
            else if (log.severity >= RS2_LOG_SEVERITY_WARN)
            {
                color = orange;
            }
            else
            {
                color = greenish;
            }

            auto margin = ImGui::GetTextLineHeightWithSpacing() - ImGui::GetTextLineHeight();
            auto size = ImGui::CalcTextSize(line.c_str());

            auto t = single_wave(static_cast<float>(time_now - log.time_added + 0.3f)) * 0.2f;
            if (log.selected) t = 0.2f;

            auto pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled({ pos.x, pos.y },
                        { pos.x + log_area_width, pos.y + size.y + 2 * margin },
                        ImColor(alpha(saturate(color, 0.3f + t), 0.7f + t)));
            ImGui::GetWindowDrawList()->AddLine({ pos.x, pos.y + size.y + 2 * margin },
                        { pos.x + log_area_width, pos.y + size.y + 2 * margin }, ImColor(alpha(color, 0.5f)));

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::Text("%s", log.timestamp.c_str()); ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);

            std::string label = rsutils::string::from() << "##log_entry" << i++;
            ImGui::InputTextEx(label.c_str(),NULL,
                        (char*)line.data(),
                        static_cast<int>(line.size() + 1),
                        ImVec2(-1, size.y + margin),
                        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);

            ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
            ImGui::PushStyleColor(ImGuiCol_Text, black);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5,5));
            label = rsutils::string::from() << "##log_entry" << i << "_context_menu";
            if (ImGui::BeginPopupContextItem(label.c_str()))
            {
                log.selected = true;
                ImGui::PushFont(win.get_font());
                if (ImGui::Selectable("Copy Line")) {
                    glfwSetClipboardString(win, full.c_str());
                }
                if (ImGui::Selectable("Copy All")) {
                    copy_all = true;
                }
                if (ImGui::Selectable("Save As...")) {
                    save_all = true;
                }
                ImGui::PopFont();
                ImGui::EndPopup();
            }
            else log.selected = false;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            ImGui::PopStyleColor(3);
            ImGui::PopFont();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);

            output_strings.push_back(full);
        });

        std::stringstream ss;
        for (auto&& s : output_strings) ss << s << "\n";
        if (copy_all)
            glfwSetClipboardString(win, ss.str().c_str());

        if (save_all)
        {
            if (auto fn = file_dialog_open(file_dialog_mode::save_file, "Log File\0*.log\0", nullptr, nullptr))
            {
                std::ofstream out(fn);
                if (out.good())
                {
                    out << ss.str();
                }
                out.close();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();


        ImGui::SetCursorPos(ImVec2(7, h - ImGui::GetTextLineHeightWithSpacing() - 2));
        std::string minus_square_icon_str = std::string(rsutils::string::from()
                            << textual_icons::minus_square_o);
        ImGui::Text("%s", minus_square_icon_str.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(30, h - ImGui::GetTextLineHeightWithSpacing() - 4));


        ImGui::PushFont(win.get_monofont());
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
        ImGui::PushItemWidth( w - get_dashboard_width() - 30 );

        bool force_refresh = false;
        if (ImGui::IsWindowFocused() && (ImGui::GetIO().KeysDown[ImGuiKey_UpArrow] || ImGui::GetIO().KeysDown[ImGuiKey_DownArrow]))
        {
            if (commands_histroy.size())
            {
                if (ImGui::GetIO().KeysDown[ImGuiKey_UpArrow]) history_offset = (history_offset + 1) % commands_histroy.size();
                if (ImGui::GetIO().KeysDown[ImGuiKey_DownArrow]) history_offset = (history_offset - 1 + (int)commands_histroy.size()) % commands_histroy.size();
                command_line = commands_histroy[history_offset];

                force_refresh = true;
            }
        }

        if (ImGui::IsWindowFocused() && ImGui::GetIO().KeysDown[GLFW_KEY_TAB])
        {
            if (!autocomplete.size() || !starts_with(to_lower(autocomplete.front()), to_lower(command_line)))
            {
                std::string commands_xml = config_file::instance().get(configurations::viewer::commands_xml);
                std::ifstream f(commands_xml.c_str());
                if (f.good())
                {
                    std::string str((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

                    autocomplete.clear();
                    std::regex exp("Command Name=\"(\\w+)\"");
                    std::smatch res;
                    std::string::const_iterator searchStart(str.cbegin());
                    while (regex_search(searchStart, str.cend(), res, exp))
                    {
                        if (starts_with(to_lower(res[1]), to_lower(command_line)))
                            autocomplete.push_back(res[1]);
                        searchStart = res.suffix().first;
                    }
                }
            }
            if (autocomplete.size())
            {
                auto temp = autocomplete.front();
                autocomplete.pop_front();
                autocomplete.push_back(temp);

                if (starts_with(to_lower(temp), command_line))
                    command_line = to_lower(autocomplete.front());
                else
                    command_line = autocomplete.front();
                force_refresh = true;
            }
        }

        memcpy(buff, command_line.c_str(), command_line.size());
        buff[command_line.size()] = 0;

        int flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (force_refresh)
        {
            flags = ImGuiInputTextFlags_ReadOnly;
        }

        ImGui::PushStyleColor(ImGuiCol_FrameBg, scrollbar_bg);
        if (ImGui::InputText("##TerminalCommand", buff, 1023, flags))
        {

        }
        if (!command_focus && !new_log) command_line = buff;
        ImGui::PopStyleColor();
        if (command_focus || new_log) ImGui::SetKeyboardFocusHere();
        ImGui::PopFont();
        ImGui::PopStyleColor();

        if (ImGui::IsWindowFocused() && (ImGui::GetIO().KeysDown[GLFW_KEY_ENTER] || ImGui::GetIO().KeysDown[GLFW_KEY_KP_ENTER]))
        {
            if (commands_histroy.size() > 100) commands_histroy.pop_back();
            commands_histroy.push_front(command_line);
            run_command(command_line, device_models);
            command_line = "";
            command_focus = true;
        }
        else command_focus = false;

        if (ImGui::IsWindowFocused() && ImGui::GetIO().KeysDown[GLFW_KEY_ESCAPE])
        {
            command_line = "";
        }

        float child_height = 0;
        for( auto && dash : dashboards )
        {
            child_height += dash->get_height();
        }
        float new_dashboard_button_height = 40.f;
        child_height == 0 ? child_height = new_dashboard_button_height : child_height += 5;
        
        auto dashboard_width = get_dashboard_width();
        ImGui::SetCursorPos( ImVec2( w - dashboard_width, 35 ) );

        ImGui::BeginChild( "##StatsArea", ImVec2( dashboard_width - 3.f, h - 38 ), true );

        const ImVec2 collapse_dashboard_button_size = { 28, 28 };
        const int max_dashboard_width = (int)( ( 0.3f * w ) );
        const int min_dashboard_width = static_cast<int>(collapse_dashboard_button_size.x) + 2;

        if( is_dashboard_open )
        {
            if( ImGui::Button( textual_icons::circle_chevron_right, collapse_dashboard_button_size ) )  // close dashboard
            {
                is_dashboard_open = false;
                config_file::instance().set( configurations::viewer::dashboard_open, is_dashboard_open );
                default_dashboard_w = min_dashboard_width;
            }
            if( ImGui::IsItemHovered() )
            {
                RsImGui::CustomTooltip( "Collapse dashboard" );
            }

            // Animation of opening dashboard panel
            if( default_dashboard_w.value() != max_dashboard_width )
                default_dashboard_w = max_dashboard_width;
        }
        else
        {
            float cursor_pos_x = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX( 0 );

            if( ImGui::Button( textual_icons::circle_chevron_left, collapse_dashboard_button_size ) )  // open dashboard
            {
                is_dashboard_open = true;
                config_file::instance().set( configurations::viewer::dashboard_open, is_dashboard_open );
                default_dashboard_w = max_dashboard_width;
            }
            if( ImGui::IsItemHovered() )
            {
                RsImGui::CustomTooltip( "Open dashboard" );
            }
            ImGui::SetCursorPosX( cursor_pos_x );

            // Animation of closing dashboard panel
            if( default_dashboard_w.value() != min_dashboard_width )
                default_dashboard_w = min_dashboard_width;
        }

        auto top = 0;
        if( is_dashboard_open && dashboard_width == max_dashboard_width )
        {
            for( auto && dash : dashboards )
            {
                auto h = dash->get_height();
                auto r = rect{ 0.f, (float)top, get_dashboard_width() - 8.f, (float)h };
                dash->draw( win, r );
                top += h;
            }
        }

        dashboards.erase(std::remove_if(dashboards.begin(), dashboards.end(),
        [](std::shared_ptr<stream_dashboard> p){
            return p->closing();
        }), dashboards.end());

        bool can_add = false;
        for (auto&& kvp : available_dashboards)
        {
            auto name = kvp.first;
            auto it = std::find_if(dashboards.begin(), dashboards.end(),
            [name](std::shared_ptr<stream_dashboard> p){
                return p->get_name() == name;
            });
            if (it == dashboards.end()) can_add = true;
        }

        if (can_add)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            const auto new_dashboard_name = "new_dashboard";
            ImGui::SameLine();
            std::string add_dash_str = rsutils::string::from() << textual_icons::sort_asc << " Add Dashboard";
            if (ImGui::Button(add_dash_str.c_str(), ImVec2(-1, 25)))
            {
                ImGui::OpenPopup(new_dashboard_name);
            }

            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("Add one of the available stream dashboards to view");
                win.link_hovered();
            }

            ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
            ImGui::PushStyleColor(ImGuiCol_Text, black);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5,5));
            if (ImGui::BeginPopup(new_dashboard_name))
            {
                for (auto&& kvp : available_dashboards)
                {
                    auto name = kvp.first;
                    auto it = std::find_if(dashboards.begin(), dashboards.end(),
                    [name](std::shared_ptr<stream_dashboard> p){
                        return p->get_name() == name;
                    });
                    if (it == dashboards.end())
                    {
                        name = name + "##New";
                        bool selected = false;
                        if (ImGui::Selectable(name.c_str(), &selected))
                        {
                            dashboards.push_back(kvp.second(kvp.first));
                        }
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
        }


        ImGui::EndChild();

        ImGui::PopStyleColor();
    }
    else foreach_log([&](log_entry& log) {});


    ImGui::End();
    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar();
    ImGui::PopFont();

    {
        std::lock_guard<std::mutex> lock(devices_mutex);
        this->devices.clear();
        for (auto && dev_model : device_models)
            this->devices.push_back(dev_model->dev);
    }
}

void output_model::foreach_log(std::function<void(log_entry& line)> action)
{
    std::lock_guard<std::recursive_mutex> lock(m);

    // Process only the messages that are available upon invocation
    log_entry le;
    for (size_t len = 0; len < incoming_log_queue.size(); len++)
    {
        if (incoming_log_queue.try_dequeue(&le))
        {
            if (le.severity >= RS2_LOG_SEVERITY_ERROR) number_of_errors++;
            else if (le.severity >= RS2_LOG_SEVERITY_WARN) number_of_warnings++;
            else number_of_info++;

            notification_logs.push_back(le);
        }
    }

    // Limit the notification window
    while( notification_logs.size() > max_notifications_kept )
    {
        auto&& le = notification_logs.front();
        if (le.severity >= RS2_LOG_SEVERITY_ERROR) number_of_errors--;
        else if (le.severity >= RS2_LOG_SEVERITY_WARN) number_of_warnings--;
        else number_of_info--;
        notification_logs.pop_front();
    }

    for (auto&& l : notification_logs)
        action(l);

    if (new_log)
    {
        ImGui::SetScrollHereY();
        new_log = false;
    }
}

// Callback function must not include mutex
void output_model::add_log(rs2_log_severity severity, std::string filename, int line_number, std::string line)
{
    if (!line.size()) return;

    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer,sizeof(buffer),"%H:%M:%S",timeinfo);

    log_entry e;
    e.line = line;
    e.line_number = line_number;
    e.filename = filename;
    e.severity = severity;
    e.timestamp = buffer;
    e.time_added = glfwGetTime();

    incoming_log_queue.enqueue(std::move(e));
    new_log = true;
}

void output_model::run_command(std::string command, device_models_list & device_models)
{
    std::string opcode_error_as_string = "";
    try
    {
        if (to_lower(command) == "clear")
        {
            while (notification_logs.size() > 0)
            {
                auto&& le = notification_logs.front();
                if (le.severity >= RS2_LOG_SEVERITY_ERROR) number_of_errors--;
                else if (le.severity >= RS2_LOG_SEVERITY_WARN) number_of_warnings--;
                else number_of_info--;
                notification_logs.pop_front();
                for (auto& d : dashboards)
                    d->clear(true);
            }

            return;
        }

        if( user_defined_command( command, device_models ) )
            return;

        std::regex e( "([0-9A-Fa-f]{2}\\s)+" );

        if (std::regex_match(command, e))
        {
            add_log(RS2_LOG_SEVERITY_INFO, __FILE__, 0, rsutils::string::from() << "Trying to send " << command << "...");

            std::vector<uint8_t> raw_data;
            std::stringstream ss(command);
            std::string word;
            while (ss >> word)
            {
                std::stringstream converter;
                int temp;
                converter << std::hex << word;
                converter >> temp;
                raw_data.push_back(temp);
            }
            if (raw_data.empty())
                throw std::runtime_error("Invalid input!");

            bool found = false;
            for (auto&& dev : devices)
            {
                if (auto dbg = dev.as<rs2::debug_protocol>())
                {
                    found = true;
                    auto res = dbg.send_and_receive_raw_data(raw_data);

                    std::stringstream ss;
                    int i = 0;
                    for (auto& elem : res)
                    {
                        ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(elem) << " ";
                        i++;
                        if (i > 80)
                        {
                            ss << "\n";
                            i = 0;
                        }
                    }
                    add_log(RS2_LOG_SEVERITY_INFO, __FILE__, 0, ss.str());

                    return;
                }
            }

            if (!found)
            {
                add_log(RS2_LOG_SEVERITY_WARN, __FILE__, __LINE__, "No device is available to receive the command");
                return;
            }
        }

        std::string commands_xml = config_file::instance().get(configurations::viewer::commands_xml);
        std::ifstream f(commands_xml.c_str());
        if (f.good())
        {
            std::string str((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
            auto terminal_parser = rs2::terminal_parser(str);

            auto buffer = terminal_parser.parse_command(to_lower(command));

            std::stringstream ss; ss << command << " = ";
            for (auto& elem : buffer)
                 ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(elem) << " ";

            add_log(RS2_LOG_SEVERITY_INFO, __FILE__, 0, ss.str());

            bool found = false;
            for (auto&& dev : devices)
            {
                if (auto dbg = dev.as<rs2::debug_protocol>())
                {
                    found = true;
                    auto res = dbg.send_and_receive_raw_data(buffer);
                    if (res.data())
                    {
                        int8_t opcode = *res.data();
                        opcode_error_as_string = dbg.get_opcode_string(opcode);
                    }
                    std::string response = rsutils::string::from() << "\n" << terminal_parser.parse_response(to_lower(command), res);
                    add_log(RS2_LOG_SEVERITY_INFO, __FILE__, 0, response);
                }
            }

            if (!found)
                add_log(RS2_LOG_SEVERITY_WARN, __FILE__, __LINE__, "No device is available to receive the command");

            return;
        }

        add_log(RS2_LOG_SEVERITY_WARN, __FILE__, __LINE__, rsutils::string::from() << "Unrecognized command '" << command << "'");
    }
    catch(const std::exception& ex)
    {
        std::string error_string = rsutils::string::from() << ex.what();
        if (opcode_error_as_string != "")
            error_string = rsutils::string::from() << error_string << " (" << opcode_error_as_string << ")";
        add_log( RS2_LOG_SEVERITY_ERROR, __FILE__, __LINE__, error_string);
    }
}

bool output_model::user_defined_command( std::string command, device_models_list & device_models )
{
    bool user_defined_command_detected = false;
    bool user_defined_command_activated = false;

    // If a known command is detected , it will treated as a user_defined_command and will not be
    // passed to the FW commands check logic.
    // Note: For now we find the first device that supports the command and activate the command only on it.

    if( to_lower( command ) == "get-nest" )
    {
        user_defined_command_detected = true;

        for( auto && dev : devices )
        {
            if( auto dbg = dev.as< rs2::debug_protocol >() )
            {
                 // Verify minimal version for handling this command
                 std::vector< uint8_t > special_command
                     = { 'G', 'E', 'T', '-', 'N', 'E', 'S', 'T' };
                 auto res = dbg.send_and_receive_raw_data( special_command );
                 user_defined_command_activated = true;
            }
        }
    }

    // Log a warning if a known command was not activated
    if( user_defined_command_detected && ! user_defined_command_activated )
    {
        add_log( RS2_LOG_SEVERITY_WARN,
                 __FILE__,
                 __LINE__,
                 rsutils::string::from() << "None of the connected devices supports '" << command << "'" );
    }

    return user_defined_command_detected;
}


void output_model::update_dashboards(rs2::frame f)
{
    for (auto&& d : dashboards)
        d->add_frame(f);
}

void stream_dashboard::draw_dashboard(ux_window& win, rect& r)
{
    auto min_x = 0.f;
    auto max_x = 1.f;
    auto min_y = 0.f;
    auto max_y = 1.f;

    if (xy.size())
    {
        min_x = xy[0].first;
        max_x = xy[0].first;
        min_y = xy[0].second;
        max_y = xy[0].second;
        for (auto&& p : xy)
        {
            min_x = std::min(min_x, p.first);
            min_y = std::min(min_y, p.second);
            max_x = std::max(max_x, p.first);
            max_y = std::max(max_y, p.second);
        }
    }

    auto gap_y = max_y - min_y;
    auto gap_x = max_x - min_x;
    auto height_y = r.h - 2 * ImGui::GetTextLineHeight() - 10;
    auto ticks_y = ceil(height_y / ImGui::GetTextLineHeight());

    auto max_y_label_width = 0.f;
    for (int i = 0; i <= ticks_y; i++)
    {
        auto y = max_y - i * (gap_y / ticks_y);
        std::string y_label = rsutils::string::from() << std::fixed << std::setprecision(2) << y;
        auto size = ImGui::CalcTextSize(y_label.c_str());
        max_y_label_width = std::max(max_y_label_width,
            size.x);
    }

    auto pos = ImGui::GetCursorScreenPos();

    ImGui::PushStyleColor(ImGuiCol_Text, white);

    ImGui::GetWindowDrawList()->AddRectFilled({ pos.x, pos.y },
                { pos.x + r.w - 1, pos.y + get_height() - 1 }, ImColor(header_color));
    ImGui::GetWindowDrawList()->AddRect({ pos.x, pos.y },
                { pos.x + r.w, pos.y + get_height() }, ImColor(dark_sensor_bg));

    auto size = ImGui::CalcTextSize(name.c_str());
    float collapse_buton_h = 28.f + 3.f;  // Dashboard button size plus some spacing
    ImGui::SetCursorPos(ImVec2( r.w / 2 - size.x / 2, 5 + collapse_buton_h));
    ImGui::Text("%s", name.c_str());
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, grey);
    ImGui::SetCursorPosX(r.w - 25);
    ImGui::SetCursorPosY( 3.f + collapse_buton_h );
    std::string id = rsutils::string::from() << textual_icons::times << "##Close_" << name;
    if (ImGui::Button(id.c_str(),ImVec2(22,22)))
    {
        close();
    }
    if (ImGui::IsItemHovered())
    {
        RsImGui::CustomTooltip("Remove Dashboard from View");
        win.link_hovered();
    }
    ImGui::PopStyleColor();

    ImGui::GetWindowDrawList()->AddRectFilled({ pos.x + max_y_label_width + 15, pos.y + ImGui::GetTextLineHeight() + 5 },
                 { pos.x + r.w - 10, pos.y + r.h - ImGui::GetTextLineHeight() - 5 }, ImColor(almost_white_bg));

    //ImGui::PushFont(win.get_monofont());
    for (int i = 0; i <= ticks_y; i++)
    {
        auto y = max_y - i * (gap_y / ticks_y);
        std::string y_label = rsutils::string::from() << std::fixed << std::setprecision(2) << y;
        auto y_pixel = ImGui::GetTextLineHeight() + i * (height_y / ticks_y);
        ImGui::SetCursorPos(ImVec2( 10, y_pixel + collapse_buton_h));
        ImGui::Text("%s", y_label.c_str());

        ImGui::GetWindowDrawList()->AddLine({ pos.x + max_y_label_width + 15.f, pos.y + y_pixel + 5.f },
                 { pos.x + r.w - 10.f, pos.y + y_pixel + 5.f }, ImColor(light_grey));
    }

    auto graph_width = r.w - max_y_label_width - 25;

    int ticks_x = 2;
    bool has_room = true;
    while (has_room)
    {
        float total = 0;
        for (int i = 0; i <= ticks_x; i++)
        {
            auto x = min_x + i * (gap_x / ticks_x);
            std::string x_label = rsutils::string::from() << std::fixed << std::setprecision(2) << x;
            auto size = ImGui::CalcTextSize(x_label.c_str());
            total += size.x;
        }
        if (total < graph_width) ticks_x++;
        else has_room = false;
    }
    ticks_x -= 3;

    for (int i = 0; i < ticks_x; i++)
    {
        auto x = min_x + i * (gap_x / ticks_x);
        std::string x_label = rsutils::string::from() << std::fixed << std::setprecision(2) << x;
        ImGui::SetCursorPos(ImVec2( 15 + max_y_label_width+ i * (graph_width / ticks_x), r.h - ImGui::GetTextLineHeight() + collapse_buton_h));
        ImGui::Text("%s", x_label.c_str());

        ImGui::GetWindowDrawList()->AddLine({ pos.x + 15 + max_y_label_width + i * (graph_width / ticks_x), pos.y + ImGui::GetTextLineHeight() + 5 },
                 { pos.x + max_y_label_width + 15 + i * (graph_width / ticks_x), pos.y + ImGui::GetTextLineHeight() + 5 + height_y }, ImColor(light_grey));
    }

    std::sort(xy.begin(), xy.end(), [](const std::pair<float, float>& a, const std::pair<float, float>& b) { return a.first < b.first; });

    for (int i = 0; i + 1 < xy.size(); i++)
    {
        auto x0 = xy[i].first;
        auto y0 = xy[i].second;

        auto x1 = xy[i+1].first;
        auto y1 = xy[i+1].second;

        x0 = (x0 - min_x) / (max_x - min_x);
        x1 = (x1 - min_x) / (max_x - min_x);

        y0 = (y0 - min_y) / (max_y - min_y);
        y1 = (y1 - min_y) / (max_y - min_y);

        ImGui::GetWindowDrawList()->AddLine({ pos.x + 15 + max_y_label_width + x0 * graph_width, pos.y + ImGui::GetTextLineHeight() + 5 + height_y * (1.f - y0) },
                 { pos.x + 15 + max_y_label_width + x1 * graph_width, pos.y + ImGui::GetTextLineHeight() + 5 + height_y * (1.f - y1) }, ImColor(black));
    }

    //ImGui::PopFont();
    ImGui::PopStyleColor();

    xy.clear();
}

void frame_drops_dashboard::process_frame(rs2::frame f)
{
    shared_data.write([&](){
        double ts = glfwGetTime();
        if (method == 1) ts = f.get_timestamp() / 1000.f;
        auto it = stream_to_time.find(f.get_profile().unique_id());
        if (it != stream_to_time.end())
        {
            auto last = stream_to_time[f.get_profile().unique_id()];

            double fps = (double)f.get_profile().fps();

            if( f.supports_frame_metadata( RS2_FRAME_METADATA_ACTUAL_FPS ) )
                fps = f.get_frame_metadata( RS2_FRAME_METADATA_ACTUAL_FPS ) / 1000.;

            if (1000. * (ts - last) > 1.5 * (1000. / fps)) {
                drops++;
            }
        }

        counter++;

        if (ts - last_time > 1.f)
        {
            if (drops_history.size() > 100) drops_history.pop_front();
            drops_history.push_back(drops);
            *total = counter;
            *frame_drop_count = drops;
            drops = 0;
            last_time = ts;
            counter = 0;
        }

        stream_to_time[f.get_profile().unique_id()] = ts;
    });
}

void frame_drops_dashboard::draw(ux_window& win, rect r)
{
    auto hist = shared_data.read<std::deque<int>>([&](){ return drops_history; });
    for (int i = 0; i < hist.size(); i++)
    {
        add_point((float)i, (float)hist[i]);
    }
    r.h -= ImGui::GetTextLineHeightWithSpacing() + 10;

    if( config_file::instance().get( configurations::viewer::dashboard_open) )
        draw_dashboard(win, r);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 40);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
    ImGui::Text("%s", "Measurement Metric:"); ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);

    ImGui::SetCursorPosX( 11.5f * win.get_font_size() );

    std::vector<const char*> methods;
    methods.push_back("Viewer Processing Rate");
    methods.push_back("Camera Timestamp Rate");

    ImGui::PushItemWidth(-1.f);
    if (ImGui::Combo("##fps_method", &method, methods.data(), (int)(methods.size())))
    {
        clear(false);
    }
    ImGui::PopItemWidth();
}

int frame_drops_dashboard::get_height() const
{
    return (int)(160 + ImGui::GetTextLineHeightWithSpacing());
}

void frame_drops_dashboard::clear(bool full)
{
    shared_data.write([&](){
        stream_to_time.clear();
        last_time = 0;
        *total = 0;
        *frame_drop_count = 0;
        if (full)
        {
            drops_history.clear();
            for (int i = 0; i < 100; i++)
                drops_history.push_back(0);
        }
    });
}
