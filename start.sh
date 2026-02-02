#!/bin/bash

sudo rm -rf build/

mkdir -p build
cd build

cmake -G Ninja .. &> cmake_output.txt
ninja &> build_output.txt 

cd ..

build/src/MyGame