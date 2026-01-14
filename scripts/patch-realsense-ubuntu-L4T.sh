#!/bin/bash
# The script utilizes `sources_sync.sh` provided as
# part of NVIDIA(R) SDK installer
set -e

echo -e "\e[32mThe script patches and applies in-tree kernel modules required for Librealsense SDK\e[0m"

#Locally suppress stderr to avoid raising not relevant messages
exec 3>&2
exec 2> /dev/null
con_dev=$(ls /dev/video* | wc -l)
exec 2>&3

function DisplayNvidiaLicense {
    revision=$1
    license_path="https://developer.download.nvidia.com/embedded/L4T/${revision}/Tegra_Software_License_Agreement-Tegra-Linux.txt"
    echo -e "\nPlease notice: This script will download the kernel source (from nv-tegra, NVIDIA's public git repository) which is subject to the following license:"
    echo -e "${license_path}\n"

    license="$(curl -L -s ${license_path})"
    [[ -z $license || "$license" == "Not found" ]] && echo "License link not found" && exit 2

    ## display the page ##
    echo -e "\n${license}\n"

    read -t 30 -n 1 -s -r -e -p $'\e[33mPress any key within 30 seconds to ACCEPT and continue...\e[0m'
    echo
}

function version_lt {
	IFS='.' read -r -a v1 <<< "$1"
	IFS='.' read -r -a v2 <<< "$2"
	for i in 0 1 2; do
		[[ v1[i] -lt v2[i] ]] && return 0
		[[ v1[i] -gt v2[i] ]] && return 1
	done
	return 1
}

if [[ $con_dev -ne 0 ]];
then
	echo -e "\e[32m"
	read -p "Remove all RealSense cameras attached. Hit any key when ready"
	echo -e "\e[0m"
fi

#Include usability functions
source ./scripts/patch-utils-hwe.sh

#Activate fan to prevent overheat during KM compilation
if [[ -f /sys/devices/pwm-fan/target_pwm ]]; then
	echo 200 | sudo tee /sys/devices/pwm-fan/target_pwm || true
fi

#Tegra-specific
KERNEL_RELEASE="4.9"
#Identify the Jetson board
JETSON_BOARD=$(tr -d '\0' </proc/device-tree/model)
echo -e "\e[32mJetson Board (proc/device-tree/model): ${JETSON_BOARD}\e[0m"

JETSON_L4T=""
# With L4T 32.3.1, NVIDIA added back /etc/nv_tegra_release
if [[ -f /etc/nv_tegra_release ]]; then
	JETSON_L4T_STRING=$(head -n 1 /etc/nv_tegra_release)
	JETSON_L4T_RELEASE=$(echo $JETSON_L4T_STRING | cut -f 2 -d ' ' | grep -Po '(?<=R)[^;]+')
	# Extract revision + trim trailing zeros to convert 32.5.0 => 32.5 to match git tags
	JETSON_L4T_REVISION_LONG=$(echo $JETSON_L4T_STRING | cut -f 2 -d ',' | grep -Po '(?<=REVISION: )[^;]+')
	JETSON_L4T_REVISION=$(echo $JETSON_L4T_REVISION_LONG | sed 's/.0$//g')
	JETSON_L4T_VERSION=$JETSON_L4T_RELEASE.$JETSON_L4T_REVISION
	echo -e "\e[32mJetson L4T version: ${JETSON_L4T_VERSION}\e[0m"
else
	echo -e "\e[41m/etc/nv_tegra_release not present, aborting script\e[0m"
	exit;
fi

# setting UBUNTU_CODENAME
[[ -f /etc/os-release ]] && eval $(cat /etc/os-release|grep UBUNTU_CODENAME=)

