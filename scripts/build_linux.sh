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

for distro in "${distros[@]}"
do
    echo "Running podman to build for distribution ${distro}"
    container=$CONTAINER_REGISTRY/vcpkg_mpv_apps_$distro:build
    # This is a prebuilt container that has installed and compiled the require vcpkg packages
    podman pull $container
    podman run --rm --privileged=true --name simpleiptv_build \
            -v "${root}":/tmp/simpleiptv/:Z \
            -e SIMPLEIPTV_VERSION="${SIMPLEIPTV_VERSION}" \
            $container \
            /tmp/simpleiptv/scripts/build_linux_app.sh $distro
done
