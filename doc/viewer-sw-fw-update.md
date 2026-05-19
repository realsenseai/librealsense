# Viewer tool SW/FW updates

The `librealsense Viewer` supports several ways of notifying the user that a software or firmware update is available.
Both manual (clicking the `Check for updates` button) and automatic (when connecting a new device) triggers are possible.

## Online updates

The Viewer will try to download and query a versions database from the Internet and will create a version update notification based on the connected device's recommended version.

*The online versions database may be behind the [GitHub SW releases webpage](https://github.com/realsenseai/librealsense/releases) or [RealSense FW releases webpage](https://dev.realsenseai.com/docs/firmware-updates).
We recommend checking those links for getting the latest released versions.

## Where to get firmware

The SDK no longer ships a "bundled" firmware binary. To update firmware, download the appropriate `.bin` for your device directly from the [RealSense FW releases webpage](https://dev.realsenseai.com/docs/firmware-updates), then flash it via the Viewer's `Update Firmware...` menu, or with `rs-fw-update -f <path-to-bin>`.

## Updates notifications logic flow

### Sequence Diagram
![LRS Updates Flow](./img/updates/updates.png)

*Created using  [DrawIO](https://app.diagrams.net/)





