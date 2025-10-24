<!-- LOGO -->
<h1>
<p align="center">
  <br>Jar (Just Another Renderer)
  
</h1>
  <p align="center">
    An experimental DirectX 12 renderer
    <br />
    <a href="#about">About</a>
    ·
    <a href="#building">Building</a>
    ·
    <a href="#dependencies">Dependencies</a>
    ·
    <a href="#similar-projects">Similar Projects</a>
  </p>
</p>

<p align="center">
<img width="761" height="481" alt="Screenshot_6289" src="https://github.com/user-attachments/assets/b0acb876-e4c5-4e13-936a-ec951ecca328" />
</p>






> [!WARNING]
> **This project is highly experimental.**
> - This project is mainly a learning exercise to explore graphics techniques and will probably have many bugs or unsupported features.

## About

Jar is a DirectX 12 only renderer built as a personal playground for exploring computer graphics. Currently, it features a PBR pipeline, Slang shader compilation, and a minimal editor interface. The initial abstraction was inspired by the [Microsoft MiniEngine](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine). The goal of this project is to provide an easy to use tool for 3D model viewing.

This is a learning project and personal renderer, prioritizing experimentation over production readiness.

## Requirements

- **Windows only**
- Visual Studio 2022
- C++23
- CMake 3.22+
- Ninja 
- vcpkg for Windows-specific dependencies


## Building

The project uses a PowerShell `build.ps1`  script that automatically downloads all dependencies (assuming vcpkg installed).
```powershell
# Build and run
.\build.ps1 -run

.\build.ps1 -build

.\build.ps1 -build -release

.\build.ps1 -clean

.\build.ps1 -tests
```

## Dependencies

| Library | Purpose |
|---------|---------|
| [SDL3](https://github.com/libsdl-org/SDL) | Window management and input |
| [Slang](https://shader-slang.com/) | Shader compiler with reflection |
| [D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator) | GPU memory allocation |
| [VectorMath](https://github.com/glampert/vectormath) | SIMD-optimized math |
| [spdlog](https://github.com/gabime/spdlog) | Logging |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing |
| [Dear ImGui](https://github.com/ocornut/imgui) | UI (docking branch) |
| [GoogleTest](https://github.com/google/googletest) | Unit testing |

### vcpkg Dependencies
```bash
vcpkg install directxtk12:x64-windows
vcpkg install directxmesh:x64-windows
```

### NuGet Dependencies
- Microsoft.Direct3D.D3D12 (Agility SDK)
- WinPixEventRuntime (PIX profiling support)

## Project Status
These are the major features that have higher priority and will get more development time. This table will get updated as needed with more or less features.

| Feature | Status |
|---------|:------:|
| Forward rendering | ✅ |
| Slang Integration | ✅ |
| Deferred rendering | ⚠️ |
| Image based lighting | ❌ |
| Cluster rendering | ❌ |


> [!NOTE]
> Features marked ⚠️ are partially implemented or broken. Features marked ❌ are planned but not yet started.

## Similar Projects
- [Microsoft MiniEngine](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine) - Microsoft's example of the DirectX 12 API with an engine.


## License

MIT License - See [LICENSE](LICENSE) file for details.


