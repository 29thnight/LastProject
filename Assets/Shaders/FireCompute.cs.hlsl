// FireCompute.hlsl - 두 개의 텍스처를 사용한 불 효과

// 입력 텍스처들
Texture2D BaseFireTexture : register(t0); // 기본 불 패턴 텍스처
Texture2D NoiseTexture : register(t1); // 노이즈 텍스처

// 샘플러
SamplerState LinearSampler : register(s0);
SamplerState WrapSampler : register(s1);

// 출력 텍스처
RWTexture2D<float4> OutputTexture : register(u0);

// 상수 버퍼
cbuffer FireParameters : register(b0)
{
    float time;
    float intensity;
    float speed;
    float colorShift;
    float noiseScale;
    float verticalFactor;
    float flamePower;
    float detailScale;
}

// 메인 컴퓨트 셰이더 함수
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 출력 텍스처 크기 가져오기
    uint width, height;
    OutputTexture.GetDimensions(width, height);
    
    // 정규화된 좌표
    float2 uv = float2(DTid.xy) / float2(width, height);
    
    // Y 좌표 뒤집기 (불이 아래에서 위로 타오름)
    float2 fireUV = float2(uv.x, 1.0 - uv.y);
    
    // 시간을 이용한 애니메이션
    float timeValue = time * speed * 0.1;
    
    // 기본 불 텍스처 UV 변형 (시간에 따라 흔들림)
    float2 distortedUV = fireUV;
    distortedUV.x += sin(fireUV.y * 10.0 + timeValue) * 0.02;
    
    // 노이즈 텍스처 샘플링 (움직이는 노이즈)
    float2 noiseUV1 = fireUV * noiseScale + float2(timeValue * 0.3, timeValue);
    float2 noiseUV2 = fireUV * detailScale + float2(-timeValue * 0.2, timeValue * 0.7);
    
    float noise1 = NoiseTexture.SampleLevel(WrapSampler, noiseUV1, 0).r;
    float noise2 = NoiseTexture.SampleLevel(WrapSampler, noiseUV2, 0).r;
    
    // 두 노이즈 결합
    float combinedNoise = noise1 * 0.7 + noise2 * 0.3;
    
    // 노이즈를 이용해 UV 왜곡
    distortedUV += (combinedNoise - 0.5) * 0.035 * distortedUV.y;
    
    // 기본 불 텍스처 샘플링
    float4 baseFireColor = BaseFireTexture.SampleLevel(LinearSampler, distortedUV, 0);
    
    // 화염 모양 조정 (아래쪽에서 위쪽으로 강도 감소)
    float flameShape = pow(1.0 - fireUV.y, verticalFactor);
    
    // 노이즈를 이용해 불꽃 모양 변형
    float flameMask = flameShape * (1.0 + combinedNoise * 0.5 - 0.25);
    flameMask = pow(saturate(flameMask), flamePower);
    
    // 색상 조정
    float3 finalColor = baseFireColor.rgb;
    
    // 색상 이동 (화염 상단의 색상을 조정)
    if (colorShift > 0)
    {
        // 화염 상단으로 갈수록 노란색에 가까워짐
        float3 yellowColor = float3(1.0, 0.9, 0.3);
        finalColor = lerp(finalColor, yellowColor, flameMask * colorShift);
    }
    
    // 최종 색상 강도 적용
    finalColor *= intensity;
    
    // 화염의 알파값 계산 (불꽃 모양에 따라)
    float alpha = flameMask * baseFireColor.a;
    
    // 흔들림 효과 추가
    alpha *= 1.0 + 0.2 * sin(time * 4.0 + uv.y * 20.0);
    
    // 최종 결과 출력
    OutputTexture[DTid.xy] = float4(finalColor, alpha);
}