SamplerState LinearSampler : register(s0);
SamplerState WrapSampler : register(s1);

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

    float2 padding = tileUVSize * 0.005;

    float2 tileStartUV = float2(tileX, tileY) * tileUVSize + padding;

    float2 actualTileSize = tileUVSize - padding * 2.0;

    float2 sourceUV = tileStartUV + pixelPos * actualTileSize;

    sourceUV = clamp(sourceUV, tileStartUV, tileStartUV + actualTileSize);

    float4 color = BaseFireTexture.SampleLevel(LinearSampler, sourceUV, 0);

    color.rgb *= intensity;
    
    float alphaThreshold = 0.2;
    float alphaSmoothness = 0.1;
    color.a = smoothstep(alphaThreshold, alphaThreshold + alphaSmoothness, color.a);
    
    float2 posInTile = frac(pixelPos * float2(width, height) / size);

    float dotSize = 0.05;
    
    if (length(posInTile - float2(0.5, 0.5)) < dotSize)
    {
        color = float4(1.0, 0.0, 0.0, 1.0);
    }
    
    OutputTexture[DTid.xy] = color;
}