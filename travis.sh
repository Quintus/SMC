#!/usr/bin/env bash

set -xe
COMMIT=`git rev-parse --short HEAD`
DATE=`date -u +'%Y-%m-%d-%H:%M'`
CMAKE_LOG=$PWD/cmake-log

mkdir install
export ROOT=$PWD
export INSTALL_PREFIX=$ROOT/install

hg clone -b v0-8 https://bitbucket.org/cegui/cegui
cd cegui
mkdir build
cd build
cmake -G Ninja .. -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX

sed -i "s:$INSTALL_PREFIX:././:" cegui/include/CEGUI/Config.h
ninja install
rm $INSTALL_PREFIX/bin/CEGUI*
cd ../..

curl -Lo sfml.tgz https://www.sfml-dev.org/files/SFML-2.4.2-linux-gcc-64-bit.tar.gz
tar xf sfml.tgz
cp -r SFML-2.4.2/* $INSTALL_PREFIX

mkdir build
cd build
cmake -G Ninja ../tsc -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX | tee $CMAKE_LOG

TSC_VER=`grep 'TSC version' $CMAKE_LOG | grep -o '[0-9].*'`

sed -i "s:$INSTALL_PREFIX:././:" config.hpp
ninja install -j3

if [ "$TRAVIS_SUDO" == "true" ]; then
    echo "Building AppImage..."

    mv $INSTALL_PREFIX usr
    curl -Lo functions.sh \
        https://raw.githubusercontent.com/probonopd/AppImages/master/functions.sh
    . ./functions.sh

    patch_usr
    copy_deps
    cp \
      /lib/x86_64-linux-gnu/libpng12.so.* \
      /lib/x86_64-linux-gnu/libpcre.so.* \
      usr/lib/x86_64-linux-gnu
    delete_blacklisted

    export APP=TSC
    export VERSION=$TSC_VER-$COMMIT
    mkdir TSC.AppDir
    mv usr TSC.AppDir
    cp ../tsc/extras/tsc.desktop TSC.AppDir/TSC.desktop

    cd TSC.AppDir
    get_apprun
    cd ..

    APPIMAGE=TSC-$DATE-$COMMIT-x86_64.AppImage
    curl -LO https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool-x86_64.AppImage
    ./appimagetool-x86_64.AppImage TSC.AppDir $APPIMAGE

    curl -F "file=@$APPIMAGE" https://file.io
fi
