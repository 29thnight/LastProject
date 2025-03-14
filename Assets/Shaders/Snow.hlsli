cbuffer SnowCB : register(b0)
{
    matrix View;
    matrix Projection;
    float4 CameraPosition;
    float Time;
    float3 Padding;
};

cbuffer SnowParamBuffer : register(b1)
{
    float3 WindDirection;
    float WindStrength;
    float SnowFallSpeed;
    float SnowAmount;
    float SnowSize;
    float time;
    float3 SnowColor;
    float SnowOpacity;
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

struct GS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};

// 난수 생성 함수
float random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

// 해시 함수 - 생성 시드로부터 일관된 랜덤 값 생성
float hash(uint seed)
{
    seed = seed * 747796405 + 2891336453;
    seed = ((seed >> 13) ^ seed) * 1664525;
    seed = ((seed >> 16) ^ seed) * 1013904223;
    return float(seed & 0x00FFFFFF) / float(0x01000000);
}