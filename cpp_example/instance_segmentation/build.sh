#!/bin/bash

cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

if [[ -f "hailort.log" ]]; then
    rm hailort.log
fi
