#include "CenterOfMass.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace COM {

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------

static constexpr uint16_t SUBTRACT_FROM_DEPTH  = 400;  // depths below this map to depth8U=0
static constexpr float    SCALE_DEPTH          = 20.0f;
static constexpr int      MIN_DEPTH            = 400;
static constexpr int      MAX_DEPTH            = 8000;
static constexpr int      NO_DEPTH             = 10000;
static constexpr float    GOOD_DEPTH_RATIO     = 0.3f;
static constexpr int      NUM_DEPTH_SAMPLES    = 5;
static constexpr int      MAX_DEPTH_FOR_SAMPLES = 650;
static constexpr int      MAX_STD_DEPTH_SAMPLES = 450;

// depth8U value for MAX_DEPTH_FOR_BLOB (4500 mm): ceil((4500-400)/20) = 205
static const int MaxDepth8U =
    (int)std::ceil((4500 - (int)SUBTRACT_FROM_DEPTH) / SCALE_DEPTH);

// ---------------------------------------------------------------------------
// 3D helpers (used only when intrinsics != nullptr)
// ---------------------------------------------------------------------------

static Vec3f PixelToCamera(Vec2i pixel, float depth_mm, const CameraIntrinsics& K)
{
    return {
        (pixel.x - K.cx) * depth_mm / K.fx,
        (pixel.y - K.cy) * depth_mm / K.fy,
        depth_mm
    };
}

// ---------------------------------------------------------------------------
// CreateDepth8U
// ---------------------------------------------------------------------------

void CenterOfMassCalculator::CreateDepth8U(const DepthImage16& depth, DepthImage8& result)
{
    int len = depth.width * depth.height;
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
// getMeanSurroundingDepth
// ---------------------------------------------------------------------------

int CenterOfMassCalculator::getMeanSurroundingDepth(
    const DepthImage16& depth, Vec2i pt,
    int interval, int minRange, int maxRange,
    float fractionNonZero, int maxRangeSurrounding)
{
    if (depth.width == 0 || depth.height == 0) return 0;

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
            vals[nVals++] = (v > minRange && v < maxRange) ? v : 0;
        }

    int windowSz   = (2 * interval + 1) * (2 * interval + 1);
    int minPixels  = (int)(fractionNonZero * windowSz);
    int numNonZero = 0;
    for (int i = 0; i < nVals; ++i) if (vals[i] > 0) ++numNonZero;
    if (numNonZero < minPixels) return 0;

    // First-pass mean
    long long sum = 0; int cnt = 0;
    for (int i = 0; i < nVals; ++i) if (vals[i] > 0) { sum += vals[i]; ++cnt; }
    if (cnt == 0) return 0;
    int meanTemp = (int)(sum / cnt);

    // Second-pass mean within ±maxRangeSurrounding
    int minVal = std::max(meanTemp - maxRangeSurrounding, minRange + 1);
    int maxVal = std::min(meanTemp + maxRangeSurrounding, maxRange - 1);
    sum = 0; cnt = 0;
    for (int i = 0; i < nVals; ++i) if (vals[i] >= minVal && vals[i] <= maxVal) { sum += vals[i]; ++cnt; }
    return cnt > 0 ? (int)(sum / cnt) : 0;
}

// ---------------------------------------------------------------------------
// GetDepthAtColorPixel
// With aligned depth the color pixel IS the depth pixel.
// ---------------------------------------------------------------------------

int CenterOfMassCalculator::GetDepthAtColorPixel(
    const DepthImage16& depth, Vec2f colorPt)
{
    Vec2i pt = {(int)(colorPt.x + 0.5f), (int)(colorPt.y + 0.5f)};
    pt.x = std::max(0, std::min(pt.x, depth.width  - 1));
    pt.y = std::max(0, std::min(pt.y, depth.height - 1));
    return getMeanSurroundingDepth(depth, pt, 5, 0, NO_DEPTH);
}

