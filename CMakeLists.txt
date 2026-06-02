cmake_minimum_required(VERSION 3.30)

option(ENABLE_TESTS "Build tests" ON)
option(ASAN_ENABLE "Build with sanitization on" OFF)

if(ENABLE_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

project(simpleiptv LANGUAGES C CXX
    VERSION 0.0.1
    DESCRIPTION "Simple IPTV"
    )

if(DEFINED ENV{SIMPLEIPTV_VERSION})
  SET(SIMPLEIPTV_VERSION "$ENV{SIMPLEIPTV_VERSION}")
endif()

if(NOT SIMPLEIPTV_VERSION)
  SET(SIMPLEIPTV_VERSION "0.0.1")
endif()


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FFmpeg/mpv headers include <stdatomic.h>; MSVC needs experimental C11 atomics.
if(MSVC)
  add_compile_options(/experimental:c11atomics)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/std:c11>)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(EXECUTABLE_NAME simpleiptv)

add_subdirectory(src)

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

SET(CPACK_PACKAGE_NAME "simpleiptv")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple IPTV")
SET(CPACK_RESOURCE_FILE_LICENSE_PROVIDED FALSE)
SET(CPACK_INTERACTIVE FALSE)
SET(CPACK_INCLUDE_SUBDIR TRUE)
SET(CPACK_PACKAGE_VERSION ${SIMPLEIPTV_VERSION})
SET(CPACK_PACKAGE_CONTACT "Sergiu Giurgiu <sgiurgiu11@gmail.com>")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
SET(CPACK_PACKAGE_VENDOR "Sergiu Giurgiu <sgiurgiu11@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfreetype6")
set(CPACK_FREEBSD_PACKAGE_MAINTAINER "Sergiu Giurgiu <sgiurgiu11@gmail.com>")
set(CPACK_FREEBSD_PACKAGE_DEPS "security/openssl" "print/freetype2" "graphics/mesa-dri")
set(CPACK_RPM_RELOCATION_PATHS "/${CMAKE_INSTALL_SYSCONFDIR}" "${CMAKE_INSTALL_BINDIR}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0) # don't prepend package name
#set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
#set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postinstall")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

if(DEFINED ENV{OS_DIST_TAG})
  set(CPACK_RPM_PACKAGE_RELEASE "1.$ENV{OS_DIST_TAG}")
  #    set(CPACK_RPM_PACKAGE_RELEASE_DIST "$ENV{OS_DIST_TAG}")
else()
  set(CPACK_RPM_PACKAGE_RELEASE "1")
endif()

if(DEFINED ENV{OS_DIST_TAG})
  set(CPACK_DEBIAN_PACKAGE_RELEASE "1+$ENV{OS_DIST_TAG}")
else()
  set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
endif()

# FreeBSD revision: bump only when rebuilding the same version
set(CPACK_FREEBSD_PACKAGE_REVISION "1")

# Optional epoch (rarely needed)
# set(CPACK_FREEBSD_PACKAGE_EPOCH "0")


install(FILES "${CMAKE_SOURCE_DIR}/icons/simpleiptv-icon_24.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/24x24/apps" RENAME "simpleiptv-icon.png")
install(FILES "${CMAKE_SOURCE_DIR}/icons/simpleiptv-icon_32.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps" RENAME "simpleiptv-icon.png")
install(FILES "${CMAKE_SOURCE_DIR}/icons/simpleiptv-icon_64.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps" RENAME "simpleiptv-icon.png")
install(FILES "${CMAKE_SOURCE_DIR}/icons/simpleiptv-icon_128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "simpleiptv-icon.png")
install(FILES "${CMAKE_SOURCE_DIR}/icons/simpleiptv-icon_256.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps" RENAME "simpleiptv-icon.png")
install(FILES "${CMAKE_SOURCE_DIR}/simpleiptv.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications" RENAME "simpleiptv.desktop")


set(CPACK_WIX_LICENSE_PATH "${CMAKE_SOURCE_DIR}/gpl-3.0.rtf" CACHE INTERNAL "")
set(CPACK_WIX_ICON_PATH "${CMAKE_SOURCE_DIR}/icons/simpleiptv-icon_64.png" CACHE INTERNAL "")

set(CPACK_PACKAGE_EXECUTABLES "${EXECUTABLE_NAME};Simple IPTV")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/CPackWIXOptions.cmake")

INCLUDE(CPack)
