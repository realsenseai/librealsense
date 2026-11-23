# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pathlib import Path
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
    subprocess.run([str(binary)], check=True, timeout=10)
