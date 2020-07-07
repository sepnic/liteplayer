#!/bin/bash

set -x

TOP_DIR=${PWD}/..
OUTPUT_DIR=${PWD}/out
OUTPUT_LIB_DIR=${OUTPUT_DIR}/lib
OUTPUT_BIN_DIR=${OUTPUT_DIR}/bin

# build liteplayercore lib
echo "Building liteplayercore"
mkdir -p ${OUTPUT_LIB_DIR}/liteplayercore
cd ${OUTPUT_LIB_DIR}/liteplayercore
cmake ${TOP_DIR}/library
make

# build liteplayer_demo bin
echo "Building liteplayer_demo"
mkdir -p ${OUTPUT_BIN_DIR}/liteplayer_demo
cd ${OUTPUT_BIN_DIR}/liteplayer_demo
cmake ${TOP_DIR}/example/liteplayer_demo
make

# build liteplayer_mngr bin
echo "Building liteplayer_mngr"
mkdir -p ${OUTPUT_BIN_DIR}/liteplayer_mngr
cd ${OUTPUT_BIN_DIR}/liteplayer_mngr
cmake ${TOP_DIR}/example/liteplayer_mngr
make
