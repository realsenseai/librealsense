// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "ds-calib-parsers.h"
#include "ds-private.h"

#include "ds/d400/d400-private.h"
#include "ds/d500/d500-private.h"

namespace librealsense
{
    using namespace ds;

    mm_calib_handler::mm_calib_handler(std::shared_ptr<hw_monitor> hw_monitor, uint16_t pid) :
        _hw_monitor(hw_monitor), _pid(pid)
    {
        _imu_eeprom_raw = [this]() {
            return get_imu_eeprom_raw();
        };

        _calib_parser = [this]() {

            std::vector<uint8_t> raw(tm1_eeprom_size);
            uint16_t calib_id = dm_v2_eeprom_id; //assume DM V2 IMU as default platform
            bool valid = false;

            try
            {
                raw = *_imu_eeprom_raw;
                calib_id = *reinterpret_cast<uint16_t*>(raw.data());
                valid = true;
            }
            catch (const std::exception&)
            {
                // in case calibration table errors (invalid table, empty table, or corrupted table), data is invalid and default intrinsic and extrinsic will be used
                LOG_WARNING("IMU Calibration is not available, default intrinsic and extrinsic will be used.");
            }

            std::shared_ptr<mm_calib_parser> prs = nullptr;
            switch (calib_id)
            {
            case dm_v2_eeprom_id: // DM V2 id
                prs = std::make_shared<dm_v2_imu_calib_parser>(raw, _pid, valid); break;
            case tm1_eeprom_id: // TM1 id
                prs = std::make_shared<tm1_imu_calib_parser>(raw); break;
            case l500_eeprom_id: // L515
                prs = std::make_shared<l500_imu_calib_parser>(raw, valid); break;
            default:
                throw recoverable_exception(rsutils::string::from() << "Motion Intrinsics unresolved - "
                    << ((valid) ? "device is not calibrated" : "invalid calib type "),
                    RS2_EXCEPTION_TYPE_BACKEND);
            }
            return prs;
        };
    }

    std::vector<uint8_t> mm_calib_handler::get_imu_eeprom_raw() const
    {
        const int offset = 0;
        const int size = eeprom_imu_table_size;
        command cmd(MMER, offset, size);
        return _hw_monitor->send(cmd);
    }

    ds::imu_intrinsic mm_calib_handler::get_intrinsic(rs2_stream stream)
    {
        return (*_calib_parser)->get_intrinsic(stream);
    }

    rs2_extrinsics mm_calib_handler::get_extrinsic(rs2_stream stream)
    {
        return (*_calib_parser)->get_extrinsic_to(stream);
    }

    const std::vector<uint8_t> mm_calib_handler::get_fisheye_calib_raw()
    {
        auto fe_calib_table = (*(ds::check_calib<ds::tm1_eeprom>(*_imu_eeprom_raw))).calibration_table.calib_model.fe_calibration;
        uint8_t* fe_calib_ptr = reinterpret_cast<uint8_t*>(&fe_calib_table);
        return std::vector<uint8_t>(fe_calib_ptr, fe_calib_ptr + ds::fisheye_calibration_table_size);
    }

    rs2_extrinsics tm1_imu_calib_parser::get_extrinsic_to(rs2_stream stream)
    {
        if (!(RS2_STREAM_ACCEL == stream) && !(RS2_STREAM_GYRO == stream) && !(RS2_STREAM_FISHEYE == stream))
            throw std::runtime_error(rsutils::string::from() << "TM1 Calibration does not provide extrinsic for : " << rs2_stream_to_string(stream) << " !");

        auto fe_calib = calib_table.calibration_table.calib_model.fe_calibration;

        auto rot = fe_calib.fisheye_to_imu.rotation;
        auto trans = fe_calib.fisheye_to_imu.translation;

        pose ex = { { { rot(0,0), rot(1,0),rot(2,0)},
                      { rot(0,1), rot(1,1),rot(2,1) },
                      { rot(0,2), rot(1,2),rot(2,2) } },
                      { trans[0], trans[1], trans[2] } };

        if (RS2_STREAM_FISHEYE == stream)
            return inverse(from_pose(ex));
        else
            return from_pose(ex);
    }

