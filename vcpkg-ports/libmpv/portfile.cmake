vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  f6de490af15971991563050bb67fbc58478857b1
    SHA512 2f9ea6db89c243af3b1fc33ce7fae58f3eac4861e260fb51f7e271aff6c7385a4f0226a7a5e5abe2e85d715c6cd57ae7f1000f30f01782f80e748192e69c1fe4
    HEAD_REF libmpv_placebo
    PATCHES
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-desktop-libs.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-smtc.patch"
)

set(MESON_OPTIONS
    -Ddefault_library=static
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
    -Dcuda-hwaccel=enabled
    -Dcuda-interop=enabled
    -Dcdda=disabled
    -Djpeg=disabled
    -Dwin32-smtc=disabled
    -Degl-angle=disabled
    -Degl-angle-lib=disabled
    -Degl-angle-win32=disabled
    -Dgl-win32=disabled
    -Dd3d11=disabled
)

set(NO_STATIC_WINDOWS_LIBS false)
set(MESON_ADDITIONAL_PROPERTIES "")

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    list(APPEND MESON_OPTIONS -Dprefer_static=true)
endif()

if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
    # Meson cannot find Windows SDK import libs with prefer_static (see libplacebo port).
    set(NO_STATIC_WINDOWS_LIBS true)
    set(MESON_ADDITIONAL_PROPERTIES "no_static_windows_libs = true")
endif()

if(MESON_ADDITIONAL_PROPERTIES)
    vcpkg_configure_meson(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS ${MESON_OPTIONS}
        ADDITIONAL_PROPERTIES "${MESON_ADDITIONAL_PROPERTIES}"
    )
else()
    vcpkg_configure_meson(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS ${MESON_OPTIONS}
    )
endif()
vcpkg_install_meson()

set(WIN_DESKTOP_LIBS "-lavrt -ldwmapi -lgdi32 -limm32 -lntdll -lole32 -lpathcch -lshcore -luser32 -luuid -luxtheme -lversion ")

if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
    set(pkgconfig_file "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/meson-private/mpv.pc")
    if(EXISTS "${pkgconfig_file}")
        if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
            vcpkg_replace_string("${pkgconfig_file}" "Libs: " "Libs: ${WIN_DESKTOP_LIBS}")
        endif()
        file(COPY "${pkgconfig_file}" DESTINATION "${CURRENT_PACKAGES_DIR}/lib/pkgconfig")
    endif()
endif()
if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
    set(pkgconfig_file "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/meson-private/mpv.pc")
    if(EXISTS "${pkgconfig_file}")
        if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
            vcpkg_replace_string("${pkgconfig_file}" "Libs: " "Libs: ${WIN_DESKTOP_LIBS}")
        endif()
        file(COPY "${pkgconfig_file}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig")
    endif()
endif()

vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL" "${SOURCE_PATH}/LICENSE.LGPL")
