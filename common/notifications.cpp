// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "notifications.h"
#include <imgui_internal.h>
#include <realsense_imgui.h>
#include "model-views.h"
#include "os.h"
#include "viewer.h"
#include "metadata-helper.h"
#include "ux-window.h"

#include <thread>
#include <algorithm>
#include <regex>
#include <cmath>

#include <opengl3.h>

using namespace std;
using namespace chrono;

namespace rs2
{
    notification_data::notification_data(std::string description,
        rs2_log_severity severity,
        rs2_notification_category category)
        : _description(description),
          _severity(severity),
          _category(category)
    {
        _timestamp = (double)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

    rs2_notification_category notification_data::get_category() const
    {
        return _category;
    }

    std::string notification_data::get_description() const
    {
        return _description;
    }


    double notification_data::get_timestamp() const
    {
        return _timestamp;
    }

    rs2_log_severity notification_data::get_severity() const
    {
        return _severity;
    }

    notification_model::notification_model()
    {
        custom_action = []{};
        last_x = 500000;
        last_y = 200;
        message = "";
        last_moved = std::chrono::system_clock::now();

        created_time = std::chrono::system_clock::now();
        last_interacted = std::chrono::system_clock::now() - std::chrono::milliseconds(500);
    }

    notification_model::notification_model(const notification_data& n)
        : notification_model()
    {
        message = n.get_description();
        timestamp = n.get_timestamp();
        severity = n.get_severity();
        created_time = std::chrono::system_clock::now();
        last_interacted = std::chrono::system_clock::now() - std::chrono::milliseconds(500);
        category = n.get_category();
    }

    double notification_model::get_age_in_ms(bool total) const
    {
        auto interacted = duration<double, milli>(last_interacted - created_time).count();
        return duration<double, milli>(system_clock::now() - created_time).count() - (total ? 0.0 : interacted);
    }

    bool notification_model::interacted() const
    {
        return duration<double, milli>(system_clock::now() - last_interacted).count() < 100;
    }

    // Pops the N colors that were pushed in set_color_scheme
    void notification_model::unset_color_scheme() const
    {
        ImGui::PopStyleColor(6);
    }

    void progress_bar::draw(ux_window & win, int bar_width, int progress)
    {
        auto now = system_clock::now();
        auto ellapsed = duration_cast<milliseconds>(now - last_progress_time).count() / 1000.f;

        auto new_progress = last_progress + ellapsed * progress_speed;
        curr_progress_value = std::min(threshold_progress, new_progress);

        if (last_progress != progress)
        {
            last_progress_time = system_clock::now();

            int delta = progress - last_progress;

            if (ellapsed > 0.f) progress_speed = delta / ellapsed;

            threshold_progress = float(std::min(100, progress + delta));

            last_progress = progress;
        }

        auto filled_w = (curr_progress_value * (bar_width - 4)) / 100.f;

        auto pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled({ float(pos.x), float(pos.y) },
        { float(pos.x + bar_width), float(pos.y + 20) }, ImColor(black));

        if (curr_progress_value > 0.f)
        {
            for (int i = 20; i >= 0; i -= 2)
            {
                auto a = curr_progress_value / 100.f;
                ImGui::GetWindowDrawList()->AddRectFilled({ float(pos.x + 3 - i), float(pos.y + 3 - i) },
                { float(pos.x + filled_w + i), float(pos.y + 17 + i) },
                    ImColor(alpha(light_blue, sqrt(a) * 0.02f)), float(i));
            }

            ImGui::GetWindowDrawList()->AddRectFilled({ float(pos.x + 3), float(pos.y + 3) },
                { float(pos.x + filled_w), float(pos.y + 17) }, ImColor(light_blue));

            ImGui::GetWindowDrawList()->AddRectFilledMultiColor({ float(pos.x + 5), float(pos.y + 5) },
                { float(pos.x + filled_w), float(pos.y + 15) },
                ImColor(saturate(light_blue, 1.f)), ImColor(saturate(light_blue, 0.9f)),
                ImColor(saturate(light_blue, 0.8f)), ImColor(saturate(light_blue, 1.f)));

            rs2::rect pbar{ float(pos.x + 3), float(pos.y + 3), float(bar_width), 17.f };
            auto mouse = win.get_mouse();
            if (pbar.contains(mouse.cursor))
            {
                std::string progress_str = rsutils::string::from() << progress << "%";
                RsImGui::CustomTooltip("%s", progress_str.c_str());
            }
        }

        ImGui::SetCursorScreenPos({ float(pos.x), float(pos.y + 25) });
    }

    void process_notification_model::draw_progress_bar(ux_window & win, int bar_width)
    {
        auto progress = update_manager->get_progress();
        _progress_bar.draw(win, bar_width, progress);
    }

    /* Sets color scheme for notifications, must be used with unset_color_scheme to pop all colors in the end
       Parameter t indicates the transparency of the nofication interface */
    void notification_model::set_color_scheme(float t) const
    {
        ImVec4 c;

        ImGui::PushStyleColor(ImGuiCol_Button, saturate(c, 1.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, saturate(c, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(c, 1.5f));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        c = alpha(white, 1 - t);
        ImGui::PushStyleColor(ImGuiCol_Text, c);

        if (severity == RS2_LOG_SEVERITY_ERROR ||
            severity == RS2_LOG_SEVERITY_WARN)
        {
            c = alpha(dark_red, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            c = alpha(saturate(grey, 0.7f), 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
    }

    int notification_model::get_max_lifetime_ms() const
    {
        return 10000;
    }

    void notification_model::draw_text(const char* msg, int x, int y, int h)
    {
        std::string text_name = rsutils::string::from() << "##notification_text_" << index;
        ImGui::PushTextWrapPos(x + width - 100.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
        if (enable_click) ImGui::Text("%s", msg);
        else ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(msg),
            strlen(msg) + 1, { float(width - (count > 1 ? 40 : 10)), float(h) },
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(6);
        ImGui::PopTextWrapPos();

        if (ImGui::IsItemHovered())
        {
            last_interacted = system_clock::now();
        }
    }

    std::string notification_model::get_title()
    {
        auto title = message;

        auto parts = split_string(title, '`');
        if (parts.size() > 1) title = parts[0];

        return title;
    }

    int notification_model::calc_height()
    {
        auto title = get_title();
        auto lines = static_cast<int>(std::count(title.begin(), title.end(), '\n') + 1);
        return int((lines + 1) * ImGui::GetTextLineHeight() + 5);
    }

    void process_notification_model::draw_pre_effect(int x, int y)
    {
        // TODO: Make polymorphic
        if (update_state == 2)
        {
            auto k = duration_cast<milliseconds>(system_clock::now() - _progress_bar.last_progress_time).count() / 500.f;
            if (k <= 1.f)
            {
                auto size = 100;
                k = pow(1.f - smoothstep(static_cast<float>(k), 0.f, 1.f), 2.f);
                ImGui::GetWindowDrawList()->AddRectFilled({ float(x - size * k), float(y - size * k) },
                { float(x + width + size * k), float(y + height + size * k) },
                    ImColor(alpha(white, (1.f - k) / 2)), (size * k) / 2);
            }
        }
    }

    void notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        draw_text(get_title().c_str(), x, y, height - 35);
    }

    void rs2::notification_model::draw_dismiss(ux_window & win, int x, int y)
    {
        ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

        string id = rsutils::string::from() << "Dismiss" << "##" << index;

        ImGui::PushStyleColor(ImGuiCol_Text, black);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

        ImGui::PushFont(win.get_font());

        ImGui::SetNextWindowPos({ float(x + width - 125), float(y + height - 25) });
        ImGui::SetNextWindowSize({ 120, 70 });

        std::string dismiss_popup = rsutils::string::from() << "Dismiss Options" << "##" << index;
        if (ImGui::BeginPopup(dismiss_popup.c_str()))
        {
            if (ImGui::Selectable("Just this time"))
            {
                dismiss(true);
            }

            if (ImGui::Selectable("Remind me later"))
            {
                delay(7);
                dismiss(true);
            }

            if (ImGui::Selectable("Don't show again"))
            {
                delay(1000);
                dismiss(true);
            }

            ImGui::EndPopup();
        }

        ImGui::PopFont();
        ImGui::PopStyleColor(4);

        if (ImGui::Button(id.c_str(), { 100, 20 }))
        {
            if (enable_complex_dismiss)
                ImGui::OpenPopup(dismiss_popup.c_str());
            else dismiss(true);
        }
        if (ImGui::IsItemHovered())
        {
            win.link_hovered();
        }
    }

    std::function<void()> notification_model::draw(ux_window& win, int w, int y,
        std::shared_ptr<notification_model>& selected, std::string& error_message)
    {
        std::function<void()> action;
        while(dispatch_queue.try_dequeue(&action)) action();

        std::function<void()> follow_up = []{};

        if (visible)
        {
            auto stack = std::min(count, max_stack);
            auto x = w - width - 10;

            if (dismissed)
            {
                x = w + width;
            }

            if (!animating && (fabs(x - last_x) > 1.f || fabs(y - last_y) > 1.f))
            {
                if (last_x > 100000)
                {
                    last_x = x + 500.f;
                    last_y = float(y);
                }
                last_moved = system_clock::now();
                animating = true;
            }

            auto elapsed = duration<double, milli>(system_clock::now() - last_moved).count();
            auto s = smoothstep(static_cast<float>(elapsed / 250.f), 0.0f, 1.0f);

            if (s < 1.f)
            {
                x = int(s * x + (1 - s) * last_x);
                y = int(s * y + (1 - s) * last_y);
            }
            else
            {
                last_x = float(x); last_y = float(y);
                animating = false;
                if (dismissed && !expanded) to_close = true;
            }

            auto ms = get_age_in_ms() / get_max_lifetime_ms();
            auto t = smoothstep(static_cast<float>(ms), 0.8f, 1.f);
            if (pinned) t = 0.f;

            set_color_scheme(t);

            height = calc_height();

            auto c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
            c.w = smoothstep(static_cast<float>(get_age_in_ms(true) / 200.f), 0.0f, 1.0f);
            c.w = std::min(c.w, 1.f - t);

            draw_pre_effect(x, y);

            for (int i = stack - 1; i >= 0; i--)
            {
                auto ccopy = alpha(c, (0.9f * c.w) / (i + 1));

                ImVec4 shadow{ 0.1f, 0.1f, 0.1f, 0.1f };

                ImGui::GetWindowDrawList()->AddRectFilled({ float(x + 2 + i * stack_offset), float(y + 2 + i * stack_offset) },
                { float(x + 2 + width + i * stack_offset), float(y + 2 + height + i * stack_offset) }, ImColor(shadow));

                ImGui::GetWindowDrawList()->AddRectFilledMultiColor({ float(x + i  * stack_offset), float(y + i * stack_offset) },
                { float(x + width + i * stack_offset), float(y + height + i * stack_offset) },
                    ImColor(saturate(ccopy, 0.9f)), ImColor(saturate(ccopy, 0.95f)),
                    ImColor(saturate(ccopy, 1.2f)), ImColor(saturate(ccopy, 1.1f)));

                ImGui::GetWindowDrawList()->AddRect({ float(x + i * stack_offset), float(y + i * stack_offset) },
                { float(x + width + i * stack_offset), float(y + height + i * stack_offset) }, ImColor(saturate(ccopy, 0.5f)));
            }

            ImGui::SetCursorScreenPos({ float(x), float(y) });

            if (enable_click)
            {
                std::string button_name = rsutils::string::from() << "##" << index;

                ImGui::PushStyleColor(ImGuiCol_Button, transparent);

                if (ImGui::Button(button_name.c_str(), { (float)width, (float)height }))
                {
                    follow_up = custom_action;
                    dismiss(false);
                }
                if (ImGui::IsItemHovered())
                    win.link_hovered();

                ImGui::PopStyleColor();
            }

            if (count > 1)
            {
                std::string count_str = rsutils::string::from() << "x " << count;
                ImGui::SetCursorScreenPos({ float(x + width - 22 - count_str.size() * 5), float(y + 7) });
                ImGui::Text("%s", count_str.c_str());
            }

            ImGui::SetCursorScreenPos({ float(x + 5), float(y + 5) });

            draw_content(win, x, y, t, error_message);

            if (enable_expand)
            {
                ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

                ImGui::PushFont(win.get_large_font());

                ImGui::PushStyleColor(ImGuiCol_Button, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
                string id = rsutils::string::from() << textual_icons::ellipsis_h << "##" << index;
                if (ImGui::Button(id.c_str()))
                {
                    selected = shared_from_this();
                }

                if (ImGui::IsItemHovered())
                    win.link_hovered();

                ImGui::PopStyleColor(3);
                ImGui::PopFont();
            }

            if (enable_dismiss)
            {
                draw_dismiss(win, x, y);
            }

            unset_color_scheme();
        }

        if (expanded)
        {
            draw_expanded(win, error_message);
        }


        return follow_up;
    }

    std::shared_ptr<notification_model> notifications_model::add_notification(const notification_data& n)
    {
        return add_notification(n, []{}, false);
    }

    std::shared_ptr<notification_model> notifications_model::add_notification(const notification_data& n,
        std::function<void()> custom_action, bool use_custom_action)
    {
        std::shared_ptr<notification_model> result = nullptr;
        {
            using namespace std;
            using namespace chrono;
            lock_guard<recursive_mutex> lock(m); // need to protect the pending_notifications queue because the insertion of notifications
                                       // done from the notifications callback and proccesing and removing of old notifications done from the main thread

            for (auto&& nm : pending_notifications)
            {
                if (nm->category == n.get_category() && nm->message == n.get_description())
                {
                    nm->last_interacted = std::chrono::system_clock::now();
                    nm->count++;
                    return nm;
                }
            }

            auto m = std::make_shared<notification_model>(n);
            m->index = index++;
            result = m;
            m->timestamp = duration<double, milli>(system_clock::now().time_since_epoch()).count();

            if (n.get_category() == RS2_NOTIFICATION_CATEGORY_COUNT)
            {
                m->pinned = true;
            }

            if (use_custom_action)
            {
                m->custom_action = custom_action;
                m->enable_click = true;
                m->enable_expand = false;
                m->enable_dismiss = false;
            }

            pending_notifications.push_back(m);

            if (pending_notifications.size() > (size_t)MAX_SIZE)
            {
                auto it = pending_notifications.begin();
                while (it != pending_notifications.end() && (*it)->pinned) it++;

                if (it != pending_notifications.end())
                    pending_notifications.erase(it);
            }
        }

        output.add_log(n.get_severity(), __FILE__, __LINE__, n.get_description());
        return result;
    }

    void notifications_model::add_notification(std::shared_ptr<notification_model> model)
    {
        {
            using namespace std;
            using namespace chrono;
            lock_guard<recursive_mutex> lock(m); // need to protect the pending_notifications queue because the insertion of notifications
                                       // done from the notifications callback and proccesing and removing of old notifications done from the main thread

            model->index = index++;
            model->timestamp = duration<double, milli>(system_clock::now().time_since_epoch()).count();

            pending_notifications.push_back(model);

            if (pending_notifications.size() > (size_t)MAX_SIZE)
            {
                auto it = pending_notifications.begin();
                while (it != pending_notifications.end() && (*it)->pinned) it++;

                if (it != pending_notifications.end())
                    pending_notifications.erase(it);
            }
        }

        output.add_log(model->severity, __FILE__, __LINE__, model->get_title());
    }

    bool notifications_model::draw(ux_window& win, int w, int h, std::string& error_message)
    {
        bool modal_notification_found = false;
        ImGui::PushFont(win.get_font());

        std::vector<std::function<void()>> follow_up;

        {
            bool pinned_drawn = false;
            std::lock_guard<std::recursive_mutex> lock(m);
            if (pending_notifications.size() > 0)
            {
                // loop over all notifications, remove "old" ones
                pending_notifications.erase(std::remove_if(std::begin(pending_notifications),
                    std::end(pending_notifications),
                    [&](std::shared_ptr<notification_model>& n)
                {
                    if (n->snoozed && n->pinned)
                    {
                        n->dismissed = false;
                        n->to_close = false;
                        return true;
                    }
                    return ((n->get_age_in_ms() > n->get_max_lifetime_ms() &&
                            !n->pinned && !n->expanded) || n->to_close);
                }), end(pending_notifications));

                auto height = 60;
                for (auto& noti : pending_notifications)
                {
                    modal_notification_found = modal_notification_found || noti->expanded;
                    if (pinned_drawn && noti->pinned && !noti->forced)
                    {
                        continue;
                    }

                    follow_up.push_back(noti->draw(win, w, height, selected, error_message));

                    if (noti->pinned) pinned_drawn = true;

                    if (noti->visible)
                        height += noti->height + 4 +
                            std::min(noti->count, noti->max_stack) * noti->stack_offset;
                }
            }


            ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
            //ImGui::Begin("Notification parent window", nullptr, flags);

            //selected.set_color_scheme(0.f);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

            if (selected && selected->message != "")
                ImGui::OpenPopup("Notification from Hardware");
            if (ImGui::BeginPopupModal("Notification from Hardware", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Received the following notification:");

                auto parts = split_string(selected->message, '`');

                std::stringstream ss;
                ss << "Timestamp: "
                    << std::fixed << selected->timestamp
                    << "\nSeverity: " << selected->severity
                    << "\nDescription: ";

                for (auto&& part : parts) ss << part << "\n";

                auto s = ss.str();
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
                ImGui::InputTextMultiline("##notification", const_cast<char*>(s.c_str()),
                    s.size() + 1, { 500,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor();

                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    selected->message = "";
                    selected = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    std::string clip = "";
                    auto lines = split_string(selected->message, '\n');
                    for (auto & line : lines)
                    {
                        if (line.size() && line[0] == '$') clip += line.substr(2) + "\n";
                    }
                    if (clip != "")
                    {
                        ImGui::SameLine();
                        if (ImGui::Button(" Copy Commands "))
                        {
                            glfwSetClipboardString(win, clip.c_str());
                        }
                        if (ImGui::IsItemActive())
                            RsImGui::CustomTooltip("Paste the copied commands to a terminal and enter your password to run");
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);

            ImGui::PopStyleColor();
        }

        for (auto& action : follow_up)
        {
            try
            {
                action();
            }
            catch (...) {}
        }

        ImGui::PopFont();

        return modal_notification_found;
    }

    version_upgrade_model::version_upgrade_model(int version)
        : process_notification_model(nullptr), _version(version)
    {
        enable_expand = false;
        enable_dismiss = true;
        update_state = 2;
        //pinned = true;
        _progress_bar.last_progress_time = system_clock::now();
    }

    void version_upgrade_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);
        ImGui::PopStyleColor(1);
        auto c = alpha(sensor_bg, 1 - t);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
    }

    void version_upgrade_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        if (_first)
        {
            _progress_bar.last_progress_time = system_clock::now();
            _first = false;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1.f - t));
        ImGui::PushFont(win.get_large_font());

        ImGui::SetCursorScreenPos({ float(x + 20), float(y + 16) });
        ImGui::Text("Welcome to"); ImGui::SameLine();
        std::string txt = rsutils::string::from() << "librealsense " << RS2_API_VERSION_STR << "!";

        ImGui::PushStyleColor(ImGuiCol_Text, alpha(white, 1.f - t));
        ImGui::Text("%s", txt.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::SetCursorScreenPos({ float(x + 17), float(y + 41) });

        std::string link = rsutils::string::from() << "https://github.com/realsenseai/librealsense/wiki/Release-Notes#release-" << _version;

        ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_blue, 1.f - t));
        if (ImGui::Button("What's new"))
        {
            open_url(link.c_str());
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
        {
            win.link_hovered();
            RsImGui::CustomTooltip("Open the Release Notes. Internet connection is required");
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 5);
        ImGui::Text("in this release?");
        ImGui::PopStyleColor();
    }

    int version_upgrade_model::calc_height()
    {
        return 80;
    }

    void process_manager::log(std::string line)
    {
        std::lock_guard<std::mutex> lock(_log_lock);
        _log += line + "\n";
    }

    const std::string process_manager::get_log() const
    {
        std::lock_guard< std::mutex > lock( _log_lock );
        return _log;
    }

    void process_manager::reset()
    {
        _progress = 0;
        _started = false;
        _done = false;
        _failed = false;
        _last_error = "";
    }

    void process_manager::fail(std::string error)
    {
        _last_error = error;
        _progress = 0;
        log("\nERROR: " + error);
        _failed = true;
    }

    void notification_model::invoke(std::function<void()> action)
    {
        // We use a weak_ptr for making sure the queue object is
        // alive when we access it from the dispatcher thread
        auto sptr_q = std::make_shared<single_consumer_queue<bool>> ();
        std::weak_ptr<single_consumer_queue<bool>> wptr_q( sptr_q );

        dispatch_queue.enqueue([wptr_q, &action](){
            try
            {
                action();
                auto q = wptr_q.lock();
                if( ! q )
                    return;
                q->enqueue( true );
            }
            catch(...)
            {
                auto q = wptr_q.lock();
                if( ! q )
                    return;
                q->enqueue(false);
            }
        });
        bool res;
        if (!sptr_q->dequeue(&res, 100000) || !res)
            throw std::runtime_error("Invoke operation failed!");

    }

    void process_manager::start(invoker invoke)
    {
        auto cleanup = [invoke]() {
        };

        log( rsutils::string::from() << "Started " << _process_name << " process" );

        auto me = shared_from_this();
        std::weak_ptr<process_manager> ptr(me);

        std::thread t([ptr, cleanup, invoke]() {
            auto self = ptr.lock();
            if (!self) return;

            try
            {
                self->process_flow(cleanup, invoke);
            }
            catch (const error& e)
            {
                self->fail(error_to_string(e));
                cleanup();
            }
            catch (const std::exception& ex)
            {
                self->fail(ex.what());
                cleanup();
            }
            catch (...)
            {
                self->fail( rsutils::string::from() << "Unknown error during " << self->_process_name << " process!" );
                cleanup();
            }
        });
        t.detach();

        _started = true;
    }

    void export_manager::process_flow(
        std::function<void()> cleanup,
        invoker invoke)
    {
        _progress = 5;
        _exporter->process(_data);
        _progress = 100;

        _done = true;
    }

    void export_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        using namespace std;
        using namespace chrono;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
        { float(x + width), float(y + 25) }, ImColor(shadow));

        if (update_state != STATE_COMPLETE)
        {
            ImGui::Text("Export in progress");

            ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });

            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1.f - t));

            std::string s = "Saving 3D view to " + get_file_name( get_manager().get_filename() );
            ImGui::Text("%s", s.c_str());

            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::Text("Export Completed");

            ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
            ImGui::PushFont(win.get_large_font());
            std::string txt = rsutils::string::from() << textual_icons::trophy;
            ImGui::Text("%s", txt.c_str());
            ImGui::PopFont();

            ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });
            std::string s = "Finished saving 3D view  to " + get_file_name( get_manager().get_filename() );

            ImGui::Text("%s", s.c_str());
        }

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        const auto bar_width = width - 115;

