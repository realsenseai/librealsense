# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Exercise production Viewer panel lifecycle without a camera or GL context."""

from pathlib import Path
import shutil
import subprocess

import pytest


def test_passive_depth_panel_lifecycle(tmp_path):
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("Viewer lifecycle harness requires g++ or clang++")
    source = (Path(__file__).resolve().parents[2] / "common/viewer.cpp").read_text()
    gc_start = source.index("    void viewer_model::gc_streams()")
    gc_end = source.index("\n    void ", gc_start + 1)
    assert "update_passive_streams();" in source[gc_start:gc_end]
    start = source.index("    static bool is_alternating_depth(")
    end = source.index("    bool viewer_model::is_3d_texture_source(", start)
    lifecycle = source[start:end]
    start = source.index("    bool viewer_model::is_passive_frame(")
    end = source.index("    std::shared_ptr< subdevice_model > viewer_model::get_frame_subdevice", start)
    routing = source[start:end]
    preamble = r'''
#include <cassert>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
enum {RS2_STREAM_DEPTH, RS2_STREAM_INFRARED, RS2_STREAM_COLOR,
      RS2_OPTION_PASSIVE_DEPTH_MODE, RS2_PASSIVE_DEPTH_MODE_ALTERNATING=1,
      RS2_FRAME_METADATA_FRAME_EMITTER_MODE=10, RS2_EMITTER_MODE_OFF=0};
namespace rs2 {
struct stream_profile {
    int type=RS2_STREAM_DEPTH, id=0;
    stream_profile() = default;
    stream_profile(int type, int id):type(type),id(id) {}
    int stream_type() const {return type;}
    int unique_id() const {return id;}
};
struct frame {
    stream_profile profile;
    int emitter=1;
    bool metadata=true;
    stream_profile get_profile() const {return profile;}
    bool supports_frame_metadata(int) const {return metadata;}
    int get_frame_metadata(int) const {return emitter;}
};
struct frame_queue {explicit frame_queue(int) {}};
}
using rs2::stream_profile;
struct device_model;
struct sensor {
    int mode=1, reads=0;
    bool supported=true;
    std::vector<stream_profile> active;
    bool supports(int) const {return supported;}
    int get_option(int) {++reads;return mode;}
    std::vector<stream_profile> get_active_streams() const {return active;}
};
struct subdevice_model {
    std::shared_ptr<sensor> s=std::make_shared<sensor>();
    device_model* dev_model=nullptr;
    bool streaming=true;
};
struct device_model {std::vector<std::shared_ptr<subdevice_model>> subdevices;};
struct viewer_model;
struct stream_model {
    std::shared_ptr<subdevice_model> dev;
    stream_profile original_profile;
    bool passive=false, split=false, passive_depth_alternating=false;
    int ui_key=0, begins=0;
    void begin_stream(std::shared_ptr<subdevice_model> d,stream_profile p,const viewer_model&) {
        dev=d;original_profile=p;ui_key=p.unique_id();passive=split=false;++begins;
    }
};
struct viewer_model {
    static constexpr int PASSIVE_STREAM_KEY_OFFSET=0x10000000;
    std::mutex streams_mutex;
    std::map<int,stream_model> streams;
    std::map<int,int> passive_streams, streams_origin, _gpu_ruler_samples;
    stream_model* selected_stream=nullptr;
    struct {
        std::mutex frames_queue_mutex;
        std::map<int,rs2::frame_queue> frames_queue;
        void start() {}
    } ppf;
    void begin_stream(std::shared_ptr<subdevice_model>,rs2::stream_profile);
    void update_passive_streams();
    bool is_passive_frame(const rs2::frame&) const;
    bool is_split_passive_frame(const rs2::frame&);
    int last_frames_key(const rs2::frame&);
};
'''
    checks = r'''
int main() {
    stream_profile depth(RS2_STREAM_DEPTH,1), ir(RS2_STREAM_INFRARED,2);
    stream_profile left(RS2_STREAM_COLOR,3), right(RS2_STREAM_COLOR,4);
    for(int mode=0;mode<3;++mode) {
        viewer_model viewer;
        device_model device, other;
        auto sub=std::make_shared<subdevice_model>();
        auto rgb=std::make_shared<subdevice_model>();
        sub->dev_model=&device;device.subdevices={sub};
        rgb->dev_model=&other;other.subdevices={rgb};
        sub->s->mode=mode;sub->s->active={depth,ir};
        rgb->s->active={left,right};
        viewer.begin_stream(sub,depth);viewer.begin_stream(sub,ir);
        assert(viewer.passive_streams.empty());
        assert(viewer.streams.size()==2);  // Another camera's RGB cannot enable TDM.
        rs2::frame off;off.profile=depth;off.emitter=0;
        assert(viewer.last_frames_key(off)==1);  // Explicit laser-off still uses the primary tile.
        sub->s->active={depth,ir,left,right};
        viewer.begin_stream(sub,left);
        auto check=[&](bool split) {
            assert(viewer.streams.at(1).split==split);
            assert(viewer.streams.at(2).split==split);
            assert(viewer.passive_streams.size()==(split?2U:0U));
            assert(viewer.last_frames_key(off)==(split?1+viewer.PASSIVE_STREAM_KEY_OFFSET:1));
            off.emitter=1;assert(viewer.last_frames_key(off)==1);off.emitter=0;
            off.metadata=false;assert(viewer.last_frames_key(off)==1);off.metadata=true;
            assert(sub->s->mode==mode);
        };
        check(mode==1);
        int reads=sub->s->reads;
        sub->s->active={depth,ir,right};viewer.update_passive_streams();
        check(mode==1);  // One remaining RGB member is sufficient.
        sub->s->active={depth,ir};
        if(mode==1) {
            int key=1+viewer.PASSIVE_STREAM_KEY_OFFSET;
            viewer.selected_stream=&viewer.streams.at(key);
            viewer._gpu_ruler_samples[key]=7;
        }
        viewer.update_passive_streams();check(false);
        assert(viewer._gpu_ruler_samples.empty());
        if(mode==1)assert(viewer.selected_stream==&viewer.streams.at(1));
        assert(viewer.streams.at(1).begins==1);  // Refresh must not reset live depth.
        assert(sub->s->reads==reads);  // No per-frame option/XU polling.
        device.subdevices.push_back(rgb);rgb->dev_model=&device;
        viewer.update_passive_streams();check(mode==1);
        rgb->streaming=false;viewer.update_passive_streams();check(false);
        sub->streaming=false;viewer.update_passive_streams();check(false);
        sub->streaming=true;sub->s->active={depth,left,right};
        viewer.begin_stream(sub,depth);
        assert(viewer.streams.at(1).split==(mode==1));
        assert(!viewer.streams.at(2).split);  // Stale IR tile is no longer active.
        sub->streaming=false;viewer.update_passive_streams();
        assert(viewer.passive_streams.empty());

        viewer_model reverse;
        sub->streaming=true;sub->s->active={depth,left,right};
        reverse.begin_stream(sub,left);reverse.begin_stream(sub,depth);
        assert(reverse.streams.at(1).split==(mode==1));
        sub->s->mode=0;reverse.begin_stream(sub,depth);
        assert(reverse.passive_streams.empty());  // Reopen after mode change.
        sub->s->mode=1;sub->s->supported=false;
        reverse.begin_stream(sub,depth);
        assert(reverse.passive_streams.empty());
    }
}
'''
    path = tmp_path / "viewer-passive-depth.cpp"
    path.write_text(preamble + lifecycle + routing + checks)
    executable = tmp_path / "viewer-passive-depth.exe"
    subprocess.run([compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror",
                    str(path), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
