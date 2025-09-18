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
```bash
git clone https://github.com/hailo-ai/hailort-drivers/
cd hailort-drivers/
git checkout hailo8
# Compile driver locally. Compiled binary will be hailo_pci.ko
cd linux/pcie
make all
# Install driver in /lib/modules
sudo make install
# Load driver once, after driver will be loaded on boot
sudo modprobe hailo_pci

cd ./../../
./download_firmware.sh
sudo mkdir -p /lib/firmware/hailo
# <VERSION> refers to download firmware from previous step
sudo mv hailo8_fw.<VERSION>.bin /lib/firmware/hailo/hailo8_fw.bin

# Update device manager
sudp cp ./linux/pcie/51-hailo-udev.rules /etc/udev/rules.d/
# To apply changes, reboot or run :
sudo udevadm control --reload-rules && sudo udevadm trigger
```
#### Possible system related issues ::

If encountering the following error log when running your compiled examples for testing :
```bash
[HailoRT] [error] CHECK failed - max_desc_page_size given 16384 is bigger than hw max desc page size 4096
```

Verify if the hailo_pci.conf file is existant. If not, create it. Then add "force_desc_page_size=4096" line to the document: 
```bash 
[ -f "/etc/modprobe.d/hailo_pci.conf" ] && echo "File exists" || echo "File does not exist"
# verify if exists, if not, make it : $touch "/etc/modprobe.d/hailo_pci.conf"
echo force_desc_page_size=4096 >> /etc/modprobe.d/hailo_pci.conf

# save, close document and run following update commands:
sudo depmod -a
sudo modprobe -r hailo_pci
sudo modprobe  hailo_pci
dmesg | grep hailo
```

`force_desc_page_size` determines the max DMA descriptor page size, must be a power of 2, needs to be set on platforms that do not support large PCIe transactions.


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


### 4. Python Bindings 
        - PyHailoRT for the platform architecture and installed python version (if using python wrapper)
            - See chapter 3.4.3 Compiling Specific HailoRT Targets for the compilation of HailoRT-python-binding
            - create venv at root of hailo environment and install : 
                - pip install hailort-<version>-cp<python-version[38,39,310,311]>-cp<python-version[38,39,310,311]>-linux_aarch54.whl

### 5. Gstreamer Bindings


    - Trial out c++ examples with files given in hailort/libhailort/examples/cpp
        - Compile 

        - Note :    All function calls are based on the headers provided in the directory hailort/include/hailo.
                    One can include hailort/include/hailo/hailort.hpp to access all functions.

