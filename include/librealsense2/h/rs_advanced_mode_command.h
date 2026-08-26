/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

/**
 * @file   rs_advanced_mode_command.h
 * @brief  Advanced Mode Commands header file
 */

#ifndef RS2_ADVANCED_MODE_COMMAND_H
#define RS2_ADVANCED_MODE_COMMAND_H

#include <stdint.h>

typedef struct
{
    uint32_t plusIncrement;
    uint32_t minusDecrement;
    uint32_t deepSeaMedianThreshold;
    uint32_t scoreThreshA;
    uint32_t scoreThreshB;
    uint32_t textureDifferenceThreshold;
    uint32_t textureCountThreshold;
    uint32_t deepSeaSecondPeakThreshold;
    uint32_t deepSeaNeighborThreshold;
    uint32_t lrAgreeThreshold;
}STDepthControlGroup;

typedef struct
{
    uint32_t rsmBypass;
    float    diffThresh;
    float    sloRauDiffThresh;
    uint32_t removeThresh;
}STRsm;

typedef struct
{
    uint32_t minWest;
    uint32_t minEast;
    uint32_t minWEsum;
    uint32_t minNorth;
    uint32_t minSouth;
    uint32_t minNSsum;
    uint32_t uShrink;
    uint32_t vShrink;
}STRauSupportVectorControl;

typedef struct
{
    uint32_t disableSADColor;
    uint32_t disableRAUColor;
    uint32_t disableSLORightColor;
    uint32_t disableSLOLeftColor;
    uint32_t disableSADNormalize;
}STColorControl;

typedef struct
{
    uint32_t rauDiffThresholdRed;
    uint32_t rauDiffThresholdGreen;
    uint32_t rauDiffThresholdBlue;
}STRauColorThresholdsControl;

typedef struct
{
    uint32_t diffThresholdRed;
    uint32_t diffThresholdGreen;
    uint32_t diffThresholdBlue;
}STSloColorThresholdsControl;

typedef struct
{
    uint32_t sloK1Penalty;
    uint32_t sloK2Penalty;
    uint32_t sloK1PenaltyMod1;
    uint32_t sloK2PenaltyMod1;
    uint32_t sloK1PenaltyMod2;
    uint32_t sloK2PenaltyMod2;
}STSloPenaltyControl;


typedef struct
{
    float    lambdaCensus;
    float    lambdaAD;
    uint32_t ignoreSAD;
}STHdad;

typedef struct
{
    float    colorCorrection1;
    float    colorCorrection2;
    float    colorCorrection3;
    float    colorCorrection4;
    float    colorCorrection5;
    float    colorCorrection6;
    float    colorCorrection7;
    float    colorCorrection8;
    float    colorCorrection9;
    float    colorCorrection10;
    float    colorCorrection11;
    float    colorCorrection12;
}STColorCorrection;

typedef struct
{
    uint32_t meanIntensitySetPoint;
}STAEControl;

typedef struct
{
    uint32_t depthUnits;
    int32_t  depthClampMin;
    int32_t  depthClampMax;
    uint32_t disparityMode;
    int32_t  disparityShift;
}STDepthTableControl;

typedef struct
{
    uint32_t uDiameter;
    uint32_t vDiameter;
}STCensusRadius;

typedef struct
{
    float   amplitude;
}STAFactor;

/* Every control group, with an entry per mode: [0] the value, [1] the minimum, [2] the maximum */
#define RS2_ADVANCED_MODE_MODES 3

typedef struct
{
    STDepthControlGroup          depth_control[RS2_ADVANCED_MODE_MODES];
    STRsm                        rsm[RS2_ADVANCED_MODE_MODES];
    STRauSupportVectorControl    rsvc[RS2_ADVANCED_MODE_MODES];
    STColorControl               color_control[RS2_ADVANCED_MODE_MODES];
    STRauColorThresholdsControl  rctc[RS2_ADVANCED_MODE_MODES];
    STSloColorThresholdsControl  sctc[RS2_ADVANCED_MODE_MODES];
    STSloPenaltyControl          spc[RS2_ADVANCED_MODE_MODES];
    STHdad                       hdad[RS2_ADVANCED_MODE_MODES];
    STColorCorrection            cc[RS2_ADVANCED_MODE_MODES];
    STDepthTableControl          depth_table[RS2_ADVANCED_MODE_MODES];
    STAEControl                  ae[RS2_ADVANCED_MODE_MODES];
    STCensusRadius               census[RS2_ADVANCED_MODE_MODES];
    STAFactor                    amp_factor[RS2_ADVANCED_MODE_MODES];
}STAdvancedModeControls;

#ifdef __cplusplus
extern "C"{
#endif


#ifdef __cplusplus
}
#endif

#endif /*RS2_ADVANCED_MODE_COMMAND_H*/
