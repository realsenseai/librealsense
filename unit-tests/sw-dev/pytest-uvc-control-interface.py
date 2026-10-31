# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Negotiate identical RGB profiles on distinct RSUSB interfaces without hardware."""

from pathlib import Path
import shutil
import subprocess

import pytest


def test_stream_control_stays_on_selected_interface(tmp_path):
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("RSUSB control regression requires a C++14 compiler")
    source = (Path(__file__).resolve().parents[2] / "src/uvc/uvc-device.cpp").read_text()
    start = source.index("        usb_status rs_uvc_device::get_stream_ctrl_format_size(")
    end = source.index("\n        usb_status rs_uvc_device::query_stream_ctrl(", start)
    production = source[start:end]
    swap = next(line for line in source.splitlines() if line.startswith("#define SWAP_UINT32"))
    preamble = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
enum usb_status { RS2_USB_STATUS_SUCCESS, RS2_USB_STATUS_INVALID_PARAM, USB_ERROR };
enum { UVC_GET_MAX = 3 };
struct uvc_frame_desc_t {
    uint16_t wWidth = 1280, wHeight = 720;
    uint8_t bFrameIndex = 2;
    std::vector<uint32_t> intervals = {400000, 333333};
};
struct uvc_format_desc_t {
    uint32_t guidFormat[4] = {};
    uint8_t bFormatIndex = 1;
    std::vector<uvc_frame_desc_t> frame_descs = {uvc_frame_desc_t{}};
};
struct uvc_format_t {
    uint32_t fourcc = 0;
    int width = 1280, height = 720, fps = 30;
    uint8_t interfaceNumber = 7;
};
struct uvc_stream_ctrl_t {
    uint8_t bInterfaceNumber = 0, bmHint = 0, bFormatIndex = 0, bFrameIndex = 0;
    uint32_t dwFrameInterval = 0;
};
struct parser {
    std::map<int, std::vector<uvc_format_desc_t>> formats;
    const std::map<int, std::vector<uvc_format_desc_t>>& get_formats() { return formats; }
};
struct rs_uvc_device {
    std::shared_ptr<parser> _parser = std::make_shared<parser>();
    std::map<uint32_t, uint32_t> fourcc_map;
    std::vector<uint8_t> queries;
    unsigned probes = 0;
    usb_status query_status = RS2_USB_STATUS_SUCCESS, probe_status = RS2_USB_STATUS_SUCCESS;
    usb_status get_stream_ctrl_format_size(uvc_format_t,
                                          const std::shared_ptr<uvc_stream_ctrl_t>&);
    usb_status query_stream_ctrl(const std::shared_ptr<uvc_stream_ctrl_t>& control,
                                int probe, int request) {
        assert(probe == 1 && request == UVC_GET_MAX);
        queries.push_back(control->bInterfaceNumber);
        return query_status;
    }
    usb_status probe_stream_ctrl(const std::shared_ptr<uvc_stream_ctrl_t>&) {
        ++probes;
        return probe_status;
    }
};
'''
    main = r'''
int main() {
    const uint32_t nv12 = 0x4e563132U, yuyv = 0x59555956U, yuy2 = 0x59555932U;
    unsigned cases = 0;
    for (auto encoding : {nv12, yuyv}) {
        for (int fps : {0, 25, 30}) {
            for (uint8_t selected : {3, 7}) {
                rs_uvc_device device;
                for (int interface : {3, 7}) {
                    uvc_format_desc_t descriptor;
                    descriptor.guidFormat[0] = SWAP_UINT32(encoding == yuyv ? yuy2 : encoding);
                    descriptor.bFormatIndex = static_cast<uint8_t>(interface + 1);
                    device._parser->formats[interface] = {descriptor};
                }
                device.fourcc_map[yuy2] = yuyv;
                uvc_format_t format;
                format.fourcc = encoding;
                format.fps = fps;
                format.interfaceNumber = selected;
                auto control = std::make_shared<uvc_stream_ctrl_t>();
                assert(device.get_stream_ctrl_format_size(format, control) == RS2_USB_STATUS_SUCCESS);
                assert(device.queries == std::vector<uint8_t>{selected});
                assert(control->bInterfaceNumber == selected);
                assert(control->bFormatIndex == selected + 1);
                assert(control->bFrameIndex == 2 && control->bmHint == 1);
                assert(control->dwFrameInterval == (fps == 30 ? 333333U : 400000U));
                assert(device.probes == 1);
                ++cases;
            }
        }
    }
    for (int mismatch = 0; mismatch != 5; ++mismatch) {
        rs_uvc_device device;
        uvc_format_desc_t descriptor;
        descriptor.guidFormat[0] = SWAP_UINT32(nv12);
        device._parser->formats[3] = {descriptor};
        device._parser->formats[7] = {descriptor};
        uvc_format_t format;
        format.fourcc = nv12;
        if (mismatch == 0) format.interfaceNumber = 9;
        if (mismatch == 1) device._parser->formats[7].clear();
        if (mismatch == 2) device._parser->formats[7][0].guidFormat[0] = SWAP_UINT32(yuyv);
        if (mismatch == 3) device._parser->formats[7][0].frame_descs[0].wWidth = 640;
        if (mismatch == 4) device._parser->formats[7][0].frame_descs[0].intervals = {400000};
        auto control = std::make_shared<uvc_stream_ctrl_t>();
        assert(device.get_stream_ctrl_format_size(format, control) == RS2_USB_STATUS_INVALID_PARAM);
        assert(device.queries.empty() && device.probes == 0);
        ++cases;
    }
    for (bool query_fails : {false, true}) {
        rs_uvc_device device;
        uvc_format_desc_t descriptor;
        descriptor.guidFormat[0] = SWAP_UINT32(nv12);
        device._parser->formats[7] = {descriptor};
        device.query_status = query_fails ? USB_ERROR : RS2_USB_STATUS_SUCCESS;
        device.probe_status = USB_ERROR;
        uvc_format_t format;
        format.fourcc = nv12;
        auto control = std::make_shared<uvc_stream_ctrl_t>();
        assert(device.get_stream_ctrl_format_size(format, control) == USB_ERROR);
        assert(device.queries == std::vector<uint8_t>{7});
        assert(device.probes == (query_fails ? 0U : 1U));
        ++cases;
    }
    assert(cases == 19);
    std::cout << "19 RSUSB interface negotiation cases passed\n";
}
'''
    harness = tmp_path / "uvc-interface.cpp"
    executable = tmp_path / "uvc-interface"
    harness.write_text(preamble + swap + "\n" + production + main)
    subprocess.run([compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror",
                    str(harness), "-o", str(executable)], check=True, capture_output=True, text=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert result.stdout == "19 RSUSB interface negotiation cases passed\n"
