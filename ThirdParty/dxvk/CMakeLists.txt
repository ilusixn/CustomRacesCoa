# DXVK v2.5.3 (the version the CoA install ships for Ascension.exe), built from source as an x64 d3d9.dll: CoA64
# loads it from <exe dir>\x64\ (CoA/Source/Runtime/DxvkClient.cpp). meson + ninja through cmake/BuildDxvk.cmake;
# d3d9 only, release. Missing tools skip DXVK with a warning (the client then uses the system d3d9).
include(ExternalProject)

find_program(DXVK_MESON meson HINTS "$ENV{APPDATA}/Python/Python314/Scripts")
find_program(DXVK_NINJA ninja HINTS "$ENV{APPDATA}/Python/Python314/Scripts")
find_program(DXVK_GLSLANG glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin")

if (NOT DXVK_MESON OR NOT DXVK_NINJA OR NOT DXVK_GLSLANG)
    message(WARNING "DXVK: meson, ninja or glslangValidator not found (meson='${DXVK_MESON}' ninja='${DXVK_NINJA}' glslangValidator='${DXVK_GLSLANG}'); not building bin/Win/x64/d3d9.dll.")
    return ()
endif ()

# Empty: clone the pinned tag. Set to a local checkout of that commit (with submodules) to build without network.
set(DXVK_SOURCE_DIR "" CACHE PATH "Local DXVK v2.5.3 checkout to build instead of cloning")

set(DXVK_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/Win/x64)
set(DXVK_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/dxvk-build)
set(DXVK_DRIVER
    ${CMAKE_COMMAND}
    -DMESON=${DXVK_MESON}
    -DNINJA=${DXVK_NINJA}
    -DGLSLANG=${DXVK_GLSLANG}
    -DBINARY_DIR=${DXVK_BINARY_DIR}
    -DOUTPUT_DIR=${DXVK_OUTPUT_DIR}
)

if (DXVK_SOURCE_DIR)
    set(DXVK_SOURCE_ARGS
        SOURCE_DIR ${DXVK_SOURCE_DIR}
        DOWNLOAD_COMMAND ""
    )
    set(DXVK_SOURCE ${DXVK_SOURCE_DIR})
else ()
    set(DXVK_SOURCE_ARGS
        GIT_REPOSITORY https://github.com/doitsujin/dxvk.git
        # v2.5.3
        GIT_TAG c707d9026f33b6ab89639f154b6ac5f6326fa037
        GIT_SUBMODULES_RECURSE ON
        GIT_SHALLOW OFF
        UPDATE_DISCONNECTED ON
    )
    set(DXVK_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/dxvk-prefix/src/dxvk)
endif ()

# Not BUILD_ALWAYS: once built, the steps' stamps keep later builds from re-running meson / ninja.
ExternalProject_Add(dxvk
    ${DXVK_SOURCE_ARGS}
    BINARY_DIR ${DXVK_BINARY_DIR}
    CONFIGURE_COMMAND ${DXVK_DRIVER} -DSTEP=configure -DSOURCE_DIR=${DXVK_SOURCE} -P ${CMAKE_SOURCE_DIR}/cmake/BuildDxvk.cmake
    BUILD_COMMAND ${DXVK_DRIVER} -DSTEP=build -P ${CMAKE_SOURCE_DIR}/cmake/BuildDxvk.cmake
    INSTALL_COMMAND ${DXVK_DRIVER} -DSTEP=install -P ${CMAKE_SOURCE_DIR}/cmake/BuildDxvk.cmake
    BUILD_BYPRODUCTS ${DXVK_OUTPUT_DIR}/d3d9.dll
    USES_TERMINAL_BUILD ON
)
