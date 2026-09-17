# libdisplay-info exists here only so mpv links it statically. Fedora's copy is
# pre-1.0 and bumps its soname every minor release (F43 shipped .so.2, F44 ships
# .so.3), which is what made an F43-built RPM uninstallable on F44. mpv pulls it in
# via -Ddrm, and drm cannot be turned off because vaapi-drm is the only surviving
# VAAPI path once wayland/x11 are disabled -- so the dep has to be absorbed, not
# dropped. Pinned to 0.3.0 rather than 0.4.0 because that is the version Fedora 44
# builds mpv against; the library is pre-1.0 and does break API between minors.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://gitlab.freedesktop.org/emersion/libdisplay-info.git
    REF 47a5590e9c4eb35d67651b8c05a55f1a48259329 # 0.3.0
    HEAD_REF main
)

# The CLI tool and the test harness are dead weight here, and test/ reaches for shell
# harnesses and a v4l-utils subproject. Upstream offers no option to skip them.
vcpkg_replace_string("${SOURCE_PATH}/meson.build" "subdir('di-edid-decode')" "")
vcpkg_replace_string("${SOURCE_PATH}/meson.build" "subdir('test')" "")

# tool/gen-search-table.py turns pnp.ids into a lookup table at build time.
vcpkg_find_acquire_program(PYTHON3)
get_filename_component(PYTHON3_DIR "${PYTHON3}" DIRECTORY)
vcpkg_add_to_path("${PYTHON3_DIR}")

# That table is generated from hwdata's pnp.ids. meson looks for the hwdata .pc first
# and otherwise hardcodes /usr/share/hwdata/pnp.ids, failing at configure time if
# neither is there. Check up front so the build container's missing package is named.
find_program(PKG_CONFIG_FOR_HWDATA NAMES pkg-config pkgconf)
set(HWDATA_FOUND FALSE)
if(PKG_CONFIG_FOR_HWDATA)
  execute_process(
    COMMAND "${PKG_CONFIG_FOR_HWDATA}" --exists hwdata
    RESULT_VARIABLE HWDATA_PC_RESULT
    OUTPUT_QUIET ERROR_QUIET
  )
  if(HWDATA_PC_RESULT EQUAL 0)
    set(HWDATA_FOUND TRUE)
  endif()
endif()
if(NOT HWDATA_FOUND AND EXISTS "/usr/share/hwdata/pnp.ids")
  set(HWDATA_FOUND TRUE)
endif()
if(NOT HWDATA_FOUND)
  message(FATAL_ERROR
    "libdisplay-info needs hwdata's pnp.ids to generate its PNP id table, and neither "
    "the hwdata pkg-config file nor /usr/share/hwdata/pnp.ids was found.\n"
    "Install the 'hwdata' package in the build container.")
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -Ddefault_library=static
)
vcpkg_install_meson()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
