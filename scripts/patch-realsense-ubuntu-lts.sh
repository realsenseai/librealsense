#!/bin/bash

#Break execution on any error received
set -e

#Locally suppress stderr to avoid raising not relevant messages
exec 3>&2
exec 2> /dev/null
con_dev=$(ls /dev/video* | wc -l)
exec 2>&3

#Parse user inputs
reset_driver=0
xhci_patch=0
build_usbcore_modules=0
rebuild_ko=0
debug_uvc=0

#Parse input
while test $# -gt 0; do
  case "$1" in
  --help)
		;&
	-h)
		echo "Parse user-provided flags"
		echo "[Flag]                  [Operation]"
		echo " "
		echo "options:"
		echo "-h, --help              Show script activation flags"
		echo "reset                   Remove custom kernel patches and rebuild ko with Ubuntu stock driver"
		echo "xhci-patch              Rebuild XHCI host controller modules to retrofit kernel fixes. Applicable for Kernel 4.4 only"
		echo "build_usbcore_modules   Rebuilds usbcore/usbhost kernel modules"
		echo "ko                      Rebuild modified kernel modules w/o modifying the source tree"
		echo "uvc_dbg                 Add profyling messages to uvcvideo and videobuf2 modules"
		exit 0
		;;
	reset)
		reset_driver=1; shift;;
	build_usbcore_modules)
		build_usbcore_modules=1 ;& # fallthrough: usbcore implies xhci
	xhci-patch)
		xhci_patch=1; shift ;;
	ko)
		rebuild_ko=1; shift ;;
	uvc_dbg)
		debug_uvc=1;  shift ;;
	*)
		echo -e "\e[36mUnrecognized flag:  $1\e[0m"; shift;;
  esac
done

if [ $con_dev -ne 0 ];
then
	echo -e "\e[32m"
	read -p "Remove all RealSense cameras attached. Hit any key when ready"
	echo -e "\e[0m"
fi

#Include usability functions
source ./scripts/patch-utils.sh

# Get the required tools and headers to build the kernel
sudo apt-get install linux-headers-generic build-essential git bc -y
#Packages to build the patched modules
require_package libusb-1.0-0-dev
require_package libssl-dev

LINUX_BRANCH=$(uname -r)
#Get kernel major.minor
IFS='.' read -a kernel_version <<< ${LINUX_BRANCH}
k_maj_min=$((${kernel_version[0]}*100 + ${kernel_version[1]}))
if [[ ( ${xhci_patch} -eq 1 ) && ( ${k_maj_min} -ne 404 ) ]]; then
	echo -e "\e[43mThe xhci_patch flag is compatible with LTS branch 4.4 only, currently selected kernel is $(uname -r)\e[0m"
	exit 1
fi

# Construct branch name from distribution codename {focal, jammy...} and kernel version
ubuntu_codename=`. /etc/os-release; echo ${UBUNTU_CODENAME/*, /}`

kernel_branch=$(choose_kernel_branch ${LINUX_BRANCH} ${ubuntu_codename})
kernel_name="ubuntu-${ubuntu_codename}-$kernel_branch"
echo -e "\e[32mCreate patches workspace in \e[93m${kernel_name} \e[32mfolder\n\e[0m"

#Distribution-specific packages
if [ "${ubuntu_codename}" == "focal" ]; 
then
	require_package libelf-dev
	require_package elfutils
	#Ubuntu 18.04 kernel 4.18 + 20.04/ 5.4
	require_package bison
	require_package flex
fi


# Get the linux kernel and change into source tree
if [ ! -d ${kernel_name} ]; then
	mkdir ${kernel_name}
	cd ${kernel_name}
	git init
	git remote add origin https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/${ubuntu_codename}
	cd ..
fi

cd ${kernel_name}

