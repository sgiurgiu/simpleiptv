if(MSVC)
    set(WIN32_APP_OPTIONS WIN32)
endif()

set(Boost_USE_MULTITHREADED      ON)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(Threads REQUIRED)

set(STV_LIBARIES ${CMAKE_DL_LIBS} Threads::Threads)
set(STV_INCLUDES)

set(Boost_USE_STATIC_LIBS    ON)
# Boost doesn't provide a pkgconfig file, so we just have to have it installed
# and discoverable by cmake
find_package(Boost 1.81 REQUIRED system filesystem program_options asio signals2 url)
list(APPEND STV_LIBARIES Boost::boost Boost::system Boost::filesystem Boost::url)

if(VCPKG_TOOLCHAIN)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)
    find_package(OpenGL COMPONENTS OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(fmt CONFIG REQUIRED)
    find_package(freetype CONFIG REQUIRED)
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    find_package(SOCI CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(Stb REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    if(UNIX)
		find_package(sdbus-c++ CONFIG REQUIRED)
		list(APPEND STV_LIBARIES SDBusCpp::sdbus-c++)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
        list(APPEND STV_LIBARIES PkgConfig::MPV)
    endif()

    if(UNIX AND NOT APPLE) # freebsd ... sorry
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)
        list(APPEND STV_LIBARIES PkgConfig::Systemd)
    endif()

    list(APPEND STV_LIBARIES OpenGL::GL glfw GLEW::GLEW freetype imgui::imgui)
    list(APPEND STV_LIBARIES OpenSSL::SSL)
    list(APPEND STV_LIBARIES spdlog::spdlog_header_only fmt::fmt-header-only)
    list(APPEND STV_LIBARIES unofficial::sqlite3::sqlite3 SOCI::soci_sqlite3_static)
    list(APPEND STV_LIBARIES nlohmann_json::nlohmann_json)
    
    list(APPEND STV_INCLUDES ${Stb_INCLUDE_DIR})
else()
    message(FATAL "VCPKG is required")
endif()

if(WIN32)
    find_library(LIBMPV mpv ${LIBMPV_DIR})
    list(APPEND STV_LIBARIES ${LIBMPV} "crypt32.lib")
    list(APPEND STV_INCLUDES "win" ${LIBMPV_INCLUDE})
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
    simpleiptv.cpp
    channels_window.cpp
    utils.cpp
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
    settings/http_proxy_dialog.cpp
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

if(UNIX)
list(APPEND STV_SRCS dbus_mpris_service.cpp)
endif()

if(MSVC)
    list(APPEND STV_SRCS win/AppIcon.rc)
endif()

set(EXECUTABLE_SOURCES
    ${WIN32_APP_OPTIONS}
    ${STV_SRCS}
)

add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES})

set_property(TARGET ${EXECUTABLE_NAME} PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE  ${STV_LIBARIES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${STV_INCLUDES})


target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SPDLOG_FMT_EXTERNAL_HO SPDLOG_FMT_EXTERNAL)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE GL_GLEXT_PROTOTYPES BOOST_ASIO_NO_DEPRECATED)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<CONFIG:Debug>:STV_DEBUG>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
if(ASAN_ENABLE)
    target_compile_options(${EXECUTABLE_NAME} PUBLIC $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-Wall -pedantic -Wextra -fsanitize=undefined,address -fno-omit-frame-pointer>)
    target_link_options(${EXECUTABLE_NAME} PUBLIC $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-fsanitize=undefined,address>)
else()
    target_compile_options(${EXECUTABLE_NAME} PUBLIC $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-Wall -pedantic -Wextra>)
endif(ASAN_ENABLE)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:NOMINMAX>")
# Windows 7 target
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0601>")

set_property(TARGET ${EXECUTABLE_NAME} PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


if(WIN32_APP_OPTIONS)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE WIN32_WINMAIN)
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})


if(WIN32)
    file(REAL_PATH ${LIBMPV_DIR} MPV_DIR)    
    install(FILES "${MPV_DIR}/mpv-2.dll"  DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
