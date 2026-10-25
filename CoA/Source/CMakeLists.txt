file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(CoA STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(CoA
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)
