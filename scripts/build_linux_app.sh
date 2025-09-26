#!/bin/bash

set -x

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
    fi
    if [[ "${distro}" == "fedora" || "${distro}" == "redhat" || "${distro}" == "centos" || "${distro}" == "suse" ]]
    then
        echo "Building an rpm package"
        CMAKE_PRESET="release"
    fi
fi

cmake_exe=$(find /opt/ -name cmake -type f -executable -print)
if [[ -z ${cmake_exe} ]]
then
    echo "Cannot find cmake in /opt using whatever is installed"
    cmake_exe="cmake"
fi

cd /tmp/simpleiptv
${cmake_exe} -DCPACK_DISTRIBUTION=${distro} --workflow --preset=${CMAKE_PRESET}

mkdir -p /tmp/simpleiptv/packages
cp /tmp/build/simpleiptv-${SIMPLEIPTV_VERSION}-${distro}.* /tmp/simpleiptv/packages/