if [ $rebuild_ko -eq 0 ];
then
	#Search the repository for the tag that matches the mmaj.min.patch-build of Ubuntu kernel
	kernel_full_num=$(echo $LINUX_BRANCH | cut -d '-' -f 1,2)
	kernel_git_tag=$(git ls-remote --tags origin | grep "${kernel_full_num}\." | grep '[^^{}]$' | tail -n 1 | awk -F/ '{print $NF}')
	echo -e "\e[32mFetching Ubuntu LTS tag \e[47m${kernel_git_tag}\e[0m \e[32m to the local kernel sources folder\e[0m"
	git fetch origin tag ${kernel_git_tag} --no-tags --depth 1

	# Verify that there are no trailing changes., warn the user to make corrective action if needed
	if [ $(git status | grep 'modified:' | wc -l) -ne 0 ];
	then
		echo -e "\e[36mThe kernel has modified files:\e[0m"
		git status | grep 'modified:'
		echo -e "\e[36mProceeding will reset all local kernel changes. Press 'n' within 3 seconds to abort the operation"
		set +e
		read -n 1 -t 3 -r -p "Do you want to proceed? [Y/n]" response
		set -e
		response=${response,,}    # tolower
		if [[ $response =~ ^(n|N)$ ]];
		then
			echo -e "\e[41mScript has been aborted on user requiest. Please resolve the modified files are rerun\e[0m"
			exit 1
		else
			echo -e "\e[0m"
			printf "Resetting local changes in %s folder\n " ${kernel_name}
			git reset --hard
		fi
	fi

	echo -e "\e[32mSwitching to LTS tag ${kernel_git_tag}\e[0m"
	git checkout ${kernel_git_tag}


	if [ $reset_driver -eq 1 ];
	then
		echo -e "\e[43mUser requested to rebuild and reinstall ubuntu-${ubuntu_codename} stock drivers\e[0m"
	else
		# Patching kernel for RealSense devices
		echo -e "\e[32mApplying patches for \e[36m${ubuntu_codename}-${kernel_branch}\e[32m line\e[0m"
		echo -e "\e[32mApplying realsense-uvc patch\e[0m"
		patch -p1 < ../scripts/realsense-camera-formats-${ubuntu_codename}-${kernel_branch}.patch || patch -p1 < ../scripts/realsense-camera-formats-${ubuntu_codename}-master.patch
		echo -e "\e[32mApplying realsense-metadata patch\e[0m"
		patch -p1 < ../scripts/realsense-metadata-${ubuntu_codename}-${kernel_branch}.patch || patch -p1 < ../scripts/realsense-metadata-${ubuntu_codename}-master.patch
		echo -e "\e[32mApplying realsense-hid patch\e[0m"
		patch -p1 < ../scripts/realsense-hid-${ubuntu_codename}-${kernel_branch}.patch || patch -p1 < ../scripts/realsense-hid-${ubuntu_codename}-master.patch
		echo -e "\e[32mApplying realsense-powerlinefrequency-fix patch\e[0m"
		patch -p1 < ../scripts/realsense-powerlinefrequency-control-fix.patch
		# Applying 3rd-party patch that affects USB2 behavior
		# See reference https://patchwork.kernel.org/patch/9907707/
		if [ ${k_maj_min} -lt 418 ];
		then
			echo -e "\e[32mRetrofit UVC bug fix rectified in 4.18+\e[0m"
			if patch -N --dry-run -p1 < ../scripts/v1-media-uvcvideo-mark-buffer-error-where-overflow.patch; then
				patch -N -p1 < ../scripts/v1-media-uvcvideo-mark-buffer-error-where-overflow.patch
			else
				echo -e "\e[36m  Skip the patch - it is already found in the source tree\e[0m"
			fi
		fi
		if [ $xhci_patch -eq 1 ]; then
			echo -e "\e[32mApplying streamoff hotfix patch in videobuf2-core\e[0m"
			patch -p1 < ../scripts/01-Backport-streamoff-vb2-core-hotfix.patch
			echo -e "\e[32mApplying 01-xhci-Add-helper-to-get-hardware-dequeue-pointer-for patch\e[0m"
			patch -p1 < ../scripts/01-xhci-Add-helper-to-get-hardware-dequeue-pointer-for.patch
			echo -e "\e[32mApplying 02-xhci-Add-stream-id-to-to-xhci_dequeue_state-structur patch\e[0m"
			patch -p1 < ../scripts/02-xhci-Add-stream-id-to-to-xhci_dequeue_state-structur.patch
			echo -e "\e[32mApplying 03-xhci-Find-out-where-an-endpoint-or-stream-stopped-fr patch\e[0m"
			patch -p1 < ../scripts/03-xhci-Find-out-where-an-endpoint-or-stream-stopped-fr.patch
			echo -e "\e[32mApplying 04-xhci-remove-unused-stopped_td-pointer patch\e[0m"
			patch -p1 < ../scripts/04-xhci-remove-unused-stopped_td-pointer.patch
		fi
		echo -e "\e[32mIncrease UVC_URBs in uvcvideo\e[0m"
		patch -p1 < ../scripts/uvcvideo_increase_UVC_URBS.patch
		if [ $debug_uvc -eq 1 ]; then
			echo -e "\e[32mApplying uvcvideo and videobuf2 debug patch\e[0m"
			patch -p1 < ../scripts/uvc_debug.patch
		fi
	fi

	#Copy configuration
	sudo cp /usr/src/linux-headers-$(uname -r)/.config .
	sudo cp /usr/src/linux-headers-$(uname -r)/Module.symvers .

	# Basic build for kernel modules
	echo -e "\e[32mPrepare kernel modules configuration\e[0m"
	#Retpoline script manual retrieval. based on https://github.com/realsenseai/librealsense/issues/1493
	#Required since the retpoline patches were introduced into Ubuntu kernels
	if [ ! -f scripts/ubuntu-retpoline-extract-one ]; then
		pwd
		for f in $(find . -name 'retpoline-extract-one'); do cp ${f} scripts/ubuntu-retpoline-extract-one; done;
		echo $$$
	fi

	#Reuse current kernel configuration. Assign default values to newly-introduced options.
	sudo make olddefconfig modules_prepare
	#Replacement of  USBcore modules implies usbcore is modular
	[ ${build_usbcore_modules} -eq 1 ] && sudo make menuconfig modules_prepare

	#Vermagic identity is required
	sudo sed -i "s/\".*\"/\"$LINUX_BRANCH\"/g" ./include/generated/utsrelease.h
	sudo sed -i "s/.*/$LINUX_BRANCH/g" ./include/config/kernel.release


	if [[ ( $xhci_patch -eq 1 ) && ( $build_usbcore_modules -eq 0 ) ]]; then
		sudo make -j$(($(nproc)-1))
		sudo make modules -j$(($(nproc)-1))
		sudo make modules_install -j$(($(nproc)-1))
		sudo make install
		echo -e "\e[92m\n\e[1m`sudo make kernelrelease` Kernel has been successfully installed."
		echo -e "\e[92m\n\e[1mScript has completed. Please reboot and load the newly installed Kernel from GRUB list.\n\e[0m"
	exit 0
	fi