#Select the kernel patches revision that matches the paltform configuration
RELEASE_STRING="release"
case ${JETSON_L4T_VERSION} in
	"32.2.1" | "32.2.3" | "32.3.1" | "32.4.3")
		PATCHES_REV="4.4"		# Baseline for the patches
		echo -e "\e[32mNote: the patch makes changes to kernel device tree to support HID IMU sensors\e[0m"
	;;
	"32.4.4" | "32.5" | "32.5.1" | "32.6.1" | "32.7.1")
		PATCHES_REV="4.4.1"	# JP 4.4.1, 32.7.1 is JP 4.6.1
	;;
	"35.1" | "35.4.1")
		PATCHES_REV="5.0"	# JP 5.0.2, JP 5.1.2
		KERNEL_RELEASE="5.10"
		[[ $JETSON_L4T_VERSION = "35.1" ]] && RELEASE_STRING="Release"
		KBASE=./Tegra/kernel/kernel-$KERNEL_RELEASE
	;;
	"36.3" | "36.4" | "36.4.3" | "36.4.4" | "36.4.7")
		# 36.3 --> 6.0
		# 36.4 -> 6.1
		# 36.4.3 --> 6.2
		# 36.4.4 --> 6.2.1
		PATCHES_REV="6.0"
		KERNEL_RELEASE="5.15"
		UBUNTU_CODENAME=jammy
		KBASE=./Tegra/kernel/kernel-${UBUNTU_CODENAME}-src
	;;
	"38.2" | "38.2.1" | "38.2.2")
		# for revisions 38 licence link is inconsistent
		JETSON_L4T_REVISION_LONG="2.0"
		# 38.2 --> 7.0
		;&
	"38.4")
		# 38.4 --> 7.1
		PATCHES_REV="7.0"
		KERNEL_RELEASE="6.8"
		UBUNTU_CODENAME=noble
		KBASE=./Tegra/kernel/kernel-${UBUNTU_CODENAME}-src
	;;
	*)
	echo -e "\e[41mUnsupported JetPack revision ${JETSON_L4T_VERSION} aborting script\e[0m"
	exit 1;
	;;
esac
echo -e "\e[32mL4T ${JETSON_L4T_VERSION} to use patches revision ${PATCHES_REV}\e[0m"

# Get the required tools to build the patched modules
sudo apt-get install build-essential git libssl-dev curl -y

#Retrieve tegra tag version for sync, required for get and sync kernel source with Jetson:
#https://forums.developer.nvidia.com/t/r32-1-tx2-how-can-i-build-extra-module-in-the-tegra-device/72942/9
#Download kernel and peripheral sources as the L4T github repo is not self-contained to build kernel modules
sdk_dir=$(pwd)
echo -e "\e[32mCreate the sandbox - NVIDIA L4T source tree(s)\e[0m"
mkdir -p ${sdk_dir}/Tegra
TEGRA_SOURCE_SYNC_SH="sync.sh"
TEGRA_TAG="jetson_$JETSON_L4T_RELEASE.$JETSON_L4T_REVISION"

cp ./scripts/Tegra/$TEGRA_SOURCE_SYNC_SH ${sdk_dir}/Tegra
cp ./scripts/Tegra/${PATCHES_REV}.repos ${sdk_dir}/Tegra/repos

# Display NVIDIA license
DisplayNvidiaLicense "r${JETSON_L4T_RELEASE}_Release_v${JETSON_L4T_REVISION_LONG}/${RELEASE_STRING}"

# Download NVIDIA source
./Tegra/$TEGRA_SOURCE_SYNC_SH -k ${TEGRA_TAG}

pushd ${KBASE} > /dev/null

L4T_Patches_Dir=${sdk_dir}/scripts/Tegra/LRS_Patches/
if [[ ! -d ${L4T_Patches_Dir} ]]; then
	echo -e "\e[41mThe L4T kernel patches directory  ${L4T_Patches_Dir} was not found, aborting\e[0m"
	exit 3
else
	echo -e "\e[32mCopy LibRealSense patches to the sandbox\e[0m"
	cp -r ${L4T_Patches_Dir} .
