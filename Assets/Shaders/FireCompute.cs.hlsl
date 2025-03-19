// 입력 텍스처들
Texture2D BaseFireTexture : register(t0);   // 기본 불 패턴 텍스처
Texture2D NoiseTexture : register(t1);      // 노이즈 텍스처
Texture2D FireAlphaTexture : register(t2);
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
    float pad;
    
    float colorShift;
    float noiseScale;
    float verticalFactor;
    float flamePower;
    
    // 4개 불 이펙트의 위치와 크기를 제어하는 파라미터
    float4 firePosition1; // = float4(0.1, 0.1, 0.3, 0.3); // 좌측 하단
    float4 firePosition2; // = float4(0.6, 0.1, 0.3, 0.3); // 우측 하단
    float4 firePosition3; // = float4(0.1, 0.6, 0.3, 0.3); // 좌측 상단
    float4 firePosition4; // = float4(0.6, 0.6, 0.3, 0.3); // 우측 상단
    
    float detailScale;
    // 패턴 변화 속도 설정
    float patternChangeSpeed;
    
    // 불 이펙트의 개수 (1-4)
    int numFireEffects; // = 4; // 기본값은 4개의 불 이펙트
    int pad2;
    // 각 불의 오프셋 시간
    float timeOffset1; // = 0.0;
    float timeOffset2; // = 1.57;
    float timeOffset3; // = 3.14;
    float timeOffset4; // = 4.71;
    
    float4 Color;
}

