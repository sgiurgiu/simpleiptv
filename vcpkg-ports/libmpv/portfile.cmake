vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  d3541cdca8e20f3d35c8b4be1ed38a2003ed8117
    SHA512 6395cc80028e562f12b079c0c972d7a79873b8e79da371feb9f1494280fa6d6aff6ccca171fa41fa335e30ad0179daac39072679a6de30f61a695df11d2377d5
    HEAD_REF libmpv_placebo
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
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL" "${SOURCE_PATH}/LICENSE.LGPL")

