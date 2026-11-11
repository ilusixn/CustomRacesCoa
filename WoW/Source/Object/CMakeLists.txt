file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(Object STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Object
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# Unit_C's retail rider poses (header-only MountOverrides.h); not WOWDECOMP_RETAIL_ASSETS, which also turns on the Ethereal blocks.
target_compile_definitions(Object PRIVATE COA_MOUNT_SKINS=1)
