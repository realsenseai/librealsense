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
// A cluster is treated as noise if a neighbour within NEARBY_REJECT_RANGE_D8U depth_8u
// units (≈1050 mm) is at least NEARBY_REJECT_RATIO times larger.
// The ratio threshold (3.0×) ensures only genuine clutter is rejected — real person
// clusters are at most ~2× smaller than a background wall, while clutter is typically
// 5–150× smaller than the person behind it.
static constexpr float    MIN_CLUSTER_FRACT       = 0.15f;  // auto-pass — no neighbour check
static constexpr float    NEARBY_REJECT_RATIO     = 3.0f;   // neighbour must be ≥3× larger
static constexpr int      NEARBY_REJECT_RANGE_D8U = 35;     // ≈1050 mm window
static constexpr float    NEARBY_REJECT_DEPTH_RATIO = 2.2f; // background >2.2× farther in mm also counts as "nearby"
// Fraction of ROI height used for histogram — wide enough to sample the full torso.
static constexpr float    COM_UPPER_FRACTION   = 0.65f;
// Fraction of ROI height used for the 2D centroid (dot position) — restricts to the
// upper chest/shoulder zone so the dot never lands below the upper torso.
// Max dot position = 40% from top of bbox (chest area).  Falls back to
// COM_UPPER_FRACTION if no cluster pixels exist in this narrower region.
static constexpr float    COM_CENTROID_FRACTION = 0.40f;
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

    // X: full mask height — wider sample for a stable horizontal centroid, blended
    // toward bbox center proportionally to left-right imbalance (IR depth shadows
    // cause one-sided coverage that biases the raw centroid).
    long long sumX = 0; long long cntX = 0;
    int leftPx = 0, rightPx = 0;
    int const midX = mask_width / 2;
    for (int y = 0; y < mask_height; ++y)
        for (int x = 0; x < mask_width; ++x)
            if (mask[y * mask_width + x]) {
                sumX += x; ++cntX;
                if (x < midX) ++leftPx; else ++rightPx;
            }

    // Y: upper portion only — excludes leg pixels that pull the centroid downward.
    long long sumY = 0; long long cntY = 0;
    for (int y = 0; y < max_y; ++y)
        for (int x = 0; x < mask_width; ++x)
            if (mask[y * mask_width + x]) { sumY += y; ++cntY; }

    if (cntX == 0 || cntY == 0) return false;

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
    const rect& roi, float& depth_mean, vec2i& center_mass_point)
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

    // Pick the NEAREST cluster (smallest midpoint depth_8u).
    // A small cluster (< MIN_CLUSTER_FRACT) is skipped when a significantly larger
    // neighbour exists within NEARBY_REJECT_RANGE_D8U — this rejects noise/clutter
    // that sits just in front of the real body.  A cluster that passes MIN_CLUSTER_FRACT
    // is always accepted; a small cluster with no dominant neighbour nearby is also
    // accepted (e.g. person occupying a small fraction of a wide bbox with far background).
    int histRangeStart = allRanges[0].start;
    int histRangeEnd   = allRanges[0].end;
    int bestMidD       = INT_MAX;

    for (int i = 0; i < (int)allRanges.size(); ++i) {
        int midD = (allRanges[i].start + allRanges[i].end) / 2;
        if (midD >= bestMidD) continue;  // not nearer than current best

        if (allRanges[i].fract < MIN_CLUSTER_FRACT) {
            // Check whether a dominant neighbour makes this cluster noise.
            // "Nearby" means within NEARBY_REJECT_RANGE_D8U bins (~1050 mm), or the
            // neighbour is a background layer >2.2× farther in mm — catches large d8u
            // gaps that the bin-distance check would miss.
            bool dominated = false;
            float const depth_i_mm = midD * SCALE_DEPTH + SUBTRACT_FROM_DEPTH;
            for (int j = 0; j < (int)allRanges.size(); ++j) {
                if (j == i) continue;
                int midJ = (allRanges[j].start + allRanges[j].end) / 2;
                float const depth_j_mm = midJ * SCALE_DEPTH + SUBTRACT_FROM_DEPTH;
                bool const nearby = (std::abs(midJ - midD) <= NEARBY_REJECT_RANGE_D8U)
                                 || (depth_j_mm > NEARBY_REJECT_DEPTH_RATIO * depth_i_mm);
                if (nearby && allRanges[j].fract >= NEARBY_REJECT_RATIO * allRanges[i].fract) {
                    dominated = true;
                    break;
                }
            }
            if (dominated) continue;
        }

        bestMidD = midD;
        histRangeStart = allRanges[i].start;
        histRangeEnd   = allRanges[i].end;
    }

    if ((histRangeEnd - histRangeStart) >= (MaxDepth8U - 1)) return false;

    float meanDepth8U = calc_hist_range_mean(hist, histRangeStart, histRangeEnd);

    // Optionally extend toward a nearby head cluster (closer to camera, within ~150 mm).
    for (auto const& r : allRanges) {
        if (r.end < histRangeStart) {
            float meanRange = calc_hist_range_mean(hist, r.start, r.end);
            if ((meanDepth8U - meanRange) <= 5) { histRangeStart = r.start; break; }
        }
    }
    // Recompute mean over the final range (possibly extended to include head cluster).
    meanDepth8U = calc_hist_range_mean(hist, histRangeStart, histRangeEnd);
    depth_mean = std::floor(meanDepth8U * SCALE_DEPTH + SUBTRACT_FROM_DEPTH);

    // Build mask and find 2D center-of-mass.
    // Use COM_CENTROID_FRACTION (narrower than the histogram region) so the dot
    // lands in the upper-body (chest/shoulder) area rather than the waist.
    // If the selected cluster has no pixels that high, place the dot at the
    // horizontal centroid of the cluster and the vertical center of the upper region.
    std::vector<uint8_t> mask(roiW * roiH, 0);
    for (int j = 0; j < (int)roiData.size(); ++j)
        if (roiData[j] >= histRangeStart && roiData[j] <= histRangeEnd) mask[j] = 1;

    int const centroidMaxY = std::max(1, (int)(roiH * COM_CENTROID_FRACTION));
    vec2i com;
    if (!calc_center_of_mask(mask, roiW, roiH, com, centroidMaxY))
    {
        // Cluster pixels are below the upper-body region; fall back to the
        // wider histogram zone for both X and Y centroid.
        int const histMaxY = std::max(1, (int)(roiH * COM_UPPER_FRACTION));
        if (!calc_center_of_mask(mask, roiW, roiH, com, histMaxY))
            return false;
    }

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
    vec2f                       depth_shift)
{
    if (!raw_depth.data || raw_depth.width == 0 || raw_depth.height == 0)
        return false;
    if (!depth_8u.data || depth_8u.width != raw_depth.width || depth_8u.height != raw_depth.height)
        return false;

    // Apply the precomputed color→raw-depth pixel shift.
    // Round to the nearest integer so the forward and reverse shifts are consistent.
    int const shift_xi = (int)std::round(depth_shift.x);
    int const shift_yi = (int)std::round(depth_shift.y);
    rect shifted_bbox{ color_bbox.x + shift_xi, color_bbox.y + shift_yi,
                       color_bbox.width, color_bbox.height };
    rect roi = clamp_rect_to_image(shifted_bbox, raw_depth.width, raw_depth.height);

    float depth_mean = 0.0f;
    vec2i center_mass_point = {0, 0};
    bool  status = calculate_com_with_depth_range(depth_8u, roi, depth_mean, center_mass_point);

    if (status) {
        result.mean_body_depth = (depth_mean <= MIN_DEPTH) ? 0.0f : depth_mean;

        if (intrinsics && depth_mean > MIN_DEPTH) {
            // Project COM pixel (in raw-depth space) using histogram depth_mean as Z.
            vec3f pt = pixel_to_camera(center_mass_point, depth_mean, *intrinsics);
            result.world_pos = pt;
            // Reverse-shift image_pos from raw-depth space back to the caller's input space.
            result.image_pos = { (float)center_mass_point.x - shift_xi,
                                 (float)center_mass_point.y - shift_yi };
            result.mean_body_depth = std::sqrt(pt.x*pt.x + pt.y*pt.y + pt.z*pt.z);
        }
    } else {
        // Histogram failed — depth data too sparse. Fall back to column sampling.
        run_non_range_com_calculation_flow(color_bbox, raw_depth, person_center_color, intrinsics, result);
    }

    return true;
}

} // namespace com
