struct VS_INPUT
{
    float4 position : SV_POSITION;
    float2 size : TEXCOORD0;
    float4 color : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 size : TEXCOORD0;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = input.position;
    output.size = input.size;
    output.color = input.color;
    
    return output;
}