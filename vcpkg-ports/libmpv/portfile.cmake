vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  aa53c15d06ade14e22a65bba03bb3a7feaa65cfd
    SHA512 969434c2898b2e2fca5fcd324749eb0bebd35a9693a0d9fb82dfa781f1ce68e7ccd187ca082d887cd46522b6cad6da008d636b83650976a06c65c32176dbff74
    HEAD_REF libmpv_placebo
)

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS_RELEASE
        -Dbuildtype=release # Explicitly setting build type
        -Ddebug=true       # Enable debug symbols
        -Doptimization=0   # Reduce optimization to keep symbols clearer
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
    OPTIONS
        -Ddebug=false       # Enable debug symbols
        -Doptimization=0   # Reduce optimization to keep symbols clearer
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