// ---------------------------------------------------------------------------
// ClampRectToImage
// With aligned depth the color bbox is already in depth image space.
// ---------------------------------------------------------------------------

Rect CenterOfMassCalculator::ClampRectToImage(
    const Rect& rect, int imgWidth, int imgHeight)
{
    Rect out;
    out.x = std::max(0, rect.x);
    out.y = std::max(0, rect.y);
    int x2 = std::min(rect.x + rect.width,  imgWidth)  - 1;
    int y2 = std::min(rect.y + rect.height, imgHeight) - 1;
    out.width  = std::max(0, x2 - out.x + 1);
    out.height = std::max(0, y2 - out.y + 1);
    return out;
}

// ---------------------------------------------------------------------------
// CalcHistRangeMean
// ---------------------------------------------------------------------------

float CenterOfMassCalculator::CalcHistRangeMean(
    const std::vector<float>& hist, int rangeStart, int rangeEnd)
{
    float sumEl = 0, numEl = 0;
    for (int i = rangeStart; i <= rangeEnd; ++i) {
        sumEl += hist[i] * i;
        numEl += hist[i];
    }
    return numEl > 0 ? sumEl / numEl : 0.0f;
}

// ---------------------------------------------------------------------------
// CalcCenterOfMask
// ---------------------------------------------------------------------------

bool CenterOfMassCalculator::CalcCenterOfMask(
    const std::vector<uint8_t>& mask, int maskWidth, int maskHeight, Vec2i& com)
{
    int sumAll = 0;
    for (uint8_t v : mask) if (v) ++sumAll;
    if (sumAll == 0) return false;
    int halfNum = (sumAll + 1) / 2;

    std::vector<int> projX(maskWidth, 0);
    for (int y = 0; y < maskHeight; ++y)
        for (int x = 0; x < maskWidth; ++x)
            if (mask[y * maskWidth + x]) projX[x]++;

    int acc = 0; com.x = maskWidth - 1;
    for (int x = 0; x < maskWidth; ++x) {
        acc += projX[x];
        if (acc > halfNum) { com.x = x; break; }
    }

    std::vector<int> projY(maskHeight, 0);
    for (int y = 0; y < maskHeight; ++y)
        for (int x = 0; x < maskWidth; ++x)
            if (mask[y * maskWidth + x]) projY[y]++;

    acc = 0; com.y = maskHeight - 1;
    for (int y = 0; y < maskHeight; ++y) {
        acc += projY[y];
        if (acc > halfNum) { com.y = y; break; }
    }
    return true;
}

// ---------------------------------------------------------------------------
// CalculateComWithDepthRange
// ---------------------------------------------------------------------------

