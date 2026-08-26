/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include <thread>
#include <sstream>

#include "types.h"
#include <librealsense2/rs_advanced_mode.h>
#include "core/advanced_mode.h"
#include "api.h"

#include <rsutils/string/make-less-screamy.h>



#define STRCASE( T, X )                                                                                                \
    case RS2_##T##_##X: {                                                                                              \
        static std::string s##T##_##X##_str = rsutils::string::make_less_screamy( #X );                                \
        return s##T##_##X##_str.c_str();                                                                               \
    }

namespace librealsense
{
    RS2_ENUM_HELPERS(rs2_rs400_visual_preset, RS400_VISUAL_PRESET)

    const char* get_string(rs2_rs400_visual_preset value)
    {
        #define CASE(X) STRCASE(RS400_VISUAL_PRESET, X)
        switch (value)
        {
        CASE(CUSTOM)
        CASE(HAND)
        CASE(HIGH_ACCURACY)
        CASE(HIGH_DENSITY)
        CASE(MEDIUM_DENSITY)
        CASE(DEFAULT)
        CASE(REMOVE_IR_PATTERN)
        default: assert(!is_valid(value)); return UNKNOWN_VALUE;
        }
        #undef CASE
    }
}

using namespace librealsense;

const char* rs2_rs400_visual_preset_to_string(rs2_rs400_visual_preset preset){ return get_string(preset); }

void rs2_toggle_advanced_mode(rs2_device* dev, int enable, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->toggle_advanced_mode(enable > 0);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, enable)

void rs2_is_enabled(rs2_device* dev, int* enabled, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(enabled);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    *enabled = advanced_mode->is_enabled();
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, enabled)

void rs2_set_depth_control(rs2_device* dev, const STDepthControlGroup* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_depth_control_group(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_depth_control(rs2_device* dev, STDepthControlGroup* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_depth_control_group(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_rsm(rs2_device* dev, const STRsm* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_rsm(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_rsm(rs2_device* dev, STRsm* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_rsm(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_rau_support_vector_control(rs2_device* dev, const STRauSupportVectorControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_rau_support_vector_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_rau_support_vector_control(rs2_device* dev, STRauSupportVectorControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_rau_support_vector_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_color_control(rs2_device* dev, const STColorControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_color_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_color_control(rs2_device* dev, STColorControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_color_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_rau_thresholds_control(rs2_device* dev, const STRauColorThresholdsControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_rau_color_thresholds_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_rau_thresholds_control(rs2_device* dev, STRauColorThresholdsControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_rau_color_thresholds_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_slo_color_thresholds_control(rs2_device* dev, const STSloColorThresholdsControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_slo_color_thresholds_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_slo_color_thresholds_control(rs2_device* dev, STSloColorThresholdsControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_slo_color_thresholds_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_slo_penalty_control(rs2_device* dev, const STSloPenaltyControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_slo_penalty_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_slo_penalty_control(rs2_device* dev, STSloPenaltyControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_slo_penalty_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_hdad(rs2_device* dev, const STHdad* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_hdad(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_hdad(rs2_device* dev, STHdad* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_hdad(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_color_correction(rs2_device* dev, const STColorCorrection* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_color_correction(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_color_correction(rs2_device* dev, STColorCorrection* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_color_correction(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_depth_table(rs2_device* dev, const STDepthTableControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_depth_table_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_depth_table(rs2_device* dev, STDepthTableControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_depth_table_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_ae_control(rs2_device* dev, const STAEControl* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_ae_control(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_ae_control(rs2_device* dev, STAEControl* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_ae_control(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_census(rs2_device* dev, const STCensusRadius* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_census_radius(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_census(rs2_device* dev, STCensusRadius* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_census_radius(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_set_amp_factor(rs2_device* dev, const  STAFactor* group, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->set_amp_factor(*group);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group)

void rs2_get_amp_factor(rs2_device* dev, STAFactor* group, int mode, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(group);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_amp_factor(group, mode);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, group, mode)

void rs2_get_all_advanced_controls(rs2_device* dev, STAdvancedModeControls* controls, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(controls);
    auto advanced_mode = VALIDATE_INTERFACE(dev->device, librealsense::ds_advanced_mode_interface);
    advanced_mode->get_all_controls(controls);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, controls)
