// SimplifiedBillboardVS.hlsl
struct VSInput
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    uint TexIndex : TEXCOORD1;
    float Padding : TEXCOORD2;
    float4 Color : COLOR0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    uint TexIndex : TEXCOORD1;
    float4 Color : COLOR0;
};

cbuffer ModelConstants : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.Position = input.Position;
    output.TexCoord = input.TexCoord;
    output.TexIndex = input.TexIndex;
    output.Color = input.Color; 
    
    return output;
}