include(FetchContent)

option(USE_SLANG "Build with Slang shader compiler support" ON)
option(USE_SLANG_PREBUILT "Use prebuilt Slang binaries instead of building from source" ON)

if(USE_SLANG)
    if(USE_SLANG_PREBUILT)
        message(STATUS "Using prebuilt Slang binaries...")

        set(SLANG_VERSION "2025.21.2")
        set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-windows-x86_64.zip")

        set(SLANG_PREBUILT_DIR "${CMAKE_BINARY_DIR}/_deps/slang_prebuilt-src")

        FetchContent_Declare(
            slang_prebuilt
            URL ${SLANG_URL}
            SOURCE_DIR ${SLANG_PREBUILT_DIR}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )

        # Only download if not already present
        FetchContent_GetProperties(slang_prebuilt)
        if(NOT slang_prebuilt_POPULATED)
            message(STATUS "Downloading Slang prebuilt binaries (one-time download)...")
            FetchContent_MakeAvailable(slang_prebuilt)
        else()
            message(STATUS "Using cached Slang binaries from ${SLANG_PREBUILT_DIR}")
        endif()

        # Set paths for prebuilt binaries
        set(SLANG_ROOT "${slang_prebuilt_SOURCE_DIR}")
        set(SLANG_INCLUDE_DIR "${SLANG_ROOT}/include")
        set(SLANG_LIB_DIR "${SLANG_ROOT}/lib")
        set(SLANG_BIN_DIR "${SLANG_ROOT}/bin")

    else()
        message(STATUS "Building Slang from source (one-time build)...")
        message(WARNING "Building Slang from source requires significant time and disk space")

        # Set source and build directories for caching
        set(SLANG_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/slang-src")
        set(SLANG_BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/slang-build")

        FetchContent_Declare(
            slang
            GIT_REPOSITORY https://github.com/shader-slang/slang.git
            GIT_TAG        v${SLANG_VERSION}
            GIT_SHALLOW    TRUE
            GIT_SUBMODULES_RECURSE TRUE
            SOURCE_DIR     ${SLANG_SOURCE_DIR}
            BINARY_DIR     ${SLANG_BINARY_DIR}
            UPDATE_DISCONNECTED TRUE
        )

        set(SLANG_ENABLE_TESTS OFF CACHE BOOL "Disable Slang tests")
        set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "Disable Slang examples")
        set(SLANG_ENABLE_GFX OFF CACHE BOOL "Disable GFX library (we only need the compiler)")
        set(SLANG_LIB_TYPE STATIC CACHE STRING "Build Slang as static library")

        FetchContent_GetProperties(slang)
        if(NOT slang_POPULATED)
            message(STATUS "Cloning and building Slang (this will take a while on first run)...")
            FetchContent_MakeAvailable(slang)
        else()
            message(STATUS "Using cached Slang build from ${SLANG_BINARY_DIR}")
        endif()
    endif()

    function(configure_slang target)
        if(USE_SLANG_PREBUILT)
            target_include_directories(${target} PRIVATE ${SLANG_INCLUDE_DIR})
            target_link_directories(${target} PRIVATE ${SLANG_LIB_DIR})
            target_link_libraries(${target} PRIVATE slang)

            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SLANG_BIN_DIR}"
                "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Copying Slang runtime DLLs to output directory"
            )

            target_compile_definitions(${target} PRIVATE HAS_SLANG)
            message(STATUS "Configured prebuilt Slang for ${target}")
        elseif(TARGET slang)

            target_link_libraries(${target} PRIVATE slang)
            target_include_directories(${target} PRIVATE ${slang_SOURCE_DIR}/include)
            target_compile_definitions(${target} PRIVATE HAS_SLANG)
            message(STATUS "Configured Slang from source for ${target}")
        else()
            message(WARNING "Slang not configured properly")
        endif()
    endfunction()

    function(add_slang_shader TARGET SHADER_FILE)
        if(IS_ABSOLUTE ${SHADER_FILE})
            set(SHADER_PATH ${SHADER_FILE})
            get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        else()
            set(SHADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}")
            get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        endif()

        set(SLANG_DEST "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.slang")

        add_custom_command(
            OUTPUT ${SLANG_DEST}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_PATH} ${SLANG_DEST}
            DEPENDS ${SHADER_PATH}
            COMMENT "Copying shader source: ${SHADER_NAME}.slang"
            VERBATIM
        )

        set_source_files_properties(${SLANG_DEST} PROPERTIES GENERATED TRUE)
        target_sources(${TARGET} PRIVATE ${SLANG_DEST})

        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SLANG_DEST}
            "$<TARGET_FILE_DIR:${TARGET}>/shaders/${SHADER_NAME}.slang"
            COMMENT "Installing shader: ${SHADER_NAME}.slang"
        )

        message(STATUS "Added Slang shader: ${SHADER_NAME}.slang")
    endfunction()

    function(add_slang_shader_hlsl TARGET SHADER_FILE)
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.hlsl")

        add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
            COMMAND slangc
                -profile sm_6_6
                -target hlsl
                -o ${OUTPUT_FILE}
                ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
            COMMENT "Compiling Slang shader to HLSL: ${SHADER_FILE}"
        )

        target_sources(${TARGET} PRIVATE ${OUTPUT_FILE})
    endfunction()

else()
    message(STATUS "Slang shader compiler disabled")

    function(configure_slang target)
    endfunction()

    function(add_slang_shader TARGET SHADER_FILE)
    endfunction()
endif()
