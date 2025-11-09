// Bindless descriptor helpers in the common module

implementing common;

/// Constant
public static const uint INVALID_INDEX = 0xFFFFFFFF;

////////////////////////////////////////////////
/// TYPEDEFS

/// Use these instead of writing DescriptorHandle<T>, a bit cleaner.
public typedef DescriptorHandle<Texture2D> Texture2DHandle;
public typedef DescriptorHandle<TextureCube> TextureCubeHandle;
public typedef DescriptorHandle<Texture3D> Texture3DHandle;

/// Generic structured buffer handle which instantiate with specific type
/// Example: StructuredBufferHandle<float4> positions;
__generic<T>
public typealias StructuredBufferHandle = DescriptorHandle<StructuredBuffer<T>>;

/// Generic RW structured buffer handle
__generic<T>
public typealias RWStructuredBufferHandle = DescriptorHandle<RWStructuredBuffer<T>>;

/// RW Texture handles for compute shaders
public typedef DescriptorHandle<RWTexture2D<float4>> RWTexture2DHandle;
public typedef DescriptorHandle<RWTexture3D<float4>> RWTexture3DHandle;


////////////////////////////////////////////////
/// Helper Functions

/// Create a Texture2DHandle from a single uint index.
/// Sampler is handled separately, most likely either static or passed
/// as second component.
public Texture2DHandle CreateTexture2DHandle(uint index)
{
    return Texture2DHandle(uint2(index, 0));
}

/// Create a Texture2DHandle from separate texture and sampler indices.
public Texture2DHandle CreateTexture2DHandle(uint textureIndex, uint samplerIndex)
{
    return Texture2DHandle(uint2(textureIndex, samplerIndex));
}

/// Create a TextureCubeHandle from a single uint index.
public TextureCubeHandle CreateTextureCubeHandle(uint index)
{
    return TextureCubeHandle(uint2(index, 0));
}

/// Create a TextureCubeHandle from separate texture and sampler indices.
public TextureCubeHandle CreateTextureCubeHandle(uint textureIndex, uint samplerIndex)
{
    return TextureCubeHandle(uint2(textureIndex, samplerIndex));
}

/// Create a StructuredBufferHandle from a uint index.
__generic<T>
public StructuredBufferHandle<T> CreateStructuredBufferHandle(uint index)
{
    return StructuredBufferHandle<T>(uint2(index, 0));
}

/// Create an RWStructuredBufferHandle from a uint index.
__generic<T>
public RWStructuredBufferHandle<T> CreateRWStructuredBufferHandle(uint index)
{
    return RWStructuredBufferHandle<T>(uint2(index, 0));
}

/// Create an RWTexture2DHandle from a uint index.
public RWTexture2DHandle CreateRWTexture2DHandle(uint index)
{
    return RWTexture2DHandle(uint2(index, 0));
}
