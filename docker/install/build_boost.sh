#!/usr/bin/env bash

cd "$(dirname "${BASH_SOURCE[0]}")"

wget https://archives.boost.io/release/1.69.0/source/boost_1_69_0.tar.gz

VERSION="1_69_0"
PKG_NAME="boost_${VERSION}.tar.gz"

tar xzf "${PKG_NAME}"
pushd "boost_${VERSION}"
    ./bootstrap.sh  
    ./b2 --prefix=/usr/local variant=release install -j$(nproc)
popd

ldconfig

# Clean up
rm -rf "boost_${VERSION}" "${PKG_NAME}"
