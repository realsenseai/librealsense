// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <common/utilities/com/center-of-mass.h>
#include <climits>
#include <cmath>
#include <algorithm>
#include <vector>

namespace com {

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------

static constexpr uint16_t SUBTRACT_FROM_DEPTH  = 400;  // depths below this map to depth_8u=0
static constexpr float    SCALE_DEPTH          = 30.0f;  // covers MAX_DEPTH in uint8: ceil((8000-400)/30)=254
static constexpr int      MIN_DEPTH            = 400;
static constexpr int      MAX_DEPTH            = 8000;
static constexpr int      NO_DEPTH             = 10000;
static constexpr float    GOOD_DEPTH_RATIO     = 0.3f;
// Fraction of ROI height used for centroid — limits the search to the torso region
// and prevents leg pixels (which dominate the lower bbox) from pulling the COM down.
static constexpr float    COM_UPPER_FRACTION   = 0.65f;
static constexpr int      NUM_DEPTH_SAMPLES    = 5;

// depth_8u value for MAX_DEPTH: ceil((MAX_DEPTH-400)/SCALE_DEPTH) — fits in uint8 with SCALE_DEPTH=30
static const int MaxDepth8U =
    (int)std::ceil(((int)MAX_DEPTH - (int)SUBTRACT_FROM_DEPTH) / SCALE_DEPTH);

// ---------------------------------------------------------------------------
// 3D helpers (used only when intrinsics != nullptr)
// ---------------------------------------------------------------------------

static vec3f pixel_to_camera(vec2i pixel, float depth_mm, const camera_intrinsics& K)
{
    return {
        (pixel.x - K.cx) * depth_mm / K.fx,
        (pixel.y - K.cy) * depth_mm / K.fy,
        depth_mm
    };
}

// ---------------------------------------------------------------------------
// create_depth_8u
// ---------------------------------------------------------------------------

void center_of_mass_calculator::create_depth_8u(const depth_image_16& depth, depth_image_8& result)
{
    int len = depth.width * depth.height;
    if (!depth.data || len == 0) return;
    if (!result.data || result.width * result.height < len) return;
    constexpr float factor = 1.0f / SCALE_DEPTH;
    for (int i = 0; i < len; ++i) {
        int t = (int)depth.data[i] - (int)SUBTRACT_FROM_DEPTH;
        if (t < 0) t = 0;
        int v = (int)(t * factor + 0.5f);
        result.data[i] = (uint8_t)(v > 255 ? 255 : v);
    }
}

// ---------------------------------------------------------------------------
// get_mean_surrounding_depth
// ---------------------------------------------------------------------------

int center_of_mass_calculator::get_mean_surrounding_depth(
    const depth_image_16& depth, vec2i pt,
    int interval, int min_range, int max_range,
    float fraction_non_zero, int max_range_surrounding)
{
    if (depth.width == 0 || depth.height == 0) return 0;
    if (interval > 5) interval = 5;  // MAX_WINDOW sized for interval <= 5 (11×11 = 121)

    int minX = std::max(pt.x - interval, 2);
    int maxX = std::min(pt.x + interval, depth.width  - 2);
    int minY = std::max(pt.y - interval, 2);
    int maxY = std::min(pt.y + interval, depth.height - 2);
    if (maxX <= minX || maxY <= minY) return 0;

    // Stack buffer — max interval is 5, so max window is 11×11 = 121 elements
    constexpr int MAX_WINDOW = 121;
    int vals[MAX_WINDOW];
    int nVals = 0;
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x) {
            int v = (int)depth.data[y * depth.width + x];
            vals[nVals++] = (v > min_range && v < max_range) ? v : 0;
        }

    int windowSz   = (2 * interval + 1) * (2 * interval + 1);
    int minPixels  = (int)(fraction_non_zero * windowSz);
    int numNonZero = 0;
    for (int i = 0; i < nVals; ++i) if (vals[i] > 0) ++numNonZero;
    if (numNonZero < minPixels) return 0;

    // First-pass mean
    long long sum = 0; int cnt = 0;
    for (int i = 0; i < nVals; ++i) if (vals[i] > 0) { sum += vals[i]; ++cnt; }
    if (cnt == 0) return 0;
    int meanTemp = (int)(sum / cnt);

    // Second-pass mean within ±max_range_surrounding
    int minVal = std::max(meanTemp - max_range_surrounding, min_range + 1);
    int maxVal = std::min(meanTemp + max_range_surrounding, max_range - 1);
    sum = 0; cnt = 0;
    for (int i = 0; i < nVals; ++i) if (vals[i] >= minVal && vals[i] <= maxVal) { sum += vals[i]; ++cnt; }
    return cnt > 0 ? (int)(sum / cnt) : 0;
}

