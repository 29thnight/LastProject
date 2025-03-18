#include "Sampler.hlsli"
Texture2D BaseFireTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);
cbuffer ExplodeParameters : register(b0)
{
    float time;
    float intensity;
    float speed;
    float pad;
    
    float2 size;
    float2 range;
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    OutputTexture.GetDimensions(width, height);
    
    float spriteSheetWidth, spriteSheetHeight;
    BaseFireTexture.GetDimensions(spriteSheetWidth, spriteSheetHeight);

    uint totalTiles = uint(range.x * range.y);
    uint currentTileIndex = uint(fmod(floor(time * speed), totalTiles));

    uint tileX = currentTileIndex % uint(range.x);
    uint tileY = currentTileIndex / uint(range.x);

    float2 pixelPos = float2(DTid.xy) / float2(width, height);

    float2 tileUVSize = size / float2(spriteSheetWidth, spriteSheetHeight);

    float2 tileStartUV = float2(tileX, tileY) * tileUVSize;
    
    float2 sourceUV = tileStartUV + pixelPos * tileUVSize;

    float4 color = BaseFireTexture.SampleLevel(LinearSampler, sourceUV, 0);

    float2 centerPosInTile = pixelPos - float2(0.5, 0.5);
    
    float dotSize = 0.01f;
    
    if(length(centerPosInTile) < dotSize)
    {
        color = float4(1.0, 0.0, 0.0, 1.0);
    }
    
    color.rgb *= intensity;

    OutputTexture[DTid.xy] = color;
}