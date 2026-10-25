file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(CoATests ${TEST_SOURCES})

target_include_directories(CoATests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(CoATests
    PRIVATE
        CoA
)

add_test(NAME CoATests COMMAND CoATests)