// ---------------------------------------------------------------------------
// get_depth_at_color_pixel
// With aligned depth the color pixel IS the depth pixel.
// ---------------------------------------------------------------------------

int center_of_mass_calculator::get_depth_at_color_pixel(
    const depth_image_16& depth, vec2f color_pt)
{
    vec2i pt = {(int)(color_pt.x + 0.5f), (int)(color_pt.y + 0.5f)};
    pt.x = std::max(0, std::min(pt.x, depth.width  - 1));
    pt.y = std::max(0, std::min(pt.y, depth.height - 1));
    return get_mean_surrounding_depth(depth, pt, 5, 0, NO_DEPTH);
}

// ---------------------------------------------------------------------------
// clamp_rect_to_image
// With aligned depth the color bbox is already in depth image space.
// ---------------------------------------------------------------------------

rect center_of_mass_calculator::clamp_rect_to_image(
    const rect& r, int img_width, int img_height)
{
    rect out;
    out.x = std::max(0, r.x);
    out.y = std::max(0, r.y);
    int x2 = std::min(r.x + r.width,  img_width)  - 1;
    int y2 = std::min(r.y + r.height, img_height) - 1;
    out.width  = std::max(0, x2 - out.x + 1);
    out.height = std::max(0, y2 - out.y + 1);
    return out;
}

// ---------------------------------------------------------------------------
// calc_hist_range_mean
// ---------------------------------------------------------------------------

float center_of_mass_calculator::calc_hist_range_mean(
    const std::vector<float>& hist, int range_start, int range_end)
{
    float sumEl = 0, numEl = 0;
    for (int i = range_start; i <= range_end; ++i) {
        sumEl += hist[i] * i;
        numEl += hist[i];
    }
    return numEl > 0 ? sumEl / numEl : 0.0f;
}

// ---------------------------------------------------------------------------
// calc_center_of_mask
// ---------------------------------------------------------------------------

bool center_of_mass_calculator::calc_center_of_mask(
    const std::vector<uint8_t>& mask, int mask_width, int mask_height, vec2i& com,
    int max_y)
{
    if (max_y <= 0 || max_y > mask_height) max_y = mask_height;

    // Both X and Y are computed over the upper max_y rows only — keeps the symmetry
    // metric and both centroids consistent with the same torso-region pixels.
    long long sumX = 0, sumY = 0; long long cntX = 0, cntY = 0;
    int leftPx = 0, rightPx = 0;
    int const midX = mask_width / 2;
    for (int y = 0; y < max_y; ++y)
        for (int x = 0; x < mask_width; ++x)
            if (mask[y * mask_width + x]) {
                sumX += x; sumY += y; ++cntX;
                if (x < midX) ++leftPx; else ++rightPx;
            }
    cntY = cntX;

    if (cntX == 0) return false;

    float const centroidX = float(sumX) / cntX;
    float const symmetry  = 2.0f * std::min(leftPx, rightPx) / float(leftPx + rightPx);
    com.x = (int)(symmetry * centroidX + (1.0f - symmetry) * midX);
    com.y = (int)(float(sumY) / cntY);
    return true;
}

// ---------------------------------------------------------------------------
// calculate_com_with_depth_range
// ---------------------------------------------------------------------------

