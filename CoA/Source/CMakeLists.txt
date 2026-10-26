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
)

# Own HLSL shaders compiled by fxc at build time (headers in ${CMAKE_BINARY_DIR}/generated/CoA/Shaders).
include(${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/FullscreenVS.hlsl PROFILE vs_3_0 NAME kFullscreenVS)
coa_embed_shader(TARGET CoA FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/Shaders/DepthViewPS.hlsl PROFILE ps_3_0 NAME kDepthViewPS)
