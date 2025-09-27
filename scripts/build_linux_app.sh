#!/bin/bash

set -x

export VCPKG_ROOT=/opt/vcpkg
CMAKE_PRESET=""
distro=""
if [ -n "$1" ]
then
    distro=$1
    package_name="simpleiptv-${SIMPLEIPTV_VERSION}-${distro}"
    if [[ "${distro}" == "ubuntu" || "${distro}" == "debian" ]]
    then
        echo "Building a deb package"
        CMAKE_PRESET="release-deb"
    fi
    if [[ "${distro}" == "fedora" || "${distro}" == "redhat" || "${distro}" == "centos" || "${distro}" == "suse" ]]
    then
        echo "Building an rpm package"
        CMAKE_PRESET="release"
    fi
    if [[ "${distro}" == "appimage" ]]
    then
        echo "Building an appimage package"
        package_name="simpleiptv-${SIMPLEIPTV_VERSION}.AppImage"
        CMAKE_PRESET="release-tgz"
    fi
fi

cmake_exe=$(find /opt/ -name cmake -type f -executable -print)
if [[ -z ${cmake_exe} ]]
then
    echo "Cannot find cmake in /opt using whatever is installed"
    cmake_exe="cmake"
fi

cd /tmp/simpleiptv
export DISTRIBUTION=${distro}
rm -rf build-release
${cmake_exe}  --workflow --preset=${CMAKE_PRESET}

mkdir -p /tmp/simpleiptv/packages
if [[ "${distro}" == "appimage" ]]
then
    export NO_STRIP=true
    rm -rf /tmp/simpleiptv/AppDir
    mkdir -p /tmp/simpleiptv/AppDir/usr
    tar -xvf /tmp/simpleiptv/build-release/simpleiptv-${SIMPLEIPTV_VERSION}-${distro}.tar.gz -C /tmp/simpleiptv/AppDir/usr
    cp -f /tmp/simpleiptv/simpleiptv_appimage.desktop /tmp/simpleiptv/AppDir/usr/share/applications/simpleiptv.desktop
    /opt/appimagetool-x86_64.AppImage -s deploy ./AppDir/usr/share/applications/simpleiptv.desktop
else
    cp /tmp/simpleiptv/build-release/${package_name}.* /tmp/simpleiptv/packages/
fi