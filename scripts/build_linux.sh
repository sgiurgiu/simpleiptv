#!/bin/bash

set -ex

if [ -z ${CI_PROJECT_DIR+x} ]; then
    root=$(git rev-parse --show-toplevel)
else
    root=${CI_PROJECT_DIR}
fi

if [[ -z ${CONTAINER_REGISTRY} ]]; then
    echo "FATAL: Please set the CONTAINER_REGISTRY environment variable to point to where the containers are located."
    exit 1
fi

if [[ -z "${CONTAINER_REGISTRY+x}" ]]; then
    echo "FATAL: Please set the CONTAINER_REGISTRY environment variable to point to where the containers are located."
    exit 1
fi
SIMPLEIPTV_VERSION=$(git describe --tags || true)
if [ -z ${SIMPLEIPTV_VERSION} ]; then
    SIMPLEIPTV_VERSION="1.0.dev"
fi

if [ -z $1 ]; then
    distros=("fedora" "appimage" "debian")
else
    distros=($1)
fi

# Long enough that CPack/debugedit can rewrite DWARF source paths in place for the
# -debuginfo RPM: the source dir must be at least as long as "/usr/src/debug/src_0"
# (20 chars). "/tmp/simpleiptv-workspace" is 25.
workspace=/tmp/simpleiptv-workspace

for distro in "${distros[@]}"
do
    echo "Running podman to build for distribution ${distro}"
    container=$CONTAINER_REGISTRY/vcpkg_mpv_apps_$distro:build
    # This is a prebuilt container that has installed and compiled the require vcpkg packages
    podman pull $container
    podman run --rm --privileged=true --name simpleiptv_build \
            -v "${root}":"${workspace}"/:Z \
            -e SIMPLEIPTV_VERSION="${SIMPLEIPTV_VERSION}" \
            $container \
            "${workspace}"/scripts/build_linux_app.sh $distro
done
