# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""RS400 advanced-mode controls, flattened into OptionInfo objects."""

import pyrealsense2 as rs
from typing import Dict, List

from app.core.errors import RealSenseError
from app.models.option import OptionInfo


def control(field: str, value, minimum, maximum) -> OptionInfo:
    return OptionInfo(
        option_id=field,
        current_value=value,
        default_value=value,  # advanced mode exposes no per-field default
        min_value=minimum,
        max_value=maximum,
        # JSON loses the difference between 10 and 10.0, so an integer field says so
        # through its step. The SDK states no step for the others.
        step=1 if isinstance(value, int) else None,
    )


def status(dev) -> Dict[str, bool]:
    """Whether the device has advanced mode, and whether it is currently on.

    The wrapper nulls itself out for a device without advanced mode, so a raise here is
    what "unsupported" looks like.
    """
    am = rs.rs400_advanced_mode(dev)
    try:
        return {"supported": True, "enabled": bool(am.is_enabled())}
    except RuntimeError:
        return {"supported": False, "enabled": False}


def toggle(dev, enable: bool) -> None:
    """Turn advanced mode on or off. The device restarts, so it must be re-resolved after."""
    rs.rs400_advanced_mode(dev).toggle_advanced_mode(enable)


def _enabled(dev):
    """The device's advanced-mode wrapper, or 409 when the mode is off: firmware answers
    every control read/write with a bare error code then."""
    am = rs.rs400_advanced_mode(dev)
    if not am.is_enabled():
        raise RealSenseError(status_code=409, detail="Advanced mode is disabled on this device")
    return am


def controls(dev) -> Dict[str, List[OptionInfo]]:
    """Every advanced-mode control the device has, keyed by the group the writes go to."""
    groups = _enabled(dev).get_all_controls()
    by_group: Dict[str, List[OptionInfo]] = {}
    for group in groups.keys():
        values, mins, maxes = groups[group]
        by_group[group] = [control(f, values[f], mins[f], maxes[f]) for f in values.keys()]
    return by_group


def set_control(dev, group: str, field: str, value: float) -> OptionInfo:
    """Set one control: firmware only accepts whole groups, so it is a read-modify-write."""
    am = _enabled(dev)
    values = am[group]
    values[field] = value  # the SDK rounds to the field's own type
    am[group] = values
    value, minimum, maximum = am[group][field], am[group, 1][field], am[group, 2][field]
    return control(field, value, minimum, maximum)
