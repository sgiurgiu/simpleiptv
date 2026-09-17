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

# vcpkg's binary cache lives on the host so built packages are reused between builds.
# The container images set VCPKG_BINARY_SOURCES to point at /vcpkg-cache.
# Lowercase :z because the cache is shared by every build container.
cache_mount=()
if [ -d /srv/vcpkg-cache ]; then
    cache_mount=(-v /srv/vcpkg-cache:/vcpkg-cache:z)
fi

for distro in "${distros[@]}"
do
    echo "Running podman to build for distribution ${distro}"
    container=$CONTAINER_REGISTRY/vcpkg_mpv_apps_$distro:build
    podman pull $container
    podman run --rm --privileged=true --name simpleiptv_build \
            -v "${root}":"${workspace}"/:Z \
            "${cache_mount[@]}" \
            -e SIMPLEIPTV_VERSION="${SIMPLEIPTV_VERSION}" \
            $container \
            "${workspace}"/scripts/build_linux_app.sh $distro
done
