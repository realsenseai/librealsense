# rs-gpu-frame Sample

## Overview

This sample demonstrates the **`rs2::gpu_frame` extension** — the zero-copy GPU pointer API.
`get_data()` on the base frame always returns the **host** pointer (unchanged); the GPU device
pointer is reached through a separate extension you cast to with **`frame::as<rs2::gpu_frame>()`**.
On a build with `BUILD_WITH_CUDA_ZEROCOPY` running on an integrated GPU (Jetson), a frame's pixels
live in GPU-mapped memory, so the cast is non-null and `gpu_frame::get_gpu_data()` returns a CUDA
device pointer that aliases the frame — feed it straight to a CUDA kernel / TensorRT / NPP with
**no host→device copy**.

The two entry points:

| Call | Returns |
|---|---|
| `frame::get_data()` | host (CPU) pointer — always valid |
| `frame::as<rs2::gpu_frame>()` → `gpu_frame::get_gpu_data()` | device pointer, **non-null only when the frame is GPU-resident** |
| `frame::get_gpu_data_or_upload(&copied)` | a device pointer **always** on a CUDA build (zero-copy → `copied=false`, else an SDK-managed upload → `copied=true`) |

## Expected Output

The sample prints a per-frame line and a summary. On a zero-copy build on a Jetson:

```
=== rs2::gpu_frame / zero-copy GPU pointer demo ===
  host / gpu_frame / device / path  (legend)

frame 31  1280x720  host=0x2047d2000  gpu_frame=yes  device=0x2047d2000  path=ZERO-COPY (no copy)
...
ZERO-COPY ACTIVE — frame.as<rs2::gpu_frame>() returns a CUDA device pointer ...
```

On a `cuda` (non-zero-copy) build or a discrete GPU the cast is null and it reports the
**UPLOAD** path; on a non-CUDA build it reports **NONE** and you use `get_data()` + your own upload.

## When is the GPU pointer available?

`frame::as<rs2::gpu_frame>()` is non-null (and `get_gpu_data()` returns a device pointer) only when
**all** of the following hold:

1. The SDK was built with `-DBUILD_WITH_CUDA=ON -DBUILD_WITH_CUDA_ZEROCOPY=ON`.
2. It runs on an **integrated GPU** (e.g. NVIDIA Jetson), where CPU and GPU share memory and
   frame buffers are GPU-mapped.

Otherwise the cast is null. Use `get_gpu_data_or_upload()` (an SDK-managed device copy) or fall
back to `get_data()` plus your own upload. Writing the code with this branch keeps it correct and
portable across every build and platform — discrete GPU, non-CUDA builds, and Jetson alike.

## Notes

- The GPU pointer aliases the **shared** frame buffer: treat it as **read-only** (other consumers
  read the same memory) and keep the `rs2::frame` alive until your GPU work has completed.
- The buffer is mapped pinned memory: ideal for streaming reads (e.g. an NN input); it is not
  intended for heavy random/atomic GPU access directly on the frame.
