// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Live Improved Close Range Depth demo using rs_depth + librealsense2.
//
// Streams from a RealSense D4xx, runs the close-range depth improver on each
// frame, and prints a single live-updating status line showing how much of the
// close-range band was recovered that the camera couldn't see on its own.
//
// Loads librs_depth_range.so at runtime via dlopen() and resolves the C API
// with dlsym() — no link-time dependency on the library.
//
// Build:
//   g++ -std=c++17 range_depth.cpp \
//       -I/opt/librealsense2-enhanced-depth/include \
//       $(pkg-config --cflags --libs realsense2) \
//       -ldl -o range_depth
//
// Run:
//   ./range_depth        # Ctrl-C to stop

#include <librealsense2/rs.hpp>
#include <rs_depth_calibration.hpp>
#include <rs_depth_range.h>   // C API types/signatures only — resolved via dlopen below

#include <dlfcn.h>

#include <array>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static volatile std::sig_atomic_t g_stop = 0;

// ── dlopen-based replacement for rs_depth::DepthRangeImprover ──────────────
// Loads librs_depth_range.so at runtime and calls the C API through
// dlsym-resolved function pointers instead of link-time symbols.
class DynamicDepthRangeImprover {
public:
    explicit DynamicDepthRangeImprover(
        const rs_depth::Calibration& cal,
        std::array<int, 4>           crop_region     = {-1, -1, -1, -1},
        int                          min_z_threshold = 0,
        float                        scale_factor    = 0.35f,
        const char*                  lib_path = "/opt/librealsense2-enhanced-depth/lib/librs_depth_range.so")
    {
        lib_ = dlopen(lib_path, RTLD_NOW | RTLD_LOCAL);
        if (!lib_)
            throw std::runtime_error(std::string("dlopen failed: ") + dlerror());

        create_     = load<decltype(&rs_depth_range_create)>("rs_depth_range_create");
        destroy_    = load<decltype(&rs_depth_range_destroy)>("rs_depth_range_destroy");
        process_    = load<decltype(&rs_depth_range_process)>("rs_depth_range_process");
        last_error_ = load<decltype(&rs_depth_range_last_error)>("rs_depth_range_last_error");

        if (min_z_threshold <= 0)
            min_z_threshold = cal.min_z_threshold_mm();
        handle_ = create_(cal.focal_length_px, cal.baseline_m,
                          min_z_threshold, scale_factor,
                          crop_region[0], crop_region[1],
                          crop_region[2], crop_region[3]);
        if (!handle_)
            throw std::runtime_error(
                std::string("DepthRangeImprover init failed: ") + last_error_(nullptr));
    }

    ~DynamicDepthRangeImprover() {
        if (handle_) destroy_(handle_);
        if (lib_)    dlclose(lib_);
    }

    DynamicDepthRangeImprover(const DynamicDepthRangeImprover&)            = delete;
    DynamicDepthRangeImprover& operator=(const DynamicDepthRangeImprover&) = delete;

    void process(const uint8_t*       ir_left,
                 const uint8_t*       ir_right,
                 const uint16_t*      depth_mm,
                 uint16_t*            depth_out,
                 const FrameMetadata& metadata)
    {
        int rc = process_(handle_, ir_left, ir_right, depth_mm, depth_out, &metadata);
        if (rc != 0)
            throw std::runtime_error(
                std::string("DepthRangeImprover::process failed: ") + last_error_(handle_));
    }

private:
    template <typename Fn>
    Fn load(const char* name) {
        auto fn = reinterpret_cast<Fn>(dlsym(lib_, name));
        if (!fn)
            throw std::runtime_error(std::string("dlsym failed for ") + name + ": " + dlerror());
        return fn;
    }

    void*        lib_    = nullptr;
    RsDepthRange handle_ = nullptr;
    decltype(&rs_depth_range_create)     create_     = nullptr;
    decltype(&rs_depth_range_destroy)    destroy_    = nullptr;
    decltype(&rs_depth_range_process)    process_    = nullptr;
    decltype(&rs_depth_range_last_error) last_error_ = nullptr;
};

