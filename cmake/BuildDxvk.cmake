# DXVK meson / ninja driver for ThirdParty/dxvk (run with cmake -P from the ExternalProject steps).
#   -DSTEP=configure|build|install -DSOURCE_DIR=<dxvk checkout> -DBINARY_DIR=<meson build dir>
#   -DMESON=<meson.exe> -DNINJA=<ninja.exe> -DGLSLANG=<glslangValidator.exe> -DOUTPUT_DIR=<dir for d3d9.dll>
# meson needs the MSVC x64 environment (vcvars64) the CMake build runs in; it finds ninja and glslangValidator on
# PATH, which is why their directories are put in front of it here.

get_filename_component(NINJA_DIR ${NINJA} DIRECTORY)
get_filename_component(GLSLANG_DIR ${GLSLANG} DIRECTORY)
file(TO_NATIVE_PATH "${NINJA_DIR}" NINJA_DIR)
file(TO_NATIVE_PATH "${GLSLANG_DIR}" GLSLANG_DIR)
set(ENV{PATH} "${NINJA_DIR};${GLSLANG_DIR};$ENV{PATH}")

if (STEP STREQUAL "configure")
    # The x64 compiler must be the one meson picks up: a Win32 (vcvars32) environment would build a 32-bit d3d9.dll.
    if (NOT "$ENV{VSCMD_ARG_TGT_ARCH}" STREQUAL "x64")
        message(FATAL_ERROR "DXVK: the build must run in an MSVC x64 environment (vcvars64); VSCMD_ARG_TGT_ARCH='$ENV{VSCMD_ARG_TGT_ARCH}'")
    endif ()

    set(SETUP_ARGS
        --backend=ninja
        --buildtype=release
        -Denable_d3d8=false
        -Denable_d3d9=true
        -Denable_d3d10=false
        -Denable_d3d11=false
        -Denable_dxgi=false
    )

    if (EXISTS ${BINARY_DIR}/build.ninja)
        list(APPEND SETUP_ARGS --reconfigure)
    endif ()

    execute_process(
        COMMAND ${MESON} setup ${SETUP_ARGS} ${BINARY_DIR} ${SOURCE_DIR}
        RESULT_VARIABLE RESULT
    )
elseif (STEP STREQUAL "build")
    execute_process(
        COMMAND ${NINJA} -C ${BINARY_DIR}
        RESULT_VARIABLE RESULT
    )
elseif (STEP STREQUAL "install")
    file(MAKE_DIRECTORY ${OUTPUT_DIR})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BINARY_DIR}/src/d3d9/d3d9.dll ${OUTPUT_DIR}/d3d9.dll
        RESULT_VARIABLE RESULT
    )
else ()
    message(FATAL_ERROR "DXVK: unknown STEP '${STEP}'")
endif ()

if (NOT RESULT EQUAL 0)
    message(FATAL_ERROR "DXVK: ${STEP} failed (${RESULT})")
endif ()