fi # rebuild_ko==0

# Build the uvc, accel and gyro modules
KBASE=`pwd`
cd drivers/media/usb/uvc
sudo cp $KBASE/Module.symvers .

echo -e "\e[32mCompiling uvc module\e[0m"
sudo make -j -C $KBASE M=$KBASE/drivers/media/usb/uvc/ modules
echo -e "\e[32mCompiling accelerometer and gyro modules\e[0m"
sudo make -j -C $KBASE M=$KBASE/drivers/iio/accel modules
sudo make -j -C $KBASE M=$KBASE/drivers/iio/gyro modules
echo -e "\e[32mCompiling v4l2-core modules\e[0m"
sudo make -j -C $KBASE M=$KBASE/drivers/media/v4l2-core modules
if [[ ( $xhci_patch -eq 1 ) || ( $debug_uvc -eq 1 ) ]]; then
	echo -e "\e[32mCompiling media/common modules\e[0m"
	sudo make -j -C $KBASE M=$KBASE/drivers/media/common/videobuf2 modules
fi

# Copy the patched modules to a  location
sudo cp $KBASE/drivers/media/usb/uvc/uvcvideo.ko ~/$LINUX_BRANCH-uvcvideo.ko
sudo cp $KBASE/drivers/iio/accel/hid-sensor-accel-3d.ko ~/$LINUX_BRANCH-hid-sensor-accel-3d.ko
sudo cp $KBASE/drivers/iio/gyro/hid-sensor-gyro-3d.ko ~/$LINUX_BRANCH-hid-sensor-gyro-3d.ko
sudo cp $KBASE/drivers/media/v4l2-core/videodev.ko ~/$LINUX_BRANCH-videodev.ko
if [[ ( $xhci_patch -eq 1 ) || ( $debug_uvc -eq 1 ) ]]; then
	sudo cp $KBASE/drivers/media/common/videobuf2/videobuf2-common.ko ~/$LINUX_BRANCH-videobuf2-common.ko
	sudo cp $KBASE/drivers/media/common/videobuf2/videobuf2-v4l2.ko ~/$LINUX_BRANCH-videobuf2-v4l2.ko
fi

if [ $build_usbcore_modules -eq 1 ]; then
	sudo make -j -C $KBASE M=$KBASE/drivers/usb/core modules
	sudo make -j -C $KBASE M=$KBASE/drivers/usb/host modules
	sudo make -j -C $KBASE M=$KBASE/drivers/hid/usbhid modules
	sudo cp $KBASE/drivers/media/v4l2-core/videobuf2-v4l2.ko ~/$LINUX_BRANCH-videobuf2-v4l2.ko
	sudo cp $KBASE/drivers/media/v4l2-core/videobuf2-core.ko ~/$LINUX_BRANCH-videobuf2-core.ko
	sudo cp $KBASE/drivers/media/v4l2-core/v4l2-common.ko ~/$LINUX_BRANCH-v4l2-common.ko
	sudo cp $KBASE/drivers/usb/core/usbcore.ko ~/$LINUX_BRANCH-usbcore.ko
	sudo cp $KBASE/drivers/usb/host/ehci-hcd.ko ~/$LINUX_BRANCH-ehci-hcd.ko
	sudo cp $KBASE/drivers/usb/host/ehci-pci.ko ~/$LINUX_BRANCH-ehci-pci.ko
	sudo cp $KBASE/drivers/usb/host/xhci-hcd.ko ~/$LINUX_BRANCH-xhci-hcd.ko
	sudo cp $KBASE/drivers/usb/host/xhci-pci.ko ~/$LINUX_BRANCH-xhci-pci.ko
	sudo cp $KBASE/drivers/hid/usbhid/usbhid.ko ~/$LINUX_BRANCH-usbhid.ko
