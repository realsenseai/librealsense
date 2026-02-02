# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:device each(D500*)
# test:donotrun:!nightly

import pyrealsense2 as rs
from rspy import test
from rspy import log
from rspy import tests_wrapper as tw

dev, _ = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()

tw.start_wrapper( dev )

# No use continuing the test if camera is not in advanced mode.
# All current models support it and all CI cameras should already be in advanced mode enabled state
with test.closure( 'Advanced mode support', on_fail=test.ABORT ):
    am_dev = rs.rs400_advanced_mode(dev)
    test.check( am_dev != None )
    test.check( am_dev.is_enabled() )

with test.closure( 'Visual Preset support', on_fail=test.ABORT ): # Cameras with advanced mode enabled should support visual preset
    test.check( depth_sensor.supports( rs.option.visual_preset ) )
    
with test.closure( 'Set Default Visual Preset' ):
    depth_sensor.set_option( rs.option.visual_preset, int(rs.rs400_visual_preset.default ) )
    test.check( depth_sensor.get_option( rs.option.visual_preset ) == rs.rs400_visual_preset.default )
    
with test.closure( 'Set Depth Control'):
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
    am_dev.set_depth_control( dc )
    new_dc = am_dev.get_depth_control()
    test.check_equal( new_dc.plusIncrement, 11 )
    test.check_equal( new_dc.minusDecrement, 12 )
    test.check_equal( new_dc.deepSeaMedianThreshold, 13 )
    test.check_equal( new_dc.scoreThreshA, 14 )
    test.check_equal( new_dc.scoreThreshB, 22 )
    test.check_equal( new_dc.textureDifferenceThreshold, 23 )
    test.check_equal( new_dc.textureCountThreshold, 24 )
    test.check_equal( new_dc.deepSeaSecondPeakThreshold, 25 )
    test.check_equal( new_dc.deepSeaNeighborThreshold, 26 )
    test.check_equal( new_dc.lrAgreeThreshold, 27 )
    
with test.closure( 'Set RSM'):
    rsm = am_dev.get_rsm()
    rsm.diffThresh = 3.4
    rsm.sloRauDiffThresh = 1.1875 # 1.2 was out of step
    rsm.rsmBypass = 1
    rsm.removeThresh = 123
    am_dev.set_rsm( rsm )
    new_rsm = am_dev.get_rsm()
    test.check_approx_abs( new_rsm.diffThresh, 3.4, 0.01 )
    test.check_approx_abs( new_rsm.sloRauDiffThresh, 1.1875, 0.01 )
    test.check_equal( new_rsm.removeThresh, 123 )

with test.closure( 'Set RAU'):
    rau = am_dev.get_rau_support_vector_control()
    rau.minWest = 1
    rau.minEast = 2
    rau.minWEsum = 3
    rau.minNorth = 0
    rau.minSouth = 1
    rau.minNSsum = 6
    rau.uShrink = 1
    rau.vShrink = 2
    am_dev.set_rau_support_vector_control( rau )
    new_rau = am_dev.get_rau_support_vector_control()
    test.check_equal( new_rau.minWest, 1 )
    test.check_equal( new_rau.minEast, 2 )
    test.check_equal( new_rau.minWEsum, 3 )
    test.check_equal( new_rau.minNorth, 0 )
    test.check_equal( new_rau.minSouth, 1 )
    test.check_equal( new_rau.minNSsum, 6 )
    test.check_equal( new_rau.uShrink, 1 )
    test.check_equal( new_rau.vShrink, 2 )

with test.closure( 'Set Color Control'):
    color_control = am_dev.get_color_control()
    color_control.disableSADColor = 1
    color_control.disableRAUColor = 0
    color_control.disableSLORightColor = 1
    color_control.disableSLOLeftColor = 0
    color_control.disableSADNormalize = 1
    am_dev.set_color_control( color_control )
    new_color_control = am_dev.get_color_control()
    test.check_equal( new_color_control.disableSADColor, 1 )
    test.check_equal( new_color_control.disableRAUColor, 0 )
    test.check_equal( new_color_control.disableSLORightColor, 1 )
    test.check_equal( new_color_control.disableSLOLeftColor, 0 )
    test.check_equal( new_color_control.disableSADNormalize, 1 )

with test.closure( 'Set RAU Thresholds Control'):
    rau_tc = am_dev.get_rau_thresholds_control()
    rau_tc.rauDiffThresholdRed = 10
    rau_tc.rauDiffThresholdGreen = 20
    rau_tc.rauDiffThresholdBlue = 30
    am_dev.set_rau_thresholds_control( rau_tc )
    new_rau_tc = am_dev.get_rau_thresholds_control()
    test.check_equal( new_rau_tc.rauDiffThresholdRed, 10 )
    test.check_equal( new_rau_tc.rauDiffThresholdGreen, 20 )
    test.check_equal( new_rau_tc.rauDiffThresholdBlue, 30 )

