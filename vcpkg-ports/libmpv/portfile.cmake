vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sgiurgiu/mpv
    REF  f6de490af15971991563050bb67fbc58478857b1
    SHA512 2f9ea6db89c243af3b1fc33ce7fae58f3eac4861e260fb51f7e271aff6c7385a4f0226a7a5e5abe2e85d715c6cd57ae7f1000f30f01782f80e748192e69c1fe4
    HEAD_REF libmpv_placebo
    PATCHES
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-desktop-libs.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-rc-codepage.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-thread-stdcall.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-io-int128.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-ta-align.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-win32-smtc.patch"
        "${CMAKE_CURRENT_LIST_DIR}/fix-vulkan.patch"
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
    -Dcuda-hwaccel=disabled
    -Dcuda-interop=disabled
    -Dcdda=disabled
    -Djpeg=disabled
    -Dwin32-smtc=disabled
    -Degl-angle=disabled
    -Degl-angle-lib=disabled
    -Degl-angle-win32=disabled
    -Dgl-win32=disabled
    -Dd3d11=disabled
    # Disabled to keep the Linux RPM installable across Fedora releases. vcpkg's own
    # deps get linked statically, but anything mpv auto-detects from the build
    # container stays a dynamic soname dep, and these bump between Fedora releases
    # (vapoursynth went .so.71 -> .so.0 between F43 and F44, which made the F43 RPM
    # uninstallable on F44). None of them are reachable from this app:
    #   vapoursynth - .vpy filter bridge, an encoder tool
    #   caca        - terminal ASCII-art VO; we set vo=libmpv, so no mpv VO is used
    #   jack        - pro-audio server output; alsa/pulse/pipewire already cover us
    #   zimg        - software scaler/colorspace conversion; libplacebo does this on GPU
    #   rubberband  - pitch-corrected speed change (also drags in fftw3 + samplerate);
    #                 the app exposes no speed control
    #   uchardet    - charset guessing for non-UTF-8 external subtitles; the app only
    #                 selects embedded tracks via sid and never loads a sub file
    # NOTE: -Ddrm must stay enabled. It gates libdisplay-info, but vaapi-drm requires
    # features['drm'] and (with wayland/x11 disabled above) is the only surviving VAAPI
    # path, so disabling drm would silently kill hwdec=auto hardware decoding.
    -Dvapoursynth=disabled
    -Dcaca=disabled
    -Djack=disabled
    -Dzimg=disabled
    -Drubberband=disabled
    -Duchardet=disabled
)

set(MESON_ADDITIONAL_PROPERTIES "vulkan_headers_inc = '${CURRENT_INSTALLED_DIR}/include'")

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
  list(APPEND MESON_OPTIONS -Dprefer_static=true)
endif()

if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
  # Meson cannot find Windows SDK import libs with prefer_static (see libplacebo port).
  string(APPEND MESON_ADDITIONAL_PROPERTIES "\nno_static_windows_libs = true")

  # Meson atomic/stdatomic probes and FFmpeg need MSVC experimental C11 atomics.
  vcpkg_cmake_get_vars(cmake_vars_file)
  include("${cmake_vars_file}")
  if(VCPKG_DETECTED_CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(MSVC_C11_ATOMICS_FLAGS "/experimental:c11atomics /std:c11")
    file(READ "${cmake_vars_file}" contents)
    string(APPEND contents "\nset(VCPKG_DETECTED_CMAKE_C_FLAGS \"${VCPKG_DETECTED_CMAKE_C_FLAGS} ${MSVC_C11_ATOMICS_FLAGS}\")")
    string(APPEND contents "\nset(VCPKG_COMBINED_C_FLAGS_DEBUG \"${VCPKG_COMBINED_C_FLAGS_DEBUG} ${MSVC_C11_ATOMICS_FLAGS}\")")
    string(APPEND contents "\nset(VCPKG_COMBINED_C_FLAGS_RELEASE \"${VCPKG_COMBINED_C_FLAGS_RELEASE} ${MSVC_C11_ATOMICS_FLAGS}\")")
    file(WRITE "${cmake_vars_file}" "${contents}")
    set(cmake_vars_file "${cmake_vars_file}" CACHE INTERNAL "")
  endif()
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${MESON_OPTIONS}
    ADDITIONAL_PROPERTIES "${MESON_ADDITIONAL_PROPERTIES}"
)
vcpkg_install_meson()

# mpv Vulkan Win32 context calls loader WSI entry points (e.g. vkCreateWin32SurfaceKHR).
set(WIN_DESKTOP_LIBS "-lavrt -ldwmapi -lgdi32 -limm32 -lntdll -lole32 -lpathcch -lshcore -luser32 -luuid -luxtheme -lversion -lvulkan-1 ")

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
