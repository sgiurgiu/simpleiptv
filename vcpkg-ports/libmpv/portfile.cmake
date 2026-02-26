vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  c6b7923c95876fbe8eb3fcd857e359547ced2f4a
    SHA512 8b5d0791083c481ab6a502a783e340cf056875cf319e0879651bae5b9ddc3f46f1ccb5b308674b444e360ffc5f9b2acb482632875d852f9671aad0d3357bb512
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

