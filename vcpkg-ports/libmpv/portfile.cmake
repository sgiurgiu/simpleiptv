vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  dc03bf9d441f60ebd78f8a64eaa0626136811d94
    SHA512 52eebd1cb3a727432a9d1e57a959442db793dd205a0a02b0ae13821fe7cee38f569f79aaa2875e38bc2d3356c356db470fecb6e2bbcb53ed508a261a3a254987
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

