// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

namespace rsutils {

  // GPU acceleration probes.  Each probe runs at most once per process and
  // caches its result, so repeated calls are cheap.  Detection is performed
  // at runtime by dlopen/LoadLibrary'ing the vendor driver -- no link-time
  // dependency on libcuda / libamdhip64 is introduced -- so these functions
  // are safe to call from builds compiled without CUDA or HIP support and
  // on hosts where the matching driver is absent.

  bool rs2_is_cuda_available();   // true iff the NVIDIA CUDA driver reports >= 1 visible device
  bool rs2_is_hip_available();    // true iff the AMD HIP runtime reports >= 1 visible device
  bool rs2_is_gpu_available();    // true iff rs2_is_cuda_available() || rs2_is_hip_available()

}  // namespace rsutils
