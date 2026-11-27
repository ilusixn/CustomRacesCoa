# NVIDIA Streamline SDK v2.14.16 (DLSS Super Resolution / DLAA for the x64 client, loaded at runtime beside DXVK): the
# SDK's headers (INTERFACE target streamline_headers) and NVIDIA's signed production DLLs, staged into bin/Win/x64 next
# to DXVK's d3d9.dll. The SDK zip (~276 MB) is downloaded once into the build dir and only the headers and the four
# production DLLs are extracted; nothing of it is checked in. Never the SDK's bin/x64/development DLLs.
set(STREAMLINE_VERSION 2.14.16)
set(STREAMLINE_URL https://github.com/NVIDIA-RTX/Streamline/releases/download/v${STREAMLINE_VERSION}/streamline-sdk-v${STREAMLINE_VERSION}.zip)
set(STREAMLINE_SHA256 32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b)

# Empty: download the pinned release. Set to a local extracted SDK (with include/ and bin/x64/) to build without network.
set(WOWDECOMP_STREAMLINE_SDK_DIR "" CACHE PATH "Local extracted Streamline SDK v${STREAMLINE_VERSION} to use instead of downloading")

set(STREAMLINE_DLLS sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll sl.dlss_g.dll nvngx_dlssg.dll sl.reflex.dll
    sl.pcl.dll NvLowLatencyVk.dll)
set(STREAMLINE_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/Win/x64)

if (WOWDECOMP_STREAMLINE_SDK_DIR)
    set(STREAMLINE_SDK ${WOWDECOMP_STREAMLINE_SDK_DIR})
else ()
    set(STREAMLINE_SDK ${CMAKE_CURRENT_BINARY_DIR}/sdk)
    set(STREAMLINE_ZIP ${CMAKE_CURRENT_BINARY_DIR}/streamline-sdk-v${STREAMLINE_VERSION}.zip)
    # Keyed on the DLL list too, so adding a DLL re-extracts.
    string(MD5 STREAMLINE_DLLS_HASH "${STREAMLINE_DLLS}")
    set(STREAMLINE_STAMP ${STREAMLINE_SDK}/extracted-${STREAMLINE_SHA256}-${STREAMLINE_DLLS_HASH}.stamp)

    # The stamp keeps later configures from hashing the zip again; file(DOWNLOAD) itself also reuses a cached zip whose
    # hash matches.
    if (NOT EXISTS ${STREAMLINE_STAMP})
        message(STATUS "Streamline: fetching SDK v${STREAMLINE_VERSION} (about 276 MB, cached in ${CMAKE_CURRENT_BINARY_DIR})")
        file(DOWNLOAD ${STREAMLINE_URL} ${STREAMLINE_ZIP}
            EXPECTED_HASH SHA256=${STREAMLINE_SHA256}
            SHOW_PROGRESS
            STATUS STREAMLINE_DOWNLOAD_STATUS
        )
        list(GET STREAMLINE_DOWNLOAD_STATUS 0 STREAMLINE_DOWNLOAD_CODE)
        if (NOT STREAMLINE_DOWNLOAD_CODE EQUAL 0)
            file(REMOVE ${STREAMLINE_ZIP})
            message(FATAL_ERROR "Streamline: downloading ${STREAMLINE_URL} failed (${STREAMLINE_DOWNLOAD_STATUS}); set WOWDECOMP_STREAMLINE_SDK_DIR to a local extracted SDK to build offline.")
        endif ()

        file(REMOVE_RECURSE ${STREAMLINE_SDK})
        set(STREAMLINE_PATTERNS include)
        foreach (STREAMLINE_DLL IN LISTS STREAMLINE_DLLS)
            list(APPEND STREAMLINE_PATTERNS bin/x64/${STREAMLINE_DLL})
        endforeach ()
        file(ARCHIVE_EXTRACT INPUT ${STREAMLINE_ZIP} DESTINATION ${STREAMLINE_SDK} PATTERNS ${STREAMLINE_PATTERNS})
        file(WRITE ${STREAMLINE_STAMP} "${STREAMLINE_SHA256}\n")
    endif ()
endif ()

if (NOT EXISTS ${STREAMLINE_SDK}/include/sl.h)
    message(FATAL_ERROR "Streamline: ${STREAMLINE_SDK}/include/sl.h not found")
endif ()

add_library(streamline_headers INTERFACE)

target_include_directories(streamline_headers
    INTERFACE
        ${STREAMLINE_SDK}/include
)

set(STREAMLINE_STAGED)
foreach (STREAMLINE_DLL IN LISTS STREAMLINE_DLLS)
    set(STREAMLINE_DLL_PATH ${STREAMLINE_SDK}/bin/x64/${STREAMLINE_DLL})
    if (NOT EXISTS ${STREAMLINE_DLL_PATH})
        message(FATAL_ERROR "Streamline: ${STREAMLINE_DLL_PATH} not found")
    endif ()
    list(APPEND STREAMLINE_STAGED ${STREAMLINE_OUTPUT_DIR}/${STREAMLINE_DLL})
    add_custom_command(
        OUTPUT ${STREAMLINE_OUTPUT_DIR}/${STREAMLINE_DLL}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STREAMLINE_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STREAMLINE_DLL_PATH} ${STREAMLINE_OUTPUT_DIR}/${STREAMLINE_DLL}
        DEPENDS ${STREAMLINE_DLL_PATH}
        VERBATIM
    )
endforeach ()

# Wow depends on this (root CMakeLists), so a normal build stages the DLLs beside DXVK's d3d9.dll.
add_custom_target(StreamlineDlls DEPENDS ${STREAMLINE_STAGED})
