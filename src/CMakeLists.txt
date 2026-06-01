if(MSVC)
  set(WIN32_APP_OPTIONS WIN32)
endif()

set(Boost_USE_MULTITHREADED      ON)

find_package(Threads REQUIRED)

set(STV_LIBRARIES  Threads::Threads)
set(STV_INCLUDES)

set(Boost_USE_STATIC_LIBS    ON)

find_package(Boost 1.81 REQUIRED system filesystem program_options asio signals2 url)
list(APPEND STV_LIBRARIES Boost::boost Boost::system Boost::filesystem Boost::url)

if(VCPKG_TOOLCHAIN)
  set(Boost_USE_STATIC_RUNTIME ON)
  set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)
  find_package(VulkanHeaders CONFIG)
  find_package(VulkanMemoryAllocator CONFIG REQUIRED)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(OpenSSL REQUIRED)
  find_package(spdlog CONFIG REQUIRED)
  find_package(fmt CONFIG REQUIRED)
  find_package(freetype CONFIG REQUIRED)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
  find_package(SOCI CONFIG REQUIRED)
  find_package(Stb REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  find_package(date CONFIG REQUIRED)
  find_package(unofficial-shaderc CONFIG REQUIRED)
  find_package(FFMPEG REQUIRED)
  find_package(volk CONFIG REQUIRED)
  find_package(libunibreak CONFIG REQUIRED)

  # pkgconf is a host tool in vcpkg; ensure CMake finds it on Windows CI.
  if(WIN32 AND NOT PKG_CONFIG_EXECUTABLE)
    get_filename_component(_vcpkg_installed_root "${CMAKE_BINARY_DIR}/vcpkg_installed" ABSOLUTE)
    if(DEFINED VCPKG_TARGET_TRIPLET)
      set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
    else()
      set(_vcpkg_triplet "x64-windows-static")
    endif()
    string(REGEX REPLACE "-static$" "" _vcpkg_host_triplet "${_vcpkg_triplet}")
    find_program(PKG_CONFIG_EXECUTABLE NAMES pkgconf pkg-config
      HINTS
        "${_vcpkg_installed_root}/${_vcpkg_triplet}/tools/pkgconf"
        "${_vcpkg_installed_root}/${_vcpkg_host_triplet}/tools/pkgconf"
      NO_DEFAULT_PATH)
  endif()
  find_package(PkgConfig REQUIRED)
  if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR)
    set(_pkg_config_path "${VCPKG_INSTALLED_DIR}/lib/pkgconfig")
    if(DEFINED ENV{PKG_CONFIG_PATH})
      string(APPEND _pkg_config_path ";$ENV{PKG_CONFIG_PATH}")
    endif()
    set(ENV{PKG_CONFIG_PATH} "${_pkg_config_path}")
  endif()
  pkg_check_modules(LibMPV REQUIRED IMPORTED_TARGET mpv)
  list(APPEND STV_LIBRARIES PkgConfig::LibMPV)

  if(WIN32)
    # libmpv Vulkan Win32 code links loader WSI symbols (vkCreateWin32SurfaceKHR, etc.).
    find_package(Vulkan REQUIRED)
    list(APPEND STV_LIBRARIES Vulkan::Vulkan)
    list(APPEND STV_LIBRARIES crypt32.lib)
    list(APPEND STV_INCLUDES win)
  endif()

  if(UNIX)
    find_package(sdbus-c++ CONFIG REQUIRED)
    list(APPEND STV_LIBRARIES SDBusCpp::sdbus-c++)
    pkg_check_modules(Libass REQUIRED IMPORTED_TARGET libass)
    pkg_check_modules(Libebur128 REQUIRED IMPORTED_TARGET libebur128)
    list(APPEND STV_LIBRARIES PkgConfig::Libass PkgConfig::Libebur128)
    pkg_check_modules(SpirvCrossC REQUIRED IMPORTED_TARGET spirv-cross-c)
    list(APPEND STV_LIBRARIES PkgConfig::SpirvCrossC)
    pkg_check_modules(Libplacebo REQUIRED IMPORTED_TARGET libplacebo)
    list(APPEND STV_LIBRARIES PkgConfig::Libplacebo)
  endif()

  if(UNIX AND NOT APPLE) # freebsd ... sorry
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)
    list(APPEND STV_LIBRARIES PkgConfig::Systemd)
  endif()

  list(APPEND STV_LIBRARIES glfw freetype)
  list(APPEND STV_LIBRARIES OpenSSL::SSL)
  list(APPEND STV_LIBRARIES spdlog::spdlog_header_only fmt::fmt-header-only)
  list(APPEND STV_LIBRARIES unofficial::sqlite3::sqlite3 SOCI::soci_sqlite3_static)
  list(APPEND STV_LIBRARIES nlohmann_json::nlohmann_json)
  list(APPEND STV_LIBRARIES date::date date::date-tz)
  list(APPEND STV_LIBRARIES Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
  list(APPEND STV_LIBRARIES volk::volk_headers)

  list(APPEND STV_LIBRARIES ${FFMPEG_LIBRARIES})
  list(APPEND STV_LIBRARIES libunibreak::libunibreak)

  list(APPEND STV_INCLUDES ${Stb_INCLUDE_DIR})
  list(APPEND STV_INCLUDES ${FFMPEG_INCLUDE_DIRS})
  if(UNIX)
   pkg_check_modules(Libunwind REQUIRED IMPORTED_TARGET libunwind libunwind-coredump)
   list(APPEND STV_LIBRARIES PkgConfig::Libunwind)
  endif()
else()
  message(FATAL "VCPKG is required")
endif()

if(UNIX AND NOT APPLE)
  set(DBUS_CLIENT_FILES dbus/org.freedesktop.ScreenSaver.xml
    )
  set(DBUS_SERVER_FILES
        dbus/org.mpris.MediaPlayer2.xml
        dbus/org.mpris.MediaPlayer2.Player.xml
        dbus/org.mpris.MediaPlayer2.TrackList.xml
    )

  find_program(SDBUS-XML2CPP sdbus-c++-xml2cpp DOC "SDBUS XML2CPP program")

  foreach(FILE ${DBUS_CLIENT_FILES})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/dbus)
    execute_process(
            COMMAND ${SDBUS-XML2CPP} --proxy=${CMAKE_BINARY_DIR}/include/${FILE}.h ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND_ECHO STDOUT)
  endforeach()
  foreach(FILE ${DBUS_SERVER_FILES})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/dbus)
    execute_process(
            COMMAND ${SDBUS-XML2CPP} --adaptor=${CMAKE_BINARY_DIR}/include/${FILE}.h ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND_ECHO STDOUT)
  endforeach()

  list(APPEND STV_INCLUDES ${CMAKE_BINARY_DIR}/include)
