#!/bin/bash

# install and build seeing
cd /opt/seeing
mkdir -p build
cd build
cmake ..
make
cd ..
