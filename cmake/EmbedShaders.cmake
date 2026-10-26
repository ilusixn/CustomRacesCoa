# Compiles own HLSL shaders to D3D9 SM3 bytecode with fxc at build time and embeds them as C arrays.
#   coa_embed_shader(TARGET <lib> FILE <hlsl> PROFILE <vs_3_0|ps_3_0> NAME <kName>)
# generates ${CMAKE_BINARY_DIR}/generated/CoA/Shaders/<Name without leading k>.h (include as "CoA/Shaders/<Name>.h")
# holding `static const unsigned char <kName>[]`. Unlike DXVK, the shaders are required, so a missing fxc is fatal.

file(GLOB COA_FXC_SDK_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
list(SORT COA_FXC_SDK_DIRS COMPARE NATURAL ORDER DESCENDING) # newest SDK first
find_program(COA_FXC fxc HINTS "$ENV{WindowsSdkVerBinPath}/x64" ${COA_FXC_SDK_DIRS})
if (NOT COA_FXC)
    message(FATAL_ERROR "fxc.exe not found (Windows 10 SDK required to embed the CoA shaders)")
endif ()

set(COA_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/CoA/Shaders")

function(coa_embed_shader)
    cmake_parse_arguments(ARG "" "TARGET;FILE;PROFILE;NAME" "" ${ARGN})
    string(REGEX REPLACE "^k" "" HEADER_NAME "${ARG_NAME}")
    set(OUT "${COA_SHADER_OUTPUT_DIR}/${HEADER_NAME}.h")
    add_custom_command(
        OUTPUT "${OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${COA_SHADER_OUTPUT_DIR}"
        COMMAND "${COA_FXC}" /nologo /O3 /T ${ARG_PROFILE} /E main /Vn ${ARG_NAME} /Fh "${OUT}" "${ARG_FILE}"
        COMMAND ${CMAKE_COMMAND} -DHEADER=${OUT} -P "${CMAKE_SOURCE_DIR}/cmake/PatchShaderHeader.cmake"
        DEPENDS "${ARG_FILE}" "${CMAKE_SOURCE_DIR}/cmake/PatchShaderHeader.cmake"
        COMMENT "fxc ${ARG_PROFILE}: ${ARG_FILE}"
        VERBATIM)
    target_sources(${ARG_TARGET} PRIVATE "${OUT}")
    target_include_directories(${ARG_TARGET} PUBLIC "${CMAKE_BINARY_DIR}/generated")
endfunction()
