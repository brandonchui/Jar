  RWTexture2D<float4> outputTexture : register(u0);

  [shader("compute")]
  [numthreads(8, 8, 1)]
  void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
  {
      uint2 pixelCoord = dispatchThreadID.xy;

      uint width, height;

      outputTexture.GetDimensions(width, height);
      float2 uv = float2(pixelCoord) / float2(width, height);

      outputTexture[pixelCoord] = float4(uv.x, uv.y, 0.0, 1.0);
  }
