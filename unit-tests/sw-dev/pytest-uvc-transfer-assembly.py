# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pathlib import Path
import os
import shutil
import subprocess
import sys

import pytest


def test_large_mapping_transfer_assembly(tmp_path):
    if sys.platform != "linux":
        pytest.skip("This sanitizer harness targets the Linux RSUSB backend")
    compiler = shutil.which("g++")
    if not compiler:
        pytest.skip("A host C++ compiler is required")
    root = Path(__file__).resolve().parents[2]
    source = (root / "src/uvc/uvc-streamer.cpp").read_text()
    start = source.index("        void uvc_streamer::consume_transfer(")
    end = source.index("\n        void uvc_streamer::start()", start)
    production = source[start:end]
    fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#define LOG_WARNING(...) ((void)0)
struct backend_frame { std::vector<uint8_t> pixels=std::vector<uint8_t>(24); };
using backend_frame_ptr=std::unique_ptr<backend_frame,void(*)(backend_frame*)>;
void cleanup_frame(backend_frame* frame){delete frame;}
struct Archive {
    bool fail=false;
    backend_frame* allocate(){return fail ? nullptr : new backend_frame;}
};
struct Watchdog {unsigned kicks=0; void kick(){++kicks;}};
using Queue=std::vector<std::vector<uint8_t>>;
void uvc_process_bulk_payload(backend_frame_ptr frame,size_t size,Queue& queue) {
    queue.emplace_back(frame->pixels.begin(),frame->pixels.begin()+size);
}
struct uvc_streamer {
    unsigned _transfer_buffer_size=8;
    bool _frame_arrived=false,_discard_payload=false;
    size_t _partial_size=0;
    backend_frame_ptr _partial_frame{nullptr,nullptr};
    std::shared_ptr<Archive> _frames_archive=std::make_shared<Archive>();
    std::shared_ptr<Watchdog> _watchdog=std::make_shared<Watchdog>();
    Queue _queue;
    void consume_transfer(const uint8_t*,size_t);
};
/* PRODUCTION */
int main() {
    uvc_streamer stream;
    uint8_t bytes[32];
    for (unsigned index=0;index<32;++index) bytes[index]=index;
    stream.consume_transfer(bytes,8);
    stream.consume_transfer(bytes+8,8);
    assert(stream._queue.empty());
    stream.consume_transfer(bytes+16,4);
    assert(stream._queue.size()==1 && stream._queue.back().size()==20);
    for (unsigned index=0;index<20;++index) assert(stream._queue.back()[index]==index);
    assert(stream._frame_arrived && stream._watchdog->kicks==1);
    // A zero-length packet terminates an exact multiple of the transfer size.
    stream.consume_transfer(bytes,8);
    stream.consume_transfer(bytes+8,8);
    stream.consume_transfer(bytes,0);
    assert(stream._queue.size()==2 && stream._queue.back().size()==16);
    stream.consume_transfer(bytes,0);
    assert(stream._queue.size()==2);
    // Discard the whole over-capacity payload until its short packet.
    for (unsigned part=0;part<4;++part) stream.consume_transfer(bytes+part*8,8);
    stream.consume_transfer(bytes,1);
    assert(stream._queue.size()==2 && stream._partial_size==0);
    stream.consume_transfer(bytes,4);
    assert(stream._queue.size()==3 && stream._queue.back().size()==4);
    // A frame-pool failure must preserve the next payload boundary.
    stream._frames_archive->fail=true;
    stream.consume_transfer(bytes,8);
    stream.consume_transfer(bytes,4);
    assert(stream._queue.size()==3);
    stream._frames_archive->fail=false;
    stream.consume_transfer(bytes,4);
    assert(stream._queue.size()==4 && !stream._partial_frame);
}
'''
    code, binary = tmp_path / "assembly.cpp", tmp_path / "assembly"
    code.write_text(fixture.replace("/* PRODUCTION */", production))
    subprocess.run([
        compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-pie", "-no-pie",
        str(code), "-o", str(binary),
    ], check=True)
    # This standalone executable owns its ASan runtime, including in the TSan Python job.
    native_env = dict(os.environ)
    native_env.pop("LD_PRELOAD", None)
    subprocess.run([str(binary)], check=True, timeout=10, env=native_env)


def test_mapping_profile_and_payload_validation(tmp_path):
    from rspy import repo
    compiler = shutil.which("g++")
    if not compiler or sys.platform != "linux":
        pytest.skip("A Linux host C++ compiler is required")
    root = Path(__file__).resolve().parents[2]
    code = r'''
