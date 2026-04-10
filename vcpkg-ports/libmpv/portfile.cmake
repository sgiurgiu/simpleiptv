vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  f6de490af15971991563050bb67fbc58478857b1
    SHA512 2f9ea6db89c243af3b1fc33ce7fae58f3eac4861e260fb51f7e271aff6c7385a4f0226a7a5e5abe2e85d715c6cd57ae7f1000f30f01782f80e748192e69c1fe4
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

