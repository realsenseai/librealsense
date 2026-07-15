// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include "notifications.h"


namespace rs2
{
    class viewer_model;

    bool is_upgradeable(const std::string& curr, const std::string& available);

    class firmware_update_manager : public process_manager
    {
    public:
        firmware_update_manager(std::shared_ptr<notifications_model> not_model, device_model& model, device dev, context ctx, std::vector<uint8_t> fw, bool is_signed)
            : process_manager("Firmware Update"), _not_model(not_model), _model(model),
              _fw(fw), _is_signed(is_signed), _dev(dev), _ctx(ctx) {}

        const device_model& get_device_model() const { return _model; }
        std::shared_ptr<notifications_model> get_protected_notification_model() { return _not_model.lock(); };

    protected:
        void process_flow(std::function<void()> cleanup,
            invoker invoke) override;
        void process_mipi_signed_fw(std::function<void()> cleanup);
        bool check_for(
            std::function<bool()> action, std::function<void()> cleanup,
            std::chrono::system_clock::duration delta);

        bool wait_for_device_reconnect(const std::string& serial, std::function<void()> cleanup);
        void backup_firmware(updatable& upd, int& next_progress, const std::string& serial);
        void switch_device_to_recovery_mode(updatable& upd, const std::string& serial, update_device& dfu, std::function<void()> cleanup);

        std::weak_ptr<notifications_model> _not_model;
        device _dev;
        context _ctx;
        std::vector<uint8_t> _fw;
        bool _is_signed;
        device_model& _model;
        bool _is_d500_device = false;
    };

    struct fw_update_notification_model : public process_notification_model
    {
        fw_update_notification_model(std::string name,
            std::shared_ptr<firmware_update_manager> manager, bool expaned);

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        void draw_expanded(ux_window& win, std::string& error_message) override;
        int calc_height() override;
    };
}
