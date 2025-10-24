if(NOT BUILD_TESTS)
    return()
endif()

message(STATUS "Configuring unit tests...")

add_jar_test(basic_tests
    BasicTest.cpp
)

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS basic_tests
    COMMENT "Running all tests..."
)
