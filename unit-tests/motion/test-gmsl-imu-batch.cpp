// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:custom-main
// Protocol-only test: no device or firmware connection required.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../src/platform/gmsl-imu-batch.h"
#include <assert.h>
#include <array>
#include <cstring>
using namespace gmsl_imu_batch;
int main() {
 std::array<uint8_t,256> p{};std::array<uint8_t,32> a{},g{};
 a[0]=1;g[0]=2;a[1]=g[1]=2;
 const uint8_t ts[8]={0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
 memcpy(a.data()+2,ts,8);memcpy(g.data()+2,ts,8);g[2]=0xff;
 begin(p.data(),0xfedcba98);assert(!valid(p.data(),p.size()));
 assert(!append(p.data(),g.data(),0)); // first record must be accel
 assert(append(p.data(),a.data(),0xffffffff));assert(!valid(p.data(),256));
 assert(!append(p.data(),a.data(),1)); // never accept accel + accel
 assert(append(p.data(),g.data(),123));
 assert(valid(p.data(),256));assert(p[5]==2);assert(read_u32(p.data()+8)==0xfedcba98);
 assert(!memcmp(p.data()+16+2,ts,8));assert(p[16+40+2]==0xff);
 assert(read_u32(p.data()+16+32)==0xffffffff);assert(read_u32(p.data()+56+32)==123);
 for(size_t n=0;n<96;n++) assert(!valid(p.data(),n));
 assert(valid(p.data(),96));
 auto bad=p;bad[4]=2;assert(!valid(bad.data(),256));
 bad=p;bad[5]=1;assert(!valid(bad.data(),256));
 bad=p;bad[5]=3;assert(!valid(bad.data(),256));
 bad=p;bad[7]=39;assert(!valid(bad.data(),256));
 bad=p;bad[16]=2;assert(!valid(bad.data(),256));
 bad=p;bad[56]=1;assert(!valid(bad.data(),256));
 bad=p;bad[13]=1;assert(!valid(bad.data(),256));
 bad=p;bad[16+1]=0;assert(!valid(bad.data(),256));
 assert(!valid(nullptr,256));assert(!has_magic(a.data(),32));
 assert(!append(p.data(),g.data(),456));
 // Single requested sensor: one record, full 256-byte line, zero padding.
 for(unsigned mask=1;mask<=2;mask++) {
  begin(p.data(),7,mask);
  assert(!valid(p.data(),256));
  const auto& wanted=mask==1?a:g;const auto& other=mask==1?g:a;
  assert(!append(p.data(),other.data(),9));
  assert(append(p.data(),wanted.data(),10));assert(valid(p.data(),256));
  assert(p[5]==1 && p[12]==mask && p[16]==mask);
  assert(!append(p.data(),wanted.data(),11));
  for(size_t n=0;n<56;n++) assert(!valid(p.data(),n));
  for(size_t n=56;n<256;n++) assert(p[n]==0);
 }
 for(unsigned mask: {0U,4U,255U}) {
  begin(p.data(),0,mask);assert(!append(p.data(),a.data(),0));
  assert(!valid(p.data(),256));
 }
 begin(p.data(),0,3);
 assert(append(p.data(),a.data(),0x12345678ffffffffULL));
 assert(append(p.data(),g.data(),0x1234567900000000ULL));
 assert(valid(p.data(),256));
 assert(read_u64(p.data()+16+32)==0x12345678ffffffffULL);
 assert(read_u64(p.data()+56+32)==0x1234567900000000ULL);

 // Both firmware layouts preserve every HID byte and the full source sequence.
 for(unsigned stride: {40U,46U}) {
  for(unsigned mask: {1U,2U,3U}) {
   p.fill(0);begin(p.data(),123,mask);p[7]=stride;p[5]=count_for_mask(mask);
   for(unsigned i=0;i<p[5];++i) {
    auto r=p.data()+header_bytes+i*stride;
    for(unsigned j=0;j<stride-8;++j)r[j]=uint8_t(j*7+i);
    r[0]=report_for_record(mask,i);r[1]=2;
    write_u64(r+stride-8,0x12345678ffffffffULL+i);
   }
   assert(valid(p.data(),256));
   for(size_t n=0;n<header_bytes+p[5]*stride;++n)assert(!valid(p.data(),n));
   std::array<uint8_t,sample_bytes> sample{};
   for(unsigned i=0;i<p[5];++i) {
    assert(decode_record(p.data(),256,i,sample.data(),sample.size()));
    assert(!memcmp(sample.data(),p.data()+header_bytes+i*stride,stride-8));
    assert(read_u64(sample.data()+sample_sequence_offset)==0x12345678ffffffffULL+i);
    assert(is_decoded_sample(sample.data(),sample.size()));
    assert(!is_decoded_sample(sample.data(),sample_tag_offset+3));
   }
   assert(!decode_record(p.data(),256,p[5],sample.data(),sample.size()));
   assert(!decode_record(p.data(),256,0,sample.data(),sample.size()-1));
   assert(!decode_record(p.data(),256,0,nullptr,sample.size()));
   p[7]=45;assert(!valid(p.data(),256));
   assert(!decode_record(p.data(),256,0,sample.data(),sample.size()));
  }
 }
 assert(!is_decoded_sample(nullptr,sample_bytes));

}
