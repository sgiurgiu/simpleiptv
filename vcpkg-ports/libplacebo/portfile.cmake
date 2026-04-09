function(fetch)
set(oneValueArgs DESTINATION URL REF SOURCE)
cmake_parse_arguments(PL "" "${oneValueArgs}" "" ${ARGN})

if(EXISTS ${PL_SOURCE}/${PL_DESTINATION})
    file(REMOVE_RECURSE "${PL_SOURCE}/${PL_DESTINATION}")
endif()

vcpkg_execute_required_process(
        COMMAND ${GIT} clone --depth 1 ${PL_URL} ${PL_DESTINATION}
        WORKING_DIRECTORY ${PL_SOURCE}
        LOGNAME build-${TARGET_TRIPLET})
vcpkg_execute_required_process(
        COMMAND ${GIT} fetch --depth 1 origin ${PL_REF}
        WORKING_DIRECTORY ${PL_SOURCE}/${PL_DESTINATION}
        LOGNAME build-${TARGET_TRIPLET})
vcpkg_execute_required_process(
        COMMAND ${GIT} checkout FETCH_HEAD
        WORKING_DIRECTORY ${PL_SOURCE}/${PL_DESTINATION}
        LOGNAME build-${TARGET_TRIPLET})


endfunction()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO haasn/libplacebo
    REF "v7.360.1"
    SHA512 209b1713cff34f06149af16fb3ea52e3662a566ef5df6b29811ad295aa8cb6388f827a93fc8e0eed1a72f35b3b3aae835520c933079e706a51d11136a8128799
    HEAD_REF master
)

message(STATUS "Fetching submodules")
fetch(
    DESTINATION 3rdparty/fast_float
    URL https://github.com/fastfloat/fast_float
    REF 2b2395f9ac836ffca6404424bcc252bff7aa80e4 
    SOURCE ${SOURCE_PATH}
    )
fetch(
    DESTINATION 3rdparty/glad
    URL https://github.com/Dav1dde/glad
    REF d08b1aa01f8fe57498f04d47b5fa8c48725be877 
    SOURCE ${SOURCE_PATH}
    )
fetch(
    DESTINATION 3rdparty/jinja
    URL https://github.com/pallets/jinja
    REF b08cd4bc64bb980df86ed2876978ae5735572280 
    SOURCE ${SOURCE_PATH}
    )
fetch(
    DESTINATION 3rdparty/markupsafe
    URL https://github.com/pallets/markupsafe
    REF c0254f0cfe51720ecc9e72e8896022af29af5b44 
    SOURCE ${SOURCE_PATH}
    )
    
    
set(EXTRA_OPTIONS "")

if("vulkan" IN_LIST FEATURES)
    set(REGISTRY_XML "${CURRENT_INSTALLED_DIR}/share/vulkan/registry/vk.xml")
    list(APPEND EXTRA_OPTIONS "-Dvulkan=enabled")
    list(APPEND EXTRA_OPTIONS "-Dvk-proc-addr=disabled")
    list(APPEND EXTRA_OPTIONS "-Dopengl=disabled")
    list(APPEND EXTRA_OPTIONS "-Dgl-proc-addr=disabled")
    list(APPEND EXTRA_OPTIONS "-Dvulkan-registry=${REGISTRY_XML}")
endif()
if("opengl" IN_LIST FEATURES)
    list(APPEND EXTRA_OPTIONS "-Dopengl=enabled")
    list(APPEND EXTRA_OPTIONS "-Dgl-proc-addr=disabled")
    list(APPEND EXTRA_OPTIONS "-Dvulkan=disabled")
    list(APPEND EXTRA_OPTIONS "-Dvk-proc-addr=disabled")
endif()

if(NOT VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    list(APPEND EXTRA_OPTIONS "-Ddefault_library=static")
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
      -Ddemos=false
      -Dtests=false
      -Dbench=false
      -Dfuzz=false
      -Dunwind=disabled
      -Ddebug-abort=false
      -Dxxhash=disabled
      -Dglslang=enabled
      ${EXTRA_OPTIONS}
)

vcpkg_install_meson()

vcpkg_copy_pdbs()

vcpkg_fixup_pkgconfig()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