fi

#Clean the kernel WS
echo -e "\e[32mPrepare workspace for kernel build\e[0m"

export ARCH=arm64

RUNNING_KERNEL=/lib/modules/$(uname -r)
if [[ -L $RUNNING_KERNEL/build ]]; then
	export KBUILD_EXTRA_SYMBOLS=$RUNNING_KERNEL/build/Module.symvers
fi

make mrproper -j$(($(nproc)-1))
if version_lt "$PATCHES_REV" "6.0"; then
	make tegra_defconfig -j$(($(nproc)-1))
else
	echo -e "\e[32mUpdate the kernel tree to support HID IMU sensors\e[0m"
	# appending config to defconfig so later .config will be generated with all necessary dependencies
	echo 'CONFIG_HID_SENSOR_HUB=m' >> arch/arm64/configs/defconfig
	echo 'CONFIG_HID_SENSOR_ACCEL_3D=m' >> arch/arm64/configs/defconfig
	echo 'CONFIG_HID_SENSOR_GYRO_3D=m' >> arch/arm64/configs/defconfig
	echo 'CONFIG_HID_SENSOR_IIO_COMMON=m' >> arch/arm64/configs/defconfig
	echo 'CONFIG_HID_SENSOR_IIO_TRIGGER=m' >> arch/arm64/configs/defconfig
	make defconfig -j$(($(nproc)-1))
fi

#JetPack prior to 4.4.1 requires manual reconfiguration of kernel
if [[ "$PATCHES_REV" = "4.4" ]]; then
	echo -e "\e[32mUpdate the kernel tree to support HID IMU sensors\e[0m"
	sed -i '/CONFIG_HID_SENSOR_ACCEL_3D/c\CONFIG_HID_SENSOR_ACCEL_3D=m' .config
	sed -i '/CONFIG_HID_SENSOR_GYRO_3D/c\CONFIG_HID_SENSOR_GYRO_3D=m' .config
	sed -i '/CONFIG_HID_SENSOR_IIO_COMMON/c\CONFIG_HID_SENSOR_IIO_COMMON=m\nCONFIG_HID_SENSOR_IIO_TRIGGER=m' .config
fi

#Remove previously applied patches
git reset --hard
echo -e "\e[32mApply LibRealSense kernel patches\e[0m"
if version_lt "${PATCHES_REV}" "6.0"; then
	patch -p1 < ./LRS_Patches/01-realsense-camera-formats-L4T-${PATCHES_REV}.patch
	patch -p1 < ./LRS_Patches/02-realsense-metadata-L4T-${PATCHES_REV}.patch
	if [[ "$PATCHES_REV" = "4.4" ]]; then # for JetPack 4.4 only
		patch -p1 < ./LRS_Patches/03-realsense-hid-L4T-4.9.patch
	fi
	if [[ "$PATCHES_REV" != "5.0" ]]; then
		patch -p1 < ./LRS_Patches/04-media-uvcvideo-mark-buffer-error-where-overflow.patch
	fi
	patch -p1 < ./LRS_Patches/05-realsense-powerlinefrequency-control-fix.patch
else
	patch -p1 < ${sdk_dir}/scripts/realsense-camera-formats-"${UBUNTU_CODENAME}"-master.patch
	[[ -f ${sdk_dir}/scripts/realsense-powerlinefrequency-control-fix-"${UBUNTU_CODENAME}".patch ]] \
		&& patch -p1 < ${sdk_dir}/scripts/realsense-powerlinefrequency-control-fix-"${UBUNTU_CODENAME}".patch
	[[ -f ${sdk_dir}/scripts/makefile-${UBUNTU_CODENAME}-${PATCHES_REV}.patch ]] \
		&& patch -p1 < ${sdk_dir}/scripts/makefile-${UBUNTU_CODENAME}-${PATCHES_REV}.patch
	sed -i s'/1.1.1/1.1.1-realsense/'g ./drivers/media/usb/uvc/uvcvideo.h
