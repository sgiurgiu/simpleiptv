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
find_package(Boost 1.74 REQUIRED system filesystem program_options)
list(APPEND STV_LIBARIES Boost::boost Boost::system Boost::filesystem)

if(VCPKG_TOOLCHAIN)
    set(Boost_USE_STATIC_RUNTIME ON)
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

    if(UNIX)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
        list(APPEND STV_LIBARIES PkgConfig::MPV)
    endif()

    list(APPEND STV_LIBARIES OpenGL::GL glfw GLEW::GLEW freetype imgui::imgui)
    list(APPEND STV_LIBARIES OpenSSL::SSL)
    list(APPEND STV_LIBARIES spdlog::spdlog_header_only fmt::fmt-header-only)
    list(APPEND STV_LIBARIES unofficial::sqlite3::sqlite3 SOCI::soci_sqlite3_static)
else()
    message(FATAL "VCPKG is required")
endif()

if(WIN32)
    find_library(LIBMPV mpv ${LIBMPV_DIR})
    list(APPEND STV_LIBARIES ${LIBMPV} "crypt32.lib")
    list(APPEND STV_INCLUDES "win" ${LIBMPV_INCLUDE})
endif()



set(STV_SRCS main.cpp
    mpvplayer.cpp
    simpleiptv.cpp
    channels_window.cpp
    utils.cpp
    )


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
