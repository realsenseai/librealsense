// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Generic, reusable composite (multi-field, atomically-exchanged) XU control - NOT named after
// any specific feature. Any future multi-param XU control can reuse this class; the only
// per-feature footprint is the (extension_unit, ctrl_id, wire_size) triple passed to the ctor.
//
// Implements ONLY composite_option_interface - zero relationship to librealsense::option (no
// shared base). No set(float)/query(), so it's never mistaken for a scalar option by code
// walking options_container::get_supported_options().
//
// Get/set are a plain uvc XU passthrough on every transport: FW authors the dpp_header on get,
// the app carries it back on set (on GMSL the driver pipes the whole struct as well). The range
// differs: USB asks FW via get_xu_range, but GMSL cannot - V4L2 reports one scalar bound for an
// array control and the FW registers hold only the current value - so on MIPI it is built from
// the static `dpp_control_desc` tables instead.

#pragma once

#include <src/composite-option-interface.h>
#include <src/uvc-sensor.h>
#include <src/platform/uvc-device.h>

#include <memory>
#include <string>
#include <cstdint>
#include <vector>


namespace librealsense {

// Describes a DPP composite XU control (ctl_id / param_count / param_type + per-slot ranges).
// USB doesn't consume it (FW authors it into the payload); MIPI uses it only to synthesize the
// range. The description belongs to the control itself, not to the transport - hence callers
// pass it unconditionally to `create()`.
struct dpp_control_desc
{
    uint16_t ctl_id;         // dpp_header.ctl_id of the synthesized range (MIPI-only)
    uint8_t  param_count;    // active int32 slots
    uint8_t  param_type;     // dpp_header.param_type of the synthesized range (MIPI-only)
    const int32_t * min;     // param_count entries each
    const int32_t * max;
    const int32_t * step;
    const int32_t * def;
};

class composite_xu_option : public composite_option_interface
{
public:
    // `desc` is used only when `is_mipi` (to synthesize the range); validated against wire_size.
    composite_xu_option( bool is_mipi,
                         std::weak_ptr< uvc_sensor > ep,
                         platform::extension_unit xu,
                         uint8_t ctrl_id,
                         uint32_t wire_size,
                         std::string description,
                         const dpp_control_desc & desc );

    static std::shared_ptr< composite_xu_option > create( bool is_mipi,
                                                          std::weak_ptr< uvc_sensor > ep,
                                                          platform::extension_unit xu,
                                                          uint8_t ctrl_id,
                                                          uint32_t wire_size,
                                                          std::string description,
                                                          const dpp_control_desc & desc );

    // composite_option_interface: EXACTLY one get_xu()/set_xu() call - the whole payload
    // travels atomically. This is the non-negotiable HW/FW invariant this class exists for.
    std::vector< uint8_t > get_raw() const final;
    void set_raw( const void * data, size_t size ) final;

    // USB: one get_xu_range() call (a read-only metadata query, not subject to the get_raw/set_raw
    // atomicity contract above). MIPI: built from the static desc. Either way packs
    // {min, max, step, def} per the convention on composite_option_interface::get_raw_range().
    std::vector< uint8_t > get_raw_range() const final;

    bool is_enabled() const final { return true; }
    bool is_read_only() const final { return false; }
    const char * get_description() const final { return _description.c_str(); }

private:
    platform::control_range synthesize_range() const;

    std::weak_ptr< uvc_sensor > _ep;
    platform::extension_unit _xu;
    uint8_t _ctrl_id;
    uint32_t _wire_size;
    std::string _description;
    bool _is_mipi;
    dpp_control_desc _desc;  // by value; its min/max/step/def arrays must have static lifetime
};

}  // namespace librealsense
