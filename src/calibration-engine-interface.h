// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/hw-monitor.h>


namespace librealsense
{
    
enum class calibration_state : uint8_t
{
    IDLE = 0,
    PROCESS,
    SUCCESS,        // D585S wire byte 2 — legacy TC
    FAILURE,        // D585S wire byte 3 — legacy TC
    FLASH_UPDATE,
    COMPLETE,
    HEALTH_CHECK    // D5x5 interactive triggered calibration only — candidate cached in RAM, awaiting host COMMIT/CANCEL. Synthesized from wire byte 2 when parsing the new reply format; never sent on the legacy D585S path.
};

enum class calibration_result : uint8_t
{
    UNKNOWN = 0,    // aka INIT in the D5x5 interactive triggered calibration spec — same wire value
    SUCCESS,
    FAILED_TO_CONVERGE,
    FAILED_TO_RUN
};

enum class calibration_mode
{
    RESERVED = 0,
    RUN,
    ABORT,          // D585S semantics. D5x5 interactive triggered calibration broadens this to CANCEL (valid from any state except FLASH_UPDATE). Wire value 2 on both paths.
    DRY_RUN,
    COMMIT,         // D5x5 interactive triggered calibration only — host approves the HEALTH_CHECK-cached candidate; device flashes it, no payload.
    TRY             // D5x5 interactive triggered calibration only — apply NEW/OLD table live to RAM for preview, sub-selection in payload.
};

// D5x5 interactive triggered calibration only. Sub-selection for calibration_mode::TRY. Wire byte carried alongside mode.
enum class try_calibration_selection : uint8_t
{
    NEW = 0,        // apply the HEALTH_CHECK-cached candidate live for comparison
    OLD = 1         // re-apply the currently-committed flash table live, undoing a prior NEW
};

// D5x5 interactive triggered calibration only. Populated by firmware at HEALTH_CHECK/COMPLETE. Wire-format struct — do not reorder.
#pragma pack(push, 1)
struct calibration_health_metrics
{
    float coverage_safe_for_depth;   // [0,1]  — informational; pass gate TBD (spec §5.5, ~0.50)
    float rect_health;               // px    — pass: <  rect_health_pass_threshold_px
    float rect_improvement;          // px    — informational only
    float scale_health;              // px    — informational; pass gate TBD (spec §5.5, ~0.50)
    float scale_improvement;         // px    — informational only
};
#pragma pack(pop)

// Provisional pass threshold per spec §5.5. Only rect_health is host-gated today — coverage and scale
// remain informational until the multi-metric gate lands (add matching constants at that time).
// The viewer mirrors this in common/d500-on-chip-calib.h — keep the two in sync.
static constexpr float rect_health_pass_threshold_px = 0.4f;

class calibration_engine_interface
{
public:
    virtual void update_triggered_calibration_status() = 0;
    virtual std::vector<uint8_t> run_triggered_calibration(calibration_mode _mode) = 0;
    virtual calibration_state get_triggered_calibration_state() const = 0;
    virtual calibration_result get_triggered_calibration_result() const = 0;
    virtual int8_t get_triggered_calibration_progress() const = 0;
    virtual std::vector<uint8_t> get_calibration_table(std::vector<uint8_t>& current_calibration) const = 0;
    virtual void write_calibration(std::vector<uint8_t>& calibration) const = 0;
    virtual std::string get_calibration_config() const = 0;
    virtual void set_calibration_config(const std::string& calibration_config_json_str) const = 0;

    // D5x5 interactive triggered calibration — no-op / not supported on the legacy D585S path by default.
    virtual void set_interactive_triggered_calibration_enabled( bool ) {}
    virtual bool is_interactive_triggered_calibration_enabled() const { return false; }
    virtual calibration_health_metrics get_triggered_calibration_health() const { return {}; }
    virtual std::vector<uint8_t> run_triggered_calibration_try( try_calibration_selection ) { return {}; }
};

} // namespace librealsense
