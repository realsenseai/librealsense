#!/bin/bash -xe

#Locally suppress stderr to avoid raising not relevant messages
exec 3>&2
exec 2> /dev/null
con_dev=$(ls /dev/video* | wc -l)
exec 2>&3

if [ $con_dev -ne 0 ];
then
	echo -e "\e[32m"
	read -p "Remove all RealSense cameras attached. Hit any key when ready"
	echo -e "\e[0m"
fi

lsb_release -a
echo "Kernel version $(uname -r)"
sudo apt-get update
cd ~/
sudo rm -rf ./librealsense_build
mkdir librealsense_build && cd librealsense_build

if [ $(sudo swapon --show | wc -l) -eq 0 ];
then
	echo "No swapon - setting up 1Gb swap file"
	sudo fallocate -l 2G /swapfile
	sudo chmod 600 /swapfile
	sudo mkswap /swapfile
	sudo swapon /swapfile
	sudo swapon --show
fi

echo Installing Librealsense-required dev packages
sudo apt-get install git cmake libssl-dev freeglut3-dev libusb-1.0-0-dev pkg-config libgtk-3-dev unzip -y
rm -f ./master.zip

wget https://github.com/realsenseai/librealsense/archive/master.zip
unzip ./master.zip -d .
cd ./librealsense-master

echo Install udev-rules
sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/ 
sudo cp config/99-realsense-d4xx-mipi-dfu.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger 
mkdir build && cd build
cmake ../ -DFORCE_LIBUVC=true -DCMAKE_BUILD_TYPE=release
make -j2
sudo make install
echo -e "\e[92m\n\e[1mLibrealsense script completed.\n\e[0m"




