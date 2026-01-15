function(fetch)
set(oneValueArgs DESTINATION URL REF SOURCE)
cmake_parse_arguments(PL "" "${oneValueArgs}" "" ${ARGN})

if(EXISTS ${PL_SOURCE}/${PL_DESTINATION})

vcpkg_execute_required_process(
        COMMAND rm -rf ${PL_DESTINATION}
        WORKING_DIRECTORY ${PL_SOURCE}
        LOGNAME build-${TARGET_TRIPLET})
endif()

vcpkg_execute_required_process(
        COMMAND ${GIT} clone --depth 1 --recursive ${PL_URL} ${PL_DESTINATION}
        WORKING_DIRECTORY ${PL_SOURCE}
        LOGNAME build-${TARGET_TRIPLET})
vcpkg_execute_required_process(
        COMMAND ${GIT} fetch --depth 1 origin ${PL_REF}
        WORKING_DIRECTORY ${PL_SOURCE}/${PL_DESTINATION}
        LOGNAME build-${TARGET_TRIPLET})
vcpkg_execute_required_process(
        COMMAND ${GIT} checkout FETCH_HEAD
        WORKING_DIRECTORY ${PL_SOURCE}/${PL_DESTINATION}
        LOGNAME build-${TARGET_TRIPLET})


endfunction()

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF 22a66ae25d8c1c4693541a3b87f3651ab547de27
    SHA512 ebb0c910f48952d540fc04d23420af22c9e6345c7145f17c84507929560167be185705377984b792c32e5bc35d13c784e6cf630d4dbe12f3c3ef7e5cf6fc64ea
    HEAD_REF libmpv_placebo
)

fetch(
    DESTINATION subprojects/libplacebo
    URL https://code.videolan.org/videolan/libplacebo.git
    REF 3188549fba13bbdf3a5a98de2a38c2e71f04e21e
    SOURCE ${SOURCE_PATH}
  )

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -Ddefault_library=static
        -Dprefer_static=true
	-Dtests=false
        -Dcplayer=false
        -Dlibmpv=true
        -Djavascript=disabled
        -Dlua=disabled
        -Dsdl2-gamepad=disabled
        -Dmanpage-build=disabled
        -Dlibbluray=disabled
        -Dsdl2-video=disabled
        -Dvulkan=enabled
        -Ddvdnav=disabled
        -Ddvbin=disabled
        -Dlibarchive=disabled
        -Dx11=disabled
        -Dx11-clipboard=disabled
        -Dwayland=disabled
        -Ddmabuf-wayland=disabled
        -Dxv=disabled
        -Dcuda-hwaccel=disabled
        -Dcuda-interop=disabled
        -Dcdda=disabled
        -Djpeg=disabled
        -Dwayland=disabled
)
vcpkg_install_meson()
vcpkg_fixup_pkgconfig()


file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${SOURCE_PATH}/subprojects/libplacebo/src/include/libplacebo" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL" "${SOURCE_PATH}/LICENSE.LGPL")

