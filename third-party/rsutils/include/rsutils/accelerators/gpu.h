// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

namespace rsutils {

    // Returns true if a CUDA-capable NVIDIA GPU is usable on this machine.
    // Runtime probe via the CUDA Driver API (libcuda.so.1 / nvcuda.dll) - no
    // compile-time CUDA dependency, so the binary loads on systems without CUDA.
    // Result is cached for the lifetime of the process; safe to call concurrently
    // from multiple threads (C++11 magic-static one-time init).
    // Do not call from DllMain / global constructors: the probe calls LoadLibrary
    // on Windows which acquires the loader lock.
    bool rs2_is_cuda_available();

}  // namespace rsutils