bool CenterOfMassCalculator::CalculateComWithDepthRange(
    const DepthImage8& depth8U,
    const Rect& roi, float& depthMean, Vec2i& centerMassPoint)
{
    int roiW = roi.width, roiH = roi.height;
    if (roiW <= 0 || roiH <= 0) return false;

    // Extract compact ROI
    std::vector<uint8_t> roiData(roiW * roiH);
    for (int y = 0; y < roiH; ++y)
        for (int x = 0; x < roiW; ++x)
            roiData[y * roiW + x] = depth8U.data[(roi.y + y) * depth8U.width + (roi.x + x)];

    // Histogram: bin i = count of pixels with depth8U value i
    int histSize = MaxDepth8U + 1;
    std::vector<float> hist(histSize, 0.0f);
    for (uint8_t v : roiData)
        if (v >= 1 && v < histSize) hist[v] += 1.0f;

    int sumEl = 0;
    for (float v : hist) sumEl += (int)v;
    if (sumEl < (int)(GOOD_DEPTH_RATIO * roiW * roiH)) return false;

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

        if (maxVal < 0.01) break;

        // Extend high until bins drop below 1%
        int rangeEnd = maxLoc + 1;
        while (rangeEnd < histSize && histFract[rangeEnd] >= 0.01f) ++rangeEnd;
        rangeEnd = std::min(rangeEnd, histSize - 1);

        // Extend low until bins drop below 1%
        int rangeStart = maxLoc - 1;
        while (rangeStart >= 1 && histFract[rangeStart] >= 0.01f) --rangeStart;
        rangeStart = std::max(rangeStart, 1);

        // Sum fraction over the full range and zero it
        float fract = 0.0f;
        for (int j = rangeStart; j <= rangeEnd; ++j) {
            fract += histFract[j];
            histFract[j] = 0;
        }
        histFract[0] = 0;

        allRanges.push_back({rangeStart, rangeEnd, fract});
    }

    if (allRanges.empty()) return false;

    // Select the dominant cluster.
    //
    // The previous approach selected the cluster with the most pixels (largest fraction).
    // That caused the background to win whenever it occupied more of the bounding box
    // than the person, making the COM teleport to the wall / floor.
    //
    // Fix: sort clusters by depth (nearest first — smallest rangeStart = closest to
    // camera) and pick the first one that accounts for at least MIN_BODY_FRACTION of
    // all valid pixels.  The person is always in front of any background object, so
    // the nearest significant cluster IS the person.  If no cluster meets the
    // fraction threshold (e.g. person is very far or partially out of frame), fall
    // back to the nearest cluster overall — still better than picking the background.
    static constexpr float MIN_BODY_FRACTION = 0.10f;
    std::sort(allRanges.begin(), allRanges.end(),
        [](const DepthRange& a, const DepthRange& b) { return a.start < b.start; });

    int histRangeStart = allRanges[0].start;
    int histRangeEnd   = allRanges[0].end;
    for (auto const& r : allRanges) {
        if (r.fract >= MIN_BODY_FRACTION) {
            histRangeStart = r.start;
            histRangeEnd   = r.end;
            break;
        }
    }

    if ((histRangeEnd - histRangeStart) >= (MaxDepth8U - 1)) return false;

    float meanDepth8U = CalcHistRangeMean(hist, histRangeStart, histRangeEnd);
    depthMean = std::floor(meanDepth8U * SCALE_DEPTH + SUBTRACT_FROM_DEPTH);

    // Optionally extend toward a nearby head cluster (closer to camera, within 100 mm)
    for (auto const& r : allRanges) {
        if (r.end < histRangeStart) {
            float meanRange = CalcHistRangeMean(hist, r.start, r.end);
            if ((meanDepth8U - meanRange) <= 5) { histRangeStart = r.start; break; }
        }
    }

    // Build mask and find 2D center-of-mass
    std::vector<uint8_t> mask(roiW * roiH, 0);
    for (int j = 0; j < (int)roiData.size(); ++j)
        if (roiData[j] >= histRangeStart && roiData[j] <= histRangeEnd) mask[j] = 1;

    Vec2i com;
    if (!CalcCenterOfMask(mask, roiW, roiH, com)) return false;

    centerMassPoint = {com.x + roi.x, com.y + roi.y};
    return true;
}

// ---------------------------------------------------------------------------
// RunNonRangeComCalculationFlow  (fallback when histogram path fails)
// ---------------------------------------------------------------------------

