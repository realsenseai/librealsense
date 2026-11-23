# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""RS400 advanced-mode controls, each field as an AdvancedControl."""

import pyrealsense2 as rs
from typing import Dict

from app.models.option import AdvancedControl


def control(value, minimum, maximum) -> AdvancedControl:
    return AdvancedControl(current_value=value, min_value=minimum, max_value=maximum, integer=isinstance(value, int))


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


def controls(dev) -> Dict[str, Dict[str, AdvancedControl]]:
    """Every advanced-mode control the device has, by field, keyed by the group the writes go to."""
    groups = rs.rs400_advanced_mode(dev).get_all_controls()
    by_group: Dict[str, Dict[str, AdvancedControl]] = {}
    for group in groups.keys():
        values, mins, maxes = groups[group]
        by_group[group] = {f: control(values[f], mins[f], maxes[f]) for f in values.keys()}
    return by_group


def _check_group_exists(am, group: str) -> None:
    if group not in am.keys():
        raise KeyError(group)  # answered 404, as in the GETs, not 500 on the SDK's AttributeError


def group(dev, group: str) -> Dict[str, AdvancedControl]:
    """One group's controls, reading only that group: its values, minimums and maximums"""
    am = rs.rs400_advanced_mode(dev)
    _check_group_exists(am, group)
    values, mins, maxes = (am[group, mode] for mode in range(3))
    return {f: control(values[f], mins[f], maxes[f]) for f in values.keys()}


def set_control(dev, group: str, field: str, value: float) -> None:
    """Set one control: firmware only accepts whole groups, so it is a read-modify-write of that group."""
    am = rs.rs400_advanced_mode(dev)
    _check_group_exists(am, group)
    values = am[group]
    if field not in values.keys():
        raise KeyError(field)
    values[field] = value  # the SDK rounds to the field's own type
    am[group] = values
