#!/bin/bash -e

# USAGE:
# Normal usecase - without any parameters.
#
# [optional parameters]:
# --uninstall : remove permissions for realsense devices.

install=true

for var in "$@"
do
    if [ "$var" = "--uninstall" ]; then
        install=false
    fi
done

if [ "$install" = true ]; then
    echo "Setting-up permissions for RealSense devices"
else
    echo "Remove permissions for RealSense devices"
fi

# Dependency: v4l-utils
v4l2_util=$(which v4l2-ctl || true)
if [ -z ${v4l2_util} ]; then
  echo "v4l2-ctl not found, install with: sudo apt install v4l-utils"
  exit 1
fi

is_tegra=$(${v4l2_util} --list-devices | grep tegra  || true)
is_ipu6=$(${v4l2_util} --list-devices | grep ipu6  || true)

exec 3>&2
exec 2> /dev/null
con_dev=$(ls /dev/video* | wc -l)
exec 2>&3

if [ "$install" = true ]; then
    if [ $con_dev -ne 0 ];
    then
        echo -e "\e[32m"
        read -p "Remove all RealSense cameras attached. Hit any key when ready"
        echo -e "\e[0m"
    fi
    sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/

    if [[ -n "${is_ipu6}" ]] || [[ -n "${is_tegra}" ]]; then
        sudo cp config/99-realsense-d4xx-mipi-dfu.rules /etc/udev/rules.d/
        sudo cp scripts/rs-enum.sh /usr/local/bin/rs-enum.sh
        sudo cp scripts/rs_ipu6_d457_bind.sh /usr/local/bin/rs_ipu6_d457_bind.sh
    fi
else
    sudo rm /etc/udev/rules.d/99-realsense-libusb.rules
    if [[ -n "${is_ipu6}" ]] || [[ -n "${is_tegra}" ]]; then
        sudo rm /etc/udev/rules.d/99-realsense-d4xx-mipi-dfu.rules
        sudo rm /usr/local/bin/rs-enum.sh
        sudo rm /usr/local/bin/rs_ipu6_d457_bind.sh
    fi
fi

sudo udevadm control --reload-rules && sudo udevadm trigger
if [ "$install" = true ]; then
    echo "udev-rules successfully installed"
else
    echo "udev-rules successfully uninstalled"
fi
