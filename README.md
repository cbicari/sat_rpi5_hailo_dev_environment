# Manual Setup of rPi5-Hailo8 Development Environment 

In this manual you will find the steps to compile Hailo's PCIe driver, C++ headers, Python and basic Gstreamer bindings (Tappas (advanced Gstreamer pipelines for Hailo apps) not included). There are also some examples to test the proper installation of the software ressources and system setup such as a basic Python, Gstreamer, and Cpp example with compilation ressources for the latter. To facilitate testing and pull hef models as well as test code, clone the current repo :
```bash
git clone https://github.com/cbicari/sat_rpi5_hailo_dev_environment.git
```

> [!NOTE]
> #### Simplicity's Sake
> If ever you want the easiest and fastest install updated by rPi staff, use the following command :
> 
> ```bash
> sudo apt install hailo-all 
> ```
>
> You will not have headers and all detailed ressources handy but the PCIe driver (hailofw), Hailo Runtime binairies (hailort), advanced Gstreamer ressources with Tappas utility pipeline (hailo-tappas-core), python bindings and rpicam-apps-postprocessing will work "out of the box" and take less than five minutes. 
> At the time of writing this (11-2025), this package only works on rPi `bookworm` and not the most recent `trixie` distribution. 
> 
> To view which versions are installed with this package, run the following command : 
> 
> ```bash
> apt show hailo-all
> ```

## Steps to configuring environment 