fi

#Building modules_prepare, which:
#1. Prepares kernel headers for building external modules
#2. Generates Module.symvers if it doesn’t already exist.
# Extract the local version suffix from the running kernel (e.g., "-tegra" or "")
KERNEL_LOCALVERSION="$(uname -r | sed -E 's/^[0-9]+\.[0-9]+\.[0-9]+(-[0-9]+)?//')"
echo -e "\e[32mPrepare\e[0m"
make prepare modules_prepare LOCALVERSION="${KERNEL_LOCALVERSION}" -j$(($(nproc)-1))

echo -e "\e[32mCompiling uvcvideo kernel module\e[0m"
make -j$(($(nproc)-1)) M=drivers/media/usb/uvc/ modules
echo -e "\e[32mCompiling v4l2-core modules\e[0m"
make -j$(($(nproc)-1)) M=drivers/media/v4l2-core modules

if [[ "$PATCHES_REV" = "4.4" ]]; then # for JetPack 4.4 only
	echo -e "\e[32mCompiling accelerometer and gyro modules\e[0m"
	make -j$(($(nproc)-1)) M=drivers/iio modules
fi
if version_lt "$PATCHES_REV" "6.0"; then # for JetPack 4-5
	echo -e "\e[32mCopying the patched modules to (~/) \e[0m"
	sudo cp drivers/media/usb/uvc/uvcvideo.ko ~/${TEGRA_TAG}-uvcvideo.ko
	sudo cp drivers/media/v4l2-core/videobuf-vmalloc.ko ~/${TEGRA_TAG}-videobuf-vmalloc.ko
	sudo cp drivers/media/v4l2-core/videobuf-core.ko ~/${TEGRA_TAG}-videobuf-core.ko
else
	echo -e "\e[32mCompiling hid support, accelerometer and gyro modules\e[0m"
	make -j$(($(nproc)-1)) M=drivers/hid modules
	KBUILD_EXTRA_SYMBOLS+=" drivers/hid/Module.symvers"
	make -j$(($(nproc)-1)) M=drivers/iio modules
fi
if [[ "$PATCHES_REV" = "4.4" ]]; then # for JetPack 4.4 only
	sudo cp drivers/iio/common/hid-sensors/hid-sensor-iio-common.ko ~/${TEGRA_TAG}-hid-sensor-iio-common.ko
	sudo cp drivers/iio/common/hid-sensors/hid-sensor-trigger.ko ~/${TEGRA_TAG}-hid-sensor-trigger.ko
	sudo cp drivers/iio/accel/hid-sensor-accel-3d.ko ~/${TEGRA_TAG}-hid-sensor-accel-3d.ko
	sudo cp drivers/iio/gyro/hid-sensor-gyro-3d.ko ~/${TEGRA_TAG}-hid-sensor-gyro-3d.ko
fi

if ! version_lt "$PATCHES_REV" "6.0"; then # from JetPack 6 onward
	echo -e "\e[32mCopying the patched modules to $RUNNING_KERNEL/extra/\e[0m"
	sudo mkdir -p $RUNNING_KERNEL/extra/
	# uvc modules with formats/sku support
	sudo cp drivers/media/usb/uvc/uvcvideo.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/media/v4l2-core/videodev.ko $RUNNING_KERNEL/extra/
	# iio modules for iio-hid support
	sudo cp drivers/iio/buffer/kfifo_buf.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/iio/buffer/industrialio-triggered-buffer.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/iio/common/hid-sensors/hid-sensor-iio-common.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/hid/hid-sensor-hub.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/iio/accel/hid-sensor-accel-3d.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/iio/gyro/hid-sensor-gyro-3d.ko $RUNNING_KERNEL/extra/
	sudo cp drivers/iio/common/hid-sensors/hid-sensor-trigger.ko $RUNNING_KERNEL/extra/
	# set depmod search path to include "extra" modules
	sudo sed -i 's/search updates/search extra updates/g' /etc/depmod.d/ubuntu.conf
