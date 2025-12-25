#!/bin/bash

set -x

cd /tmp/simpleiptv
export VCPKG_ROOT=/opt/vcpkg
CMAKE_PRESET=""
distro=""
if [ -n "$1" ]
then
    distro=$1
    if [[ "${distro}" == "ubuntu" || "${distro}" == "debian" ]]
    then
        echo "Building a deb package"
        CMAKE_PRESET="release-deb"
        export OS_FAMILY=${distro}
        export OS_VERSION=$( . /etc/os-release && echo "$VERSION_ID" )
        export OS_DIST_TAG="deb${OS_VERSION}"
        if [[ "${distro}" == "ubuntu" ]]
        then
            export OS_DIST_TAG="ubuntu${OS_VERSION}"
        fi
    fi
    if [[ "${distro}" == "fedora" ]]
    then
        echo "Building an rpm package"
        CMAKE_PRESET="release"
        export OS_FAMILY=fedora
        export OS_VERSION=$(rpm --eval '%{fedora}')
        export OS_DIST_TAG="fc${OS_VERSION}"
        echo "OS_VERSION=${OS_VERSION}" > build.env
        echo "OS_FAMILY=${OS_FAMILY}" >> build.env
        echo "OS_DIST_TAG=${OS_DIST_TAG}" >> build.env
    fi
    if [[ "${distro}" == "appimage" ]]
    then
        echo "Building an appimage package"
        CMAKE_PRESET="release-tgz"
    fi
fi

cmake_exe=$(find /opt/ -name cmake -type f -executable -print | sort | head -n 1)
if [[ -z ${cmake_exe} ]]
then
    echo "Cannot find cmake in /opt using whatever is installed"
    cmake_exe="cmake"
fi

rm -rf build-release
${cmake_exe}  --workflow --preset=${CMAKE_PRESET}

mkdir -p /tmp/simpleiptv/packages
if [[ "${distro}" == "appimage" ]]
then
    export NO_STRIP=true
    rm -rf /tmp/simpleiptv/AppDir
    mkdir -p /tmp/simpleiptv/AppDir/usr
    tar -xvf /tmp/simpleiptv/build-release/simpleiptv-${SIMPLEIPTV_VERSION}*.tar.gz -C /tmp/simpleiptv/AppDir/usr
    cp -f /tmp/simpleiptv/simpleiptv_appimage.desktop /tmp/simpleiptv/AppDir/usr/share/applications/simpleiptv.desktop
    /opt/linuxdeployqt-continuous-x86_64.AppImage ./AppDir/usr/share/applications/simpleiptv.desktop -appimage
    cp SimpleIPTV*.AppImage /tmp/simpleiptv/packages/
else
    cp /tmp/simpleiptv/build-release/simpleiptv* /tmp/simpleiptv/packages/
fi
