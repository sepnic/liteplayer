#!/bin/bash

set -x

TOP_DIR=${PWD}/..
OUTPUT_DIR=${PWD}/out
OUTPUT_LIB_DIR=${OUTPUT_DIR}/lib
OUTPUT_BIN_DIR=${OUTPUT_DIR}/bin

# build libmsgutils lib
echo "Building libmsgutils"
mkdir -p ${OUTPUT_LIB_DIR}/libmsgutils
cd ${OUTPUT_LIB_DIR}/libmsgutils
cmake ${TOP_DIR}/thirdparty/msgutils
make

# build liblitecore lib
echo "Building liblitecore"
mkdir -p ${OUTPUT_LIB_DIR}/liblitecore
cd ${OUTPUT_LIB_DIR}/liblitecore
cmake ${TOP_DIR}/library
make

# build libliteplayer lib
echo "Building libliteplayer"
mkdir -p ${OUTPUT_LIB_DIR}/libliteplayer
cd ${OUTPUT_LIB_DIR}/libliteplayer
cmake ${TOP_DIR}/player
make

# build liteplayer_demo bin
echo "Building liteplayer_demo"
mkdir -p ${OUTPUT_BIN_DIR}/liteplayer_demo
cd ${OUTPUT_BIN_DIR}/liteplayer_demo
cmake ${TOP_DIR}/example/liteplayer_demo
make
