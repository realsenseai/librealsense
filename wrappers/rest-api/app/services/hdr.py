# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""HDR preset configuration, ported from the legacy viewer's hdr_model (common/hdr-model.cpp).

The camera describes its HDR sequence in the "hdr-preset" section of its serialized JSON;
a new sequence is applied by loading that JSON back with HDR disabled first.
"""

import json
from typing import Any, Dict, List, Optional

import pyrealsense2 as rs

from app.core.errors import RealSenseError


def _range(sensor, option) -> Optional[Dict[str, float]]:
    if not sensor.supports(option):
        return None
    r = sensor.get_option_range(option)
    return {"min": r.min, "max": r.max, "step": r.step, "default": r.default}


def default_preset() -> Dict[str, Any]:
    """The legacy default: two exposures, alternating every frame."""
    return {"id": "0", "iterations": 0, "control_type_auto": False, "items": [
        {"iterations": 1, "controls": {"depth_gain": 16, "depth_exp": 1, "delta_gain": 0, "delta_exp": 0}},
        {"iterations": 1, "controls": {"depth_gain": 16, "depth_exp": 8500, "delta_gain": 0, "delta_exp": 0}},
    ]}


def from_json(text: str) -> Dict[str, Any]:
    """hdr_preset::from_json: the device JSON's "hdr-preset" section as the editable struct."""
    j = json.loads(text) if text else {}
    hp = j.get("hdr-preset")
    if not hp:
        return {"id": "0", "iterations": 0, "control_type_auto": False, "items": []}
    preset: Dict[str, Any] = {"id": str(hp.get("id", "0")), "iterations": int(hp.get("iterations", "0")),
                              "control_type_auto": False, "items": []}
    for item_j in hp.get("items", []):
        ctrl = {"depth_gain": 0, "depth_exp": 0, "delta_gain": 0, "delta_exp": 0}
        skip = False
        for name, value in item_j.get("controls", {}).items():
            if name == "depth-ae":
                preset["control_type_auto"] = True
                skip = True  # a marker item without values
            elif name == "depth-ae-gain":
                preset["control_type_auto"] = True
                ctrl["delta_gain"] = int(value)
            elif name == "depth-ae-exp":
                preset["control_type_auto"] = True
                ctrl["delta_exp"] = int(value)
            elif name == "depth-gain":
                ctrl["depth_gain"] = int(value)
            elif name == "depth-exposure":
                ctrl["depth_exp"] = int(value)
        if skip:
            continue
        preset["items"].append({"iterations": int(item_j.get("iterations", "1")), "controls": ctrl})
    return preset


def to_json(preset: Dict[str, Any]) -> str:
    """hdr_preset::to_json: what serializable_device.load_json expects."""
    items: List[Dict[str, Any]] = []
    auto = bool(preset.get("control_type_auto"))
    if auto:
        items.append({"iterations": "1", "controls": {"depth-ae": "1"}})
    for item in preset.get("items", []):
        ctrl = item["controls"]
        controls = ({"depth-ae-gain": str(ctrl["delta_gain"]), "depth-ae-exp": str(ctrl["delta_exp"])} if auto
                    else {"depth-gain": str(ctrl["depth_gain"]), "depth-exposure": str(ctrl["depth_exp"])})
        items.append({"iterations": str(item.get("iterations", 1)), "controls": controls})
    return json.dumps({"hdr-preset": {"id": str(preset.get("id", "0")), "iterations": str(preset.get("iterations", 0)), "items": items}}, indent=4)


def status(dev) -> Dict[str, Any]:
    """Whether the device has HDR presets, the exposure/gain ranges, and the current preset."""
    depth = next((s for s in dev.sensors if s.is_depth_sensor()), None)
    result: Dict[str, Any] = {"supported": False, "preset": None, "exposure_range": None, "gain_range": None, "hdr_enabled": None}
    if depth is None:
        return result
    result["exposure_range"] = _range(depth, rs.option.exposure)
    result["gain_range"] = _range(depth, rs.option.gain)
    if depth.supports(rs.option.hdr_enabled):
        result["hdr_enabled"] = bool(depth.get_option(rs.option.hdr_enabled))
    try:
        text = rs.serializable_device(dev).serialize_json()
    except RuntimeError:
        return result
    if "hdr-preset" not in (json.loads(text) if text else {}):
        return result
    result["supported"] = True
    preset = from_json(text)
    result["preset"] = preset if preset["items"] else default_preset()
    return result


def apply(dev, preset: Dict[str, Any]) -> Dict[str, Any]:
    """hdr_model::apply_hdr_config: HDR off, load the sequence, report the device's view."""
    current = status(dev)
    if not current["supported"]:
        raise RealSenseError(status_code=400, detail="This device has no HDR preset support")
    depth = next(s for s in dev.sensors if s.is_depth_sensor())
    if depth.supports(rs.option.hdr_enabled) and depth.get_option(rs.option.hdr_enabled):
        depth.set_option(rs.option.hdr_enabled, 0.0)
    rs.serializable_device(dev).load_json(to_json(preset))
    return status(dev)
