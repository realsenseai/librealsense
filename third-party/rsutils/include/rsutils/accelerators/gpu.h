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

    // Returns true if the (first) CUDA device is an integrated GPU sharing physical
    // memory with the CPU (Jetson / Tegra). On such parts, mapped/zero-copy memory is a
    // win because no data physically moves; on discrete GPUs it is a loss (per-element
    // PCIe access), so the zero-copy path must be gated on this. Always false when no
    // CUDA device is present. Probed via the CUDA Driver API attribute
    // CU_DEVICE_ATTRIBUTE_INTEGRATED, cached for the process lifetime. Same threading /
    // DllMain caveats as rs2_is_cuda_available().
    bool rs2_is_cuda_integrated();

    // Returns true if the (first) HIP device is an integrated GPU sharing physical memory
    // with the CPU (unified-memory AMD APUs, e.g. Ryzen AI / MI300A -- NOT discrete
    // RDNA3/CDNA3 GPUs). Mirrors rs2_is_cuda_integrated() exactly; probed via the HIP
    // Runtime API attribute hipDeviceAttributeIntegrated, cached for the process lifetime.
    bool rs2_is_hip_integrated();

}  // namespace rsutils
