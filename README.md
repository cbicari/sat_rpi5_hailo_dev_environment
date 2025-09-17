## Steps to configuring environment 
See HailoRT User Guide, p.16-17 for more information
    - Ubuntu installer requirements : 
        - build-essential, bison, flex, libelf-dev, dkms, curl, cmake, pip, virtualenv, systemd
    - Build HailoRT PCIe driver, HailoRT lib and pyHailoRT  on your local machine 
        - HailoRT PCIe driver and FW 
            - Clone https://github.com/hailo-ai/hailort-drivers/
                - checkout to hailo8 branch
            - Follow steps in chapter 8 of HailoRT User Guide for custom driver build and install

        - HailoRT library for the platform architecture :
            - Clone https://github.com/hailo-ai/hailort/tree/hailo8
                - checkout to hailo8 branch
                run :   $cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release && cmake --build build --config release
                        $sudo cmake --build build --config release --target install 

        - PyHailoRT for the platform architecture and installed python version (if using python wrapper)
            - See chapter 3.4.3 Compiling Specific HailoRT Targets for the compilation of HailoRT-python-binding
            - create venv at root of hailo environment and install : 
                - pip install hailort-<version>-cp<python-version[38,39,310,311]>-cp<python-version[38,39,310,311]>-linux_aarch54.whl

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