// 하나의 불 이펙트를 렌더링하는 함수 (사분면 기반)
float4 RenderFireWithQuadrants(float2 uv, float localTime, float localIntensity, float localColorShift)
{
    // Y 좌표 뒤집기 (불이 아래에서 위로 타오름)
    float2 fireUV = float2(uv.x, 1.0 - uv.y);
    
    // 시간을 이용한 애니메이션
    float timeValue = localTime * speed * 0.1;
    
    // 시간에 따라 변화하는 패턴 인덱스 (0, 1, 2, 3)
    float patternChangeTime = localTime * patternChangeSpeed;
    int currentPatternIndex = int(patternChangeTime) % 4;
    int nextPatternIndex = (currentPatternIndex + 1) % 4;
    
    // 블렌딩 팩터 (0.0 ~ 1.0 사이 값)
    float blendFactor = frac(patternChangeTime);
    
    // 부드러운 블렌딩을 위해 스무스스텝 적용
    blendFactor = smoothstep(0.0, 1.0, blendFactor);
    
    // 현재 패턴과 다음 패턴의 오프셋 계산
    float2 currentQuadrantOffset, nextQuadrantOffset;
    
    if (currentPatternIndex == 0)
        currentQuadrantOffset = float2(0.0, 0.0);
    else if (currentPatternIndex == 1)
        currentQuadrantOffset = float2(0.5, 0.0);
    else if (currentPatternIndex == 2)
        currentQuadrantOffset = float2(0.0, 0.5);
    else
        currentQuadrantOffset = float2(0.5, 0.5);
    
    if (nextPatternIndex == 0)
        nextQuadrantOffset = float2(0.0, 0.0);
    else if (nextPatternIndex == 1)
        nextQuadrantOffset = float2(0.5, 0.0);
    else if (nextPatternIndex == 2)
        nextQuadrantOffset = float2(0.0, 0.5);
    else
        nextQuadrantOffset = float2(0.5, 0.5);
    
    // 기본 텍스처 UV 변환
    float2 baseCurrentQuadrantUV = fireUV * 0.5 + currentQuadrantOffset;
    float2 baseNextQuadrantUV = fireUV * 0.5 + nextQuadrantOffset;
    
    // 기본 불 텍스처 UV 변형 (시간에 따라 흔들림)
    float2 distortedCurrentUV = baseCurrentQuadrantUV;
    float2 distortedNextUV = baseNextQuadrantUV;
    distortedCurrentUV.x += sin(fireUV.y * 10.0 + timeValue) * 0.02;
    distortedNextUV.x += sin(fireUV.y * 10.0 + timeValue) * 0.02;
    
    // 노이즈 텍스처 UV도 같은 사분면 사용
    float2 noiseCurrentUV1 = (fireUV * noiseScale) * 0.5 + currentQuadrantOffset;
    float2 noiseNextUV1 = (fireUV * noiseScale) * 0.5 + nextQuadrantOffset;
    float2 noiseCurrentUV2 = (fireUV * detailScale) * 0.5 + currentQuadrantOffset;
    float2 noiseNextUV2 = (fireUV * detailScale) * 0.5 + nextQuadrantOffset;
    
    // 노이즈 텍스처에 시간 기반 이동 적용
    noiseCurrentUV1 += float2(timeValue * 0.3, timeValue);
    noiseNextUV1 += float2(timeValue * 0.3, timeValue);
    noiseCurrentUV2 += float2(-timeValue * 0.2, timeValue * 0.7);
    noiseNextUV2 += float2(-timeValue * 0.2, timeValue * 0.7);
    
    // 현재 패턴과 다음 패턴의 노이즈 텍스처 샘플링
    float noise1Current = NoiseTexture.SampleLevel(WrapSampler, noiseCurrentUV1, 0).r;
    float noise1Next = NoiseTexture.SampleLevel(WrapSampler, noiseNextUV1, 0).r;
    float noise2Current = NoiseTexture.SampleLevel(WrapSampler, noiseCurrentUV2, 0).r;
    float noise2Next = NoiseTexture.SampleLevel(WrapSampler, noiseNextUV2, 0).r;
    
    // 두 노이즈 결합
    float combinedNoiseCurrent = noise1Current * 0.7 + noise2Current * 0.3;
    float combinedNoiseNext = noise1Next * 0.7 + noise2Next * 0.3;
    
    // 노이즈를 이용해 UV 왜곡
    distortedCurrentUV += (combinedNoiseCurrent - 0.5) * 0.035 * fireUV.y;
    distortedNextUV += (combinedNoiseNext - 0.5) * 0.035 * fireUV.y;
    
    // 왜곡된 UV가 현재 사분면 범위를 벗어나지 않도록 제한
    distortedCurrentUV = clamp(distortedCurrentUV, currentQuadrantOffset, currentQuadrantOffset + 0.5);
    distortedNextUV = clamp(distortedNextUV, nextQuadrantOffset, nextQuadrantOffset + 0.5);
    
    // 현재 패턴과 다음 패턴에 대한 기본 불 텍스처 샘플링
    float4 baseFireColorCurrent = BaseFireTexture.SampleLevel(LinearSampler, distortedCurrentUV, 0);
    float4 baseFireColorNext = BaseFireTexture.SampleLevel(LinearSampler, distortedNextUV, 0);
    
    // 두 패턴 블렌딩
    float4 baseFireColor = lerp(baseFireColorCurrent, baseFireColorNext, blendFactor);
    
    // 화염 모양 조정 (아래쪽에서 위쪽으로 강도 감소)
    float flameShape = pow(1.0 - fireUV.y, verticalFactor);
    
    // 노이즈를 이용해 불꽃 모양 변형 (현재 패턴과 다음 패턴 각각)
    float flameMaskCurrent = flameShape * (1.0 + combinedNoiseCurrent * 0.5 - 0.25);
    float flameMaskNext = flameShape * (1.0 + combinedNoiseNext * 0.5 - 0.25);
    flameMaskCurrent = pow(saturate(flameMaskCurrent), flamePower);
    flameMaskNext = pow(saturate(flameMaskNext), flamePower);
    
    // 두 패턴의 화염 마스크 블렌딩
    float flameMask = lerp(flameMaskCurrent, flameMaskNext, blendFactor);
    
    // 색상 조정
    float3 finalColor = baseFireColor.rgb;
    
    // 색상 이동 (화염 상단의 색상을 조정)
    if (localColorShift > 0)
    {
        // 화염 상단으로 갈수록 노란색에 가까워짐
        float3 yellowColor = float3(1.0, 0.9, 0.3);
        finalColor = lerp(finalColor, yellowColor, flameMask * localColorShift);
    }
    
    // 최종 색상 강도 적용
    finalColor *= localIntensity;
    
    // 알파 텍스처도 같은 사분면에서 샘플링 (현재 패턴과 다음 패턴 각각)
    float4 alphaShapeCurrent = FireAlphaTexture.SampleLevel(LinearSampler, distortedCurrentUV, 0);
    float4 alphaShapeNext = FireAlphaTexture.SampleLevel(LinearSampler, distortedNextUV, 0);
    
    // 알파 패턴 블렌딩
    float alphaPattern = lerp(alphaShapeCurrent.r, alphaShapeNext.r, blendFactor);
    
    // 최종 알파 계산
    float alpha = flameMask * alphaPattern;
    
    // 알파값 증가 (더 불투명하게)
    alpha = pow(alpha, 0.05); // 이 지수값을 줄이면 알파가 더 진해짐
    alpha *= 10.0; // 알파값 배수 증가
    
    // 흔들림 효과 추가
    alpha *= 1.0 + 0.2 * sin(localTime * 4.0 + uv.y * 20.0);
    
    // 색상에 Color 적용 (RGB)
    finalColor *= Color.rgb;
    
    // 색상 알파에 Color.a도 적용
    alpha *= Color.a;
    
    // 최종 알파값 클램핑
    alpha = saturate(alpha);
    
    // 최종 결과 반환
    return float4(finalColor, alpha);
}

