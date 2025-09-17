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
```


### 3 Build HailoRT C++ binaries
        - HailoRT library for the platform architecture :
            - Clone https://github.com/hailo-ai/hailort/tree/hailo8
                - checkout to hailo8 branch
                run :   $cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release && cmake --build build --config release
                        $sudo cmake --build build --config release --target install 

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


        Possible system related issues ::
            [HailoRT] [error] CHECK failed - max_desc_page_size given 16384 is bigger than hw max desc page size 4096
            In this case : 
                    $ sudo vim /etc/modprobe.d/hailo_pci.conf
                            add line :: options hailo_pci force_desc_page_size=4096
                    $ sudo depmod -a
                    $ sudo modprobe -r hailo_pci
                    $ sudo modprobe  hailo_pci
                    $ dmesg | grep hailo