        if (update_state == STATE_IN_PROGRESS)
        {
            if (update_manager->done())
            {
                update_state = STATE_COMPLETE;
                pinned = false;
                _progress_bar.last_progress_time = last_interacted = system_clock::now();
            }

            if (!expanded)
            {
                if (update_manager->failed())
                {
                    update_manager->check_error(error_message);
                    update_state = STATE_FAILED;
                    pinned = false;
                    dismiss(false);
                }

                draw_progress_bar(win, bar_width);

                ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });
            }
        }
    }

    int export_notification_model::calc_height()
    {
        return 85;
    }

    void export_notification_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);

        ImGui::PopStyleColor(1);

        ImVec4 c;

        if (update_state == STATE_COMPLETE)
        {
            c = alpha(saturate(light_blue, 0.7f), 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            c = alpha(sensor_bg, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
    }

    export_notification_model::export_notification_model(std::shared_ptr<export_manager> manager)
        : process_notification_model(manager)
    {
        enable_expand = false;
        expanded = false;
        if (expanded) visible = false;

        message = "";
        update_state = STATE_IN_PROGRESS;
        this->severity = RS2_LOG_SEVERITY_INFO;
        this->category = RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED;

        pinned = true;
    }

    metadata_warning_model::metadata_warning_model()
        : notification_model()
    {
        enable_expand = false;
        enable_dismiss = true;
        pinned = true;
        message = "Frame Metadata is a device feature allowing\n"
                  "software synchronization between different\n"
                  "camera streams.\n"
                  "It must be explicitly enabled on Windows OS\n";
    }

    void metadata_warning_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);
        ImGui::PopStyleColor(1);
        auto c = alpha(sensor_bg, 1 - t);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
    }

    void metadata_warning_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
        { float(x + width), float(y + 25) }, ImColor(shadow));

        ImGui::Text("Frame Metadata Disabled");

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + 27) });

        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        draw_text(get_title().c_str(), x, y, height - 50);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
        std::string button_name = rsutils::string::from() << "Enable" << "##enable_metadata" << index;

        const auto bar_width = width - 115;
        if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
        {
            metadata_helper::instance().enable_metadata();
            dismiss(false);
        }
        ImGui::PopStyleColor(2);

        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("%s", "Enables metadata on connected devices (you may be prompted for administrator privileges)");
        }
    }

    bool notification_model::is_delayed() const
    {
        // Make sure we don't spam calibration remainders too often:
        time_t rawtime;
        time(&rawtime);
        std::string str = rsutils::string::from() << "notifications." << delay_id << ".next";
        long long next_time = config_file::instance().get_or_default(str.c_str(), (long long)0);

        return rawtime < next_time;
    }

    void notification_model::delay(int days)
    {
        // Make sure we don't spam calibration remainders too often:
        time_t rawtime;
        time(&rawtime);
        std::string str = rsutils::string::from() << "notifications." << delay_id << ".next";
        config_file::instance().set(str.c_str(), (long long)(rawtime + days * 60 * 60 * 24));
    }
    void notification_model::reset_delay()
    {
        if( is_delayed() )
        {
            std::string str = rsutils::string::from() << "notifications." << delay_id << ".next";
            config_file::instance().remove( str.c_str());
        }
    }

    sw_recommended_update_alert_model::sw_recommended_update_alert_model(const std::string& current_version, const std::string& recommended_version, const std::string& recommended_version_link)
        : notification_model(), _current_version(current_version), _recommended_version(recommended_version), _recommended_version_link(recommended_version_link)
    {
        enable_expand = false;
        enable_dismiss = true;
        pinned = true;
        forced = true;
        severity = RS2_LOG_SEVERITY_INFO;
        message = "Current SW version: " + _current_version +"\n" +
            "Recommended SW version: " + _recommended_version;
    }

    void sw_recommended_update_alert_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);
        ImGui::PopStyleColor(1);
        auto c = alpha(sensor_bg, 1 - t);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
    }

    void sw_recommended_update_alert_model::draw_content(
        ux_window& win, int x, int y, float t, std::string& error_message)
    {
        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
            { float(x + width), float(y + 25) },
            ImColor(shadow));

        ImGui::Text("Software Update Recommended!");

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + 27) });

        draw_text(get_title().c_str(), x, y , height - 50);

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + height - 77) });

        ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1.f - t));
        ImGui::Text("We strongly recommend you upgrade \nyour software\n");
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        auto sat = 1.f
            + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count()
                / 700.f)
            * 0.1f;
        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

        const auto bar_width = width - 115;

        ImGui::PushStyleColor(ImGuiCol_Text, alpha(white, 1.f - t));
        std::string button_name = rsutils::string::from() << "Learn More..." << "##" << index;
        if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
        {
            bool should_dismiss = true;
            try
            {
                open_url(_recommended_version_link.c_str());
            }
            catch (const exception& e)
            {
                error_message = e.what();
                should_dismiss = false;
            }
            if (should_dismiss) dismiss(false);
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
        {
            win.link_hovered();
            RsImGui::CustomTooltip("Internet connection required");
        }
        ImGui::PopStyleColor(2);
    }

    sw_update_up_to_date_model::sw_update_up_to_date_model()
        : notification_model()
    {
        enable_expand = false;
        enable_dismiss = false;
        pinned = false;
        forced = true;
        severity = RS2_LOG_SEVERITY_INFO;
        message = "SW/FW versions up to date";
    }

    void sw_update_up_to_date_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);
        ImGui::PopStyleColor();

        ImVec4 c;
        c = alpha(saturate(light_blue, 0.7f), 1 - t);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
    }


    void sw_update_up_to_date_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        using namespace std;
        using namespace chrono;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
            { float(x + width), float(y + 25) }, ImColor(shadow));

        ImGui::Text("Updates Status");

        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
        ImGui::PushFont(win.get_large_font());
        std::string txt = rsutils::string::from() << textual_icons::trophy;
        ImGui::Text("%s", txt.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });
        ImGui::Text("SW/FW Versions All Up To Date");

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
    }

    ucal_disclaimer_model::ucal_disclaimer_model()
        : notification_model()
    {
        enable_expand = false;
        enable_dismiss = true;
        enable_complex_dismiss = true; // Allow to inhibit the disclaimer
        pinned = false;

    }

    void ucal_disclaimer_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
            { float(x + width), float(y + 25) }, ImColor(orange));

        ImGui::Text("Self-Calibration - Disclaimer");
        ImGui::SetCursorScreenPos({ float(x + 5), float(y + 27) });
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        std::string message = "For D415 please refer to the links for details:";
        draw_text(message.c_str(), x, y, 30);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 47) });
        hyperlink(win, "1. Self-Calibration Whitepaper", "https://dev.realsenseai.com/docs/self-calibration-for-depth-cameras");
        ImGui::SetCursorScreenPos({ float(x + 10), float(y + 67) });
        hyperlink(win, "2. Firmware Releases / Errata", "https://dev.realsenseai.com/docs/firmware-updates");

    }

    fl_cal_limitation_model::fl_cal_limitation_model()
        : notification_model()
    {
        enable_expand = false;
        enable_dismiss = true;
        enable_complex_dismiss = false;
        pinned = false;
        message = "Focal-Length Calibration for this device\n"
            " requires USB3 connection.\n"
            "Please switch the connection port and rerun";
    }

    void fl_cal_limitation_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
            { float(x + width), float(y + 25) }, ImColor(grey));

        ImGui::Text("User Notification");

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + 25) });
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x+2), float(y+27) },
            { float(x + width), float(y + 79) }, ImColor(orange));
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        draw_text(get_title().c_str(), x, y, height - 50);
        ImGui::PopStyleColor();
    }
}
