if (VCPKG_TARGET_IS_EMSCRIPTEN)
    # emscripten has built-in glfw3 library
    set(VCPKG_BUILD_TYPE release)
    file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/glfw3Config.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/glfw3")
    set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
    return()
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO glfw/glfw
    REF ed6452b13c76f7b4da216a9952bc7837aeb0f031
    SHA512 8c6a833061d8d8c32f14f8f42d81de55c2d004964d9b1491bbf10b2a5f492317ca3270497b3d094fd98bfa29919fac15648c1c46f9867bf9fbccc50837655661
    HEAD_REF master
    PATCHES
        wayland-fullscreen-current-monitor.patch
)

if(VCPKG_TARGET_IS_LINUX)
    message(
"GLFW3 currently requires the following libraries from the system package manager:
    xinerama
    xcursor
    xorg
    libglu1-mesa
    pkg-config

These can be installed on Ubuntu systems via sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config

Alternatively, when targeting the Wayland display server, use the packages listed in the GLFW documentation here:

https://www.glfw.org/docs/3.3/compile.html#compile_deps_wayland")
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
    wayland         GLFW_BUILD_WAYLAND
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DGLFW_BUILD_EXAMPLES=OFF
        -DGLFW_BUILD_TESTS=OFF
        -DGLFW_BUILD_DOCS=OFF
        ${FEATURE_OPTIONS}
    MAYBE_UNUSED_VARIABLES
        GLFW_USE_WAYLAND
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/glfw3)

vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