fi

echo -e "\e[32mPatched kernels modules were created successfully\n\e[0m"

# Load the newly-built modules
# As a precausion start with unloading the core uvcvideo:
try_unload_module uvcvideo
try_unload_module videobuf2_v4l2
[ ${k_maj_min} -ge 500 ] && try_unload_module videobuf2_common
try_unload_module videodev

if [ $build_usbcore_modules -eq 1 ]; then
	echo -e "\e[32mReplacing USB Core modules\e[0m"

	try_unload_module videobuf2_core
	try_unload_module v4l2_common

	#Replace usb subsystem modules
	try_unload_module usbhid
	try_unload_module xhci-pci
	try_unload_module xhci-hcd
	try_unload_module ehci-pci
	try_unload_module ehci-hcd

	try_module_insert usbcore				~/$LINUX_BRANCH-usbcore.ko 				/lib/modules/`uname -r`/kernel/drivers/usb/core/usbcore.ko
	try_module_insert ehci-hcd				~/$LINUX_BRANCH-ehci-hcd.ko 			/lib/modules/`uname -r`/kernel/drivers/usb/host/ehci-hcd.ko
	try_module_insert ehci-pci				~/$LINUX_BRANCH-ehci-pci.ko 			/lib/modules/`uname -r`/kernel/drivers/usb/host/ehci-pci.ko
	try_module_insert xhci-hcd				~/$LINUX_BRANCH-xhci-hcd.ko 			/lib/modules/`uname -r`/kernel/drivers/usb/host/xhci-hcd.ko
	try_module_insert xhci-pci				~/$LINUX_BRANCH-xhci-pci.ko 			/lib/modules/`uname -r`/kernel/drivers/usb/host/xhci-pci.ko

	try_module_insert videodev				~/$LINUX_BRANCH-videodev.ko 			/lib/modules/`uname -r`/kernel/drivers/media/v4l2-core/videodev.ko
	try_module_insert v4l2-common			~/$LINUX_BRANCH-v4l2-common.ko			/lib/modules/`uname -r`/kernel/drivers/media/v4l2-core/v4l2-common.ko

	try_module_insert videobuf2_core		~/$LINUX_BRANCH-videobuf2-core.ko		/lib/modules/`uname -r`/kernel/drivers/media/v4l2-core/videobuf2-core.ko
	try_module_insert videobuf2_v4l2		~/$LINUX_BRANCH-videobuf2-v4l2.ko		/lib/modules/`uname -r`/kernel/drivers/media/v4l2-core/videobuf2-v4l2.ko
fi

try_module_insert videodev				~/$LINUX_BRANCH-videodev.ko 			/lib/modules/`uname -r`/kernel/drivers/media/v4l2-core/videodev.ko
if [[ ( ${k_maj_min} -ge 500 ) && ( $debug_uvc -eq 1 ) ]]; then
	try_module_insert videobuf2-common		~/$LINUX_BRANCH-videobuf2-common.ko 	/lib/modules/`uname -r`/kernel/drivers/media/common/videobuf2/videobuf2-common.ko
fi
try_module_insert uvcvideo				~/$LINUX_BRANCH-uvcvideo.ko 			/lib/modules/`uname -r`/kernel/drivers/media/usb/uvc/uvcvideo.ko
try_module_insert hid_sensor_accel_3d 	~/$LINUX_BRANCH-hid-sensor-accel-3d.ko 	/lib/modules/`uname -r`/kernel/drivers/iio/accel/hid-sensor-accel-3d.ko
try_module_insert hid_sensor_gyro_3d	~/$LINUX_BRANCH-hid-sensor-gyro-3d.ko 	/lib/modules/`uname -r`/kernel/drivers/iio/gyro/hid-sensor-gyro-3d.ko


echo -e "\e[92m\n\e[1mScript has completed. Please consult the installation guide for further instruction.\n\e[0m"
