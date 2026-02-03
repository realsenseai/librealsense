// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include <jni.h>
#include <memory>
#include <vector>
#include "error.h"

#include "../../../include/librealsense2/rs.h"
#include "../../../include/librealsense2/hpp/rs_device.hpp"
#include "../../api.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_com_intel_realsense_librealsense_Device_nSupportsInfo(JNIEnv *env, jclass type, jlong handle,
                                                           jint info) {
    rs2_error *e = NULL;
    auto rv = rs2_supports_device_info(reinterpret_cast<const rs2_device *>(handle),
                                       static_cast<rs2_camera_info>(info), &e);
    handle_error(env, e);
    return rv > 0;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_intel_realsense_librealsense_Device_nGetInfo(JNIEnv *env, jclass type, jlong handle,
                                                      jint info) {
    rs2_error *e = NULL;
    const char* rv = rs2_get_device_info(reinterpret_cast<const rs2_device *>(handle),
                                         static_cast<rs2_camera_info>(info), &e);
    handle_error(env, e);
    if (NULL == rv)
        rv = "";
    return env->NewStringUTF(rv);
}

extern "C" JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_Device_nRelease(JNIEnv *env, jclass type, jlong handle) {
    rs2_delete_device(reinterpret_cast<rs2_device *>(handle));
}

extern "C"
JNIEXPORT jlongArray JNICALL
Java_com_intel_realsense_librealsense_Device_nQuerySensors(JNIEnv *env, jclass type, jlong handle) {
    rs2_error* e = nullptr;
    std::shared_ptr<rs2_sensor_list> list(
            rs2_query_sensors(reinterpret_cast<const rs2_device *>(handle), &e),
            rs2_delete_sensor_list);
    handle_error(env, e);

    auto size = rs2_get_sensors_count(list.get(), &e);
    handle_error(env, e);

    std::vector<rs2_sensor*> sensors;
    for (auto i = 0; i < size; i++)
    {
        auto s = rs2_create_sensor(list.get(), i, &e);
        handle_error(env, e);
        sensors.push_back(s);
    }
    jlongArray rv = env->NewLongArray(sensors.size());

    for (auto i = 0; i < sensors.size(); i++)
    {
        env->SetLongArrayRegion(rv, i, 1, reinterpret_cast<const jlong *>(&sensors[i]));
    }

    return rv;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_Updatable_nEnterUpdateState(JNIEnv *env, jclass type,
                                                                  jlong handle) {
    rs2_error *e = NULL;
    rs2_enter_update_state(reinterpret_cast<const rs2_device *>(handle), &e);
    handle_error(env, e);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_Updatable_nUpdateFirmwareUnsigned(JNIEnv *env,
                                                                        jobject instance,
                                                                        jlong handle,
                                                                        jbyteArray image_,
                                                                        jint update_mode) {
    jbyte *image = env->GetByteArrayElements(image_, NULL);
    auto length = env->GetArrayLength(image_);
    rs2_error *e = NULL;
    jclass cls = env->GetObjectClass(instance);
    jmethodID id = env->GetMethodID(cls, "onProgress", "(F)V");
    auto cb = [&](float progress){ env->CallVoidMethod(instance, id, progress); };
    rs2_update_firmware_unsigned_cpp(reinterpret_cast<const rs2_device *>(handle), image, length,
                                     new rs2::update_progress_callback<decltype(cb)>(cb), update_mode, &e);
    handle_error(env, e);
    env->ReleaseByteArrayElements(image_, image, 0);
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_intel_realsense_librealsense_Updatable_nCreateFlashBackup(JNIEnv *env, jobject instance,
                                                                   jlong handle) {
    rs2_error* e = NULL;
    jclass cls = env->GetObjectClass(instance);
    jmethodID id = env->GetMethodID(cls, "onProgress", "(F)V");
    auto cb = [&](float progress){ env->CallVoidMethod(instance, id, progress); };

    std::shared_ptr<const rs2_raw_data_buffer> raw_data_buffer(
            rs2_create_flash_backup_cpp(reinterpret_cast<rs2_device *>(handle), new rs2::update_progress_callback<decltype(cb)>(cb), &e),
            [](const rs2_raw_data_buffer* buff){ if(buff) delete buff;});
    handle_error(env, e);

    jbyteArray rv = env->NewByteArray(raw_data_buffer->buffer.size());
    env->SetByteArrayRegion(rv, 0, raw_data_buffer->buffer.size(),
        reinterpret_cast<const jbyte *>(raw_data_buffer->buffer.data()));
    return rv;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_intel_realsense_librealsense_Updatable_nCheckFirmwareCompatibility(JNIEnv *env, jobject instance,
                                                                            jlong handle, jbyteArray image) {
    rs2_error *e = NULL;
    auto length = env->GetArrayLength(image);
    int rv = rs2_check_firmware_compatibility(reinterpret_cast<const rs2_device *>(handle), image, length, &e);
    handle_error(env, e);
    return rv > 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_UpdateDevice_nUpdateFirmware(JNIEnv *env, jobject instance,
                                                                   jlong handle,
                                                                   jbyteArray image_) {
    jbyte *image = env->GetByteArrayElements(image_, NULL);
    auto length = env->GetArrayLength(image_);
    rs2_error *e = NULL;
    jclass cls = env->GetObjectClass(instance);
    jmethodID id = env->GetMethodID(cls, "onProgress", "(F)V");
    auto cb = [&](float progress){ env->CallVoidMethod(instance, id, progress); };
    rs2_update_firmware_cpp(reinterpret_cast<const rs2_device *>(handle), image, length,
                            new rs2::update_progress_callback<decltype(cb)>(cb), &e);
    handle_error(env, e);
    env->ReleaseByteArrayElements(image_, image, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_Device_nHardwareReset(JNIEnv *env, jclass type,
                                                            jlong handle) {
    rs2_error *e = NULL;
    rs2_hardware_reset(reinterpret_cast<const rs2_device *>(handle), &e);
    handle_error(env, e);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_intel_realsense_librealsense_Device_nIsDeviceExtendableTo(JNIEnv *env, jclass type,
                                                                   jlong handle, jint extension) {
    rs2_error *e = NULL;
    int rv = rs2_is_device_extendable_to(reinterpret_cast<const rs2_device *>(handle),
                                         static_cast<rs2_extension>(extension), &e);
    handle_error(env, e);
    return rv > 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_AutoCalibratedDevice_nRunOnChipCalibration(JNIEnv *env, jclass type, jlong handle, jstring json_cont, jobject result_object, jint timeout, jboolean oneButtonCallibration) {
    rs2_device *rs2Device = reinterpret_cast<rs2_device *>(handle);
    rs2_error *e = NULL;
    float health = MAXFLOAT;


    jclass clazz = env->GetObjectClass(result_object);

    //Calibration data
    jfieldID calibration_data_field = env->GetFieldID(clazz, "mCalibrationData", "Ljava/nio/ByteBuffer;");
    jobject calibration_data = env->GetObjectField(result_object, calibration_data_field);

    //Get the memory address of the bytebuffer
    auto new_table_buffer = (uint8_t *) (*env).GetDirectBufferAddress(calibration_data);
    auto new_table_buffer_size = (size_t) (*env).GetDirectBufferCapacity(calibration_data);

    //Convert the input jstring to the format that the library requires
    auto json_ptr = (*env).GetStringUTFChars(json_cont, NULL);
    auto json = std::string(json_ptr);

    //Run the on chip calibration
    auto result = rs2_run_on_chip_calibration(rs2Device, json.c_str(), json.size(), &health,
                                              nullptr, nullptr, timeout, &e);

    handle_error(env, e);

    //If it didn't fail, copy the new calibration data to the buffer
    if (result != NULL && result->buffer.size() <= new_table_buffer_size)
    {
        std::copy(result->buffer.begin(), result->buffer.end(), new_table_buffer);

        //Set the health result
        jfieldID health_field = env->GetFieldID(clazz, "mHealth", "F");
        env->SetFloatField(result_object, health_field, health);

        if(oneButtonCallibration)
        {
            //Seperate the health values from the returned result
            int h_both = static_cast<int>(health);
            int h_1 = (h_both & 0x00000FFF);
            int h_2 = (h_both & 0x00FFF000) >> 12;
            int sign = (h_both & 0x0F000000) >> 24;

            float health_1 = h_1 / 1000.0f;

            if (sign & 1)
                health_1 = -health_1;

            float health_2 = h_2 / 1000.0f;
            if (sign & 2)
                health_2 = -health_2;

            //Set health1 result
            jfieldID health1Field = env->GetFieldID(clazz, "mHealth1", "F");
            env->SetFloatField(result_object, health1Field, health_1);

            //Set health2 result
            jfieldID health2Field = env->GetFieldID(clazz, "mHealth2", "F");
            env->SetFloatField(result_object, health2Field, health_2);
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_AutoCalibratedDevice_nSetTable(JNIEnv *env, jclass type, jlong handle, jobject table, jboolean write) {
    rs2_device *rs2Device = reinterpret_cast<rs2_device *>(handle);
    rs2_error *e = NULL;

    //Get the memory address of the bytebuffer
    uint8_t *new_table_buffer = static_cast<uint8_t *>((*env).GetDirectBufferAddress(table));
    size_t new_table_buffer_size = (*env).GetDirectBufferCapacity(table);

    //Check the buffer is valid
    if (new_table_buffer != nullptr) {

        //Populate the calibration from the buffer
        rs2_set_calibration_table(rs2Device, new_table_buffer, new_table_buffer_size, &e);

        handle_error(env, e);

        //Save the new calibration if required
        if (write) {
            rs2_write_calibration(rs2Device, &e);
            handle_error(env, e);
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_intel_realsense_librealsense_AutoCalibratedDevice_nGetTable(JNIEnv *env, jclass type, jlong handle, jobject table) {

        rs2_device *rs2Device = reinterpret_cast<rs2_device *>(handle);
        rs2_error *e = NULL;

        //Get the calibration data from the device
        const rs2_raw_data_buffer *buffer = rs2_get_calibration_table(rs2Device, &e);

        handle_error(env, e);

        //Copy the results to the buffer
        uint8_t *new_table_buffer = (uint8_t *) env->GetDirectBufferAddress(table);
        size_t new_table_buffer_size = (size_t) env->GetDirectBufferCapacity(table);
        if (buffer->buffer.size() <= new_table_buffer_size)
            std::copy(buffer->buffer.begin(), buffer->buffer.end(), new_table_buffer);

}
