# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy import tests_wrapper
import logging
log = logging.getLogger(__name__)

# to be restored to run on nightly after FW issue is solved
pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.skip(reason="to be restored to run on nightly after FW issue is solved"),
]


def get_all_advanced_controls(dev, depth_sensor):
    advnc_mode = rs.rs400_advanced_mode(dev)
    color_sensor = dev.first_color_sensor()
    d = {}
    fields_and_attributes = {"get_ae_control": ['meanIntensitySetPoint'],
                             "get_amp_factor": ['a_factor'],
                             "get_census": ['uDiameter', 'vDiameter'],
                             "get_color_control": ['disableRAUColor', 'disableSADColor', 'disableSADNormalize',
                                                   'disableSLOLeftColor', 'disableSLORightColor'],
                             "get_color_correction": ['colorCorrection1', 'colorCorrection10', 'colorCorrection11',
                                                      'colorCorrection12', 'colorCorrection2', 'colorCorrection3',
                                                      'colorCorrection4', 'colorCorrection5', 'colorCorrection6',
                                                      'colorCorrection7', 'colorCorrection8', 'colorCorrection9'],
                             "get_depth_control": ['deepSeaMedianThreshold', 'deepSeaNeighborThreshold',
                                                   'deepSeaSecondPeakThreshold', 'lrAgreeThreshold', 'minusDecrement',
                                                   'plusIncrement', 'scoreThreshA', 'scoreThreshB',
                                                   'textureCountThreshold', 'textureDifferenceThreshold'],
                             "get_depth_table": ['depthClampMax', 'depthClampMin', 'depthUnits', 'disparityMode',
                                                 'disparityShift'],
                             "get_hdad": ['ignoreSAD', 'lambdaAD', 'lambdaCensus'],
                             "get_rau_support_vector_control": ['minEast', 'minNSsum', 'minNorth', 'minSouth',
                                                                'minWEsum', 'minWest', 'uShrink', 'vShrink'],
                             "get_rau_thresholds_control": ['rauDiffThresholdBlue', 'rauDiffThresholdGreen',
                                                            'rauDiffThresholdRed'],
                             "get_rsm": ['diffThresh', 'removeThresh', 'rsmBypass', 'sloRauDiffThresh'],
                             "get_slo_color_thresholds_control": ['diffThresholdBlue', 'diffThresholdGreen',
                                                                  'diffThresholdRed'],
                             "get_slo_penalty_control": ['sloK1Penalty', 'sloK1PenaltyMod1', 'sloK1PenaltyMod2',
                                                         'sloK2Penalty', 'sloK2PenaltyMod1', 'sloK2PenaltyMod2']
                             }

    for field in fields_and_attributes.keys():
        for attribute in fields_and_attributes[field]:
            field_obj = getattr(advnc_mode, field)()
            d[f"{field}-{attribute}"] = getattr(field_obj, attribute)

    d["depth-exposure"] = depth_sensor.get_option(rs.option.exposure)
    d["color-exposure"] = color_sensor.get_option(rs.option.exposure)
    d["color-gain"] = color_sensor.get_option(rs.option.gain)
    d["color-gamma"] = color_sensor.get_option(rs.option.gamma)
    d["color-power-line-frequency"] = color_sensor.get_option(rs.option.power_line_frequency)

    return d


@pytest.fixture
def _stop_wrapper_at_end(test_device):
    """Cleanup-only fixture: ensures stop_wrapper runs even if the test fails."""
    dev, _ = test_device
    yield
    try:
        tests_wrapper.stop_wrapper(dev)
    except Exception as e:
        log.warning(f"stop_wrapper failed: {e}")


def test_startup_values_match_default_preset(test_device, _stop_wrapper_at_end):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()

    # get startup values (before start_wrapper changes safety mode)
    startup_advanced_controls = get_all_advanced_controls(dev, depth_sensor)
    log.debug("Startup advanced controls values:")
    log.debug(startup_advanced_controls)

    # switch to default preset
    tests_wrapper.start_wrapper(dev)
    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))

    # get default preset values
    default_preset_advanced_controls = get_all_advanced_controls(dev, depth_sensor)
    log.debug("Default preset advanced controls values:")
    log.debug(default_preset_advanced_controls)

    assert startup_advanced_controls == default_preset_advanced_controls