with test.closure( 'Set SLO Color Thresholds Control'):
    slo_ctc = am_dev.get_slo_color_thresholds_control()
    slo_ctc.diffThresholdRed = 1
    slo_ctc.diffThresholdGreen = 2
    slo_ctc.diffThresholdBlue = 3
    am_dev.set_slo_color_thresholds_control( slo_ctc )
    new_slo_ctc = am_dev.get_slo_color_thresholds_control()
    test.check_equal( new_slo_ctc.diffThresholdRed, 1 )
    test.check_equal( new_slo_ctc.diffThresholdGreen, 2 )
    test.check_equal( new_slo_ctc.diffThresholdBlue, 3 )

with test.closure( 'Set SLO Penalty Control'):
    slo_pc = am_dev.get_slo_penalty_control()
    slo_pc.sloK1Penalty = 1
    slo_pc.sloK2Penalty = 2
    slo_pc.sloK1PenaltyMod1 = 3
    slo_pc.sloK2PenaltyMod1 = 4
    slo_pc.sloK1PenaltyMod2 = 5
    slo_pc.sloK2PenaltyMod2 = 6
    am_dev.set_slo_penalty_control( slo_pc )
    new_slo_pc = am_dev.get_slo_penalty_control()
    test.check_equal( new_slo_pc.sloK1Penalty, 1 )
    test.check_equal( new_slo_pc.sloK2Penalty, 2 )
    test.check_equal( new_slo_pc.sloK1PenaltyMod1, 3 )
    test.check_equal( new_slo_pc.sloK2PenaltyMod1, 4 )
    test.check_equal( new_slo_pc.sloK1PenaltyMod2, 5 )
    test.check_equal( new_slo_pc.sloK2PenaltyMod2, 6 )

with test.closure( 'Set HDAD'):
    hdad = am_dev.get_hdad()
    hdad.lambdaCensus = 1.1
    hdad.lambdaAD = 2.2
    hdad.ignoreSAD = 1
    am_dev.set_hdad( hdad )
    new_hdad = am_dev.get_hdad()
    test.check_approx_abs( new_hdad.lambdaCensus, 1.1, 0.01 )
    test.check_approx_abs( new_hdad.lambdaAD, 2.2, 0.01 )
    test.check_equal( new_hdad.ignoreSAD, 1 )

with test.closure( 'Set Color Correction'):
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
    am_dev.set_color_correction( cc )
    new_cc = am_dev.get_color_correction()
    test.check_approx_abs( new_cc.colorCorrection1, -0.1, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection2, -0.2, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection3, -0.3, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection4, -0.4, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection5, -0.5, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection6, -0.6, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection7, -0.7, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection8, -0.8, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection9, -0.9, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection10, 1.1, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection11, 1.2, 0.01 )
    test.check_approx_abs( new_cc.colorCorrection12, 1.3, 0.01 )

with test.closure( 'Set AE Control'):
    aec = am_dev.get_ae_control()
    aec.meanIntensitySetPoint = 1234
    am_dev.set_ae_control( aec )
    new_aec = am_dev.get_ae_control()
    test.check_equal( new_aec.meanIntensitySetPoint, 1234 )

with test.closure( 'Set Depth Table'):
    dt = am_dev.get_depth_table()
    dt.depthUnits = 100
    dt.depthClampMin = 10
    dt.depthClampMax = 200
    dt.disparityMode = 1
    dt.disparityShift = 2
    am_dev.set_depth_table( dt )
    new_dt = am_dev.get_depth_table()
    test.check_equal( new_dt.depthUnits, 100 )
    test.check_equal( new_dt.depthClampMin, 10 )
    test.check_equal( new_dt.depthClampMax, 200 )
    test.check_equal( new_dt.disparityMode, 1 )
    test.check_equal( new_dt.disparityShift, 2 )

with test.closure( 'Set Census'):
    census = am_dev.get_census()
    census.uDiameter = 5
    census.vDiameter = 6
    am_dev.set_census( census )
    new_census = am_dev.get_census()
    test.check_equal( new_census.uDiameter, 5 )
    test.check_equal( new_census.vDiameter, 6 )

with test.closure( 'Set Amp Factor'):
    af = am_dev.get_amp_factor()
    af.a_factor = 0.12
    am_dev.set_amp_factor( af )
    new_af = am_dev.get_amp_factor()
    test.check_approx_abs( new_af.a_factor, 0.12, 0.005 )

with test.closure( 'Return to Default Visual Preset' ):
    depth_sensor.set_option( rs.option.visual_preset, int(rs.rs400_visual_preset.default ) )
    test.check( depth_sensor.get_option( rs.option.visual_preset ) == rs.rs400_visual_preset.default )

tw.stop_wrapper( dev )    
test.print_results_and_exit()