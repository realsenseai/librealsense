// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-private.h"
#include <src/hw-monitor.h>
#include <src/calibration-engine-interface.h>

namespace librealsense
{
#pragma pack(push, 1)
// Legacy D585S/D585_LEGACY reply layout — 3-byte header + optional 512-byte table at COMPLETE (total 515).
struct d500_calibration_answer
{
    calibration_state state;
    int8_t progress;
    calibration_result result;
    ds::d500_coefficients_table depth_calibration;
};

// D5x5 interactive triggered calibration reply layout — 3-byte header + 20-byte health block + 512-byte table (total 535 from HEALTH_CHECK).
struct interactive_calibration_answer
{
    calibration_state state;            // wire byte 2 means HEALTH_CHECK on this path
    int8_t progress;
    calibration_result result;
    calibration_health_metrics health;  // valid from HEALTH_CHECK onward
    ds::d500_coefficients_table depth_calibration;  // candidate at HEALTH_CHECK, committed at COMPLETE
};
#pragma pack(pop)

// check_buffer_size_interactive derives the IDLE/PROCESS header size from these three sizeofs.
// If padding ever leaks in (e.g. the pack pragma is dropped), that check silently rejects the FW's 3-byte reply.
static_assert(sizeof(interactive_calibration_answer)
                  == 3 + sizeof(calibration_health_metrics) + sizeof(ds::d500_coefficients_table),
              "interactive_calibration_answer must be tightly packed: state+progress+result form a 3-byte header");

class debug_interface;
class d500_debug_protocol_calibration_engine : public calibration_engine_interface
{
public:
    d500_debug_protocol_calibration_engine(debug_interface* dev) : _dev(dev), _interactive_triggered_calibration(false), _calib_ans{}, _interactive_ans{} {}
    void update_triggered_calibration_status() override;
    std::vector<uint8_t> run_triggered_calibration(calibration_mode _mode) override;
    virtual calibration_state get_triggered_calibration_state() const override;
    virtual calibration_result get_triggered_calibration_result() const override;
    virtual int8_t get_triggered_calibration_progress() const override;
    virtual std::vector<uint8_t> get_calibration_table(std::vector<uint8_t>& current_calibration) const override;
    virtual void write_calibration(std::vector<uint8_t>& calibration) const override;
    virtual std::string get_calibration_config() const override;
    virtual void set_calibration_config(const std::string& calibration_config_json_str) const override;
    ds::d500_coefficients_table get_depth_calibration() const;

    // D5x5 interactive triggered calibration path.
    void set_interactive_triggered_calibration_enabled( bool enabled ) override { _interactive_triggered_calibration = enabled; }
    bool is_interactive_triggered_calibration_enabled() const override { return _interactive_triggered_calibration; }
    calibration_health_metrics get_triggered_calibration_health() const override;
    std::vector<uint8_t> run_triggered_calibration_try( try_calibration_selection selection ) override;


private:
    bool check_buffer_size_from_get_calib_status(std::vector<uint8_t> res) const;
    bool check_buffer_size_interactive(std::vector<uint8_t> res) const;
    debug_interface* _dev;
    bool _interactive_triggered_calibration;
    d500_calibration_answer _calib_ans;
    interactive_calibration_answer _interactive_ans;
};

} // namespace librealsense
