#ifdef ENABLE_BINDLESS
#include "BindlessHelpers.slang"
#endif

cbuffer BlurParams : register(b0)
{
	float blurIntensity;
};

#ifdef ENABLE_BINDLESS
struct ComputeResources
{
	Texture2D.Handle inputTexture;
	RWTexture2DHandle outputTexture;
};
ConstantBuffer<ComputeResources> g_resources : register(b1);

#define INPUT_TEXTURE g_resources.inputTexture
#define OUTPUT_TEXTURE g_resources.outputTexture

#else
Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

#define INPUT_TEXTURE inputTexture
#define OUTPUT_TEXTURE outputTexture

#endif

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint2 pixelCoord = dispatchThreadID.xy;

	uint width, height;
	INPUT_TEXTURE.GetDimensions(width, height);

	if (pixelCoord.x >= width || pixelCoord.y >= height)
		return;

	float weights[5] = {0.0545, 0.2442, 0.4026, 0.2442, 0.0545};

	float4 color = float4(0, 0, 0, 0);

	// y-axis
	for (int i = -2; i <= 2; i++)
	{
		int2 samplePos = int2(pixelCoord.x, clamp(int(pixelCoord.y) + i, 0, int(height) - 1));
		color += INPUT_TEXTURE[samplePos] * weights[i + 2];
	}

	float4 originalColor = INPUT_TEXTURE[pixelCoord];
	float4 finalColor = lerp(originalColor, color, blurIntensity);

	OUTPUT_TEXTURE[pixelCoord] = finalColor;
}
