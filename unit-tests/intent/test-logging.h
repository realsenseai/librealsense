// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include <cassert>
#include <string>
#include <vector>
#include <rsutils/easylogging/easyloggingpp.h>
class WarningCapture {
#ifdef BUILD_EASYLOGGINGPP
    class Dispatch : public el::LogDispatchCallback {
      public:
        std::vector<std::string> messages;
      protected:
        void handle(const el::LogDispatchData *data) noexcept override {
            if (data->logMessage()->level() == el::Level::Warning)
                messages.push_back(data->logMessage()->message());
        }
    };
    Dispatch *_dispatch;
#endif
  public:
    WarningCapture() {
#ifdef BUILD_EASYLOGGINGPP
        el::Configurations config;
        config.setToDefault();
        config.setGlobally(el::ConfigurationType::ToFile, "false");
        config.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
        config.setGlobally(el::ConfigurationType::Enabled, "true");
        el::Loggers::reconfigureLogger(rsutils::g_librealsense_elpp_id, config);
        el::Helpers::installLogDispatchCallback<Dispatch>("intent-test");
        _dispatch = el::Helpers::logDispatchCallback<Dispatch>("intent-test");
#endif
    }
    ~WarningCapture() {
#ifdef BUILD_EASYLOGGINGPP
        el::Helpers::uninstallLogDispatchCallback<Dispatch>("intent-test");
#endif
    }
    void contains(const std::string &name) const {
#ifdef BUILD_EASYLOGGINGPP
        for (const auto &message : _dispatch->messages)
            if (message.find(name) != std::string::npos) return;
        assert(!"missing omission warning");
#else
        (void)name;
#endif
    }
};
