add_executable(Jar
    main.cpp
    App.cpp
    App.h
    Renderer.cpp
    Renderer.h
    Mesh.cpp
    Mesh.h
    Material.h
    MaterialAsset.h
    ICamera.h
    Entity.h
    Entity.cpp
    Scene.h
    Scene.cpp
    OrbitCamera.cpp
    OrbitCamera.h
    graphics/Core.cpp
    graphics/Core.h
    graphics/CommandListManager.cpp
    graphics/CommandListManager.h
    graphics/CommandContext.cpp
    graphics/CommandContext.h
    graphics/SwapChain.cpp
    graphics/SwapChain.h
    graphics/GpuResource.cpp
    graphics/GpuResource.h
    graphics/GpuBuffer.cpp
    graphics/GpuBuffer.h
    graphics/UploadBuffer.cpp
    graphics/UploadBuffer.h
    graphics/PixelBuffer.cpp
    graphics/PixelBuffer.h
    graphics/ColorBuffer.cpp
    graphics/ColorBuffer.h
    graphics/DepthBuffer.cpp
    graphics/DepthBuffer.h
    graphics/DescriptorHeap.cpp
    graphics/DescriptorHeap.h
    graphics/Texture.cpp
    graphics/Texture.h
    graphics/StructuredBuffer.cpp
    graphics/StructuredBuffer.h
    graphics/slang/SlangCore.h
    graphics/slang/SlangTypes.h
    graphics/slang/SlangUtilities.h
    graphics/slang/SlangUtilities.cpp
    graphics/slang/SlangRootSignatureBuilder.h
    graphics/slang/SlangRootSignatureBuilder.cpp
    graphics/slang/SlangInputLayout.h
    graphics/slang/SlangInputLayout.cpp
    graphics/slang/SlangCompiler.h
    graphics/slang/SlangCompiler.cpp
    graphics/slang/SlangPSO.h
    graphics/slang/SlangPSO.cpp
    ui/UISystem.h
    ui/UISystem.cpp
    ui/Theme.h
    ui/Theme.cpp
    ui/widgets/Viewport.h
    ui/widgets/Viewport.cpp
    ui/widgets/Outliner.h
    ui/widgets/Outliner.cpp
    ui/widgets/TitleBar.h
    ui/widgets/TitleBar.cpp
    ui/widgets/Properties.h
    ui/widgets/Properties.cpp
    utils/MessageBox.cpp
    utils/MessageBox.h
)

### Platform-Specific Configuration
if(WIN32)
    # Need console for debug purposes
    set_target_properties(Jar PROPERTIES
        LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()

### Compiler Configuration
if(MSVC)
    target_compile_options(Jar PRIVATE
        /W4                 
        /permissive-        
        /Zc:__cplusplus    
        /utf-8             
        /wd4464             
        /wd4820            
        /wd5039           
        $<$<CONFIG:Debug>:/Zi>  
        $<$<CONFIG:Debug>:/Od>    
        $<$<CONFIG:Release>:/O2> 
    )
endif()


### Include Directories
# Add Agility SDK includes BEFORE everything else so the
# Agility SDK headers override the Windows ones.
if(AGILITY_SDK_FOUND)
    target_include_directories(Jar BEFORE PRIVATE
        ${AGILITY_SDK_PATH}/build/native/include
    )
    target_include_directories(Jar BEFORE PRIVATE
        ${AGILITY_SDK_PATH}/build/native/include/d3dx12
    )
endif()

target_include_directories(Jar PRIVATE
    ${CMAKE_BINARY_DIR}/_deps/vectormath-src
    ${CMAKE_BINARY_DIR}/_deps/d3d12memoryallocator-src/include
    ${CMAKE_BINARY_DIR}/_deps/sdl3-src/src/video
)

### Link Libraries
target_link_libraries(Jar PRIVATE
    SDL3::SDL3     
    D3D12MemoryAllocator
    spdlog::spdlog 
    imgui          
    nlohmann_json::nlohmann_json
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)

### Link vcpkg libraries
if(TARGET Microsoft::DirectXTK12)
    target_link_libraries(Jar PRIVATE Microsoft::DirectXTK12)
    message(STATUS "✓ Linked DirectXTK12 to Jar executable")
endif()

if(TARGET Microsoft::DirectXMesh)
    target_link_libraries(Jar PRIVATE Microsoft::DirectXMesh)
    message(STATUS "✓ Linked DirectXMesh to Jar executable")
endif()

### NuGet Package Configuration
configure_agility_sdk(Jar)
configure_pix_runtime(Jar)

### Slang Shader Compiler Configuration
# Configure Slang for shader compilation
configure_slang(Jar)
if(USE_SLANG)
    # Glob all .slang files in the shaders directory and compile
    file(GLOB SLANG_SHADERS "${CMAKE_SOURCE_DIR}/shaders/*.slang")

    foreach(SHADER_FILE ${SLANG_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
        add_slang_shader(Jar ${SHADER_FILE})
        message(STATUS "Added Slang shader: ${SHADER_NAME}")
    endforeach()
endif()

### Copy Shaders Directory
add_custom_command(
    TARGET Jar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:Jar>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:Jar>/shaders"
    COMMENT "Copying shaders directory to output folder"
)

### Copy Assets Directory
add_custom_command(
    TARGET Jar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:Jar>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Jar>/assets"
    COMMENT "Copying assets directory to output folder"
)
