# DDT occupancy-grid regression

Replays three recorded cases through the camera's own pipeline and compares the published
occupancy grid against the golden recorded with it.

| Case | Expected |
| ---- | -------- |
| `gt` | MATCH: nothing changed |
| `depth_changed` | DIFFER: one depth frame edited |
| `config_changed` | DIFFER: one configuration field edited |

Everything the replay depends on travels inside the case: the depth frame, the safety preset
and the depth calibration. The result is therefore decided by the firmware under test, not by
the scene in front of the camera. The configuration is backed up before the run and restored
afterwards, whatever happens.

## Running it by hand

Nothing has to be installed: `ddt.pyz` is a single-file zipapp carrying the whole `ddt_core`
package, and `filesrc_host` moves the files over the camera's EP14 bulk endpoint.

    cd unit-tests/live/d500/ddt
    python3 ddt_ci_validation.py corpus --slot occg_out

Takes about 8 s. It needs `pyrealsense2` importable and exactly one D5xx camera attached with
no other process streaming from it.

## Under CI

`pytest-ddt-og-validation.py` runs the same command. It is gated to **D585 and not D585S**:
the safety camera has no DDT core in its image. It also skips on firmware older than the
minimum in the test, because the goldens were recorded against a firmware that publishes the
occupancy grid as a pure payload with its attributes in the UVC metadata.

## Regenerating the goldens

Required whenever the firmware deliberately changes what it publishes. Run it against the
camera you trust, then commit the corpus:

    python3 ddt_ci_validation.py corpus --build

That recaptures `gt` from the live camera and derives the two changed cases from it. Raise the
minimum firmware version in the test to the build you recorded against, or an older firmware
will fail against goldens it cannot produce.
