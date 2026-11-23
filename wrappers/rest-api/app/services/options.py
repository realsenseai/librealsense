# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Reading and writing the options of anything implementing rs2::options: a sensor, a
post-processing filter, the colorizer."""

from typing import Dict

from app.models.option import OptionInfo


def _value_descriptions(obj, opt, rng):
    """The SDK's label for each value of an option, as far as it has them.

    There is no "this is an enum" flag, so labels can only be learned value by value.
    Stopping at the first value without one keeps a wide slider like exposure from being
    probed 165,000 times; a partial set is reported rather than dropped, since whether
    labels mean "dropdown" is the viewer's call.
    """
    descs = {}
    if rng is None or rng.step != 1.0 or rng.min != int(rng.min) or rng.max != int(rng.max):
        return descs
    for val in range(int(rng.min), int(rng.max) + 1):
        try:
            desc = obj.get_option_value_description(opt, float(val))
        except RuntimeError:
            break  # a value the option refuses to describe is where its labels end
        if not desc:
            break
        descs[str(val)] = desc
    return descs


def _info(obj, opt, value, rng) -> OptionInfo:
    """rng None: the SDK could not read the option's range, so its fields are null, as the SDK reports it"""
    return OptionInfo(
        description=obj.get_option_description(opt),
        current_value=value,
        default_value=rng and rng.default,
        min_value=rng and rng.min,
        max_value=rng and rng.max,
        step=rng and rng.step,
        read_only=obj.is_option_read_only(opt),
        value_descriptions=_value_descriptions(obj, opt, rng),
    )


def all_options(obj) -> Dict[str, OptionInfo]:
    """Every option the object supports, by its ID, values and ranges in one call, as the legacy viewer reads them;
    one it cannot read now (a temperature while not streaming) has a null value, as the SDK reports it"""
    return {o.id.name: _info(obj, o.id, o.value, o.range) for o in obj.get_supported_option_values()}


def option(obj, field: str) -> OptionInfo:
    """One option, read alone: 404 only if the object doesn't support it, a null value if it can't read it now"""
    opt = _supported(obj)[field]
    try:
        rng = obj.get_option_range(opt)
    except RuntimeError:  # as the SDK's options list, which then has no range
        rng = None
    return _info(obj, opt, obj.get_option_value(opt).value, rng)


def _supported(obj):
    return {o.name: o for o in obj.get_supported_options()}


def set_option(obj, field: str, value: float) -> None:
    """Set one option, as the SDK's set_option: nothing answered"""
    obj.set_option(_supported(obj)[field], float(value))