// UV 좌표를 해당 불 영역의 로컬 좌표로 변환하는 함수
bool GetLocalUV(float2 globalUV, float4 firePosition, out float2 localUV)
{
    // firePosition: x, y, width, height (0~1 범위의 정규화된 좌표)
    float2 fireMin = firePosition.xy;
    float2 fireMax = fireMin + firePosition.zw;
    
    // 현재 픽셀이 불 영역 내에 있는지 체크
    if (globalUV.x >= fireMin.x && globalUV.x <= fireMax.x &&
        globalUV.y >= fireMin.y && globalUV.y <= fireMax.y)
    {
        // 전역 UV를 로컬 UV(0~1)로 변환
        localUV = (globalUV - fireMin) / firePosition.zw;
        return true;
    }
    
    localUV = float2(0, 0);
    return false;
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
    
    // 최종 색상 초기화 (완전 투명)
    float4 finalColor = float4(0, 0, 0, 0);
    float2 localUV;
    float4 fireColor = float4(0, 0, 0, 0);
    
    // 첫 번째 불 이펙트
    if (numFireEffects >= 1 && GetLocalUV(uv, firePosition1, localUV))
    {
       // 프리멀티플라이드 알파를 적용한 올바른 알파 블렌딩
        fireColor = RenderFireWithQuadrants(localUV, time + timeOffset1, intensity, colorShift);
        finalColor.rgb = finalColor.rgb * (1.0 - fireColor.a) + fireColor.rgb * fireColor.a;
        finalColor.a = finalColor.a + fireColor.a * (1.0 - finalColor.a);

    }
    
    // 두 번째 불 이펙트
    if (numFireEffects >= 2 && GetLocalUV(uv, firePosition2, localUV))
    {
        fireColor = RenderFireWithQuadrants(localUV, time + timeOffset1, intensity, colorShift);
        finalColor.rgb = finalColor.rgb * (1.0 - fireColor.a) + fireColor.rgb * fireColor.a;
        finalColor.a = finalColor.a + fireColor.a * (1.0 - finalColor.a);
    }
    
    // 세 번째 불 이펙트
    if (numFireEffects >= 3 && GetLocalUV(uv, firePosition3, localUV))
    {
        fireColor = RenderFireWithQuadrants(localUV, time + timeOffset1, intensity, colorShift);
        finalColor.rgb = finalColor.rgb * (1.0 - fireColor.a) + fireColor.rgb * fireColor.a;
        finalColor.a = finalColor.a + fireColor.a * (1.0 - finalColor.a);
    }
    
    // 네 번째 불 이펙트
    if (numFireEffects >= 4 && GetLocalUV(uv, firePosition4, localUV))
    {
        fireColor = RenderFireWithQuadrants(localUV, time + timeOffset1, intensity, colorShift);
        finalColor.rgb = finalColor.rgb * (1.0 - fireColor.a) + fireColor.rgb * fireColor.a;
        finalColor.a = finalColor.a + fireColor.a * (1.0 - finalColor.a);
    }
    
    // 디버깅: 렌더링 영역 확인을 위한 색상 표시
    
    if (length(uv - (firePosition1.xy + firePosition1.zw * 0.5)) < 0.005 ||
        length(uv - (firePosition2.xy + firePosition2.zw * 0.5)) < 0.005 ||
        length(uv - (firePosition3.xy + firePosition3.zw * 0.5)) < 0.005 ||
        length(uv - (firePosition4.xy + firePosition4.zw * 0.5)) < 0.005)
    {
        finalColor = float4(1, 0, 0, 1); // 빨간색 점으로 중심점 표시
    }
   
    
    // 최종 결과 출력
    OutputTexture[DTid.xy] = finalColor;
}