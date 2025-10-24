function(nuget_restore)
    find_program(NUGET_EXE nuget
        PATHS
            "$ENV{LOCALAPPDATA}/NuGet"
            "$ENV{ProgramFiles}/NuGet"
            "$ENV{PROGRAMFILES}/NuGet"
    )

    if(NOT NUGET_EXE)
        message(STATUS "NuGet not found. Attempting to download...")
        set(NUGET_URL "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe")
        set(NUGET_EXE "${CMAKE_BINARY_DIR}/nuget.exe")

        if(NOT EXISTS ${NUGET_EXE})
            file(DOWNLOAD ${NUGET_URL} ${NUGET_EXE} STATUS download_status)
            list(GET download_status 0 status_code)
            if(NOT status_code EQUAL 0)
                message(WARNING "Failed to download NuGet. Agility SDK and PIX runtime will not be available.")
                return()
            endif()
        endif()
    endif()

    if(NUGET_EXE AND EXISTS "${CMAKE_SOURCE_DIR}/packages.config")
        message(STATUS "Restoring NuGet packages...")
        execute_process(
            COMMAND ${NUGET_EXE} restore "${CMAKE_SOURCE_DIR}/packages.config" -PackagesDirectory "${CMAKE_BINARY_DIR}/packages"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE nuget_result
        )

        if(nuget_result EQUAL 0)
            message(STATUS "NuGet packages restored successfully")

            # Set up Agility SDK
            set(AGILITY_SDK_PATH "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.D3D12.1.616.1" PARENT_SCOPE)
            set(AGILITY_SDK_FOUND TRUE PARENT_SCOPE)

            # Set up PIX Runtime
            set(PIX_RUNTIME_PATH "${CMAKE_BINARY_DIR}/packages/WinPixEventRuntime.1.0.240308001" PARENT_SCOPE)
            set(PIX_RUNTIME_FOUND TRUE PARENT_SCOPE)
        else()
            message(WARNING "NuGet restore failed")
        endif()
    endif()
endfunction()

# Configure Agility SDK
function(configure_agility_sdk target)
    if(AGILITY_SDK_FOUND)
        target_include_directories(${target} BEFORE PRIVATE
            "${AGILITY_SDK_PATH}/build/native/include"
        )

        target_compile_definitions(${target} PRIVATE D3D12_SDK_VERSION=616)

        target_link_directories(${target} PRIVATE
            "${AGILITY_SDK_PATH}/build/native/bin/x64"
        )

        # Copy D3D12Core.dll to output directory for runtime
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
            "$<TARGET_FILE_DIR:${target}>/D3D12/D3D12Core.dll"
            COMMENT "Copying D3D12 Agility SDK runtime to output directory"
        )

        # Copy D3D12SDKLayers.dll for debug builds
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${AGILITY_SDK_PATH}/build/native/bin/x64/D3D12SDKLayers.dll"
                "$<TARGET_FILE_DIR:${target}>/D3D12/D3D12SDKLayers.dll"
                COMMENT "Copying D3D12 SDK Layers for debugging"
            )
        endif()

        message(STATUS "Configured D3D12 Agility SDK for ${target}")
    endif()
endfunction()

# Configure PIX Runtime
function(configure_pix_runtime target)
    if(PIX_RUNTIME_FOUND)
        target_include_directories(${target} PRIVATE
            "${PIX_RUNTIME_PATH}/Include"
        )

        target_link_libraries(${target} PRIVATE
            "${PIX_RUNTIME_PATH}/bin/x64/WinPixEventRuntime.lib"
        )

        # Copy WinPixEventRuntime.dll to output directory
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PIX_RUNTIME_PATH}/bin/x64/WinPixEventRuntime.dll"
            "$<TARGET_FILE_DIR:${target}>/WinPixEventRuntime.dll"
            COMMENT "Copying PIX Event Runtime to output directory"
        )

        target_compile_definitions(${target} PRIVATE USE_PIX)

        message(STATUS "Configured PIX Runtime for ${target}")
    endif()
endfunction()
