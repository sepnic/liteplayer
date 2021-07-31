#!/bin/bash

set -x
set -e

TOP_DIR=${PWD}/..
CUR_DIR=${PWD}
OUTPUT_DIR=${PWD}/out

# build liteplayercore lib
echo "Building liteplayercore"
mkdir -p ${OUTPUT_DIR}/libliteplayercore
cd ${OUTPUT_DIR}/libliteplayercore
cmake ${TOP_DIR}/library
make

# build liteplayer_demo bin
echo "Building liteplayer_demo"
mkdir -p ${OUTPUT_DIR}/liteplayer_demo
cd ${OUTPUT_DIR}/liteplayer_demo
cmake ${TOP_DIR}/example/liteplayer_demo
make

# build liteplayer_mngr bin
echo "Building liteplayer_mngr"
mkdir -p ${OUTPUT_DIR}/liteplayer_mngr
cd ${OUTPUT_DIR}/liteplayer_mngr
cmake ${TOP_DIR}/example/liteplayer_mngr
make

# build android jni
cd ${CUR_DIR}
ndk-build -C android/jni