fi
popd > /dev/null
if [[ "$PATCHES_REV" = "4.4" ]]; then # for JetPack 4.4 only
	echo -e "\e[32mMove the modified modules into the modules tree\e[0m"
	#Optional - create kernel modules directories in kernel tree
	sudo mkdir -p $RUNNING_KERNEL/kernel/drivers/iio/accel
	sudo mkdir -p $RUNNING_KERNEL/drivers/iio/gyro
	sudo mkdir -p $RUNNING_KERNEL/drivers/iio/common/hid-sensors
	sudo cp  ~/${TEGRA_TAG}-hid-sensor-accel-3d.ko     $RUNNING_KERNEL/kernel/drivers/iio/accel/hid-sensor-accel-3d.ko
	sudo cp  ~/${TEGRA_TAG}-hid-sensor-gyro-3d.ko      $RUNNING_KERNEL/kernel/drivers/iio/gyro/hid-sensor-gyro-3d.ko
	sudo cp  ~/${TEGRA_TAG}-hid-sensor-iio-common.ko   $RUNNING_KERNEL/kernel/drivers/iio/common/hid-sensors/hid-sensor-iio-common.ko
	sudo cp  ~/${TEGRA_TAG}-hid-sensor-trigger.ko      $RUNNING_KERNEL/kernel/drivers/iio/common/hid-sensors/hid-sensor-trigger.ko
fi

# update kernel module dependencies
sudo depmod

# special attention to uvcvideo because it is one of the files that is set to /lib/modules/`uname -r`/updates/ folder
# when using our jetson drivers instructions
UVCVIDEO_PATH=$(modinfo -F filename uvcvideo)
if [[ -z "$UVCVIDEO_PATH" ]]; then
    UVCVIDEO_PATH="$RUNNING_KERNEL/updates/uvcvideo.ko"
    echo -e "\e[33mCould not find the uvcvideo kernel module.\nIt will be loaded into updates folder: $UVCVIDEO_PATH.\e[0m"
fi

echo -e "\e[32mInsert the modified kernel modules\e[0m"

if version_lt "$PATCHES_REV" "6.0"; then
	try_module_insert uvcvideo              ~/${TEGRA_TAG}-uvcvideo.ko                $UVCVIDEO_PATH
	try_module_insert hid_sensor_gyro_3d    ~/${TEGRA_TAG}-hid-sensor-gyro-3d.ko      $RUNNING_KERNEL/kernel/drivers/iio/gyro/hid-sensor-gyro-3d.ko
	try_module_insert hid_sensor_accel_3d   ~/${TEGRA_TAG}-hid-sensor-accel-3d.ko     $RUNNING_KERNEL/kernel/drivers/iio/accel/hid-sensor-accel-3d.ko
else
	# for JP6.0 we will try to remove old modules and then load updated ones
	echo -e "\e[32mUnload kernel modules\e[0m"
	try_unload_module uvcvideo
	try_unload_module hid_sensor_accel_3d
	try_unload_module hid_sensor_gyro_3d
	try_unload_module hid_sensor_trigger
	try_unload_module industrialio_triggered_buffer
	try_unload_module kfifo_buf
	try_unload_module videodev
	echo -e "\e[32mLoad modified kernel modules\e[0m"
	try_load_module videodev
	try_load_module kfifo_buf
	try_load_module industrialio_triggered_buffer
	try_load_module hid_sensor_trigger
	try_load_module hid_sensor_gyro_3d
	try_load_module hid_sensor_accel_3d
	try_load_module uvcvideo
fi
echo -e "\e[32mDone\e[0m"
echo -e "\e[92m\n\e[1mScript has completed. Please consult the installation guide for further instruction.\n\e[0m"
