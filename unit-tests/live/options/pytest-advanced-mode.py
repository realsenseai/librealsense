# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]


def get_am_dev(dev):
    return rs.rs400_advanced_mode(dev)


def test_advanced_mode_support(device_in_service_mode):
    """Prerequisite: camera must be in advanced mode. All CI cameras should already be enabled."""
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    assert am_dev is not None
    assert am_dev.is_enabled()


def test_visual_preset_support(device_in_service_mode):
    """Cameras with advanced mode enabled should support visual preset."""
    dev, ctx = device_in_service_mode
    depth_sensor = dev.first_depth_sensor()
    assert depth_sensor.supports(rs.option.visual_preset)


def test_set_default_visual_preset(device_in_service_mode):
    dev, ctx = device_in_service_mode
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))
    assert depth_sensor.get_option(rs.option.visual_preset) == rs.rs400_visual_preset.default


def test_set_depth_control(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    dc = am_dev.get_depth_control()
    dc.plusIncrement = 11
    dc.minusDecrement = 12
    dc.deepSeaMedianThreshold = 13
    dc.scoreThreshA = 14
    dc.scoreThreshB = 22
    dc.textureDifferenceThreshold = 23
    dc.textureCountThreshold = 24
    dc.deepSeaSecondPeakThreshold = 25
    dc.deepSeaNeighborThreshold = 26
    dc.lrAgreeThreshold = 27
    am_dev.set_depth_control(dc)
    new_dc = am_dev.get_depth_control()
    assert new_dc.plusIncrement == 11
    assert new_dc.minusDecrement == 12
    assert new_dc.deepSeaMedianThreshold == 13
    assert new_dc.scoreThreshA == 14
    assert new_dc.scoreThreshB == 22
    assert new_dc.textureDifferenceThreshold == 23
    assert new_dc.textureCountThreshold == 24
    assert new_dc.deepSeaSecondPeakThreshold == 25
    assert new_dc.deepSeaNeighborThreshold == 26
    assert new_dc.lrAgreeThreshold == 27


def test_set_rsm(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    rsm = am_dev.get_rsm()
    rsm.diffThresh = 3.4
    rsm.sloRauDiffThresh = 1.1875 # 1.2 was out of step
    rsm.rsmBypass = 1
    rsm.removeThresh = 123
    am_dev.set_rsm(rsm)
    new_rsm = am_dev.get_rsm()
    assert new_rsm.diffThresh == pytest.approx(3.4, abs=0.01)
    assert new_rsm.sloRauDiffThresh == pytest.approx(1.1875, abs=0.01)
    assert new_rsm.removeThresh == 123


def test_set_rau(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    rau = am_dev.get_rau_support_vector_control()
    rau.minWest = 1
    rau.minEast = 2
    rau.minWEsum = 3
    rau.minNorth = 0
    rau.minSouth = 1
    rau.minNSsum = 6
    rau.uShrink = 1
    rau.vShrink = 2
    am_dev.set_rau_support_vector_control(rau)
    new_rau = am_dev.get_rau_support_vector_control()
    assert new_rau.minWest == 1
    assert new_rau.minEast == 2
    assert new_rau.minWEsum == 3
    assert new_rau.minNorth == 0
    assert new_rau.minSouth == 1
    assert new_rau.minNSsum == 6
    assert new_rau.uShrink == 1
    assert new_rau.vShrink == 2


def test_set_color_control(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    color_control = am_dev.get_color_control()
    color_control.disableSADColor = 1
    color_control.disableRAUColor = 0
    color_control.disableSLORightColor = 1
    color_control.disableSLOLeftColor = 0
    color_control.disableSADNormalize = 1
    am_dev.set_color_control(color_control)
    new_cc = am_dev.get_color_control()
    assert new_cc.disableSADColor == 1
    assert new_cc.disableRAUColor == 0
    assert new_cc.disableSLORightColor == 1
    assert new_cc.disableSLOLeftColor == 0
    assert new_cc.disableSADNormalize == 1


def test_set_rau_thresholds_control(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    rau_tc = am_dev.get_rau_thresholds_control()
    rau_tc.rauDiffThresholdRed = 10
    rau_tc.rauDiffThresholdGreen = 20
    rau_tc.rauDiffThresholdBlue = 30
    am_dev.set_rau_thresholds_control(rau_tc)
    new_rau_tc = am_dev.get_rau_thresholds_control()
    assert new_rau_tc.rauDiffThresholdRed == 10
    assert new_rau_tc.rauDiffThresholdGreen == 20
    assert new_rau_tc.rauDiffThresholdBlue == 30


def test_set_slo_color_thresholds_control(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    slo_ctc = am_dev.get_slo_color_thresholds_control()
    slo_ctc.diffThresholdRed = 1
    slo_ctc.diffThresholdGreen = 2
    slo_ctc.diffThresholdBlue = 3
    am_dev.set_slo_color_thresholds_control(slo_ctc)
    new_slo_ctc = am_dev.get_slo_color_thresholds_control()
    assert new_slo_ctc.diffThresholdRed == 1
    assert new_slo_ctc.diffThresholdGreen == 2
    assert new_slo_ctc.diffThresholdBlue == 3


def test_set_slo_penalty_control(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    slo_pc = am_dev.get_slo_penalty_control()
    slo_pc.sloK1Penalty = 1
    slo_pc.sloK2Penalty = 2
    slo_pc.sloK1PenaltyMod1 = 3
    slo_pc.sloK2PenaltyMod1 = 4
    slo_pc.sloK1PenaltyMod2 = 5
    slo_pc.sloK2PenaltyMod2 = 6
    am_dev.set_slo_penalty_control(slo_pc)
    new_slo_pc = am_dev.get_slo_penalty_control()
    assert new_slo_pc.sloK1Penalty == 1
    assert new_slo_pc.sloK2Penalty == 2
    assert new_slo_pc.sloK1PenaltyMod1 == 3
    assert new_slo_pc.sloK2PenaltyMod1 == 4
    assert new_slo_pc.sloK1PenaltyMod2 == 5
    assert new_slo_pc.sloK2PenaltyMod2 == 6


def test_set_hdad(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    hdad = am_dev.get_hdad()
    hdad.lambdaCensus = 1.1
    hdad.lambdaAD = 2.2
    hdad.ignoreSAD = 1
    am_dev.set_hdad(hdad)
    new_hdad = am_dev.get_hdad()
    assert new_hdad.lambdaCensus == pytest.approx(1.1, abs=0.01)
    assert new_hdad.lambdaAD == pytest.approx(2.2, abs=0.01)
    assert new_hdad.ignoreSAD == 1


def test_set_color_correction(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    cc = am_dev.get_color_correction()
    cc.colorCorrection1 = -0.1
    cc.colorCorrection2 = -0.2
    cc.colorCorrection3 = -0.3
    cc.colorCorrection4 = -0.4
    cc.colorCorrection5 = -0.5
    cc.colorCorrection6 = -0.6
    cc.colorCorrection7 = -0.7
    cc.colorCorrection8 = -0.8
    cc.colorCorrection9 = -0.9
    cc.colorCorrection10 = 1.1
    cc.colorCorrection11 = 1.2
    cc.colorCorrection12 = 1.3
    am_dev.set_color_correction(cc)
    new_cc = am_dev.get_color_correction()
    assert new_cc.colorCorrection1 == pytest.approx(-0.1, abs=0.01)
    assert new_cc.colorCorrection2 == pytest.approx(-0.2, abs=0.01)
    assert new_cc.colorCorrection3 == pytest.approx(-0.3, abs=0.01)
    assert new_cc.colorCorrection4 == pytest.approx(-0.4, abs=0.01)
    assert new_cc.colorCorrection5 == pytest.approx(-0.5, abs=0.01)
    assert new_cc.colorCorrection6 == pytest.approx(-0.6, abs=0.01)
    assert new_cc.colorCorrection7 == pytest.approx(-0.7, abs=0.01)
    assert new_cc.colorCorrection8 == pytest.approx(-0.8, abs=0.01)
    assert new_cc.colorCorrection9 == pytest.approx(-0.9, abs=0.01)
    assert new_cc.colorCorrection10 == pytest.approx(1.1, abs=0.01)
    assert new_cc.colorCorrection11 == pytest.approx(1.2, abs=0.01)
    assert new_cc.colorCorrection12 == pytest.approx(1.3, abs=0.01)


def test_set_ae_control(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    aec = am_dev.get_ae_control()
    aec.meanIntensitySetPoint = 1234
    am_dev.set_ae_control(aec)
    new_aec = am_dev.get_ae_control()
    assert new_aec.meanIntensitySetPoint == 1234


def test_set_depth_table(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    dt = am_dev.get_depth_table()
    dt.depthUnits = 100
    dt.depthClampMin = 10
    dt.depthClampMax = 200
    dt.disparityMode = 1
    dt.disparityShift = 2
    am_dev.set_depth_table(dt)
    new_dt = am_dev.get_depth_table()
    assert new_dt.depthUnits == 100
    assert new_dt.depthClampMin == 10
    assert new_dt.depthClampMax == 200
    assert new_dt.disparityMode == 1
    assert new_dt.disparityShift == 2


def test_set_census(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    census = am_dev.get_census()
    census.uDiameter = 5
    census.vDiameter = 6
    am_dev.set_census(census)
    new_census = am_dev.get_census()
    assert new_census.uDiameter == 5
    assert new_census.vDiameter == 6


def test_set_amp_factor(device_in_service_mode):
    dev, ctx = device_in_service_mode
    am_dev = get_am_dev(dev)
    af = am_dev.get_amp_factor()
    af.a_factor = 0.12
    am_dev.set_amp_factor(af)
    new_af = am_dev.get_amp_factor()
    assert new_af.a_factor == pytest.approx(0.12, abs=0.005)


def test_return_to_default_visual_preset(device_in_service_mode):
    dev, ctx = device_in_service_mode
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))
    assert depth_sensor.get_option(rs.option.visual_preset) == rs.rs400_visual_preset.default