bool CenterOfMassCalculator::RunNonRangeComCalculationFlow(
    const Rect& colorRect, const DepthImage16& depth,
    Vec2f personCenter, const CameraIntrinsics* intrinsics,
    PersonCenterOfMass& result)
{
    // Clamp starting sample point inside the bbox
    Vec2f samplePt = personCenter;
    if (colorRect.y > personCenter.y)
        samplePt.y = (float)(colorRect.y + 10);

    float averageDepth = (float)GetDepthAtColorPixel(depth, samplePt);

    int   count = NUM_DEPTH_SAMPLES + 3;
    std::vector<float> depthSamples(count, 0.0f);
    float yProgression = colorRect.height / (float)NUM_DEPTH_SAMPLES;
    float chosenDepth  = averageDepth;

    for (int i = 0; i < count; ++i) {
        if (i >= NUM_DEPTH_SAMPLES - 1 && chosenDepth >= MIN_DEPTH) break;
        float sampleY   = colorRect.y + (i + 1) * yProgression;
        depthSamples[i] = (float)GetDepthAtColorPixel(depth, {personCenter.x, sampleY});
        if (chosenDepth <= MAX_DEPTH) {
            if ((std::abs(chosenDepth - depthSamples[i]) < MAX_DEPTH_FOR_SAMPLES
                    && chosenDepth < depthSamples[i]) ||
                (chosenDepth == 0 && depthSamples[i] < MAX_DEPTH
                    && depthSamples[i] > MIN_DEPTH))
                chosenDepth = depthSamples[i];
        }
    }

    if (chosenDepth <= MAX_DEPTH) {
        averageDepth = chosenDepth;
    } else {
        int   nonZero = 0;
        float sumV = 0, sumV2 = 0;
        for (float v : depthSamples)
            if (v > 0) { sumV += v; sumV2 += v * v; ++nonZero; }
        if (nonZero >= NUM_DEPTH_SAMPLES - 1) {
            float mean   = sumV / nonZero;
            float stdDev = std::sqrt(std::max(0.0f, sumV2 / nonZero - mean * mean));
            if (stdDev < MAX_STD_DEPTH_SAMPLES && mean < MAX_DEPTH && mean > MIN_DEPTH)
                averageDepth = mean;
        }
    }

    result.meanBodyDepth = (averageDepth <= MIN_DEPTH) ? 0.0f : averageDepth;

    if (intrinsics && averageDepth > MIN_DEPTH) {
        Vec2i centerPx = {(int)(personCenter.x + 0.5f), (int)(personCenter.y + 0.5f)};
        result.worldPos  = PixelToCamera(centerPx, averageDepth, *intrinsics);
        result.imagePos  = {personCenter.x, personCenter.y};
    }
    return true;
}

// ---------------------------------------------------------------------------
// Calculate  (main entry point)
// ---------------------------------------------------------------------------

bool CenterOfMassCalculator::Calculate(
    const DepthImage16&     rawDepth,
    const DepthImage8&      depth8U,
    const Rect&             colorBbox,
    const Vec2f&                personCenterColor,
    const CameraIntrinsics*     intrinsics,
    PersonCenterOfMass&         result)
{
    if (!rawDepth.data || rawDepth.width == 0 || rawDepth.height == 0)
        return false;
    if (!depth8U.data || depth8U.width != rawDepth.width || depth8U.height != rawDepth.height)
        return false;

    // 1. With aligned depth, the color bbox IS the depth ROI — just clamp to bounds
    Rect roi = ClampRectToImage(colorBbox, rawDepth.width, rawDepth.height);

    // 2. Histogram-based COM + mean depth
    float depthMean = 0.0f;
    Vec2i centerMassPoint = {0, 0};
    bool  status = CalculateComWithDepthRange(
                       depth8U, roi, depthMean, centerMassPoint);

    if (status) {
        result.meanBodyDepth = (depthMean <= MIN_DEPTH) ? 0.0f : depthMean;

        if (intrinsics && depthMean > MIN_DEPTH) {
            float localDepth = (float)getMeanSurroundingDepth(
                                   rawDepth, centerMassPoint, 5, 0, NO_DEPTH);
            result.worldPos = PixelToCamera(centerMassPoint, localDepth, *intrinsics);
            result.imagePos = {(float)centerMassPoint.x, (float)centerMassPoint.y};
        }
    } else {
        // Fallback: sample-based estimation along the bbox center column
        RunNonRangeComCalculationFlow(
            colorBbox, rawDepth, personCenterColor, intrinsics, result);
    }

    return true;
}

} // namespace COM
