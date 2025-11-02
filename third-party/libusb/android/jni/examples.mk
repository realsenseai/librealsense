# Android build config for libusb examples
# Copyright © 2012-2013 RealVNC Ltd. <toby.gray@realvnc.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

LOCAL_PATH := $(call my-dir)
LIBUSB_ROOT_REL := ../..
LIBUSB_ROOT_ABS := $(LOCAL_PATH)/../..

ifeq ($(USE_PC_NAME),1)
  LIBUSB_MODULE := usb-1.0
else
  LIBUSB_MODULE := libusb1.0
endif

# dpfp

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/dpfp.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := dpfp

include $(BUILD_EXECUTABLE)

# dpfp_threaded

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/dpfp.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_CFLAGS := -DDPFP_THREADED -pthread

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := dpfp_threaded

include $(BUILD_EXECUTABLE)

# fxload

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/ezusb.c \
  $(LIBUSB_ROOT_REL)/examples/fxload.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := fxload

include $(BUILD_EXECUTABLE)

# hotplugtest

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/hotplugtest.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := hotplugtest

include $(BUILD_EXECUTABLE)

# listdevs

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/listdevs.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := listdevs

include $(BUILD_EXECUTABLE)

# sam3u_benchmark

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/sam3u_benchmark.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := sam3u_benchmark

include $(BUILD_EXECUTABLE)

# xusb

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/examples/xusb.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += $(LIBUSB_MODULE)

LOCAL_MODULE := xusb

include $(BUILD_EXECUTABLE)

# unrooted_android

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LIBUSB_ROOT_REL)/android/examples/unrooted_android.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/.. \
  $(LIBUSB_ROOT_ABS)

LOCAL_SHARED_LIBRARIES += libusb1.0

LOCAL_LDLIBS += -llog

LOCAL_MODULE := unrooted_android

include $(BUILD_SHARED_LIBRARY)
