file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(CoATests ${TEST_SOURCES})

target_include_directories(CoATests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(CoATests
    PRIVATE
        CoA
        Base
        Storm
        BlizzardCore
        lua
        Os
        ConsoleBase
        BLPFile
        Images
        Gx
        Tempest
)

add_test(NAME CoATests COMMAND CoATests)

# x64: the Streamline SDK headers (sl_consts.h) for the DLSS frame-blob layout test (DlssFrame.h).
if (TARGET streamline_headers)
    target_link_libraries(CoATests PRIVATE streamline_headers)
endif ()
