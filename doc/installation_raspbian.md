# Raspbian(RaspberryPi3) Installation
This is a setup guide for Realsense with RaspberryPi

These steps are for D435 with Raspberry Pi3 and 3+.  

### Check versions
```
$ uname -a
Linux raspberrypi 4.14.34-v7+ 

$ sudo apt update;sudo apt upgrade
$ sudo reboot
$ uname -a


$ gcc -v
gcc version 6.3.0 20170516 (Raspbian 6.3.0-18+rpi1+deb9u1)

$ cmake --version
cmake version 3.7.2
```

### Add swap
Initial value is 100MB, but we need to build libraries so initial value isn't enough for that.
In this case, need to switch from 100 to `2048` (2GB). Note that this is only needed on Raspberry Pi's with a small amount of RAM. 
```
$ sudo vim /etc/dphys-swapfile
CONF_SWAPSIZE=2048

$ sudo /etc/init.d/dphys-swapfile restart swapon -s
```

### Install packages
```
$ sudo apt-get install -y libdrm-amdgpu1 libdrm-amdgpu1-dbgsym libdrm-dev libdrm-exynos1 libdrm-exynos1-dbgsym libdrm-freedreno1 libdrm-freedreno1-dbgsym libdrm-nouveau2 libdrm-nouveau2-dbgsym libdrm-omap1 libdrm-omap1-dbgsym libdrm-radeon1 libdrm-radeon1-dbgsym libdrm-tegra0 libdrm-tegra0-dbgsym libdrm2 libdrm2-dbgsym

$ sudo apt-get install -y libglu1-mesa libglu1-mesa-dev glusterfs-common libglu1-mesa libglu1-mesa-dev libglui-dev libglui2c2

$ sudo apt-get install -y libglu1-mesa libglu1-mesa-dev mesa-utils mesa-utils-extra xorg-dev libgtk-3-dev libusb-1.0-0-dev
```

### update udev rule
Now we need to get librealsense from the repo(https://github.com/realsenseai/librealsense).
```
$ cd ~
$ git clone https://github.com/realsenseai/librealsense.git
$ cd librealsense
$ sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/ 
$ sudo udevadm control --reload-rules && sudo udevadm trigger 

```

### update `cmake` version (if your cmake is 3.7.2 or before 3.11.4)
If you don't use zsh, please switch `.zshrc` to `.bashrc`.
```
$ cd ~
$ wget https://cmake.org/files/v3.11/cmake-3.11.4.tar.gz
$ tar -zxvf cmake-3.11.4.tar.gz;rm cmake-3.11.4.tar.gz
$ cd cmake-3.11.4
$ ./configure --prefix=/home/pi/cmake-3.11.4
$ make -j1
$ sudo make install
$ export PATH=/home/pi/cmake-3.11.4/bin:$PATH
$ source ~/.zshrc
$ cmake --version
cmake version 3.11.4
```

### set path
```
$ vim ~/.zshrc
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

$ source ~/.zshrc

```

### install `protobuf`
```
$ cd ~
$ git clone --depth=1 -b v3.5.1 https://github.com/google/protobuf.git
$ cd protobuf
$ ./autogen.sh
$ ./configure
$ make -j1
$ sudo make install
$ cd python
$ export LD_LIBRARY_PATH=../src/.libs
$ python3 setup.py build --cpp_implementation 
$ python3 setup.py test --cpp_implementation
$ sudo python3 setup.py install --cpp_implementation
$ export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=cpp
$ export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION_VERSION=3
$ sudo ldconfig
$ protoc --version
```

### install `TBB`
```
$ cd ~
$ wget https://github.com/PINTO0309/TBBonARMv7/raw/master/libtbb-dev_2018U2_armhf.deb
$ sudo dpkg -i ~/libtbb-dev_2018U2_armhf.deb
$ sudo ldconfig
$ rm libtbb-dev_2018U2_armhf.deb
```

### install `OpenCV`
You can build from source code, but it takes so much time. In this case, we will use pre-build version to save time.
```
Remove previous version
$ sudo apt autoremove libopencv3

Install 
$ wget https://github.com/mt08xx/files/raw/master/opencv-rpi/libopencv3_3.4.3-20180907.1_armhf.deb
$ sudo apt install -y ./libopencv3_3.4.3-20180907.1_armhf.deb
$ sudo ldconfig
```

### install `RealSense` SDK/librealsense
```
$ cd ~/librealsense
$ mkdir  build  && cd build
$ cmake .. -DBUILD_EXAMPLES=true -DCMAKE_BUILD_TYPE=Release -DFORCE_LIBUVC=true
$ make -j1
$ sudo make install
```

### install `pyrealsense2`
```
$ cd ~/librealsense/build

for python2
$ cmake .. -DBUILD_PYTHON_BINDINGS=bool:true -DPYTHON_EXECUTABLE=$(which python)

for python3
$ cmake .. -DBUILD_PYTHON_BINDINGS=bool:true -DPYTHON_EXECUTABLE=$(which python3)

$ make -j1
$ sudo make install

add python path
$ vim ~/.zshrc
export PYTHONPATH=$PYTHONPATH:/usr/local/lib

$ source ~/.zshrc

```

### change `pi` settings (enable OpenGL)
```
$ sudo apt-get install python-opengl
$ sudo -H pip3 install pyopengl
$ sudo -H pip3 install pyopengl_accelerate
$ sudo raspi-config
"7.Advanced Options" - "A7 GL Driver" - "G2 GL (Fake KMS)"
```

Finally, need to reboot pi
```
$ sudo reboot
```


### Try RealSense D435
Connected D435 to the pi and open terminal
```
$ realsense-viewer
```
