/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/rs_advanced_mode.hpp>
#include <librealsense2/hpp/rs_serializable_device.hpp>

void init_advanced_mode(py::module &m) {
    /** RS400 Advanced Mode commands **/
    py::class_<STDepthControlGroup> _STDepthControlGroup(m, "STDepthControlGroup");
    _STDepthControlGroup.def(py::init<>())
        .def_readwrite("plusIncrement", &STDepthControlGroup::plusIncrement)
        .def_readwrite("minusDecrement", &STDepthControlGroup::minusDecrement)
        .def_readwrite("deepSeaMedianThreshold", &STDepthControlGroup::deepSeaMedianThreshold)
        .def_readwrite("scoreThreshA", &STDepthControlGroup::scoreThreshA)
        .def_readwrite("scoreThreshB", &STDepthControlGroup::scoreThreshB)
        .def_readwrite("textureDifferenceThreshold", &STDepthControlGroup::textureDifferenceThreshold)
        .def_readwrite("textureCountThreshold", &STDepthControlGroup::textureCountThreshold)
        .def_readwrite("deepSeaSecondPeakThreshold", &STDepthControlGroup::deepSeaSecondPeakThreshold)
        .def_readwrite("deepSeaNeighborThreshold", &STDepthControlGroup::deepSeaNeighborThreshold)
        .def_readwrite("lrAgreeThreshold", &STDepthControlGroup::lrAgreeThreshold)
        .def("__repr__", [](const STDepthControlGroup &e) {
            std::stringstream ss;
            ss << "minusDecrement: " << e.minusDecrement << ", ";
            ss << "deepSeaMedianThreshold: " << e.deepSeaMedianThreshold << ", ";
            ss << "scoreThreshA: " << e.scoreThreshA << ", ";
            ss << "scoreThreshB: " << e.scoreThreshB << ", ";
            ss << "textureDifferenceThreshold: " << e.textureDifferenceThreshold << ", ";
            ss << "textureCountThreshold: " << e.textureCountThreshold << ", ";
            ss << "deepSeaSecondPeakThreshold: " << e.deepSeaSecondPeakThreshold << ", ";
            ss << "deepSeaNeighborThreshold: " << e.deepSeaNeighborThreshold << ", ";
            ss << "lrAgreeThreshold: " << e.lrAgreeThreshold;
            return ss.str();
        });

    py::class_<STRsm> _STRsm(m, "STRsm");
    _STRsm.def(py::init<>())
        .def_readwrite("rsmBypass", &STRsm::rsmBypass)
        .def_readwrite("diffThresh", &STRsm::diffThresh)
        .def_readwrite("sloRauDiffThresh", &STRsm::sloRauDiffThresh)
        .def_readwrite("removeThresh", &STRsm::removeThresh)
        .def("__repr__", [](const STRsm &e) {
            std::stringstream ss;
            ss << "rsmBypass: " << e.rsmBypass << ", ";
            ss << "diffThresh: " << e.diffThresh << ", ";
            ss << "sloRauDiffThresh: " << e.sloRauDiffThresh << ", ";
            ss << "removeThresh: " << e.removeThresh;
            return ss.str();
        });

    py::class_<STRauSupportVectorControl> _STRauSupportVectorControl(m, "STRauSupportVectorControl");
    _STRauSupportVectorControl.def(py::init<>())
        .def_readwrite("minWest", &STRauSupportVectorControl::minWest)
        .def_readwrite("minEast", &STRauSupportVectorControl::minEast)
        .def_readwrite("minWEsum", &STRauSupportVectorControl::minWEsum)
        .def_readwrite("minNorth", &STRauSupportVectorControl::minNorth)
        .def_readwrite("minSouth", &STRauSupportVectorControl::minSouth)
        .def_readwrite("minNSsum", &STRauSupportVectorControl::minNSsum)
        .def_readwrite("uShrink", &STRauSupportVectorControl::uShrink)
        .def_readwrite("vShrink", &STRauSupportVectorControl::vShrink)
        .def("__repr__", [](const STRauSupportVectorControl &e) {
            std::stringstream ss;
            ss << "minWest: " << e.minWest << ", ";
            ss << "minEast: " << e.minEast << ", ";
            ss << "minWEsum: " << e.minWEsum << ", ";
            ss << "minNorth: " << e.minNorth << ", ";
            ss << "minSouth: " << e.minSouth << ", ";
            ss << "minNSsum: " << e.minNSsum << ", ";
            ss << "uShrink: " << e.uShrink << ", ";
            ss << "vShrink: " << e.vShrink;
            return ss.str();
        });

    py::class_<STColorControl> _STColorControl(m, "STColorControl");
    _STColorControl.def(py::init<>())
        .def_readwrite("disableSADColor", &STColorControl::disableSADColor)
        .def_readwrite("disableRAUColor", &STColorControl::disableRAUColor)
        .def_readwrite("disableSLORightColor", &STColorControl::disableSLORightColor)
        .def_readwrite("disableSLOLeftColor", &STColorControl::disableSLOLeftColor)
        .def_readwrite("disableSADNormalize", &STColorControl::disableSADNormalize)
        .def("__repr__", [](const STColorControl &e) {
            std::stringstream ss;
            ss << "disableSADColor: " << e.disableSADColor << ", ";
            ss << "disableRAUColor: " << e.disableRAUColor << ", ";
            ss << "disableSLORightColor: " << e.disableSLORightColor << ", ";
            ss << "disableSLOLeftColor: " << e.disableSLOLeftColor << ", ";
            ss << "disableSADNormalize: " << e.disableSADNormalize;
            return ss.str();
        });

    py::class_<STRauColorThresholdsControl> _STRauColorThresholdsControl(m, "STRauColorThresholdsControl");
    _STRauColorThresholdsControl.def(py::init<>())
        .def_readwrite("rauDiffThresholdRed", &STRauColorThresholdsControl::rauDiffThresholdRed)
        .def_readwrite("rauDiffThresholdGreen", &STRauColorThresholdsControl::rauDiffThresholdGreen)
        .def_readwrite("rauDiffThresholdBlue", &STRauColorThresholdsControl::rauDiffThresholdBlue)
        .def("__repr__", [](const STRauColorThresholdsControl &e) {
            std::stringstream ss;
            ss << "rauDiffThresholdRed: " << e.rauDiffThresholdRed << ", ";
            ss << "rauDiffThresholdGreen: " << e.rauDiffThresholdGreen << ", ";
            ss << "rauDiffThresholdBlue: " << e.rauDiffThresholdBlue;
            return ss.str();
        });

    py::class_<STSloColorThresholdsControl> _STSloColorThresholdsControl(m, "STSloColorThresholdsControl");
    _STSloColorThresholdsControl.def(py::init<>())
        .def_readwrite("diffThresholdRed", &STSloColorThresholdsControl::diffThresholdRed)
        .def_readwrite("diffThresholdGreen", &STSloColorThresholdsControl::diffThresholdGreen)
        .def_readwrite("diffThresholdBlue", &STSloColorThresholdsControl::diffThresholdBlue)
        .def("__repr__", [](const STSloColorThresholdsControl &e) {
            std::stringstream ss;
            ss << "diffThresholdRed: " << e.diffThresholdRed << ", ";
            ss << "diffThresholdGreen: " << e.diffThresholdGreen << ", ";
            ss << "diffThresholdBlue: " << e.diffThresholdBlue;
            return ss.str();
        });
    py::class_<STSloPenaltyControl> _STSloPenaltyControl(m, "STSloPenaltyControl");
    _STSloPenaltyControl.def(py::init<>())
        .def_readwrite("sloK1Penalty", &STSloPenaltyControl::sloK1Penalty)
        .def_readwrite("sloK2Penalty", &STSloPenaltyControl::sloK2Penalty)
        .def_readwrite("sloK1PenaltyMod1", &STSloPenaltyControl::sloK1PenaltyMod1)
        .def_readwrite("sloK2PenaltyMod1", &STSloPenaltyControl::sloK2PenaltyMod1)
        .def_readwrite("sloK1PenaltyMod2", &STSloPenaltyControl::sloK1PenaltyMod2)
        .def_readwrite("sloK2PenaltyMod2", &STSloPenaltyControl::sloK2PenaltyMod2)
        .def("__repr__", [](const STSloPenaltyControl &e) {
            std::stringstream ss;
            ss << "sloK1Penalty: " << e.sloK1Penalty << ", ";
            ss << "sloK2Penalty: " << e.sloK2Penalty << ", ";
            ss << "sloK1PenaltyMod1: " << e.sloK1PenaltyMod1 << ", ";
            ss << "sloK2PenaltyMod1: " << e.sloK2PenaltyMod1 << ", ";
            ss << "sloK1PenaltyMod2: " << e.sloK1PenaltyMod2 << ", ";
            ss << "sloK2PenaltyMod2: " << e.sloK2PenaltyMod2;
            return ss.str();
        });
    py::class_<STHdad> _STHdad(m, "STHdad");
    _STHdad.def(py::init<>())
        .def_readwrite("lambdaCensus", &STHdad::lambdaCensus)
        .def_readwrite("lambdaAD", &STHdad::lambdaAD)
        .def_readwrite("ignoreSAD", &STHdad::ignoreSAD)
        .def("__repr__", [](const STHdad &e) {
            std::stringstream ss;
            ss << "lambdaCensus: " << e.lambdaCensus << ", ";
            ss << "lambdaAD: " << e.lambdaAD << ", ";
            ss << "ignoreSAD: " << e.ignoreSAD;
            return ss.str();
        });

    py::class_<STColorCorrection> _STColorCorrection(m, "STColorCorrection");
    _STColorCorrection.def(py::init<>())
        .def_readwrite("colorCorrection1", &STColorCorrection::colorCorrection1)
        .def_readwrite("colorCorrection2", &STColorCorrection::colorCorrection2)
        .def_readwrite("colorCorrection3", &STColorCorrection::colorCorrection3)
        .def_readwrite("colorCorrection4", &STColorCorrection::colorCorrection4)
        .def_readwrite("colorCorrection5", &STColorCorrection::colorCorrection5)
        .def_readwrite("colorCorrection6", &STColorCorrection::colorCorrection6)
        .def_readwrite("colorCorrection7", &STColorCorrection::colorCorrection7)
        .def_readwrite("colorCorrection8", &STColorCorrection::colorCorrection8)
        .def_readwrite("colorCorrection9", &STColorCorrection::colorCorrection9)
        .def_readwrite("colorCorrection10", &STColorCorrection::colorCorrection10)
        .def_readwrite("colorCorrection11", &STColorCorrection::colorCorrection11)
        .def_readwrite("colorCorrection12", &STColorCorrection::colorCorrection12)
        .def("__repr__", [](const STColorCorrection &e) {
            std::stringstream ss;
            ss << "colorCorrection1: " << e.colorCorrection1 << ", ";
            ss << "colorCorrection2: " << e.colorCorrection2 << ", ";
            ss << "colorCorrection3: " << e.colorCorrection3 << ", ";
            ss << "colorCorrection4: " << e.colorCorrection4 << ", ";
            ss << "colorCorrection5: " << e.colorCorrection5 << ", ";
            ss << "colorCorrection6: " << e.colorCorrection6 << ", ";
            ss << "colorCorrection7: " << e.colorCorrection7 << ", ";
            ss << "colorCorrection8: " << e.colorCorrection8 << ", ";
            ss << "colorCorrection9: " << e.colorCorrection9 << ", ";
            ss << "colorCorrection10: " << e.colorCorrection10 << ", ";
            ss << "colorCorrection11: " << e.colorCorrection11 << ", ";
            ss << "colorCorrection12: " << e.colorCorrection12;
            return ss.str();
        });

    py::class_<STAEControl> _STAEControl(m, "STAEControl");
    _STAEControl.def(py::init<>())
        .def_readwrite("meanIntensitySetPoint", &STAEControl::meanIntensitySetPoint)
        .def("__repr__", [](const STAEControl &e) {
            std::stringstream ss;
            ss << "Mean Intensity Set Point: " << e.meanIntensitySetPoint;
            return ss.str();
        });

    py::class_<STDepthTableControl> _STDepthTableControl(m, "STDepthTableControl");
    _STDepthTableControl.def(py::init<>())
        .def_readwrite("depthUnits", &STDepthTableControl::depthUnits)
        .def_readwrite("depthClampMin", &STDepthTableControl::depthClampMin)
        .def_readwrite("depthClampMax", &STDepthTableControl::depthClampMax)
        .def_readwrite("disparityMode", &STDepthTableControl::disparityMode)
        .def_readwrite("disparityShift", &STDepthTableControl::disparityShift)
        .def("__repr__", [](const STDepthTableControl &e) {
            std::stringstream ss;
            ss << "depthUnits: " << e.depthUnits << ", ";
            ss << "depthClampMin: " << e.depthClampMin << ", ";
            ss << "depthClampMax: " << e.depthClampMax << ", ";
            ss << "disparityMode: " << e.disparityMode << ", ";
            ss << "disparityShift: " << e.disparityShift;
            return ss.str();
        });

    py::class_<STCensusRadius> _STCensusRadius(m, "STCensusRadius");
    _STCensusRadius.def(py::init<>())
        .def_readwrite("uDiameter", &STCensusRadius::uDiameter)
        .def_readwrite("vDiameter", &STCensusRadius::vDiameter)
        .def("__repr__", [](const STCensusRadius &e) {
            std::stringstream ss;
            ss << "uDiameter: " << e.uDiameter << ", ";
            ss << "vDiameter: " << e.vDiameter;
            return ss.str();
        });

    py::class_<STAFactor> _STAFactor(m, "STAFactor");
    _STAFactor.def(py::init<>())
        .def_readwrite("a_factor", &STAFactor::amplitude)
        .def("__repr__", [](const STAFactor &e) {
            std::stringstream ss;
            ss << "a_factor: " << e.amplitude;
            return ss.str();
    });

    py::class_<rs400::advanced_mode> rs400_advanced_mode(m, "rs400_advanced_mode");
    rs400_advanced_mode.def(py::init<rs2::device>(), "device"_a)
        .def("toggle_advanced_mode", &rs400::advanced_mode::toggle_advanced_mode, "enable"_a)
        .def("is_enabled", &rs400::advanced_mode::is_enabled)
        .def("set_depth_control", &rs400::advanced_mode::set_depth_control, "group"_a) //STDepthControlGroup
        .def("get_depth_control", &rs400::advanced_mode::get_depth_control, "mode"_a = 0)
        .def("set_rsm", &rs400::advanced_mode::set_rsm, "group") //STRsm
        .def("get_rsm", &rs400::advanced_mode::get_rsm, "mode"_a = 0)
        .def("set_rau_support_vector_control", &rs400::advanced_mode::set_rau_support_vector_control, "group"_a)//STRauSupportVectorControl
        .def("get_rau_support_vector_control", &rs400::advanced_mode::get_rau_support_vector_control, "mode"_a = 0)
        .def("set_color_control", &rs400::advanced_mode::set_color_control, "group"_a) //STColorControl
        .def("get_color_control", &rs400::advanced_mode::get_color_control, "mode"_a = 0)//STColorControl
        .def("set_rau_thresholds_control", &rs400::advanced_mode::set_rau_thresholds_control, "group"_a)//STRauColorThresholdsControl
        .def("get_rau_thresholds_control", &rs400::advanced_mode::get_rau_thresholds_control, "mode"_a = 0)
        .def("set_slo_color_thresholds_control", &rs400::advanced_mode::set_slo_color_thresholds_control, "group"_a)//STSloColorThresholdsControl
        .def("get_slo_color_thresholds_control", &rs400::advanced_mode::get_slo_color_thresholds_control, "mode"_a = 0)//STSloColorThresholdsControl
        .def("set_slo_penalty_control", &rs400::advanced_mode::set_slo_penalty_control, "group"_a) //STSloPenaltyControl
        .def("get_slo_penalty_control", &rs400::advanced_mode::get_slo_penalty_control, "mode"_a = 0)//STSloPenaltyControl
        .def("set_hdad", &rs400::advanced_mode::set_hdad, "group"_a) //STHdad
        .def("get_hdad", &rs400::advanced_mode::get_hdad, "mode"_a = 0)
        .def("set_color_correction", &rs400::advanced_mode::set_color_correction, "group"_a)
        .def("get_color_correction", &rs400::advanced_mode::get_color_correction, "mode"_a = 0) //STColorCorrection
        .def("set_depth_table", &rs400::advanced_mode::set_depth_table, "group"_a)
        .def("get_depth_table", &rs400::advanced_mode::get_depth_table, "mode"_a = 0) //STDepthTableControl
        .def("set_ae_control", &rs400::advanced_mode::set_ae_control, "group"_a)
        .def("get_ae_control", &rs400::advanced_mode::get_ae_control, "mode"_a = 0) //STAEControl
        .def("set_census", &rs400::advanced_mode::set_census, "group"_a)    //STCensusRadius
        .def("get_census", &rs400::advanced_mode::get_census, "mode"_a = 0) //STCensusRadius
        .def("set_amp_factor", &rs400::advanced_mode::set_amp_factor, "group"_a)    //STAFactor
        .def("get_amp_factor", &rs400::advanced_mode::get_amp_factor, "mode"_a = 0) //STAFactor
        .def("serialize_json", &rs400::advanced_mode::serialize_json)
        .def("load_json", &rs400::advanced_mode::load_json, "json_content"_a);
}

void init_serializable_device(py::module& m) {
    py::class_<rs2::serializable_device, py_holder<rs2::serializable_device>> serializable_device(m, "serializable_device");
    serializable_device.def(py::init<rs2::device>(), "device"_a)
        .def("serialize_json", &rs2::serializable_device::serialize_json)
        .def("load_json", &rs2::serializable_device::load_json, "json_content"_a);
}
