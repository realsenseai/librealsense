# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""RS400 advanced-mode controls, flattened into OptionInfo objects.

The advanced-mode SDK API is per-group (get_<group>(mode)/set_<group>(struct)), where
mode 0/1/2 = current/min/max. We flatten each group's struct fields into individual
OptionInfo entries (category "Advanced Controls", filter_name = group) so they flow
through the same UI as any other control. Int/float and bool (min0/max1) are derived
from the live struct value types — nothing is hardcoded per field.

TODO: once realsenseai/librealsense#15569 (read all advanced-mode controls in one bulk
operation) lands, replace the per-group get_<group>() sweep in build_advanced_options()
with that single query.
"""

import re
from typing import Any, List

from app.core.errors import RealSenseError
from app.models.option import OptionInfo

# (key, display name, getter, setter) — keys become the ADV_<key>_<field> option ids.
GROUPS = [
    ("depth_control", "Depth Control", "get_depth_control", "set_depth_control"),
    ("rsm", "RSM", "get_rsm", "set_rsm"),
    ("rau_support_vector_control", "RAU Support Vector Control",
     "get_rau_support_vector_control", "set_rau_support_vector_control"),
    ("color_control", "Color Control", "get_color_control", "set_color_control"),
    ("rau_thresholds_control", "RAU Color Thresholds",
     "get_rau_thresholds_control", "set_rau_thresholds_control"),
    ("slo_color_thresholds_control", "SLO Color Thresholds",
     "get_slo_color_thresholds_control", "set_slo_color_thresholds_control"),
    ("slo_penalty_control", "SLO Penalty Control",
     "get_slo_penalty_control", "set_slo_penalty_control"),
    ("hdad", "HDAD", "get_hdad", "set_hdad"),
    ("color_correction", "Color Correction", "get_color_correction", "set_color_correction"),
    ("depth_table", "Depth Table", "get_depth_table", "set_depth_table"),
    ("ae_control", "AE Control", "get_ae_control", "set_ae_control"),
    ("census", "Census Enable Reg", "get_census", "set_census"),
    ("amp_factor", "Disparity Modulation", "get_amp_factor", "set_amp_factor"),
]

_CAMEL = re.compile(r"(?<!^)(?=[A-Z])")


def _struct_fields(struct) -> List[str]:
    return [f for f in dir(struct)
            if not f.startswith("_") and f != "_pybind11_conduit_v1_"]


def _pretty(field: str) -> str:
    return _CAMEL.sub(" ", field).replace("_", " ").title()


def build_advanced_options(am, skip_ae: bool = False) -> List[OptionInfo]:
    """Flatten all advanced-mode groups into OptionInfo entries. Requires advanced mode ON."""
    options: List[OptionInfo] = []
    for key, display, getter, _setter in GROUPS:
        if skip_ae and key == "ae_control":
            continue
        try:
            cur = getattr(am, getter)(0)
        except Exception:
            continue  # group not supported on this device
        # Min/max (mode 1/2) are best-effort: some groups (e.g. HDAD, AE Control) support
        # the current value but not ranges — those fields render as plain numeric inputs.
        try:
            lo = getattr(am, getter)(1)
            hi = getattr(am, getter)(2)
        except Exception:
            lo = hi = None
        for field in _struct_fields(cur):
            try:
                val = getattr(cur, field)
            except Exception:
                continue
            min_v = max_v = step = None
            if lo is not None and hi is not None:
                try:
                    mn, mx = getattr(lo, field), getattr(hi, field)
                    is_int = isinstance(val, int) and isinstance(mn, int) and isinstance(mx, int)
                    min_v, max_v, step = float(mn), float(mx), 1.0 if is_int else 0.01
                except Exception:
                    min_v = max_v = step = None
            options.append(OptionInfo(
                option_id=f"ADV_{key}_{field}",
                name=_pretty(field),
                description=f"{display} — {field}",
                current_value=float(val),
                default_value=float(val),  # advanced mode exposes no per-field default
                min_value=min_v,
                max_value=max_v,
                step=step,
                read_only=False,
                category="Advanced Controls",
                filter_name=display,
            ))
    return options


def set_advanced_option(am, option_id: str, value: Any) -> bool:
    """Apply a single ADV_<key>_<field> change: read the group, patch the field, set it back."""
    body = option_id[len("ADV_"):]
    for key, _display, getter, setter in GROUPS:
        prefix = key + "_"
        if not body.startswith(prefix):
            continue
        field = body[len(prefix):]
        struct = getattr(am, getter)(0)
        if not hasattr(struct, field):
            continue
        cur = getattr(struct, field)
        setattr(struct, field, int(round(float(value))) if isinstance(cur, int) else float(value))
        getattr(am, setter)(struct)
        return True
    raise RealSenseError(status_code=404, detail=f"Advanced option {option_id} not found")
