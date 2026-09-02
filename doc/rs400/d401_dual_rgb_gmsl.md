# D401 GMSL: Dual-RGB (raw) and ISP color modes

## Overview

The **D401 GMSL** builds its color image from its **two OV9782 global-shutter stereo imagers**, which are shared between **depth** and **color**. Each imager can, at any given moment, produce **either** a mono infrared image **or** a Bayer color image, never both at once. Because of this, there are two ways to obtain color from the D401 GMSL:

| Mode | Where color is produced | Color streams | Runs together with infrared? |
|------|-------------------------|---------------|------------------------------|
| **ISP color** (legacy) | Firmware ISP (`YUYV`, converted on the host) | one `Color` | **Yes**, with `Infrared 1`, `Infrared 2`, `Depth` |
| **Raw dual-RGB** | Host-side debayer of raw Bayer (`RAW8`/`BA81`) | `Color` **+** `Color 1` | **No**, both imagers are producing Bayer |

> **Firmware requirement.** Raw dual-RGB is available only on firmware **5.17.4.13 or newer**. On older firmware the camera exposes the **ISP color** path only; requesting dual-RGB (a second color stream) will fail to resolve, and the Viewer shows a single ISP color stream.

## The two modes

### ISP color (legacy)

The firmware ISP produces a single processed color stream (`YUYV`), which the SDK converts to the standard color formats. This is the original D401 GMSL color behavior and it coexists with `Depth`, `Infrared 1`, and `Infrared 2`. Use ISP color when you want color **alongside** infrared.

<p align="center">
  <img src="https://librealsense.realsenseai.com/readme-media/d401_isp_color_viewer.png" width="820" alt="ISP color mode in the Viewer: Depth, Infrared 1, Infrared 2 and a YUYV Color stream"/>
</p>
<p align="center"><em>ISP color mode, <code>Depth</code> + <code>Infrared 1</code> + <code>Infrared 2</code> + <code>Color</code> (<code>YUYV</code>) all streaming together.</em></p>

### Raw dual-RGB

Both imagers stream **raw Bayer** (`RAW8`, fourcc `BA81`). The SDK debayers and color-processes each imager on the host and exposes **two** color streams: `Color` (index 0, left imager) and `Color 1` (index 1, right imager). Because both imagers are producing Bayer color, **infrared is not available** in this mode. The Viewer additionally offers a stereo-rectification filter (on by default) that aligns the two color streams.

<p align="center">
  <img src="https://librealsense.realsenseai.com/readme-media/d401_dual_rgb_viewer.png" width="820" alt="Raw dual-RGB mode in the Viewer: Depth, Color and Color 1 both RGB8"/>
</p>
<p align="center"><em>Raw dual-RGB mode, <code>Depth</code> + <code>Color</code> + <code>Color 1</code> (both <code>RGB8</code>); infrared is unavailable while both imagers produce Bayer.</em></p>

## Streams and formats

| Stream | Formats | Notes |
|--------|---------|-------|
| `Depth` | `Z16` | Independent, always available |
| `Infrared 1` | `Y8`, `Y16` | Left imager (mono) |
| `Infrared 2` | `Y8`, `Y16` | Right imager (mono) |
| `Color` (index 0) | `RGB8`, `BGR8`, `RGBA8`, `BGRA8`, `YUYV` | Left imager, ISP color on its own; switches to raw when `Color 1` is also enabled |
| `Color 1` (index 1) | `RGB8` | Right imager, raw dual-RGB only |

The **second color stream (`Color 1`) selects the mode**: enable `Color 1` for raw dual-RGB. A single `Color` stream, in **any** format, including `RGB8`, is ISP color and coexists with infrared.

Resolutions (shared across depth, infrared, and color): `1280x720`, `848x480`, `640x480`, `640x360`, `480x270`, `424x240`.

## Valid stream combinations

Because the imagers are shared, only certain combinations can stream together. Infrared can run with a single (ISP) `Color` stream in **any** format; raw dual-RGB (`Color` + `Color 1`) consumes both imagers, so infrared is not available there. Depth is independent and can be added to any row.

