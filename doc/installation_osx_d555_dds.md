# macOS + D555 Ethernet / DDS (Apple Silicon)

This guide covers **Intel RealSense D555 (PoE / Ethernet + DDS)** on **macOS arm64**.
USB D4xx cameras still use the classic RSUSB backend; D555 is **not** USB video —
it speaks **DDS (FastDDS)** over Ethernet.

> Status: verified with librealsense **2.58.x**, macOS 15+/26 arm64, D555 factory IP
> `192.168.11.55`, host Ethernet first in service order (Wi‑Fi may stay enabled).

---

## Network (host)

| Setting | Recommended |
|---------|-------------|
| Link | Direct Ethernet (or PoE injector). Switch optional. |
| Host IPv4 | Manual, e.g. `192.168.11.100` |
| Subnet | `255.255.255.0` (`/24`) |
| Camera IP | Factory default **`192.168.11.55`** (ping should succeed) |
| **MTU** | **Jumbo 9000** (required for stable high-res streams) |
| Speed | 1000baseT full-duplex is fine |
| Service order | **Ethernet above Wi‑Fi** (System Settings → Network) |
| Multi-homed | OK if Ethernet is preferred for `192.168.11.0/24` routes |

Check:

```bash
ifconfig en0 | grep -E 'inet |mtu|media'
ping -c 2 192.168.11.55
route -n get 192.168.11.55   # interface should be en0 (or your Ethernet)
```

---

## Build (source)

Dependencies:

```bash
brew install cmake libusb pkg-config openssl
export OPENSSL_ROOT_DIR="$(brew --prefix openssl)"
```

Configure **with DDS** (required for D555):

```bash
git clone https://github.com/realsenseai/librealsense.git
cd librealsense
mkdir build && cd build

PY="$(which python3)"
PY_VER="$("$PY" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
PREFIX="$HOME/librealsense-d555"

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_WITH_DDS=ON \
  -DBUILD_TOOLS=ON \
  -DBUILD_PYTHON_BINDINGS=ON \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_GRAPHICAL_EXAMPLES=OFF \
  -DFORCE_RSUSB_BACKEND=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DPYTHON_EXECUTABLE="$PY" \
  -DPYTHON_INSTALL_DIR="lib/python${PY_VER}/site-packages/pyrealsense2"

cmake --build . -j"$(sysctl -n hw.ncpu)"
cmake --install .
```

On Apple + DDS, CMake defaults `PYTHON_INSTALL_DIR` under the **install prefix** (not Homebrew `site-packages`) so the extension and dylibs stay co-located for `@loader_path`.

### Why `BUILD_WITH_DDS=ON` and shared FastDDS on Apple?

- D555 is discovered and streamed only through the **DDS** path (`realdds` + FastDDS).
- On **macOS**, FastDDS is built as **shared** libraries (`libfastrtps`, `libfastcdr`) and linked into `librealsense2.dylib`.
- Archiving FastDDS **statically** into `librealsense2.dylib` has been observed to crash inside
  `DomainParticipantFactory::create_participant` (null PC). Fully static tools (e.g. `rs-dds-sniffer`)
  can still work; anything that **loads** `librealsense2` as a dylib does not.
- Install/RPATH: libraries use `@loader_path` so **`DYLD_LIBRARY_PATH` is not required** after `cmake --install`.

---

## Runtime config

Optional `~/.realsense-config.json`:

```json
{
  "context": {
    "dds": {
      "enabled": true,
      "domain": 0
    }
  }
}
```

| Key | Notes |
|-----|--------|
| **domain** | Default **0**. Must match camera / site policy. |
| **enabled** | `true` for Ethernet D555. |
| **sw_only** | DDS devices are software-backed; tools often need SW product-line bits. |

### Enumerate

```bash
# After install (prefix/bin on PATH, prefix/lib via rpath):
export PATH="$HOME/librealsense-d555/bin:$PATH"

rs-dds-sniffer -d 0 --participants -s
# expect: ... D555_<serial>

# DDS devices: use --sw-only (includes RS2_PRODUCT_LINE_SW_ONLY)
rs-enumerate-devices --sw-only -s
```

Without `--sw-only`, USB-only masks may hide DDS devices even when the sniffer sees the participant.

### Environment (production)

```bash
export PATH="$PREFIX/bin:$PATH"
export PYTHONPATH="$PREFIX/lib/python${PY_VER}/site-packages${PYTHONPATH:+:$PYTHONPATH}"
# Do not set DYLD_LIBRARY_PATH — tools and the extension use @rpath/@loader_path.
```

### Python

```bash
python3 - <<'PY'
import json, time
import pyrealsense2 as rs

settings = {
    "dds": {
        "enabled": True,
        "domain": 0,
        "query-devices-max": 12,
        "query-devices-min": 4,
        "device-initialization-timeout-ms": 15000,
    },
    "device-mask": 511,  # any + sw bits; see product_line in API
    "partial-device-allowed": True,
}
ctx = rs.context(json.dumps(settings))
mask = int(rs.product_line.any_intel) | int(rs.product_line.sw_only) | int(rs.product_line.non_intel)
for _ in range(15):
    devs = ctx.query_devices(mask)
    if len(devs):
        d = devs[0]
        print(d.get_info(rs.camera_info.name), d.get_info(rs.camera_info.serial_number))
        break
    time.sleep(1)
else:
    raise SystemExit("no D555")
PY
```

### Stream notes

- Prefer **native profiles** from the device (e.g. color/depth **1280×800**). Arbitrary 640×480 may fail with “Couldn't resolve requests”.
- First handshake can take several seconds on multi-homed hosts; allow `query-devices-min/max` time.

---

## Troubleshooting

| Symptom | Check |
|---------|--------|
| `ping` OK, sniffer empty | MTU 9000? Domain ID? PoE power? |
| Sniffer sees D555, `query_devices` empty | Use `sw_only` / `device-mask` including SW; wait longer |
| `create_participant` crash | Rebuild with `BUILD_WITH_DDS=ON` on Apple (shared FastDDS path) |
| `Couldn't resolve requests` | List `sensor.profiles` and enable those width/height/format/fps |
| Need `DYLD_LIBRARY_PATH` after install | RPATH not applied; reinstall with the Apple DDS rpath cmake, or run from install `lib`/`bin` layout |

---

## Related

- Base macOS build notes: [installation_osx.md](installation_osx.md)
- CMake option: `BUILD_WITH_DDS` in `CMake/lrs_options.cmake`
