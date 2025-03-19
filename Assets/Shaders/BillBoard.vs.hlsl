struct VS_INPUT
{
    float4 Position : POSITION;
    float2 Size : TEXCOORD0; // 빌보드 크기
    float4 Color : COLOR; // 선택적: 색상 정보
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 Size : TEXCOORD0;
    float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = input.Position;
    output.Size = input.Size;
    output.Color = input.Color;
    return output;
}