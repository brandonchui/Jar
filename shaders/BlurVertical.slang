cbuffer BlurParams : register(b0)
{
	float blurIntensity;
};

Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint2 pixelCoord = dispatchThreadID.xy;

	uint width, height;
	inputTexture.GetDimensions(width, height);

	if (pixelCoord.x >= width || pixelCoord.y >= height)
		return;

	float weights[5] = {0.0545, 0.2442, 0.4026, 0.2442, 0.0545};

	float4 color = float4(0, 0, 0, 0);

	// y-axis
	for (int i = -2; i <= 2; i++)
	{
		int2 samplePos = int2(pixelCoord.x, clamp(int(pixelCoord.y) + i, 0, int(height) - 1));
		color += inputTexture[samplePos] * weights[i + 2];
	}

	float4 originalColor = inputTexture[pixelCoord];
	float4 finalColor = lerp(originalColor, color, blurIntensity);

	outputTexture[pixelCoord] = finalColor;
}