bool center_of_mass_calculator::calculate_com_with_depth_range(
    const depth_image_8& depth_8u,
    const rect& roi, float& depth_mean, vec2i& center_mass_point,
    com_debug_info* dbg)
{
    int roiW = roi.width, roiH = roi.height;
    if (roiW <= 0 || roiH <= 0) return false;

    // Extract compact ROI
    std::vector<uint8_t> roiData(roiW * roiH);
    for (int y = 0; y < roiH; ++y)
        for (int x = 0; x < roiW; ++x)
            roiData[y * roiW + x] = depth_8u.data[(roi.y + y) * depth_8u.width + (roi.x + x)];

    // Restrict histogram to the upper portion of the bbox (torso region) so that
    // lower-body / floor pixels don't inflate a far-background cluster beyond the
    // person cluster.  The same upper-fraction is used later for the COM Y centroid.
    int const histRows = std::max(1, (int)(roiH * COM_UPPER_FRACTION));

    // Histogram: bin i = count of pixels with depth_8u value i
    int histSize = MaxDepth8U + 1;
    std::vector<float> hist(histSize, 0.0f);
    for (int y = 0; y < histRows; ++y)
        for (int x = 0; x < roiW; ++x) {
            uint8_t v = roiData[y * roiW + x];
            if (v >= 1 && v < histSize) hist[v] += 1.0f;
        }

    int sumEl = 0;
    for (float v : hist) sumEl += (int)v;
    if (sumEl < (int)(GOOD_DEPTH_RATIO * roiW * histRows)) return false;

    std::vector<float> histFract(histSize);
    for (int i = 0; i < histSize; ++i) histFract[i] = hist[i] / sumEl;

    // Iteratively extract every depth cluster: find the peak, extend to adjacent
    // significant bins, record the range + its fraction of valid pixels, then zero
    // the full range so the next iteration finds the next distinct cluster.
    struct DepthRange { int start, end; float fract; };
    std::vector<DepthRange> allRanges;

    while (true) {
        double maxVal = 0; int maxLoc = 0;
        for (int i = 1; i < histSize; ++i)
            if (histFract[i] > maxVal) { maxVal = histFract[i]; maxLoc = i; }

        if (maxVal < 0.001) break;

        // Extend only to adjacent bins above 1% — keeps distinct depth layers separate.
        // The peak threshold (0.001) is intentionally lower so sparse person peaks are
        // extracted, but the extension threshold stays high to prevent a sparse person
        // cluster from growing into the dense background cluster across the valley.
        int rangeEnd = maxLoc + 1;
        while (rangeEnd < histSize && histFract[rangeEnd] >= 0.01f) ++rangeEnd;
        rangeEnd = std::min(rangeEnd, histSize - 1);

        int rangeStart = maxLoc - 1;
        while (rangeStart >= 1 && histFract[rangeStart] >= 0.01f) --rangeStart;
        rangeStart = std::max(rangeStart, 1);

        // Sum fraction over the full range and zero it
        float fract = 0.0f;
        for (int j = rangeStart; j <= rangeEnd; ++j) {
            fract += histFract[j];
            histFract[j] = 0;
        }

        allRanges.push_back({rangeStart, rangeEnd, fract});
    }

    if (allRanges.empty()) return false;

    int const cx = roi.x + roi.width  / 2;
    int const cy = roi.y + roi.height / 2;
    // 5×5 patch mean around the ROI center — more stable than a single pixel
    int const x0 = std::max(cx - 2, 0), x1 = std::min(cx + 2, depth_8u.width  - 1);
    int const y0 = std::max(cy - 2, 0), y1 = std::min(cy + 2, depth_8u.height - 1);
    int patchSum = 0, patchCnt = 0;
    for (int py = y0; py <= y1; ++py)
        for (int px = x0; px <= x1; ++px) {
            uint8_t v = depth_8u.data[py * depth_8u.width + px];
            if (v >= 1) { patchSum += v; ++patchCnt; }
        }
    uint8_t const center_d8u = patchCnt > 0 ? (uint8_t)(patchSum / patchCnt) : 0;
    if (dbg) { dbg->center_d8u = center_d8u; dbg->center_d8u_cnt = patchCnt; dbg->n_clusters = (int)allRanges.size(); }

    // Always pick the NEAREST cluster (smallest midpoint depth_8u).
    // Background is always farther than the person, and the histogram is already
    // restricted to the upper torso region so floor/leg pixels can't inflate a far
    // cluster.  The nearest cluster is therefore the person's body in the common case;
    // the one exception is foreground clutter (e.g. a desk) partially overlapping the
    // bbox, where the clutter's cluster would be picked — correct by depth geometry.
    int histRangeStart = allRanges[0].start;
    int histRangeEnd   = allRanges[0].end;
    int bestMidD       = INT_MAX;
    for (auto const& r : allRanges) {
        int midD = (r.start + r.end) / 2;
        if (midD < bestMidD) { bestMidD = midD; histRangeStart = r.start; histRangeEnd = r.end; }
    }

    if (dbg) {
        // Find the fract of the selected cluster
        for (auto const& r : allRanges)
            if (r.start == histRangeStart && r.end == histRangeEnd) { dbg->cluster_fract = r.fract; break; }
        dbg->cluster_start = histRangeStart;
        dbg->cluster_end   = histRangeEnd;
    }

    if ((histRangeEnd - histRangeStart) >= (MaxDepth8U - 1)) return false;

    float const meanDepth8U = calc_hist_range_mean(hist, histRangeStart, histRangeEnd);
    depth_mean = std::floor(meanDepth8U * SCALE_DEPTH + SUBTRACT_FROM_DEPTH);

    // Build mask and find 2D center-of-mass.
    // Only use pixels in the upper COM_UPPER_FRACTION of the ROI height so that
    // leg pixels (which dominate the lower bbox) don't pull the centroid down.
    std::vector<uint8_t> mask(roiW * roiH, 0);
    for (int j = 0; j < (int)roiData.size(); ++j)
        if (roiData[j] >= histRangeStart && roiData[j] <= histRangeEnd) mask[j] = 1;

    int const comMaxY = std::max(1, (int)(roiH * COM_UPPER_FRACTION));
    vec2i com;
    if (!calc_center_of_mask(mask, roiW, roiH, com, comMaxY)) return false;

    center_mass_point = {com.x + roi.x, com.y + roi.y};
    return true;
}