Steps and information given here should be sufficient to help you get through. For more detailed explanations, please refer to the most recent user manuals provided by [Hailo](https://hailo.ai/). You will need to create an account to access developper ressources.


### 1. Ubuntu requirements : 

You may install these dependencies with `apt install`.

Ubuntu installer requirements :

- build-essential (for compiling PCIe driver)
- (optional) bison, flex, libelf-dev, dkms (needed to register PCIe driver using DKMS)
- (optional) curl (needed to download firmware when installing without PCIe driver)
- (optional) cmake (needed for compiling HailoRT examples)
- (optional) pip, virtualenv (needed for pyhailort)
- (optional) systemd (needed for Multi-Process service)


### 2. Build Hailo PCIe driver

Clone PCIe repository, checkout to proper branch and make binairies.
##### N.B. Normally, the `hailo8` branch should correspond to the latest version available for the rPi5-hailo8 system. If any issues occur, refer to the [Hailo README](https://github.com/hailo-ai/hailort-drivers/tree/hailo8/linux/pcie) or [User Manual](https://hailo.ai/).
```bash
git clone https://github.com/hailo-ai/hailort-drivers/
cd hailort-drivers/
git checkout hailo8
# Compile driver locally. Compiled binary will be hailo_pci.ko
cd linux/pcie
make all
# Install driver in /lib/modules
sudo make install
```
> [!TIP]
> If you get the following warning message and compilation fails :
>  
> ```bash
> Warning: modules_install: missing 'System.map' file. Skipping depmod.
> ```
>
> Run the following command to create symbolic link to your System.map :
>
> ```bash
> sudo ln -s /boot/System.map-$(uname -r) /usr/src/linux-headers-$(uname -r)/System.map
> ```

Follow with these commands : 

```bash
# Load driver once, after driver will be loaded on boot
sudo modprobe hailo_pci

cd ./../../
./download_firmware.sh
sudo mkdir -p /lib/firmware/hailo
# <VERSION> refers to download firmware from previous step
sudo mv hailo8_fw.<VERSION>.bin /lib/firmware/hailo/hailo8_fw.bin

# Update device manager
sudo cp ./linux/pcie/51-hailo-udev.rules /etc/udev/rules.d/
# Copy configuration file to /etc/modprobe.d/
sudo cp ./linux/pcie/hailo_pci.conf /etc/modprobe.d/

# To apply changes, reboot or run :
sudo udevadm control --reload-rules && sudo udevadm trigger
```

In order to prevent the following error when running hef models :
```bash
[HailoRT] [error] CHECK failed - max_desc_page_size given 16384 is bigger than hw max desc page size 4096
```

Modify or add the value of the max_desc_page_size as follows : 
```bash 
# Use your preferred terminal text editor (nano, vim, ..)
sudo vim /etc/modprobe.d/hailo_fw.conf
# Make sure the max DMA descriptor page size is of 4096. Currently rPi5 has this data transfer limitation. This value needs to be set on platforms that do not support large PCIe transactions. 
options hailo_pci force_desc_page_size=4096

# update kernel module 
sudo depmod -a
sudo modprobe -r hailo_pci
sudo modprobe  hailo_pci
dmesg | grep hailo
```


### 3. Set PCIe to Gen3
To achieve optimal performance from the Hailo device, it is necessary to set PCIe to Gen3. While using Gen2 is an option, it will result in lower performance. Open the Raspberry Pi configuration tool:

```bash
sudo raspi-config
```
Select option "6 Advanced Options", then select option "A8 PCIe Speed". Choose "Yes" to enable PCIe Gen 3 mode. Click "Finish" to exit. Then reboot your system. 


### 4. Build HailoRT C++ binaries
Return to the parent directory of previous driver ressources and run the following to get and compile proper Hailo Runtime binairies : 
```bash
git clone https://github.com/hailo-ai/hailort.git
cd hailort/
git checkout hailo8 
# next compilation will create "build/hailort/hailortcli/hailortcli" and "build/hailort/libhailort/src/libhailort.so.<VERSION> library.
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release && cmake --build build --config release
# run install script to manage system install of lib and binary
sudo cmake --build build --config release --target install 
```

### 5. Verify PCIe Driver and Library
If all is well configured after these compilation and installation scripts, attest by running :
```bash
hailortcli fw-control identify
```
You should get something along the lines of :
```bash
Executing on device: 0001:01:00.0
Identifying board
Control Protocol Version: 2
Firmware Version: 4.23.0 (release,app,extended context switch buffer)
Logger Version: 0
Board Name: Hailo-8
Device Architecture: HAILO8
```
Using the hef_models from this configuration repo, test run the `pose_landmark_heavy.hef` as follows :
```bash
 hailortcli run ./hef_models/pose_landmark_heavy.hef
 ```
 You should get something along the lines of :
 
 ```bash
 Running streaming inference (./hef_models/pose_landmark_heavy.hef):
  Transform data: true
    Type:      auto
    Quantized: true
Network pose_landmark_heavy/pose_landmark_heavy: 100% | 245 | FPS: 48.69 | ETA: 00:00:00
> Inference result:
 Network group: pose_landmark_heavy
    Frames count: 245
    FPS: 48.69
    Send Rate: 79.59 Mbit/s
    Recv Rate: 0.08 Mbit/s
```

### 6. Run C++ program with segmentation model to generate black and white mask

Make sure you have a camera plugged into your rpi5. We used a USB webcam for these purposes to facilitate the fetching of video stream with /dev/video0. When we tried with the MIPI-CSI camera port and the libcamera/rpicam lib distributed with current rpi-OS systems, we would have issues facilitating the video retrieval pipeline. 

To trial the cpp example, follow the nexts steps. From the root of this repo, move into the cpp examples - instance segmentation. This program takes in the video feed, segments recognized humans and gives a white on black mask. This code is only an example and you may experience buffering latency as the implementation is not optimized for production purposes.

```bash
cd ./cpp_example/instance_segmentation/

# Build program
./build.sh

# Run the program --adapt the video input to your system's feeds
./build/instance_segmentation_cpp --net ./../../hef_models/yolov5m-seg.hef --input /dev/video0

```




### 7. Optional compilation of Python and basic Gstreamer bindings 

#### 7.1 HailoRT-Python Bindings 

Move to the following directory and build the bdist wheel :
```bash
cd hailort/libhailort/bindings/python/platform
python setup.py bdist_wheel
```
Build wheel should appear in hailort/libhailort/bindings/python/platform/dist/ and should have a nomenclature similar to :  
`hailort-<version>-cp<python-version[38,39,310,311]>-cp<python-version[38,39,310,311]>-linux_aarch54.whl`

Move to root of directory and create virtual environment before running the build wheel: 
```bash
pip install <hailort-<version>-cp<python-version[38,39,310,311]>-cp<python-version[38,39,310,311]>-linux_aarch54.whl>
```
Test your bindings by moving into the proper directory and running the following python script, then verifying the outputted log:
```bash
cd python_binding_test_scripts/
python async_inference_ResNet.py
# wait for end of script then view output log, you should see the creation of the network, the HEF configuration time, and such..
cat hailort.log
```

#### 7.2 Basic Gstreamer Bindings 

If you must use Gstreamer ressources, you may have way much ease using the `apt install hailo-all` approach mentionned at the beginning of this document as you will have the whole Tappas-core environment which provides functional Gstreamer pipelines. I leave the following steps here ofr those who might need this step by step approach for other development purposes but this whole step is optional if you focus on Cpp development.

```bash
# Install Gstreamer and dependencies 
sudo apt update
sudo apt install -y \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav

# Verify Gstreamer 
# Version
gst-inspect-1.0 --version
# Simple test script
gst-launch-1.0 fakesrc num-buffers=5 ! fakesink
```
You should obtain something along the lines of : 

```bash
# Version
gst-inspect-1.0 version 1.22.0
GStreamer 1.22.0
https://tracker.debian.org/pkg/gstreamer1.0

# Simple test script
Setting pipeline to PAUSED ...
Pipeline is PREROLLING ...
Pipeline is PREROLLED ...
Setting pipeline to PLAYING ...
Redistribute latency...
New clock: GstSystemClock
Got EOS from element "pipeline0".
Execution ended after 0:00:00.000140945
Setting pipeline to NULL ...
Freeing pipeline ...
```
Build the bindings : 
```bash
cd hailort/libhailort/bindings/gstreamer
# Compiling the plugin:
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config release
```

#### Register the plugin

```bash
sudo cp build/libgsthailo.so /usr/lib/aarch64-linux-gnu/gstreamer-1.0/
# maybe this one though :   /usr/lib/arm-linux-gnueabihf/gstreamer-1.0/

# refresh plugin cache and verify plugin details 
gst-inspect-1.0 hailo
```
You should get something along the lines of :
```bash
Plugin Details:
  Name                     hailo
  Description              hailo gstreamer plugin
  Filename                 /lib/aarch64-linux-gnu/gstreamer-1.0/libgsthailo.so
  Version                  1.0
  License                  unknown
  Source module            hailo
  Binary package           GStreamer
  Origin URL               http://gstreamer.net/

  hailodevicestats: hailodevicestats element
  hailonet: hailonet element
  synchailonet: sync hailonet element

  3 features:
  +-- 3 elements
  ```

#### Gstreamer Tests : Run inference on still image
From root directory of this whole repository, run : 
```bash
gst-launch-1.0 filesrc location=./media/ComfyUI_00269_.png ! pngdec ! videoconvert ! videoscale ! video/x-raw,format=RGB,width=640,height=640 ! hailonet hef-path=./hef_models/yolov8x.hef ! autovideosink
```
You should get something along the lines of :
```bash
Setting pipeline to PAUSED ...
Pipeline is PREROLLING ...
Pipeline is PREROLLED ...
Setting pipeline to PLAYING ...
Redistribute latency...
New clock: GstSystemClock
Got EOS from element "pipeline0".
Execution ended after 0:00:00.000267500
Setting pipeline to NULL ...
Freeing pipeline ...
```






