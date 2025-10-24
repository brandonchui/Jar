include(FetchContent)

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    message(STATUS "Configuring Google Test...")

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    message(STATUS " Google Test configured")

    function(add_jar_test TEST_NAME)
        add_executable(${TEST_NAME} ${ARGN})

        target_link_libraries(${TEST_NAME} PRIVATE
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
        )

        target_compile_features(${TEST_NAME} PRIVATE cxx_std_23)

        if(WIN32)
            target_compile_definitions(${TEST_NAME} PRIVATE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _CRT_SECURE_NO_WARNINGS
            )
        endif()

        gtest_discover_tests(${TEST_NAME})

        message(STATUS "   Added test: ${TEST_NAME}")
    endfunction()

else()
    message(STATUS "Tests disabled (BUILD_TESTS=OFF)")

    function(add_jar_test TEST_NAME)
    endfunction()
endif()
