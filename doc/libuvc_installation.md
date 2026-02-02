# LibUVC-backend installation

> If kernel patching is not possible, or any steps during official [installation.md](./installation.md) fail,
> you should try **libuvc-backend** version of librealsense
> This method is not officially validated but can run on wider range of platforms including older / newer kernel version

> This script requires internet connection. Please make sure network proxy settings are correct

1. Make sure no RealSense device is connected
2. Open the terminal, run:
```sh
$ wget https://github.com/realsenseai/librealsense/raw/master/scripts/libuvc_installation.sh
$ chmod +x ./libuvc_installation.sh
$ ./libuvc_installation.sh
```
3. Wait until `Librealsense script completed` message is displayed (may take some time)
4. Connect RealSense device
5. Run `rs-enumerate-devices` from the terminal to verify the installation

> At the moment, the script assumes Ubuntu 16 with graphic subsystem

> If you encounter any problems or would like to extend the script to additional system, please [let us know via a new GitHub issue](https://github.com/realsenseai/librealsense/issues/new)
