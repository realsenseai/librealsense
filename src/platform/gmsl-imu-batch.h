// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifndef GMSL_IMU_BATCH_V1_H
#define GMSL_IMU_BATCH_V1_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Little-endian, fixed 256-byte RAW8 line. Header: IMUB, version, count,
 * header bytes, record bytes, packet sequence (u32), requested sensor mask (u8), reserved (3 bytes).
 * Records contain HID32 (40 bytes total) or HID38 (46 bytes total), then a u64 sample sequence.
 * Mask 3: one accel then one gyro; mask 1/2: only the requested sample.
 * Original per-sensor timestamps are retained. The wire line is always 256 bytes.
 */
namespace gmsl_imu_batch {
static const size_t wire_bytes = 256U;
static const size_t header_bytes = 16U;
static const size_t record_bytes = 40U;
static const size_t max_records = 2U;
static const size_t extended_record_bytes = 46U;
static const size_t sample_bytes = 64U;
static const size_t sample_sequence_offset = 48U;
static const size_t sample_tag_offset = 56U;
inline uint32_t read_u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}
inline void write_u32(uint8_t* p, uint32_t v) {
    for (unsigned i=0; i<4; ++i) p[i]=uint8_t(v>>(8*i));
}
inline uint64_t read_u64(const uint8_t* p) {
    return uint64_t(read_u32(p)) | (uint64_t(read_u32(p+4))<<32);
}
inline void write_u64(uint8_t* p, uint64_t v) {
    write_u32(p,uint32_t(v));write_u32(p+4,uint32_t(v>>32));
}
inline bool has_magic(const uint8_t* p, size_t n) {
    return p && n>=4 && p[0]=='I' && p[1]=='M' && p[2]=='U' && p[3]=='B';
}
inline unsigned count_for_mask(uint8_t mask) {
    return mask==3U ? 2U : (mask==1U || mask==2U ? 1U : 0U);
}
inline unsigned report_for_record(uint8_t mask, unsigned index) {
    return mask==3U ? index+1U : mask;
}
inline bool valid(const uint8_t* p, size_t n) {
    if (!has_magic(p,n) || n<header_bytes || p[4]!=1 ||
        count_for_mask(p[12])==0 || p[5]!=count_for_mask(p[12]) ||
        p[6]!=header_bytes || (p[7]!=record_bytes && p[7]!=extended_record_bytes) ||
        p[13]!=0 || p[14]!=0 || p[15]!=0 ||
        n<header_bytes+size_t(p[5])*p[7]) return false;
    for (unsigned i=0;i<p[5];++i) {
        const uint8_t* r=p+header_bytes+i*p[7];
        if (r[0]!=report_for_record(p[12],i) || r[1]!=2) return false;
    }
    return true;
}
// Normalize the internal trailer without changing the firmware-owned HID payload.
inline bool decode_record(const uint8_t* p, size_t n, unsigned index, uint8_t* sample, size_t capacity) {
    if (!sample || capacity < sample_bytes || !valid(p,n) || index >= p[5]) return false;
    const uint8_t* record = p + header_bytes + index*p[7];
    const size_t hid_bytes = p[7] - sizeof(uint64_t);
    memset(sample,0,sample_bytes);
    memcpy(sample,record,hid_bytes);
    memcpy(sample+sample_sequence_offset,record+hid_bytes,sizeof(uint64_t));
    memcpy(sample+sample_tag_offset,"IMS1",4);
    return true;
}
inline bool is_decoded_sample(const uint8_t* p, size_t n) {
    return p && n == sample_bytes && memcmp(p+sample_tag_offset,"IMS1",4) == 0;
}
inline void begin(uint8_t* p, uint32_t sequence, uint8_t requested_mask=3U) {
    memset(p,0,wire_bytes);
    p[0]='I';p[1]='M';p[2]='U';p[3]='B';p[4]=1;
    p[6]=uint8_t(header_bytes);p[7]=uint8_t(record_bytes);
    write_u32(p+8,sequence);p[12]=requested_mask;
}
inline bool append(uint8_t* p, const uint8_t* hid32, uint64_t sample_sequence) {
    if (!has_magic(p,wire_bytes) || p[5]>=count_for_mask(p[12]) || !hid32 ||
        hid32[0]!=report_for_record(p[12],p[5]) || hid32[1]!=2) return false;
    uint8_t* r=p+header_bytes+size_t(p[5])*record_bytes;
    memcpy(r,hid32,32);write_u64(r+32,sample_sequence);++p[5];return true;
}
}
#endif
