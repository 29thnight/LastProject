#include "Snow.hlsli"
#include "Sampler.hlsli"

Texture2D snowFlake : register(t0);

//--------------------------------------------------------------------------------------
// Snow Pixel Shader
//--------------------------------------------------------------------------------------
float4 main(GS_OUTPUT input) : SV_TARGET
{
    // 눈송이 텍스처 샘플링
    float4 texColor = snowFlake.Sample(LinearSampler, input.TexCoord);
    
    // 텍스처가 없는 경우를 위한 대체 색상 계산
    float2 center = float2(0.5, 0.5);
    float2 uv = input.TexCoord - center;
    float dist = length(uv);
    
    // 원형 눈송이 모양
    float circle = 1.0 - smoothstep(0.0, 0.5, dist);
    
    // 최종 색상 계산 (텍스처 또는 원형)
    float4 color = input.Color;
    
    // 텍스처와 원형 중 선택
#ifdef USE_TEXTURE
        color.a *= texColor.a;
#else
    color.a *= circle;
#endif
    
    // 알파 임계값 이하는 폐기 (작은 값 제거하여 성능 향상)
    if (color.a < 0.05)
        discard;
    
    return color; // 수정: input.Color 대신 계산된 color를 반환
}