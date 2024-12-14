#!/usr/bin/env bash

set -e

cd "$(dirname "${BASH_SOURCE[0]}")"

# wget https://github.com/chenshuo/muduo/archive/refs/tags/v2.0.2.tar.gz

# Install muduo.
THREAD_NUM=$(nproc)
VERSION="2.0.2"
PKG_NAME="muduo-${VERSION}.tar.gz"

tar xzf "${PKG_NAME}"
pushd "muduo-${VERSION}"
    mkdir build && cd build
    cmake .. \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=11 \
        -DCMAKE_INSTALL_PREFIX=/usr/local
    make -j$(nproc)
    make install
popd

ldconfig

# Clean up
rm -rf "muduo-${VERSION}" "${PKG_NAME}"
