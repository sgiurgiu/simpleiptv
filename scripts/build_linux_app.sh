#!/bin/bash

set -x

# Derive the workspace from the script's own location so the podman mount path is
# defined in exactly one place (scripts/build_linux.sh).
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
workspace=$(cd "${script_dir}/.." && pwd)
cd "${workspace}"
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
        CMAKE_PRESET="release-debuginfo"
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

mkdir -p "${workspace}"/packages
if [[ "${distro}" == "appimage" ]]
then
    export NO_STRIP=true
    rm -rf "${workspace}"/AppDir
    mkdir -p "${workspace}"/AppDir/usr
    tar -xvf "${workspace}"/build-release/simpleiptv-${SIMPLEIPTV_VERSION}*.tar.gz -C "${workspace}"/AppDir/usr
    cp -f "${workspace}"/simpleiptv_appimage.desktop "${workspace}"/AppDir/usr/share/applications/simpleiptv.desktop
    /opt/linuxdeployqt-continuous-x86_64.AppImage ./AppDir/usr/share/applications/simpleiptv.desktop -appimage
    cp SimpleIPTV*.AppImage "${workspace}"/packages/
else
    cp "${workspace}"/build-release/simpleiptv* "${workspace}"/packages/
fi

if [[ "${distro}" == "fedora" ]]
then
    # CPack downgrades a failed debuginfo extraction to a warning and still exits 0,
    # so check the subpackage actually exists rather than shipping a silent no-op.
    shopt -s nullglob
    debuginfo_rpms=("${workspace}"/packages/simpleiptv-debuginfo-*.rpm)
    if [ ${#debuginfo_rpms[@]} -eq 0 ]
    then
        echo "FATAL: the -debuginfo RPM was not produced."
        exit 1
    fi
fi
