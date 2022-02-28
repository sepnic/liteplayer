#!/bin/bash

set -x
set -e

TOP_DIR=${PWD}
CUR_DIR=${PWD}
OUTPUT_DIR=${PWD}/out

# build libs
echo "Building libs"
mkdir -p ${OUTPUT_DIR}/libs
cd ${OUTPUT_DIR}/libs
cmake ${TOP_DIR}/library
make

# build example
echo "Building example"
mkdir -p ${OUTPUT_DIR}/example
cd ${OUTPUT_DIR}/example
cmake ${TOP_DIR}/example/unix
make

# build android jni
cd ${CUR_DIR}
ndk-build -C android/jni