// ---------------------------------------------------------------------------
// run_non_range_com_calculation_flow  (fallback when histogram path fails)
// ---------------------------------------------------------------------------

bool center_of_mass_calculator::run_non_range_com_calculation_flow(
    const rect& color_rect, const depth_image_16& depth,
    vec2f person_center, const camera_intrinsics* intrinsics,
    person_center_of_mass& result)
{
    // Clamp starting sample point inside the bbox
    vec2f samplePt = person_center;
    if (color_rect.y > person_center.y)
        samplePt.y = (float)(color_rect.y + 10);

    // Sample the center column at NUM_DEPTH_SAMPLES+3 evenly-spaced Y positions and
    // take the NEAREST (minimum) valid depth.  The person is always in front of the
    // background, so minimum valid depth in the column equals person depth even when
    // most pixels are invalid (sparse IR) or some samples hit background.
    float yProgression = color_rect.height / (float)(NUM_DEPTH_SAMPLES + 3);
    float chosenDepth = 0.0f;

    auto tryDepth = [&](float d) {
        if (d > MIN_DEPTH && d <= MAX_DEPTH && (chosenDepth == 0.0f || d < chosenDepth))
            chosenDepth = d;
    };

    tryDepth((float)get_depth_at_color_pixel(depth, samplePt));
    for (int i = 0; i < NUM_DEPTH_SAMPLES + 3; ++i) {
        float sampleY = std::min(color_rect.y + (i + 1) * yProgression,
                                  float(color_rect.y + color_rect.height - 1));
        tryDepth((float)get_depth_at_color_pixel(depth, {person_center.x, sampleY}));
    }

    result.mean_body_depth = chosenDepth;

    if (intrinsics && chosenDepth > MIN_DEPTH) {
        vec2i centerPx = {(int)(person_center.x + 0.5f), (int)(person_center.y + 0.5f)};
        result.world_pos = pixel_to_camera(centerPx, chosenDepth, *intrinsics);
        result.image_pos = {person_center.x, person_center.y};
    }
    return true;
}

// ---------------------------------------------------------------------------
// calculate  (main entry point)
// ---------------------------------------------------------------------------

bool center_of_mass_calculator::calculate(
    const depth_image_16&   raw_depth,
    const depth_image_8&    depth_8u,
    const rect&             color_bbox,
    const vec2f&            person_center_color,
    const camera_intrinsics*    intrinsics,
    person_center_of_mass&      result,
    com_debug_info*             dbg)
{
    if (!raw_depth.data || raw_depth.width == 0 || raw_depth.height == 0)
        return false;
    if (!depth_8u.data || depth_8u.width != raw_depth.width || depth_8u.height != raw_depth.height)
        return false;

    // 1. With aligned depth, the color bbox IS the depth ROI — just clamp to bounds
    rect roi = clamp_rect_to_image(color_bbox, raw_depth.width, raw_depth.height);

    // 2. Histogram-based COM + mean depth
    float depth_mean = 0.0f;
    vec2i center_mass_point = {0, 0};
    bool  status = calculate_com_with_depth_range(
                       depth_8u, roi, depth_mean, center_mass_point, dbg);

    if (status) {
        result.mean_body_depth = (depth_mean <= MIN_DEPTH) ? 0.0f : depth_mean;

        if (intrinsics && depth_mean > MIN_DEPTH) {
            float localDepth = (float)get_mean_surrounding_depth(
                                   raw_depth, center_mass_point, 5, 0, NO_DEPTH);
            result.world_pos = pixel_to_camera(center_mass_point, localDepth, *intrinsics);
            result.image_pos = {(float)center_mass_point.x, (float)center_mass_point.y};
        }
        if (dbg) { dbg->histogram_ok = true; dbg->image_pos_x = result.image_pos.x; dbg->image_pos_y = result.image_pos.y; }
    } else {
        // Fallback: sample-based estimation along the bbox center column
        run_non_range_com_calculation_flow(
            color_bbox, raw_depth, person_center_color, intrinsics, result);
        if (dbg) { dbg->histogram_ok = false; dbg->image_pos_x = result.image_pos.x; dbg->image_pos_y = result.image_pos.y; }
    }

    return true;
}

} // namespace com
