# Software Support Matrix

## Release Process

RealSense devices pass validation and stability cycles. Every firmware + librealsense release is going through several levels of validation, from automated unit-tests to longer validation cycles.  

After passing unit-tests and basic functional testing release is made available under [librealsense/releases](https://github.com/realsenseai/librealsense/releases) and marked as a ![pre-release](./img/prerelease.png).

Once full validation report is internally reviewed and approved, release is promoted to ![latest](./img/latest_release.png) and published at [www.realsenseai.com/developers/](https://realsenseai.com/developers).

We recommend certain combinations (see **RECOMMENDED CONFIGURATIONS** at the link above) of firmware and software we have tested, but there is significant effort to keep maximum backward and forward compatibility between firmware and software versions.

Validation is done on x86 NUC machines running Windows 10 and Ubuntu 16.04 LTS.

Additional platforms are tested and supported on demand, driven by community requests and feedback.

