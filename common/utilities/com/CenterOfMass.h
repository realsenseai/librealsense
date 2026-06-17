#pragma once

#include <vector>
#include <cstdint>

namespace COM {

// ---------------------------------------------------------------------------
// Basic types
// ---------------------------------------------------------------------------

struct Vec2f { float x, y; };
struct Vec2i { int   x, y; };
struct Vec3f { float x, y, z; };

struct Rect {
    int x, y, width, height;
};

// ---------------------------------------------------------------------------
// Aligned depth image (non-owning view).
// Every pixel (u, v) in this image corresponds to the same (u, v) in the
// color image — no projection needed to relate the two.
// ---------------------------------------------------------------------------

struct DepthImage16 {
    const uint16_t* data;   // row-major: pixel(x,y) = data[y * width + x]
    int width;
    int height;
};

struct DepthImage8 {
    uint8_t* data;          // row-major: pixel(x,y) = data[y * width + x]
    int width;
    int height;
};

// ---------------------------------------------------------------------------
// Camera intrinsics — standard pinhole model, all values in pixels / mm.
// ---------------------------------------------------------------------------

struct CameraIntrinsics {
    float fx, fy;   // focal length (pixels)
    float cx, cy;   // principal point (pixels)
};

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

struct PersonCenterOfMass {
    float meanBodyDepth;    // average distance to person (mm); 0 = unreliable
    Vec3f worldPos;         // 3D camera coords (mm); zero if intrinsics not provided
    Vec2f imagePos;         // COM pixel in color/depth image; zero if intrinsics not provided
};

// ---------------------------------------------------------------------------
// CenterOfMassCalculator
// ---------------------------------------------------------------------------

class CenterOfMassCalculator {
public:
    // Convert raw 16-bit aligned depth to the internal 8-bit scaled form
    // required by Calculate(). Call once per frame, reuse for all persons.
    // result.data must point to a buffer of at least width*height bytes.
    static void CreateDepth8U(const DepthImage16& depth, DepthImage8& result);

    // Estimates the center-of-mass (COM) and mean body depth for one person
    // from an aligned depth frame (depth registered to color, same pixel grid).
    //
    // Primary path — histogram-based:
    //   1. Uses colorBbox directly as the depth ROI (no projection needed, depth is aligned).
    //   2. Builds a histogram of depth8U values inside the ROI.
    //   3. Finds the dominant depth cluster — the peak most likely to be the
    //      person's body rather than the background.
    //   4. Computes a histogram-weighted mean depth over that cluster → meanBodyDepth.
    //   5. Builds a binary mask of pixels in the cluster and finds their spatial
    //      median (X and Y independently) → the 2D COM pixel.
    //   6. Attempts to extend the range slightly to include the head if a nearby
    //      secondary peak exists within 10 depth8U bins (~200 mm).
    //
    // Fallback path (when the ROI has too few valid depth pixels for the histogram):
    //   Samples depth at NUM_DEPTH_SAMPLES evenly-spaced points along the vertical
    //   center of the bbox. Picks the best reading using a max-within-tolerance
    //   heuristic, falling back to mean±stddev filtering if no single sample wins.
    //
    // If intrinsics != nullptr, also projects the COM pixel to 3D camera space
    // (result.worldPos) using the standard pinhole model.
    //
    //   rawDepth          — aligned 16-bit depth frame (same pixel grid as color)
    //   depth8U           — output of CreateDepth8U() for the same frame
    //   colorBbox         — person bounding box in color image coordinates
    //   personCenterColor — person center in color image (bbox center or tracker output)
    //   intrinsics        — camera intrinsics; pass nullptr to skip worldPos/imagePos
    //   result            — filled on return
    //
    // Returns false on invalid input (null data, zero-size image).
    //
    // Example:
    //
    //   // --- setup (once per camera session) ---
    //   COM::CameraIntrinsics intr{ fx, fy, cx, cy };
    //
    //   // --- per frame ---
    //   COM::DepthImage16 raw{ depthPtr, 640, 480 };
    //
    //   std::vector<uint8_t> buf(640 * 480);
    //   COM::DepthImage8 depth8{ buf.data(), 640, 480 };
    //   COM::CenterOfMassCalculator::CreateDepth8U(raw, depth8);
    //
    //   // --- per detected person ---
    //   COM::Rect  bbox{ x, y, w, h };
    //   COM::Vec2f center{ x + w / 2.f, y + h / 2.f };
    //
    //   COM::PersonCenterOfMass result{};
    //   if (COM::CenterOfMassCalculator::Calculate(raw, depth8, bbox, center, &intr, result))
    //   {
    //       float distanceMm = result.meanBodyDepth;  // 0 = unreliable
    //       // result.worldPos.x/y/z — 3D camera coords in mm (if intr provided)
    //       // result.imagePos.x/y   — COM pixel in color image (if intr provided)
    //   }
    static bool Calculate(const DepthImage16&      rawDepth,
                          const DepthImage8&       depth8U,
                          const Rect&              colorBbox,
                          const Vec2f&             personCenterColor,
                          const CameraIntrinsics*  intrinsics,
                          PersonCenterOfMass&      result);

private:
    static int  GetDepthAtColorPixel(const DepthImage16& depth, Vec2f colorPt);

    static int  getMeanSurroundingDepth(const DepthImage16& depth,
                                         Vec2i pt,
                                         int interval,
                                         int minRange,
                                         int maxRange,
                                         float fractionNonZero     = 0.1f,
                                         int   maxRangeSurrounding = 500);

    static Rect ClampRectToImage(const Rect& rect, int imgWidth, int imgHeight);

    static bool CalculateComWithDepthRange(const DepthImage8& depth8U,
                                            const Rect& roiInDepth,
                                            float& depthMean,
                                            Vec2i& centerMassPoint);

    static float CalcHistRangeMean(const std::vector<float>& hist,
                                    int rangeStart, int rangeEnd);

    static bool CalcCenterOfMask(const std::vector<uint8_t>& mask,
                                  int maskWidth, int maskHeight,
                                  Vec2i& com);

    static bool RunNonRangeComCalculationFlow(const Rect&             colorRect,
                                               const DepthImage16&     depth,
                                               Vec2f                   personCenter,
                                               const CameraIntrinsics* intrinsics,
                                               PersonCenterOfMass&     result);
};

} // namespace COM