endif()

set(STV_SRCS main.cpp
    mpvplayer.cpp
    simpleiptv_ui.cpp
    simpleiptv_coordinator.cpp
    simpleiptv_vulkan.cpp
    channels_window.cpp
    stv_utils.cpp
    serverpopup.cpp
    dbconnection_pool.cpp
    workers_provider.cpp
    channels/channels_repository.cpp
    channels/channels_group.cpp
    channels/channel.cpp
    channels/root_channel_group.cpp
    proxy_repository.cpp
    settings_repository.cpp
    playerbar_window.cpp
    stb_implementation.cpp
    sleep_service.cpp
    network_resource_provider.cpp
    boost_error_code_converter.cpp
    epg_listing.cpp
    epg_listings_window.cpp
    sanitize_url.cpp
    servers/server.cpp
    servers/servers_repository.cpp
    settings/colorspace_dialog.cpp
    settings/http_proxy_dialog.cpp
    settings/screenshot_dialog.cpp
    display_tree_nodes/common.cpp
    display_tree_nodes/display_node.cpp
    display_tree_nodes/display_channel.cpp
    display_tree_nodes/display_channel_group.cpp
    display_tree_nodes/display_favourite_channel_group.cpp
    display_tree_nodes/display_remote_channel_group.cpp
    display_tree_nodes/display_server_node.cpp
    display_tree_nodes/display_server_category.cpp
    display_tree_nodes/display_root_channel_group.cpp
    )

SET(IMGUI_SRCS
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_stdlib.cpp
    imgui/imgui_tables.cpp
    imgui/misc/freetype/imgui_freetype.cpp
)

list(APPEND STV_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

if(UNIX)
  list(APPEND STV_SRCS dbus_mpris_service.cpp)
endif()



if(MSVC)
  list(APPEND STV_SRCS win/AppIcon.rc)
endif()

set(EXECUTABLE_SOURCES
    ${WIN32_APP_OPTIONS}
    ${STV_SRCS}
    ${IMGUI_SRCS}
)

add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES})

set_property(TARGET ${EXECUTABLE_NAME} PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE  ${STV_LIBRARIES})
# pkg-config shaderc.pc has wrong static link order; CMake target resolves deps correctly
target_link_libraries(${EXECUTABLE_NAME} PRIVATE unofficial::shaderc::shaderc)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${STV_INCLUDES})


target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SPDLOG_FMT_EXTERNAL_HO SPDLOG_FMT_EXTERNAL)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE BOOST_ASIO_NO_DEPRECATED)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS  IMGUI_IMPL_VULKAN_USE_VOLK VK_NO_PROTOTYPES)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SIMPLEIPTV_VERSION="${SIMPLEIPTV_VERSION}")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SIMPLEIPTV_STRING="Simple IPTV v${SIMPLEIPTV_VERSION}")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<BOOL:${UNIX}>:STV_UNIX>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<BOOL:${WIN32}>:STV_WINDOWS>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<CONFIG:Debug>:STV_DEBUG>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<CONFIG:Release>:NDEBUG>" "$<$<CONFIG:Release>:IMGUI_DISABLE_DEBUG_TOOLS>")
if(ASAN_ENABLE)
  target_compile_options(${EXECUTABLE_NAME} PUBLIC $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -pedantic -Wextra>)
  target_compile_options(${EXECUTABLE_NAME} PUBLIC $<$<AND:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-fsanitize=undefined,address -fno-omit-frame-pointer>)
  target_link_options(${EXECUTABLE_NAME} PUBLIC $<$<AND:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-fsanitize=undefined,address>)
else()
  target_compile_options(${EXECUTABLE_NAME} PUBLIC $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -pedantic -Wextra>)
endif(ASAN_ENABLE)

target_compile_options(${EXECUTABLE_NAME} PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Release,RelWithDebInfo>>:-march=native>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release,RelWithDebInfo>>:/arch:AVX2>
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:NOMINMAX>")
# Windows 7 target
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0601>")

set_property(TARGET ${EXECUTABLE_NAME} PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


if(WIN32_APP_OPTIONS)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE WIN32_WINMAIN)
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})


