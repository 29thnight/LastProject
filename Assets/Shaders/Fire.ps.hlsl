// FireRender.hlsl - 불 효과 렌더링용 픽셀 셰이더

// 텍스처와 샘플러
Texture2D FireTexture : register(t0); // 컴퓨트 셰이더가 생성한 불 텍스처
SamplerState LinearSampler : register(s0);

// 상수 버퍼
cbuffer ViewBuffer : register(b1)
{
    matrix View;
}

cbuffer ProjBuffer : register(b2)
{
    matrix Projection;
}

cbuffer ModelBuffer : register(b0)
{
    matrix World;
    matrix WorldInverseTranspose;
}

//cbuffer FireParameters : register(b3)
//{
//    float time;
//    float intensity;
//    float speed;
//    float colorShift;
//    float noiseScale;
//    float verticalFactor;
//    float flamePower;
//    float detailScale;
//}

cbuffer ExplodeParameters : register(b3)
{
    float time;
    float intensity;
    float speed;
    float pad;
    
    float2 size;
    float2 range;
}

// 버텍스 셰이더 출력 구조체
struct PixelInput
{
    float4 position : SV_POSITION;
    float4 pos : POSITION0;
    float4 wPosition : POSITION1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 texCoord : TEXCOORD0;
};

// 픽셀 셰이더 함수
float4 main(PixelInput input) : SV_TARGET
{
    // 컴퓨트 셰이더가 생성한 불 텍스처 샘플링
    float4 fireColor = FireTexture.Sample(LinearSampler, input.texCoord);
    
    // 발광 효과 추가 (HDR 효과)
    //fireColor.rgb *= (1.0 + fireColor.r * 0.5); // 밝은 부분을 더 밝게
    
    // 투명도 처리 (알파 블렌딩용)
    // 뷰 방향에 따른 투명도 조정 (엣지에서 더 투명하게)
    //float3 viewDir = normalize(-input.pos.xyz);
    //float viewFactor = abs(dot(viewDir, normalize(input.normal)));
    
    // 엣지에서 더 투명하게 처리 (프레넬 효과와 유사)
    //fireColor.a *= pow(viewFactor, 0.3);
    
    // 시간에 따른 깜박임 효과 (선택적)
    //float flicker = 1.0 + 0.1 * sin(time * 8.0 + input.texCoord.y * 5.0);
    //fireColor.rgb *= flicker;
    
    // 최종 색상 반환
    
    if (fireColor.a < 0.1)
    {
        discard; // 픽셀 폐기 (완전 투명 처리)
    }
    
    return fireColor;
}