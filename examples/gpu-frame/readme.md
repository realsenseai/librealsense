# rs-gpu-frame Sample

## Overview

This sample demonstrates `rs2::frame::get_gpu_data()` — the zero-copy GPU pointer API. It
retrieves a color frame and shows how to obtain a CUDA device pointer that aliases the frame
data, so a GPU consumer (a CUDA kernel, TensorRT inference, NPP, etc.) can read the frame
**in place without a host→device copy**.

## Expected Output

The sample prints the frame size and both the host pointer and the GPU pointer:

```
Color frame 1280x720  host=0x...  gpu=0x...
Zero-copy GPU pointer available — a CUDA/TensorRT consumer can read the frame in place, no host->device copy.
```

or, when zero-copy is not active:

```
Color frame 1280x720  host=0x...  gpu=0
No GPU pointer (discrete GPU, or non-zero-copy build) — upload yourself: ...
```

## When is the GPU pointer available?

`get_gpu_data()` returns a non-null device pointer only when **all** of the following hold:

1. The SDK was built with `-DBUILD_WITH_CUDA=ON -DBUILD_WITH_CUDA_ZEROCOPY=ON`.
2. It runs on an **integrated GPU** (e.g. NVIDIA Jetson), where CPU and GPU share memory and
   frame buffers are GPU-mapped.

Otherwise it returns `null`, and you should fall back to `get_data()` plus your own upload.
Writing the code with this branch keeps it correct and portable across every build and
platform — discrete GPU, non-CUDA builds, and Jetson alike.

## Notes

- The returned pointer is valid only while the frame is held. Keep the `rs2::frame` alive
  until your GPU work has completed.
- The buffer is mapped pinned memory: ideal for streaming reads (e.g. an NN input); it is not
  intended for heavy random/atomic GPU access directly on the frame.
