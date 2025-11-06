# Hailo-rPi5 dev environment setup 

## Steps to configuring environment 

Please refer to manual in repository for more detailed explanations or get the most recent version by creating an account with Hailo and access their software ressources.



### 1. Ubuntu requirements : 

Ubuntu installer requirements :

- build-essential (for compiling PCIe driver)
- (optional) bison, flex, libelf-dev, dkms (needed to register PCIe driver using DKMS)
- (optional) curl (needed to download firmware when installing without PCIe driver)
- (optional) cmake (needed for compiling HailoRT examples)
- (optional) pip, virtualenv (needed for pyhailort)
- (optional) systemd (needed for Multi-Process service)


### 2. Build HailoRT PCIe driver

Clone PCIe repository and checkout to proper branch (`hailo8` for our purposes)
If any issues, refer to [Hailo documentation on driver installation](https://github.com/hailo-ai/hailort-drivers/tree/hailo8/linux/pcie).
```bash
git clone https://github.com/hailo-ai/hailort-drivers/
cd hailort-drivers/
git checkout hailo8
# Compile driver locally. Compiled binary will be hailo_pci.ko
cd linux/pcie
make all
# Install driver in /lib/modules
sudo make install
# If you get a warning message: Warning: modules_install: missing 'System.map' file. Skipping depmod.
# Run the following command to create symbolic link to your System.map :
# sudo ln -s /boot/System.map-$(uname -r) /usr/src/linux-headers-$(uname -r)/System.map

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

#### Possible system related issues ::

If encountering the following error log when running your compiled examples for testing :
```bash
[HailoRT] [error] CHECK failed - max_desc_page_size given 16384 is bigger than hw max desc page size 4096
```
Modify or add the following line to /etc/modprobe.d/hailo_fw.conf to remove comment :
```bash 
options hailo_pci force_desc_page_size=4096
#force_desc_page_size determines the max DMA descriptor page size, must be a power of 2, needs to be set on platforms that do not support large PCIe transactions.

# update kernel module 
sudo depmod -a
sudo modprobe -r hailo_pci
sudo modprobe  hailo_pci
dmesg | grep hailo
```


### 3. Build HailoRT C++ binaries
If not done, change to the parent directory of previous driver ressources and run the following to get and compile proper HailoRT library:
```bash
git clone https://github.com/hailo-ai/hailort/tree/hailo8
cd hailort/
git checkout hailo8
# next compilation will create "build/hailort/hailortcli/hailortcli" binary and "build/hailort/libhailort/src/libhailort.so.<VERSION> library.
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release && cmake --build build --config release
# run install script to manage system install of lib and binary
sudo cmake --build build --config release --target install 
```

#### Verify PCIe Driver and Library
If all is well configured after these compilation and installation scripts, attest by running:
```bash
hailortcli fw-control identify
```
You should have something along the lines of:
```bash
Executing on device: 0001:01:00.0
Identifying board
Control Protocol Version: 2
Firmware Version: 4.22.0 (release,app,extended context switch buffer)
Logger Version: 0
Board Name: Hailo-8
Device Architecture: HAILO8
```
If you pulled the hef_models from this configuration repo, try running the pose_landmark_heavy.hef as follows from parent directory:
```bash
 hailortcli run ./hef_models/pose_landmark_heavy.hef
 ```
 If all is good, you should have something along the lines of:
 
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


### 4. Compilation of HailoRT-Python Bindings 

Run from hailort/libhailort/bindings/python/platform directory:
```bash
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
# wait for end of script then view output log, wou should see the creation of the network, the HEF configuration time, and such..
cat hailort.log
```


### 5. Gstreamer Bindings

```bash
cd hailort/libhailort/bindings/gstreamer
# Compiling the plugin:
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config release
```

#### Using the gstreamer plugin 
The plugin is dependent on libhailort which is included in the `hailort/lib/` directory.
Use LD_LIBRARY_PATH to specify the location of the libhailort library.