    ds::imu_intrinsic tm1_imu_calib_parser::get_intrinsic(rs2_stream stream)
    {
        ds::imu_intrinsics in_intr;
        switch (stream)
        {
        case RS2_STREAM_ACCEL:
            in_intr = calib_table.calibration_table.imu_calib_table.accel_intrinsics; break;
        case RS2_STREAM_GYRO:
            in_intr = calib_table.calibration_table.imu_calib_table.gyro_intrinsics; break;
        default:
            throw std::runtime_error(rsutils::string::from() << "TM1 IMU Calibration does not support intrinsic for : " << rs2_stream_to_string(stream) << " !");
        }
        ds::imu_intrinsic out_intr{};
        for (auto i = 0; i < 3; i++)
        {
            out_intr.sensitivity(i, i) = in_intr.scale[i];
            out_intr.bias[i] = in_intr.bias[i];
        }
        return out_intr;
    }

    dm_v2_imu_calib_parser::dm_v2_imu_calib_parser(const std::vector<uint8_t>& raw_data, uint16_t pid, bool valid)
    {
        // product id to identify platform specific parameters, imu models and physical location
        _pid = pid;

        _calib_table.module_info.dm_v2_calib_table.extrinsic_valid = 0;
        _calib_table.module_info.dm_v2_calib_table.intrinsic_valid = 0;

        _valid_intrinsic = false;
        _valid_extrinsic = false;

        // default parser to be applied when no FW calibration is available
        if (valid)
        {
            try
            {
                _calib_table = *(ds::check_calib<ds::dm_v2_eeprom>(raw_data));
                _valid_intrinsic = (_calib_table.module_info.dm_v2_calib_table.intrinsic_valid == 1) ? true : false;
                _valid_extrinsic = (_calib_table.module_info.dm_v2_calib_table.extrinsic_valid == 1) ? true : false;
            }
            catch (...)
            {
                _valid_intrinsic = false;
                _valid_extrinsic = false;
            }
        }

        // TODO - review possibly refactor into a map if necessary
        //
        // predefined platform specific extrinsic, IMU assembly transformation based on mechanical drawing (meters)
        rs2_extrinsics _def_extr;

        if (_pid == ds::RS435I_PID || _pid == ds::RS436_PID)
        {
            // D435i specific - Bosch BMI055
            _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, { -0.00552f, 0.0051f, 0.01174f} };
            _imu_2_depth_rot = { {-1,0,0},{0,1,0},{0,0,-1} };
        }
        else if (_pid == ds::RS455_PID)
        {
            // D455 specific - Bosch BMI055
            _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ -0.03022f, 0.0074f, 0.01602f } };
            _imu_2_depth_rot = { { -1,0,0 },{ 0,1,0 },{ 0,0,-1 } };
        }
        else if( _pid == ds::RS457_PID )
        {
            _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ -0.09530f, -0.00056f, 0.01740f } };
            _imu_2_depth_rot = { { -1,0,0 },{ 0,1,0 },{ 0,0,-1 } };
        }
        else if( _pid == ds::D555_PID )
        {
            _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ -0.0374f, 0.00719f, 0.0223f } };
            _imu_2_depth_rot = { { -1,0,0 },{ 0,1,0 },{ 0,0,-1 } };
        }
        else if ( _pid == ds::D585_LEGACY_PID || _pid == ds::D585S_PID )
        {
            // D585/D585S specific - Bosch BMI085
            _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ -0.0195f, 0.0f, -0.0114f } };
            _imu_2_depth_rot = { { -1,0,0 },{ 0,-1,0 },{ 0,0,1 } };
        }
        else // unmapped configurations
        {
            // IMU on new devices is oriented such that FW output is consistent with D435i
            // use same rotation matrix as D435i so that librealsense output from unsupported
            // devices will still be correctly aligned with depth coordinate system.
            _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ 0.f, 0.f, 0.f } };
            _imu_2_depth_rot = { { -1,0,0 },{ 0,1,0 },{ 0,0,-1 } };
            LOG_ERROR("Undefined platform with IMU, use default intrinsic/extrinsic data, PID: " << _pid);
        }

        // default intrinsic in case no valid calibration data is available
        // scale = 1 and offset = 0
        _def_intr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ 0.0, 0.0, 0.0 } };

        // handling extrinsic
        if (_valid_extrinsic)
        {
            // extrinsic from calibration table, by user custom calibration, The extrinsic is stored as array of floats / little-endian
            std::memcpy( &_extr, &_calib_table.module_info.dm_v2_calib_table.depth_to_imu, sizeof( rs2_extrinsics ) );
        }
        else
        {
            LOG_INFO("IMU extrinsic table not found; using CAD values");
            // default extrinsic based on mechanical drawing
            _extr = _def_extr;
        }
    }

    rs2_extrinsics dm_v2_imu_calib_parser::get_extrinsic_to(rs2_stream stream)
    {
        if (!(RS2_STREAM_ACCEL == stream) && !(RS2_STREAM_GYRO == stream))
            throw std::runtime_error(rsutils::string::from() << "Depth Module V2 does not support extrinsic for : " << rs2_stream_to_string(stream) << " !");

        return _extr;
    }

    ds::imu_intrinsic dm_v2_imu_calib_parser::get_intrinsic(rs2_stream stream)
    {
        ds::dm_v2_imu_intrinsic in_intr;
        switch (stream)
        {
        case RS2_STREAM_ACCEL:
            if (_valid_intrinsic)
            {
                in_intr = _calib_table.module_info.dm_v2_calib_table.accel_intrinsic;
            }
            else
            {
                LOG_INFO("Depth Module V2 IMU " << rs2_stream_to_string(stream) << "no valid intrinsic available, use default values.");
                in_intr = _def_intr;
            }
            break;
        case RS2_STREAM_GYRO:
            if (_valid_intrinsic)
            {
                in_intr = _calib_table.module_info.dm_v2_calib_table.gyro_intrinsic;
                in_intr.bias = in_intr.bias * static_cast<float>(d2r);        // The gyro bias is calculated in Deg/sec
            }
            else
            {
                LOG_INFO("Depth Module V2 IMU " << rs2_stream_to_string(stream) << "intrinsic not valid, use default values.");
                in_intr = _def_intr;
            }
            break;
        default:
            throw std::runtime_error(rsutils::string::from() << "Depth Module V2 does not provide intrinsic for stream type : " << rs2_stream_to_string(stream) << " !");
        }

        return { in_intr.sensitivity, in_intr.bias, {0,0,0}, {0,0,0} };
    }

    l500_imu_calib_parser::l500_imu_calib_parser(const std::vector<uint8_t>& raw_data, bool valid)
    {
        // default parser to be applied when no FW calibration is available
        _valid_intrinsic = false;
        _valid_extrinsic = false;

        // in case calibration table is provided but with incorrect header and CRC, both intrinsic and extrinsic will use default values
        // calibration table has flags to indicate if it contains valid intrinsic and extrinsic, use this as further indication if the data is valid
        // currently, the imu calibration script only calibrates intrinsic so only the intrinsic_valid field is set during calibration, extrinsic
        // will use default values derived from mechanical CAD drawing, however, if the calibration script in the future or user calibration provide
        // valid extrinsic, the extrinsic_valid field should be set in the table and detected here so the values from the table can be used.
        if (valid)
        {
            try
            {
                imu_calib_table = *(ds::check_calib<ds::dm_v2_calibration_table>(raw_data));
                _valid_intrinsic = (imu_calib_table.intrinsic_valid == 1) ? true : false;
                _valid_extrinsic = (imu_calib_table.extrinsic_valid == 1) ? true : false;
            }
            catch (...)
            {
                _valid_intrinsic = false;
                _valid_extrinsic = false;
            }
        }

        // L515 specific
        // Bosch BMI085 assembly transformation based on mechanical drawing (meters)
        // device thickness 26 mm from front glass to back surface
        // depth ground zero is 4.5mm from front glass into the device
        // IMU reference in z direction is at 20.93mm from back surface
        //
        // IMU offset in Z direction = 4.5 mm - (26 mm - 20.93 mm) = 4.5 mm - 5.07mm = - 0.57mm
        // IMU offset in x and Y direction (12.45mm, -16.42mm) from center
        //
        // coordinate system as reference, looking from back of the camera towards front,
        // the positive x-axis points to the right, the positive y-axis points down, and the
        // positive z-axis points forward.
        // origin in the center but z-direction 4.5mm from front glass into the device
        // the matrix below is such that output of motion data is consistent with convention
        // that positive direction aligned with gravity leads to -1g and opposite direction
        // leads to +1g, for example, positive z_aixs points forward away from front glass of
        // the device, 1) if place the device flat on a table, facing up, positive z-axis points
        // up, z-axis acceleration is around +1g; 2) facing down, positive z-axis points down,
        // z-axis accleration would be around -1g
        rs2_extrinsics _def_extr;
        _def_extr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ -0.01245f, 0.01642f, 0.00057f } };
        _imu_2_depth_rot = { { -1, 0, 0 },{ 0, 1, 0 },{ 0, 0, -1 } };

        // default intrinsic in case no valid calibration data is available
        // scale = 1 and offset = 0
        _def_intr = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 },{ 0.0, 0.0, 0.0 } };


        // handling extrinsic
        if (_valid_extrinsic)
        {
            // only in case valid extrinsic is available in calibration data by calibration script in future or user custom calibration
            std::memcpy( &_extr, &imu_calib_table.depth_to_imu, sizeof( rs2_extrinsics ) );
        }
        else
        {
            // L515 - BMI085 assembly transformation based on mechanical drawing
            LOG_INFO("IMU extrinsic using CAD values");
            _extr = _def_extr;
        }
    }


    ds::imu_intrinsic l500_imu_calib_parser::get_intrinsic(rs2_stream stream)
    {
        ds::dm_v2_imu_intrinsic in_intr;
        switch (stream)
        {
        case RS2_STREAM_ACCEL:
            if (_valid_intrinsic)
            {
                in_intr = imu_calib_table.accel_intrinsic;
            }
            else
            {
                LOG_INFO("L515 IMU " << rs2_stream_to_string(stream) << "no valid intrinsic available, use default values.");
                in_intr = _def_intr;
            }
            break;
        case RS2_STREAM_GYRO:
            if (_valid_intrinsic)
            {
                in_intr = imu_calib_table.gyro_intrinsic;
                in_intr.bias = in_intr.bias * static_cast<float>(d2r);        // The gyro bias is calculated in Deg/sec
            }
            else
            {
                LOG_INFO("L515 IMU " << rs2_stream_to_string(stream) << "no valid intrinsic available, use default values.");
                in_intr = _def_intr;
            }
            break;
        default:
            throw std::runtime_error(rsutils::string::from() << "L515 does not provide intrinsic for stream type : " << rs2_stream_to_string(stream) << " !");
        }

        return{ in_intr.sensitivity, in_intr.bias,{ 0,0,0 },{ 0,0,0 } };
    }

    rs2_extrinsics l500_imu_calib_parser::get_extrinsic_to(rs2_stream stream)
    {
        if (!(RS2_STREAM_ACCEL == stream) && !(RS2_STREAM_GYRO == stream))
            throw std::runtime_error(rsutils::string::from() << "L515 does not support extrinsic for : " << rs2_stream_to_string(stream) << " !");

        return _extr;
    }


}
