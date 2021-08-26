#!/bin/bash

set -x
set -e

TOP_DIR=${PWD}/..
CUR_DIR=${PWD}
OUTPUT_DIR=${PWD}/out

# build libs
echo "Building libs"
mkdir -p ${OUTPUT_DIR}/libs
cd ${OUTPUT_DIR}/libs
cmake ${TOP_DIR}/library
make

# build basic_demo bin
echo "Building basic_demo"
mkdir -p ${OUTPUT_DIR}/basic_demo
cd ${OUTPUT_DIR}/basic_demo
cmake ${TOP_DIR}/example/basic_demo
make

# build playlist_demo bin
echo "Building playlist_demo"
mkdir -p ${OUTPUT_DIR}/playlist_demo
cd ${OUTPUT_DIR}/playlist_demo
cmake ${TOP_DIR}/example/playlist_demo
make

# build android jni
cd ${CUR_DIR}
ndk-build -C android/jni
