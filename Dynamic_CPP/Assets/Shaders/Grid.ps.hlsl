cbuffer UniformBuffer : register(b0)
{
    float4 gridColor;
    float4 checkerColor;
    float fadeStart;
    float fadeEnd;
    float unitSize;
    int subdivisions;
    float3 centerOffset;
    float majorLineThickness;
    float minorLineThickness;
    float minorLineAlpha;
};
cbuffer cameraPos : register(b1)
{
    float4 cameraPos;
}
// 출력 구조체: 클립 좌표와 함께 월드 좌표를 TEXCOORD0로 전달합니다.
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

float grid(float2 pos, float unit, float thickness)
{
    // fwidth는 HLSL에서도 동일하게 사용 가능
    float2 threshold = fwidth(pos) * thickness * 0.5 / unit;
    float2 posWrapped = pos / unit;
    // HLSL에서는 step가 없으므로 삼항 연산자로 구현
    float2 step_line = (frac(-posWrapped) < threshold) ? float2(1.0, 1.0) : float2(0.0, 0.0);
    step_line += (frac(posWrapped) < threshold) ? float2(1.0, 1.0) : float2(0.0, 0.0);
    return max(step_line.x, step_line.y);
}

float checker(float2 pos, float unit)
{
    float square1 = (frac(pos.x / unit * 0.5) >= 0.5) ? 1.0 : 0.0;
    float square2 = (frac(pos.y / unit * 0.5) >= 0.5) ? 1.0 : 0.0;
    return max(square1, square2) - square1 * square2;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 posWorld = input.worldPos;
    
    // 평면상의 거리 계산 (XZ 평면)
    float distPlanar = distance(posWorld.xz, (int)cameraPos.xz);
    
    float absY = 1;//max(1 - saturate(distPlanar / 70.0), 0.0);
    
    // 메이저 라인과 서브 디비전(마이너) 라인 계산
    float step_line = grid(posWorld.xz, unitSize, majorLineThickness * absY);
    step_line += grid(posWorld.xz, unitSize / subdivisions, minorLineThickness * absY) * minorLineAlpha;
    step_line = saturate(step_line); // clamp(0.0,1.0)와 동일
    
    // 체커보드 패턴 계산
    //float chec = checker(posWorld.xz, unitSize);
    
    // 거리 페이드 계산
    float fadeFactor = 1.0 - saturate((distPlanar - fadeStart) / (fadeEnd - fadeStart));
    
    // 최종 알파값 (각각의 패턴에 따른 알파 합산 후 페이드 적용)
    float alphaGrid = step_line * gridColor.a;
    //float alphaChec = chec * checkerColor.a;
    float alpha = saturate(alphaGrid /*+ alphaChec*/) * fadeFactor * (1 - saturate(distPlanar / 100.f));
    
    // 최종 색상 (프리멀티플라이드 알파 블렌딩)
    float3 color = (checkerColor.rgb /** alphaChec*/) * (1.0 - alphaGrid) + (gridColor.rgb * alphaGrid);

    //if (alpha < 0.1)
    //    discard;
    return float4(color, alpha);
}