int main() {
    std::signal(SIGINT, [](int){ g_stop = 1; });
    constexpr int W = 1280, H = 720, FPS = 30;
    constexpr int N = W * H;

    // ── 1. Open the camera ────────────────────────────────────────────
    rs2::pipeline pipe;
    rs2::config   cfg;
    cfg.enable_stream(RS2_STREAM_INFRARED, 1, W, H, RS2_FORMAT_Y8,  FPS);
    cfg.enable_stream(RS2_STREAM_INFRARED, 2, W, H, RS2_FORMAT_Y8,  FPS);
    cfg.enable_stream(RS2_STREAM_DEPTH,       W, H, RS2_FORMAT_Z16, FPS);
    auto profile = pipe.start(cfg);

    // ── 2. Build calibration from the camera's own intrinsics/extrinsics
    auto ir1 = profile.get_stream(RS2_STREAM_INFRARED, 1).as<rs2::video_stream_profile>();
    auto ir2 = profile.get_stream(RS2_STREAM_INFRARED, 2).as<rs2::video_stream_profile>();
    auto calib = rs_depth::Calibration::from_sdk(ir1.get_intrinsics(),
                                                 ir1.get_extrinsics_to(ir2));

    // Meters per Z16 unit (RS2_OPTION_DEPTH_UNITS). Typical D4xx = 0.001 (raw
    // Z16 == mm), but high-accuracy presets and SR300 use other values — scale
    // raw values to mm before passing to the improver and the close-range comparison.
    const float depth_scale = profile.get_device()
                                     .first<rs2::depth_sensor>()
                                     .get_depth_scale();
    const float depth_to_mm = depth_scale * 1000.0f;

    // ── 3. Construct the improver (auto threshold = 1.2 * focal × baseline / 126)
    DynamicDepthRangeImprover improver(calib);
    const int T = calib.min_z_threshold_mm();
    std::printf("Close-range threshold: %d mm  (pixels closer than this are improved)\n", T);
    std::printf("Press Ctrl-C to stop\n\n");

    // ── 4. Stream + improve + live status line ────────────────────────
    std::vector<uint16_t> depth_imp(N);
    std::vector<uint16_t> depth_mm(N);
    long long hw_total = 0, imp_total = 0, rescued_total = 0;
    int frames_seen = 0;

    while (!g_stop) {
        rs2::frameset frames = pipe.wait_for_frames();
        auto ir_l  = frames.get_infrared_frame(1);
        auto ir_r  = frames.get_infrared_frame(2);
        auto depth = frames.get_depth_frame();

        const uint8_t*  ir_left  = static_cast<const uint8_t*>(ir_l.get_data());
        const uint8_t*  ir_right = static_cast<const uint8_t*>(ir_r.get_data());
        const uint16_t* raw_z16  = static_cast<const uint16_t*>(depth.get_data());

        // Convert raw Z16 → uint16 mm using the camera's depth_scale.
        const uint16_t* depth_hw = depth_mm.data();
        if (depth_to_mm == 1.0f) {
            std::memcpy(depth_mm.data(), raw_z16, N * sizeof(uint16_t));
        } else {
            for (int i = 0; i < N; ++i) {
                float mm = static_cast<float>(raw_z16[i]) * depth_to_mm;
                depth_mm[i] = (mm > 65535.0f) ? 65535
                            : (mm < 0.0f)     ? 0
                                              : static_cast<uint16_t>(mm);
            }
        }

        // Mirrors Python's FrameMetadata.from_rs2_frameset — extracts width,
        // height, exposure/gain/laser-power/temperature for IR + depth + color.
        auto meta = FrameMetadata::from_rs2_frameset(frames);
        improver.process(ir_left, ir_right, depth_hw, depth_imp.data(), meta);

        int n_hw = 0, n_imp = 0, rescued = 0;
        for (int i = 0; i < N; ++i) {
            const bool hw_close  = depth_hw[i]  > 0 && depth_hw[i]  < T;
            const bool imp_close = depth_imp[i] > 0 && depth_imp[i] < T;
            if (hw_close)               ++n_hw;
            if (imp_close)              ++n_imp;
            if (depth_hw[i] == 0 && imp_close) ++rescued;
        }

        const double hw_pct  = 100.0 * n_hw  / N;
        const double imp_pct = 100.0 * n_imp / N;
        const double recovery_pct = (n_imp > 0) ? 100.0 * (n_imp - n_hw) / n_imp : 0.0;

        std::printf("\rframe %5lld | close-range: HW %5.2f%% -> improved %5.2f%% "
                    "| close-range recovered %5.1f%% | +%6d px",
                    static_cast<long long>(depth.get_frame_number()),
                    hw_pct, imp_pct, recovery_pct, rescued);
        std::fflush(stdout);

        hw_total      += n_hw;
        imp_total     += n_imp;
        rescued_total += rescued;
        ++frames_seen;
    }

    std::printf("\n");
    pipe.stop();
    return 0;
}