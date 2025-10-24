cmake_minimum_required(VERSION 3.22)

### vcpkg Integration (must be before project()) - REQUIRED
# Check for vcpkg.json manifest file
if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
    message(STATUS "Found vcpkg.json manifest - vcpkg is REQUIRED")

    # Try to find vcpkg toolchain file
    set(VCPKG_FOUND FALSE)

    # Method 1: Check if CMAKE_TOOLCHAIN_FILE is already set (by VS or user)
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        if("${CMAKE_TOOLCHAIN_FILE}" MATCHES "vcpkg.cmake")
            message(STATUS "Using vcpkg from CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
            set(VCPKG_FOUND TRUE)
        endif()
    endif()

    # Method 2: Check VCPKG_ROOT environment variable
    if(NOT VCPKG_FOUND AND DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_TOOLCHAIN "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${VCPKG_TOOLCHAIN}")
            set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN}" CACHE STRING "vcpkg toolchain file")
            message(STATUS "Using vcpkg from VCPKG_ROOT: $ENV{VCPKG_ROOT}")
            set(VCPKG_FOUND TRUE)
        endif()
    endif()

    # Method 3: Check common VS2022 installation paths
    if(NOT VCPKG_FOUND)
        set(VS_VCPKG_PATHS
            "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
            "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
            "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
        )
        foreach(VCPKG_PATH ${VS_VCPKG_PATHS})
            if(EXISTS "${VCPKG_PATH}")
                set(CMAKE_TOOLCHAIN_FILE "${VCPKG_PATH}" CACHE STRING "vcpkg toolchain file")
                message(STATUS "Found vcpkg in Visual Studio: ${VCPKG_PATH}")
                set(VCPKG_FOUND TRUE)
                break()
            endif()
        endforeach()
    endif()

    # Fatal error if vcpkg not found
    if(NOT VCPKG_FOUND)
        message(FATAL_ERROR "\n"
            "vcpkg.json manifest found but vcpkg is not configured!\n"
            "\n"
            "Please do one of the following:\n"
            "  1. Open this project using Visual Studio 2022 Developer Command Prompt\n"
            "  2. Set VCPKG_ROOT environment variable to your vcpkg installation\n"
            "  3. Run: vcpkg integrate install\n"
            "  4. Or specify CMAKE_TOOLCHAIN_FILE when configuring:\n"
            "     cmake -B build -DCMAKE_TOOLCHAIN_FILE=\"path/to/vcpkg/scripts/buildsystems/vcpkg.cmake\"\n"
            "\n"
            "If vcpkg is not installed, install it from: https://vcpkg.io/en/getting-started\n"
        )
    endif()
else()
    message(WARNING "No vcpkg.json found - vcpkg dependencies will not be available")
endif()

### Project Configuration
project(Jar
    VERSION 1.0.0
    DESCRIPTION "Just Another Renderer"
    LANGUAGES CXX C
)

### C++ Standard Configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

### Windows Optimization Defines
if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN 
        NOMINMAX           
        _CRT_SECURE_NO_WARNINGS 
    )
endif()

### Build Configuration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

### Platform Requirements
if(NOT WIN32)
    message(FATAL_ERROR "This project is DirectX 12 only and requires Windows")
endif()

### Dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(NuGetRestore)
include(FetchSlang)
include(FetchGTest)

nuget_restore()

#--------------------  Fetch Content ----------------------------
### SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
set(SDL_STATIC OFF CACHE BOOL "Build SDL as shared library")
set(SDL_SHARED ON CACHE BOOL "Build SDL as shared library")
set(SDL_TEST OFF CACHE BOOL "Don't build SDL tests")

### VectorMath
FetchContent_Declare(
    vectormath
    GIT_REPOSITORY https://github.com/glampert/vectormath.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

### D3D12MemoryAllocator
FetchContent_Declare(
    D3D12MemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

### spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
    GIT_SHALLOW TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Don't build spdlog examples")
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Don't build spdlog tests")
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as static library")

### nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")

### Dear ImGui DOCK
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/brandonchui/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)

# Make all dependencies available
FetchContent_MakeAvailable(SDL3 vectormath D3D12MemoryAllocator spdlog json imgui)

# Create ImGui library target
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 d3d12.lib dxgi.lib)

### vcpkg Dependencies
find_package(directxtk12 CONFIG QUIET)
find_package(directxmesh CONFIG QUIET)

if(directxtk12_FOUND)
    message(STATUS "Found DirectXTK12 via vcpkg")
else()
    message(STATUS "ℹDirectXTK12 not found - install with: vcpkg install directxtk12:x64-windows")
endif()

if(directxmesh_FOUND)
    message(STATUS "Found DirectXMesh via vcpkg")
else()
    message(STATUS "DirectXMesh not found - install with: vcpkg install directxmesh:x64-windows")
endif()

add_subdirectory(src)

### Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

