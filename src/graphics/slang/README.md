# Slang Shader Compilation Module

This module provides automatic shader compilation and D3D12 pipeline setup using the [Slang shader compiler](https://github.com/shader-slang/slang).

## Overview

The Slang module automatically generates:
- **DXIL bytecode** (vertex, pixel, compute shaders)
- **Root signatures** from shader reflection
- **Input layouts** from vertex shader parameters
- **Pipeline State Objects (PSOs)** with sensible defaults

All you need is a `.slang` shader file, and the module handles the rest.

## Files

### Public API

#### `SlangCore.h`
**Main entry point** - Include this header to access all SlangHelper functionality.

```cpp
#include "graphics/slang/SlangCore.h"

// Now you have access to all SlangHelper functions
auto shaderData = SlangHelper::CompileShaderForPSO(path, device);
```

This header aggregates all other headers for convenient access.

---

### Data Structures

#### `SlangTypes.h`
Defines `CompiledShaderData` - the container for all compiled shader artifacts:

```cpp
struct CompiledShaderData {
    std::vector<uint8_t> vertexBytecode;    // Compiled vertex shader DXIL
    std::vector<uint8_t> pixelBytecode;     // Compiled pixel shader DXIL
    std::vector<uint8_t> computeBytecode;   // Compiled compute shader DXIL
    ID3D12RootSignature* rootSignature;     // Generated root signature
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;  // Vertex input layout
};
```

---

### Utilities

#### `SlangUtilities.h` / `SlangUtilities.cpp`
Provides helper functions and logger access:

- `GetLogger()` - Returns the spdlog logger (named "SlangHelper")
- `GetBindingTypeName()` - Converts Slang binding types to readable strings
- `GetStageName()` - Converts shader stages to readable strings

**Example:**
```cpp
SlangHelper::GetLogger()->info("Compiling shader...");
```

---

### Core Compilation

#### `SlangCompiler.h` / `SlangCompiler.cpp`
**Main compilation function** - Orchestrates the entire shader compilation pipeline.

**Function:**
```cpp
CompiledShaderData CompileShaderForPSO(
    const std::filesystem::path& shaderPath,
    ID3D12Device* device
);
```

**What it does:**
1. Loads and parses the `.slang` file
2. Creates Slang compilation session (DXIL target, SM 6.6)
3. Finds entry points: `vertexMain`, `pixelMain`, `computeMain`
4. Compiles to DXIL bytecode
5. Extracts input layout from vertex shader reflection
6. **Uses `RootSignatureBuilder` to generate optimized root signature** (reflection-driven)
7. Returns `CompiledShaderData` with everything needed for PSO creation

**Example:**
```cpp
auto shaderData = SlangHelper::CompileShaderForPSO("shaders/myshader.slang", device);
// shaderData now contains bytecode, root sig, input layout, etc.
```

---

### Root Signature Generation

#### `SlangRootSignatureBuilder.h` / `SlangRootSignatureBuilder.cpp`
**Reflection-driven root signature builder** - Automatically generates optimized D3D12 root signatures using Slang's reflection API.

**Design Philosophy:**
The builder uses **size-based heuristics** to choose the most efficient root signature layout:
- **Root Constants** (≤16 bytes): 1 DWORD per 4 bytes - fastest, inline in root signature
- **Root Descriptors** (17-256 bytes): 2 DWORDs - fast, direct GPU address binding
- **Descriptor Tables** (>256 bytes or resources): 1 DWORD - flexible, supports unbounded arrays

**Class Interface:**
```cpp
class RootSignatureBuilder {
public:
    RootSignatureBuilder(slang::ProgramLayout* layout, ID3D12Device* device);
    HRESULT Build();  // Analyzes reflection data and creates root signature
    ID3D12RootSignature* GetRootSignature() const;
};
```

**Three-Phase Build Process:**

1. **Phase 1: Collect Resources**
   - Traverses shader reflection data via `slang::ProgramLayout`
   - Identifies all shader parameters (CBVs, SRVs, UAVs, Samplers)
   - Handles `ParameterBlock<T>` sub-element register spaces
   - Groups resources by register space and type

2. **Phase 2: Build Root Parameters**
   - Applies size-based heuristics to choose root parameter types
   - Groups contiguous resources into descriptor table ranges
   - Creates static samplers for `SamplerState` (0 DWORD cost)
   - Calculates total root signature DWORD cost

3. **Phase 3: Create Root Signature**
   - Serializes D3D12 root signature descriptor
   - Creates the final `ID3D12RootSignature` object
   - Logs resource bindings and total DWORD cost

**Supported Resources:**
- **Constant Buffers (CBV)** - `ConstantBuffer<T>` → Root descriptor (small) or Descriptor table (large)
- **Textures** - `Texture2D`, etc. → Descriptor table (SRV)
- **RW Textures** - `RWTexture2D`, etc. → Descriptor table (UAV)
- **Structured Buffers** - `StructuredBuffer<T>` → Descriptor table (SRV)
- **RW Structured Buffers** - `RWStructuredBuffer<T>` → Descriptor table (UAV)
- **Samplers** - `SamplerState` → Static sampler (0 DWORD cost)
- **Parameter Blocks** - `ParameterBlock<Material>` → Automatic sub-space handling

**ParameterBlock Handling:**
The builder automatically detects `ParameterBlock<T>` and places resources in their sub-element register space:
```hlsl
ParameterBlock<Material> material;  // Space 2
```
- Material's textures → `t# space2`
- Material's samplers → `s# space2`
- Material's uniforms → `b0 space2` (ParameterBlock CBV)

**Example Usage (Internal):**
```cpp
// In SlangCompiler.cpp
RootSignatureBuilder builder(layout, device);
if (SUCCEEDED(builder.Build())) {
    result.rootSignature = builder.GetRootSignature();
    result.rootSignature->AddRef();
}
```

**Benefits:**
- ✅ No hardcoded shader detection
- ✅ Automatic adaptation to shader changes
- ✅ Optimal root signature layout based on resource sizes
- ✅ Proper ParameterBlock sub-space handling
- ✅ Follows D3D12 best practices (see `_root_sig_ref.cpp`)

---

#### `SlangRootSignature.h` / `SlangRootSignature.cpp` *(Deprecated)*
**Legacy root signature generation** - Retained for reference but no longer used.

Use `SlangRootSignatureBuilder` instead for all new code.

---

### Input Layout Extraction

#### `SlangInputLayout.h` / `SlangInputLayout.cpp`
Extracts vertex input layout from vertex shader reflection.

**Function:**
```cpp
std::vector<D3D12_INPUT_ELEMENT_DESC>
ExtractInputLayoutFromReflection(slang::EntryPointReflection* entryPoint);
```

**What it does:**
- Reads vertex shader parameter semantics (`POSITION`, `NORMAL`, `TEXCOORD`, etc.)
- Determines DXGI formats from types (`float3` → `DXGI_FORMAT_R32G32B32_FLOAT`)
- Calculates byte offsets for each attribute
- Handles struct inputs (common pattern for vertex data)

**Example shader:**
```hlsl
struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

[shader("vertex")]
float4 vertexMain(VertexInput input) : SV_Position { ... }
```

Automatically generates:
```cpp
D3D12_INPUT_ELEMENT_DESC layout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, ...},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, ...},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, ...}
};
```

---

### Pipeline State Object Creation

#### `SlangPSO.h` / `SlangPSO.cpp`
Creates D3D12 Pipeline State Objects from compiled shader data.

**Function:**
```cpp
ID3D12PipelineState* CreatePSOWithSlangShader(
    const CompiledShaderData& shaderData,
    ID3D12Device* device,
    DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D32_FLOAT
);
```

**Default Pipeline State:**
- **Rasterizer:** Solid fill, back-face culling, CCW winding
- **Blend:** Disabled (opaque rendering)
- **Depth:** Less-than comparison, depth writes enabled
- **Topology:** Triangle list
- **Sample count:** 1x (no MSAA)

**Example:**
```cpp
auto shaderData = SlangHelper::CompileShaderForPSO("shaders/lit.slang", device);
ID3D12PipelineState* pso = SlangHelper::CreatePSOWithSlangShader(
    shaderData,
    device,
    DXGI_FORMAT_R8G8B8A8_UNORM,  // Render target format
    DXGI_FORMAT_D32_FLOAT         // Depth buffer format
);
```

---

## Usage Example

### Complete Workflow

```cpp
#include "graphics/slang/SlangCore.h"

// 1. Compile shader and extract all data
auto shaderData = SlangHelper::CompileShaderForPSO("shaders/textured.slang", device);

// 2. Create PSO
ID3D12PipelineState* pso = SlangHelper::CreatePSOWithSlangShader(
    shaderData,
    device,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_D32_FLOAT
);

// 3. Use in rendering
commandList->SetPipelineState(pso);
commandList->SetGraphicsRootSignature(shaderData.rootSignature);
// ... bind resources, draw calls, etc.
```

### In CommandContext

The `CommandContext` class uses this automatically in `SetShader()`:

```cpp
void CommandContext::SetShader(const std::string& shaderName) {
    auto shaderData = SlangHelper::CompileShaderForPSO(path, device);
    mRootSignature.Attach(shaderData.rootSignature);
    mPipelineState.Attach(SlangHelper::CreatePSOWithSlangShader(shaderData, device));
}
```

Then in `Begin()`, the root signature is automatically set:

```cpp
void CommandContext::Begin() {
    mCommandList->Reset(mAllocator.Get(), mPipelineState.Get());
    if (mRootSignature)
        mCommandList->SetGraphicsRootSignature(mRootSignature.Get());  // Automatic!
}
```

---

## Supported Shader Resources

### Root Signature Bindings

| Slang Type | D3D12 Binding | Register | DWORD Cost | Example |
|------------|---------------|----------|------------|---------|
| `ConstantBuffer<T>` (≤16B) | Root Constants | `b#` | size/4 | Small uniforms |
| `ConstantBuffer<T>` (17-256B) | Root Descriptor (CBV) | `b#` | 2 | `ConstantBuffer<PerFrame> perFrame` |
| `ConstantBuffer<T>` (>256B) | Descriptor Table (CBV) | `b#` | 1 | Large constant buffers |
| `Texture2D` | Descriptor Table (SRV) | `t#` | 1 | `Texture2D albedoMap` |
| `RWTexture2D` | Descriptor Table (UAV) | `u#` | 1 | `RWTexture2D outputTex` |
| `StructuredBuffer<T>` | Descriptor Table (SRV) | `t#` | 1 | `StructuredBuffer<Light> lights` |
| `RWStructuredBuffer<T>` | Descriptor Table (UAV) | `u#` | 1 | `RWStructuredBuffer<Particle> particles` |
| `SamplerState` | Static Sampler | `s#` | 0 | `SamplerState linearSampler` |
| `ParameterBlock<Material>` | Descriptor Tables + CBV | `t#`, `s#`, `b#` (sub-space) | varies | Material textures & uniforms |

**Root Signature DWORD Budget:** D3D12 allows a maximum of 64 DWORDs per root signature. The builder automatically optimizes layout to minimize cost.

### Input Layout Formats

| Slang Type | DXGI Format | Bytes |
|------------|-------------|-------|
| `float` | `DXGI_FORMAT_R32_FLOAT` | 4 |
| `float2` | `DXGI_FORMAT_R32G32_FLOAT` | 8 |
| `float3` | `DXGI_FORMAT_R32G32B32_FLOAT` | 12 |
| `float4` | `DXGI_FORMAT_R32G32B32A32_FLOAT` | 16 |

---

## Logging

All functions use the "SlangHelper" logger for detailed diagnostics:

```
[00:59:00] [info] [SlangHelper]   ✅ Vertex shader set: 6180 bytes
[00:59:00] [info] [SlangHelper]   ✅ Pixel shader set: 7740 bytes
[00:59:00] [info] [SlangHelper]   ✅ Root signature created
```

Logging includes:
- Shader compilation steps
- Entry point discovery
- Bytecode sizes and validation
- Root signature parameter details
- Input layout elements
- PSO creation status

---

## Dependencies

- **Slang** - Shader compiler and reflection API
- **D3D12** - Direct3D 12 API
- **spdlog** - Logging library
- **WRL** (Windows Runtime Library) - COM pointer management

---

## Architecture Notes

### Why Separate Files?

The original `SlangHelper.hpp` was 2,177 lines in a single header. This refactoring:
- **Improves compile times** - Only changed files need recompilation
- **Clarifies responsibilities** - Each file has a clear purpose
- **Enables better testing** - Individual components can be unit tested
- **Reduces cognitive load** - Easier to understand and modify

### "Just Works" Philosophy

This module aims to provide a "batteries included" experience:
- **No manual root signature authoring** - Reflection-driven generation
- **No manual input layout specification** - Automatic extraction from vertex shaders
- **Automatic resource binding layout** - Size-based optimization
- **Sensible PSO defaults** - Standard rasterization, depth testing, etc.
- **Shader-agnostic design** - No hardcoded shader detection

You write Slang shaders with clear semantics, and the module handles the D3D12 boilerplate.

### Reflection-Driven Design

The root signature builder uses Slang's reflection API (`slang::ProgramLayout`, `slang::VariableLayoutReflection`) to automatically discover:
- Resource types (CBV, SRV, UAV, Sampler)
- Register bindings (`b#`, `t#`, `u#`, `s#`)
- Register spaces (including ParameterBlock sub-spaces)
- Resource sizes (for optimization heuristics)

This means **zero hardcoded shader knowledge** - the system adapts to any shader you write. See `_root_sig_ref.cpp` for the reference implementation patterns.

---

## Future Enhancements

Potential improvements (see TODO comments in code):
- [ ] Root constants full implementation (infrastructure exists, needs activation)
- [ ] Descriptor range merging for contiguous resources (reduce descriptor table count)
- [ ] Compute shader PSO creation
- [ ] Geometry/tessellation shader support
- [ ] Shader hot-reloading
- [ ] PSO state caching (avoid recompilation)
- [ ] Multiple render target support
- [ ] Configurable blend/depth/rasterizer states
- [ ] Named parameter binding system (avoid manual root indices)
- [ ] Unbounded descriptor arrays (`StructuredBuffer<T> lights[]`)
