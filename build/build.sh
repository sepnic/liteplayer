#!/bin/bash

set -x

TOP_DIR=${PWD}/..
OUTPUT_DIR=${PWD}/out
OUTPUT_LIB_DIR=${OUTPUT_DIR}/lib
OUTPUT_BIN_DIR=${OUTPUT_DIR}/bin

# build libmsgutils
echo "Building libmsgutils"
mkdir -p ${OUTPUT_LIB_DIR}/libmsgutils
cd ${OUTPUT_LIB_DIR}/libmsgutils
cmake ${TOP_DIR}/thirdparty/msgutils
make

# build liblitecore
echo "Building liblitecore"
mkdir -p ${OUTPUT_LIB_DIR}/liblitecore
cd ${OUTPUT_LIB_DIR}/liblitecore
cmake ${TOP_DIR}/library
make

# build fatfs_mp3player
echo "Building fatfs_mp3player"
mkdir -p ${OUTPUT_BIN_DIR}/fatfs_mp3player
cd ${OUTPUT_BIN_DIR}/fatfs_mp3player
cmake ${TOP_DIR}/example/fatfs_mp3player
make

# build fatfs_m4aplayer
echo "Building fatfs_m4aplayer"
mkdir -p ${OUTPUT_BIN_DIR}/fatfs_m4aplayer
cd ${OUTPUT_BIN_DIR}/fatfs_m4aplayer
cmake ${TOP_DIR}/example/fatfs_m4aplayer
make
