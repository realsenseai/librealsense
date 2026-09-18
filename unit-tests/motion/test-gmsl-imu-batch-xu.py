#!/usr/bin/env python3
# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""Exercise production XU negotiation/open/rollback/close without hardware."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/ds/ds-motion-common.cpp').read_text()
a=source.index('    namespace {\n        constexpr uint8_t imu_batch_supported')
b=source.index('    rs2_motion_device_intrinsic ds_motion_sensor::get_motion_intrinsics',a)
production=source[a:b]
prefix=r'''
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#define LOG_DEBUG(...) ((void)0)
#define LOG_WARNING(...) ((void)0)
using invalid_value_exception=std::invalid_argument;
enum { RS2_STREAM_ACCEL=1, RS2_STREAM_GYRO=2, RS2_STREAM_DEPTH=3 };
struct profile { int type, fps; int get_stream_type()const{return type;} int get_framerate()const{return fps;} };
using stream_profiles=std::vector<std::shared_ptr<profile>>;
namespace ds { constexpr int depth_xu=3; constexpr uint8_t DS5_HKR_IMU_BATCH=0x1b; }
namespace platform {
struct uvc_device {
 uint8_t status=0xc0; unsigned writes=0,reads=0; bool fail_get=false,fail_set=false; int active_reads=-1; bool unknown_error=false;
 bool get_xu(int unit,uint8_t control,uint8_t *out,int n) {
  assert(unit==3 && control==0x1b && n==1);reads++;
  if(unknown_error)throw 1;
  if(active_reads>=0 && active_reads--==0)status&=~0x20;
  *out=status;return !fail_get;
 }
 bool set_xu(int unit,uint8_t control,const uint8_t *in,int n) {
  assert(unit==3 && control==0x1b && n==1);writes++;
  if(status&0x20)return false;
  status=(status&0xfc)|*in;return !fail_set;
 }
};
}
struct raw_sensor_base { virtual ~raw_sensor_base(){} };
struct uvc_sensor:raw_sensor_base {
 platform::uvc_device dev;
 template<class F> auto invoke_powered(F f)->decltype(f(dev)){return f(dev);}
};
struct device { virtual ~device(){} };
struct d500_motion:device {};
struct synthetic_sensor {
 bool opened=false,fail_open=false,fail_close=false; unsigned opens=0;
 bool is_opened()const{return opened;}
 void open(const stream_profiles&) {
  if(opened)throw std::runtime_error("already open");
  opens++;if(fail_open)throw std::runtime_error("streamon failed");opened=true;
 }
 void close(){assert(opened);if(fail_close)throw std::runtime_error("close failed");opened=false;}
};
struct ds_motion_sensor:synthetic_sensor {
 device *_owner;std::shared_ptr<raw_sensor_base> raw;
 std::shared_ptr<uvc_sensor> _gmsl_batch_sensor;
 std::shared_ptr<raw_sensor_base> get_raw_sensor(){return raw;}
 void open(const stream_profiles&);void close();
};
static stream_profiles requests(int first=1,int second=2,int rate=200) {
 stream_profiles p{std::make_shared<profile>(profile{first,rate})};
 if(second)p.push_back(std::make_shared<profile>(profile{second,rate}));
 return p;
}
static bool failed(ds_motion_sensor& s,const stream_profiles& p) {
 try{s.open(p);return false;}catch(const std::exception&){return true;}
}
'''
tests=r'''
int main() {
 unsetenv("RS2_GMSL_IMU_BATCH");
 d500_motion owner;
 auto raw=std::make_shared<uvc_sensor>();ds_motion_sensor s;s._owner=&owner;s.raw=raw;
 s.open(requests());assert(s.opened && raw->dev.status==0xc3);
 unsigned before=raw->dev.writes;assert(failed(s,requests()) && raw->dev.writes==before);
 s.close();assert(raw->dev.status==0xc0 && !s._gmsl_batch_sensor);
 s.open(requests(1,0));assert(raw->dev.status==0xc1);s.close();
 s.open(requests(2,0));assert(raw->dev.status==0xc2);s.close();
 before=raw->dev.writes;auto unequal=requests();unequal[1]->fps=400;
 assert(failed(s,unequal) && raw->dev.writes==before);
 assert(failed(s,requests(3,0)) && raw->dev.writes==before);
 setenv("RS2_GMSL_IMU_BATCH","0",1);s.open(requests());assert(raw->dev.status==0xc0);s.close();
 setenv("RS2_GMSL_IMU_BATCH","bad",1);s.open(requests());
 assert(raw->dev.status==0xc3);s.close();unsetenv("RS2_GMSL_IMU_BATCH");
 s.open(requests());raw->dev.status=0xe3;raw->dev.active_reads=3;
 before=raw->dev.writes;s.close();
 assert(!s.opened && !s._gmsl_batch_sensor && raw->dev.status==0xc0 && raw->dev.writes==before+1);
 s.open(requests());raw->dev.status=0xe3;before=raw->dev.writes;
 auto start=std::chrono::steady_clock::now();s.close();
 assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(1));
 assert(!s.opened && !s._gmsl_batch_sensor && raw->dev.writes==before);
 raw->dev.status=0xc0;
 for(int fault=0;fault<3;++fault) {
  s.open(requests());raw->dev.fail_get=(fault==0);raw->dev.fail_set=(fault==1);raw->dev.unknown_error=(fault==2);
  s.close();assert(!s.opened && !s._gmsl_batch_sensor);
  raw->dev.fail_get=raw->dev.fail_set=raw->dev.unknown_error=false;
 }
 s.open(requests());s.fail_close=true;
 try{s.close();assert(false);}catch(const std::runtime_error&){}
 assert(s.opened && s._gmsl_batch_sensor);s.fail_close=false;s.close();
 for(auto status:{0,0x80,0xc4}) { // old FW, tunnel, reserved protocol bits
  raw->dev.status=status;before=raw->dev.writes;s.open(requests());
  assert(raw->dev.writes==before && !s._gmsl_batch_sensor);s.close();
 }
 raw->dev.status=0xc0;raw->dev.fail_get=true;before=raw->dev.writes;
 s.open(requests());assert(raw->dev.writes==before);s.close();raw->dev.fail_get=false;
 s.fail_open=true;assert(failed(s,requests()));
 assert(raw->dev.status==0xc0 && !s._gmsl_batch_sensor);s.fail_open=false;
 raw->dev.fail_set=true;assert(failed(s,requests()));
 assert(raw->dev.status==0xc0 && !s._gmsl_batch_sensor);raw->dev.fail_set=false;
 raw->dev.status=0xe3;assert(failed(s,requests()));assert(raw->dev.status==0xe3);
 s.raw=std::make_shared<raw_sensor_base>();s.open(requests());assert(!s._gmsl_batch_sensor);s.close();
 puts("SDK IMU XU: paired/single/legacy/opt-out/invalid-env/busy/rollback/async-release/cleanup-errors tests passed");
}
'''
with tempfile.TemporaryDirectory(prefix='sdk-imu-xu-') as tmp:
 c=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';c.write_text(prefix+production+tests)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++11','-Wall','-Wextra','-Werror',
                 '-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