#include <src/ds/d500/mapping-pcl-check.h>
#include <cassert>
#include <vector>
using namespace librealsense;
int main() {
    pcl_expected expected;
    expected.width=4;expected.height=3;expected.profile_id=0x0101;
    expected.point_stride=13;expected.point_format=2;
    std::vector<uint8_t> labels(4*3*13,42);
    md_point_cloud cloud{};
    cloud.header.md_type_id=md_type::META_DATA_INTEL_POINT_CLOUD_ID;
    cloud.header.md_size=sizeof(cloud);cloud.version=0x00020002;
    cloud.flags=uint32_t(md_point_cloud_attributes::profile_id_attribute)
        |uint32_t(md_point_cloud_attributes::point_stride_attribute)
        |uint32_t(md_point_cloud_attributes::point_format_attribute)
        |uint32_t(md_point_cloud_attributes::payload_crc32_attribute);
    cloud.profile_id=0x0101;cloud.point_stride=13;cloud.point_format=2;
    cloud.payload_crc32=rsutils::number::calc_crc32(labels.data(),labels.size());
    pcl_session session;
    auto check_cloud=[&] {
        return check_pcl_frame(labels.data(),labels.size(),
            reinterpret_cast<const uint8_t*>(&cloud),sizeof(cloud),expected,session);
    };
    assert(check_cloud()==pcl_frame_check::ok);
    cloud.point_stride=16;
    assert(check_cloud()==pcl_frame_check::format_mismatch);
    cloud.point_stride=13;cloud.profile_id=3;
    assert(check_cloud()==pcl_frame_check::profile_mismatch);

    expected.point_format=1;expected.point_stride=16;
    labels.resize(4*3*16);
    cloud.point_format=1;cloud.point_stride=16;
    cloud.payload_crc32=rsutils::number::calc_crc32(labels.data(),labels.size());
    for (uint16_t profile : {3,4}) {
        expected.profile_id=profile;cloud.profile_id=profile;
        session=pcl_session{};
        assert(check_cloud()==pcl_frame_check::ok);
        cloud.point_format=0;
        assert(check_cloud()==pcl_frame_check::format_mismatch);
        cloud.point_format=1;cloud.point_stride=12;
        assert(check_cloud()==pcl_frame_check::format_mismatch);
        cloud.point_stride=16;
        labels[0]^=1;
        assert(check_cloud()==pcl_frame_check::crc_mismatch);
        labels[0]^=1;
    }

    expected.profile_id=0x0201;expected.point_stride=1;
    std::vector<uint8_t> cells(12,50);
    md_occupancy grid{};
    grid.header.md_type_id=md_type::META_DATA_INTEL_OCCUPANCY_ID;
    grid.header.md_size=sizeof(grid);grid.version=0x00020002;
    grid.flags=uint32_t(md_occupancy_attributes::profile_id_attribute)
        |uint32_t(md_occupancy_attributes::grid_rows_attribute)
        |uint32_t(md_occupancy_attributes::grid_columns_attribute)
        |uint32_t(md_occupancy_attributes::payload_crc32_attribute);
    grid.profile_id=0x0201;grid.grid_rows=3;grid.grid_columns=4;
    grid.payload_crc32=rsutils::number::calc_crc32(cells.data(),cells.size());
    auto check_grid=[&] {
        return check_occupancy_frame(cells.data(),cells.size(),
            reinterpret_cast<const uint8_t*>(&grid),sizeof(grid),expected,session);
    };
    assert(check_grid()==pcl_frame_check::ok);
    assert(check_occupancy_frame(labels.data(),labels.size(),nullptr,0,expected,session)
           ==pcl_frame_check::size_mismatch);
    grid.header.md_type_id=md_type::META_DATA_INTEL_POINT_CLOUD_ID;
    assert(check_grid()==pcl_frame_check::abi_mismatch);
    grid.header.md_type_id=md_type::META_DATA_INTEL_OCCUPANCY_ID;
    cells[0]^=1;
    assert(check_grid()==pcl_frame_check::crc_mismatch);
    cells[0]^=1;grid.profile_id=0x0101;
    assert(check_grid()==pcl_frame_check::profile_mismatch);
    grid.profile_id=0x0201;grid.grid_columns=5;
    assert(check_grid()==pcl_frame_check::abi_mismatch);
}
'''
    source, binary = tmp_path / "mapping.cpp", tmp_path / "mapping"
    source.write_text(code)
    subprocess.run([
        compiler, "-std=c++14", "-O2", "-pthread", "-fsanitize=address,undefined",
        "-fno-pie", "-no-pie", "-I" + str(root), "-I" + str(root / "include"),
        "-I" + str(root / "third-party/rsutils/include"), str(source),
        "-I" + str(root / "third-party/easyloggingpp/src"),
        "-I" + str(Path(repo.build or root / "build") / "third-party/json/include"),
        str(root / "third-party/rsutils/src/crc32.cpp"), "-o", str(binary),
    ], check=True)
    native_env = dict(os.environ)
    native_env.pop("LD_PRELOAD", None)
    subprocess.run([str(binary)], check=True, timeout=10, env=native_env)
