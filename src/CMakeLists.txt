
set(STV_SRCS main.cpp
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