| Use case | Depth | Infrared 1 | Infrared 2 | Color | Color 1 |
|----------|:-----:|:----------:|:----------:|:-----:|:-------:|
| Depth + stereo IR | `Z16` | `Y8/Y16` | `Y8/Y16` | — | — |
| Depth + IR + **ISP** color | `Z16` | `Y8/Y16` | `Y8/Y16` | `RGB8/BGR8/YUYV/RGBA8/BGRA8` | — |
| **ISP** color only (any format) | `Z16` (opt) | — | — | `RGB8/BGR8/YUYV/RGBA8/BGRA8` | — |
| **Raw** dual-RGB (+ optional depth) | `Z16` (opt) | — | — | `RGB8` | `RGB8` |
| Infrared only | — | `Y8/Y16` | `Y8/Y16` | — | — |

## Using it in the SDK

**Raw dual-RGB, both color streams:**

```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_COLOR, 0, 848, 480, RS2_FORMAT_RGB8, 30); // Color   (left imager)
cfg.enable_stream(RS2_STREAM_COLOR, 1, 848, 480, RS2_FORMAT_RGB8, 30); // Color 1 (right imager)
// Depth may be added; infrared cannot run in this mode.

rs2::pipeline pipe;
pipe.start(cfg);
```

**ISP color together with depth and infrared:**

```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH,       848, 480, RS2_FORMAT_Z16,  30);
cfg.enable_stream(RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8,   30);
cfg.enable_stream(RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8,   30);
cfg.enable_stream(RS2_STREAM_COLOR,       848, 480, RS2_FORMAT_BGR8, 30); // ISP color

rs2::pipeline pipe;
pipe.start(cfg);
```

> A single `Color` stream is ISP regardless of format, `RGB8` on its own is ISP color and runs with infrared. Raw dual-RGB is selected by also enabling `Color 1`.
>
> On firmware older than 5.17.4.13, enabling `Color 1` throws `Couldn't resolve requests`, since raw dual-RGB is not exposed. ISP color (a single `Color` stream) continues to work.

## Using it in the Viewer

In **Stereo Module → Available Streams** you will see `Depth`, `Infrared 1`, `Infrared 2`, `Color`, and (on supported firmware) `Color 1`. The **`Color 1` checkbox chooses the mode**:

- Leave **`Color 1` off** → **ISP** color. `Color` streams in any format (including `RGB8`) alongside `Infrared 1/2`.
- Tick **`Color 1`** → **raw** dual-RGB. `Infrared 1/2` grey out and `Color` switches to `RGB8`.

The Viewer greys out any stream that cannot run in the currently selected mode, so it is not possible to pick an unstreamable combination. See the two mode screenshots above.

## Controls

On the D401 GMSL all controls are exposed on a single **Stereo Module** sensor (depth, infrared, and color are folded into it). Which controls affect the image depends on the mode:

- **Depth / infrared:** exposure, gain, auto-exposure and the depth controls behave as on any D400 stereo module.
- **ISP color:** the firmware ISP applies exposure, gain, white balance, saturation, and sharpness to the color image.
- **Raw dual-RGB color:** white balance is performed automatically on the host, so the hardware `White Balance` / `Auto White Balance` controls do not apply. When streaming color only (depth off), color exposure may be high; streaming depth alongside color lets the depth exposure govern the shared imager.

## Limitations

- **Color and infrared cannot run together in raw mode.** Both imagers produce Bayer, and `Color 1` uses the infrared imager. Keep to a single `Color` stream (do not enable `Color 1`) if you need color and infrared together.
- **Colored infrared** is not available on GMSL (infrared is delivered as `Y8` / `Y16` only).
- Raw dual-RGB requires firmware **5.17.4.13+**. Older firmware provides ISP color only.
- **Switching between ISP and raw dual-RGB needs a hardware reset** to take effect (each imager streams ISP color or raw Bayer, not both).
- **Manual exposure and gain have no effect on raw dual-RGB color** (the color-node controls do not reach the shared imager).
- **Raw dual-RGB color auto-exposure runs only while depth streams**, so with depth off the color can be over-exposed. Stream depth as a workaround.
