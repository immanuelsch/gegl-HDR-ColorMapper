#!/bin/bash

# Install GIMP and GEGL under $HOIME/opt by default:
export PREFIX=$HOME/opt

export LD_LIBRARY_PATH=${PREFIX}/lib
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig/
export PATH=$PREFIX/bin:$PATH
export XDG_DATA_DIRS="$PREFIX/share:$XDG_DATA_DIRS"
export GI_TYPELIB_PATH="${PREFIX}/lib/girepository-1.0:${PREFIX}/lib/${arch}/girepository-1.0:$GI_TYPELIB_PATH"

SRC_DIR=$(pwd)
BUILD_DIR=${SRC_DIR}/obj-$(arch)
mkdir -p $BUILD_DIR && cd $BUILD_DIR && meson -Dprefix=$PREFIX --buildtype=release $SRC_DIR && ninja

