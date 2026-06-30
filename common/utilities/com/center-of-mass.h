// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <vector>
#include <cstdint>

namespace com {

// ---------------------------------------------------------------------------
// Basic types
// ---------------------------------------------------------------------------

struct vec2f { float x, y; };
struct vec2i { int   x, y; };
struct vec3f { float x, y, z; };

struct rect {
    int x, y, width, height;
};

// ---------------------------------------------------------------------------
// Aligned depth image (non-owning view).
// Every pixel (u, v) in this image corresponds to the same (u, v) in the
// color image — no projection needed to relate the two.
// ---------------------------------------------------------------------------

struct depth_image_16 {
    const uint16_t* data;   // row-major: pixel(x,y) = data[y * width + x]
    int width;
    int height;
};

struct depth_image_8 {
    uint8_t* data;          // row-major: pixel(x,y) = data[y * width + x]
    int width;
    int height;
};

// ---------------------------------------------------------------------------
// Camera intrinsics — standard pinhole model, all values in pixels / mm.
// ---------------------------------------------------------------------------

struct camera_intrinsics {
    float fx, fy;   // focal length (pixels)
    float cx, cy;   // principal point (pixels)
};

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

struct person_center_of_mass {
    float mean_body_depth;  // Euclidean distance to person (mm); 0 = unreliable
    vec3f world_pos;        // 3D camera coords (mm); zero if intrinsics not provided
    vec2f image_pos;        // Center of Mass (COM) pixel in color/depth image; zero if intrinsics not provided
};

// ---------------------------------------------------------------------------
// center_of_mass_calculator
// ---------------------------------------------------------------------------

class center_of_mass_calculator {
public:
    // Convert raw 16-bit aligned depth to the internal 8-bit scaled form
    // required by calculate(). Call once per frame, reuse for all persons.
    // result.data must point to a buffer of at least width*height bytes.
    static void create_depth_8u(const depth_image_16& depth, depth_image_8& result);

    // Estimates the center-of-mass (COM) and mean body depth for one person
    // from an aligned depth frame (depth registered to color, same pixel grid).
    //
    // Primary path — histogram-based:
    //   1. Uses color_bbox directly as the depth ROI (no projection needed, depth is aligned).
    //   2. Builds a histogram of depth_8u values inside the ROI.
    //   3. Finds the dominant depth cluster — the peak most likely to be the
    //      person's body rather than the background.
    //   4. Computes a histogram-weighted mean depth over that cluster → mean_body_depth.
    //   5. Builds a binary mask of pixels in the cluster and finds their spatial
    //      median (X and Y independently) → the 2D COM pixel.
    //   6. Attempts to extend the range slightly to include the head if a nearby
    //      secondary peak exists within 5 depth_8u bins (~150 mm).
    //
    // Fallback path (when the ROI has too few valid depth pixels for the histogram):
    //   Samples NUM_DEPTH_SAMPLES+3 evenly-spaced points along the vertical center of
    //   the bbox and picks the nearest valid reading (minimum depth in column).
    //
    // If intrinsics != nullptr, also projects the COM pixel to 3D camera space
    // (result.world_pos) using the standard pinhole model.
    //
    //   raw_depth          — raw 16-bit depth frame (aligned or non-aligned)
    //   depth_8u           — output of create_depth_8u() for the same frame
    //   color_bbox         — person bounding box (in color image or scaled-depth coords)
    //   person_center_color — person center in the same coordinate space as color_bbox
    //   intrinsics         — depth camera intrinsics; pass nullptr to skip world_pos/image_pos
    //   result             — filled on return; image_pos is in the same space as color_bbox
    //   depth_shift        — precomputed pixel offset from color space to raw-depth space
    //                        (call rs2_project_color_pixel_to_depth_pixel at bbox center).
    //                        Pass {0,0} for aligned depth (default).
    //
    // Returns false on invalid input (null data, zero-size image).
    //
    // Example:
    //
    //   // --- setup (once per camera session) ---
    //   com::camera_intrinsics intr{ fx, fy, cx, cy };
    //
    //   // --- per frame ---
    //   com::depth_image_16 raw{ depthPtr, 640, 480 };
    //
    //   std::vector<uint8_t> buf(640 * 480);
    //   com::depth_image_8 depth8{ buf.data(), 640, 480 };
    //   com::center_of_mass_calculator::create_depth_8u(raw, depth8);
    //
    //   // --- per detected person ---
    //   com::rect  bbox{ x, y, w, h };
    //   com::vec2f center{ x + w / 2.f, y + h / 2.f };
    //
    //   com::person_center_of_mass result{};
    //   if (com::center_of_mass_calculator::calculate(raw, depth8, bbox, center, &intr, result))
    //   {
    //       float distanceMm = result.mean_body_depth;  // 0 = unreliable
    //       // result.world_pos.x/y/z — 3D camera coords in mm (if intr provided)
    //       // result.image_pos.x/y   — COM pixel in color image (if intr provided)
    //   }
    static bool calculate(const depth_image_16&      raw_depth,
                          const depth_image_8&        depth_8u,
                          const rect&                 color_bbox,
                          const vec2f&                person_center_color,
                          const camera_intrinsics*    intrinsics,
                          person_center_of_mass&      result,
                          vec2f                       depth_shift = {0.f, 0.f});

private:
    static int  get_depth_at_color_pixel(const depth_image_16& depth, vec2f color_pt);

    static int  get_mean_surrounding_depth(const depth_image_16& depth,
                                           vec2i pt,
                                           int interval,
                                           int min_range,
                                           int max_range,
                                           float fraction_non_zero    = 0.1f,
                                           int   max_range_surrounding = 500);

    static rect clamp_rect_to_image(const rect& r, int img_width, int img_height);

    static bool calculate_com_with_depth_range(const depth_image_8& depth_8u,
                                               const rect& roi,
                                               float& depth_mean,
                                               vec2i& center_mass_point);

    static float calc_hist_range_mean(const std::vector<float>& hist,
                                      int range_start, int range_end);

    static bool calc_center_of_mask(const std::vector<uint8_t>& mask,
                                    int mask_width, int mask_height,
                                    vec2i& com,
                                    int max_y = 0);  // 0 = use full height

    static bool run_non_range_com_calculation_flow(const rect&              color_rect,
                                                   const depth_image_16&    depth,
                                                   vec2f                    person_center,
                                                   const camera_intrinsics* intrinsics,
                                                   person_center_of_mass&   result);

};

} // namespace com
