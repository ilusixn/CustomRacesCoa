file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(CoA STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(CoA
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# FontRenderClient.cpp: FreeType 2.14.1 (AscFontRender).
target_link_libraries(CoA
    PRIVATE
        freetype
        nvapi
)

# NVIDIA Streamline SDK headers (sl.h, sl_security.h, ...) for the x64 DLSS integration.
# StreamlineClient.cpp / DlssClient.cpp also need the Vulkan headers (sl_helpers_vk.h, coa_dxvk_streamline.h): DXVK's
# bundled ones (its checkout's include/vulkan/include, ThirdParty/dxvk) when DXVK is built, else the Vulkan SDK's. They
# also need WinVerifyTrust for the interposer's signature and the build tag (WaystoneVersion.h) as Streamline's
# engineVersion. Without the SDK (x86) or without Vulkan headers the glue compiles to a stub that leaves DLSS off.
if (TARGET streamline_headers)
    get_property(COA_DXVK_VULKAN_INCLUDE GLOBAL PROPERTY COA_DXVK_VULKAN_INCLUDE)
    if (COA_DXVK_VULKAN_INCLUDE AND TARGET dxvk-download)
        set(COA_VULKAN_INCLUDE ${COA_DXVK_VULKAN_INCLUDE})
        add_dependencies(CoA dxvk-download)
    else ()
        file(GLOB COA_VULKAN_SDK_INCLUDES LIST_DIRECTORIES true "C:/VulkanSDK/*/Include")
        find_path(COA_VULKAN_INCLUDE_DIR vulkan/vulkan.h HINTS "$ENV{VULKAN_SDK}/Include" ${COA_VULKAN_SDK_INCLUDES})
        set(COA_VULKAN_INCLUDE ${COA_VULKAN_INCLUDE_DIR})
    endif ()
    if (COA_VULKAN_INCLUDE)
        message(STATUS "Streamline: Vulkan headers from ${COA_VULKAN_INCLUDE}")
        target_include_directories(CoA PRIVATE ${COA_VULKAN_INCLUDE})
        target_link_libraries(CoA PRIVATE streamline_headers wintrust)
        target_compile_definitions(CoA PRIVATE COA_STREAMLINE=1)
        add_dependencies(CoA WaystoneVersion)
    else ()
        message(WARNING "Streamline: no Vulkan headers (build DXVK or install the Vulkan SDK); DLSS is built as a stub.")
    endif ()
endif ()

# Own HLSL shaders compiled by fxc at build time (headers in ${CMAKE_BINARY_DIR}/generated/CoA/Shaders).
include(${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/FullscreenVS.hlsl PROFILE vs_3_0 NAME kFullscreenVS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/DepthViewPS.hlsl PROFILE ps_3_0 NAME kDepthViewPS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/StaticMotionPS.hlsl PROFILE ps_3_0 NAME kStaticMotionPS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/SkinnedMotionVS.hlsl PROFILE vs_3_0 NAME kSkinnedMotionVS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/MotionPS.hlsl PROFILE ps_3_0 NAME kMotionPS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/MotionKeyPS.hlsl PROFILE ps_3_0 NAME kMotionKeyPS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/BiasMaskPS.hlsl PROFILE ps_3_0 NAME kBiasMaskPS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/DepthRemapPS.hlsl PROFILE ps_3_0 NAME kDepthRemapPS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/MotionViewPS.hlsl PROFILE ps_3_0 NAME kMotionViewPS)
