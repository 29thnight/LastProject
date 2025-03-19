#include "Snow.hlsli"

//--------------------------------------------------------------------------------------
// 눈송이 순차적 생성 및 소멸 셰이더
//--------------------------------------------------------------------------------------
[maxvertexcount(93)] // 93개 눈송이 * 4개 정점 = 372
void main(point VS_OUTPUT input[1], inout TriangleStream<GS_OUTPUT> outputStream)
{
    // 카메라 및 환경 변수
    float3 cameraPos = CameraPosition.xyz;
    float3 cameraForward = normalize(View._m02_m12_m22);
    float snowRadius = 30.0f;
    float forwardDistance = 50.0f;
    
    // 라이프 사이클 관련 변수
    float startHeight = 50.0f; // 눈송이 시작 높이
    float endHeight = -50.0f; // 눈송이 사라지는 높이
    float totalHeight = startHeight - endHeight; // 총 이동 거리
    
    float speedMultiplier = 10.0f; // 기본 속도의 3배로 설정
    
    // 시간에 독립적인 seed 값
    uint baseSeed = uint(time) % 10000;
    
    // 눈송이 생성 매개변수
    uint snowflakeCount = uint(SnowAmount * 30);
    snowflakeCount = min(snowflakeCount, 93);
    
    // 활성화된 눈송이 수 계산
    uint activeFlakeCount = snowflakeCount;
    
    for (uint i = 0; i < snowflakeCount; i++)
    {
        // 각 눈송이마다 고유한 시드 생성 (시간과 독립적으로)
        uint snowflakeSeed = i * 1337 + 42;
        
        // 눈송이 위치 계산을 위한 기본 값
        float xPos = (hash(snowflakeSeed) * 2.0f - 1.0f) * snowRadius;
        float zPos = hash(snowflakeSeed + 1) * forwardDistance;
        
        // 눈송이 크기 및 속도 (고유하게 유지)
        float snowflakeSize = SnowSize * (0.7f + hash(snowflakeSeed + 3) * 0.6f);
        float fallSpeed = SnowFallSpeed * speedMultiplier * (0.8f + hash(snowflakeSeed + 4) * 0.4f);
        
        // 눈송이 순차적 생성을 위한 시간 오프셋
        // 각 눈송이는 전체 눈송이 수에 따라 균등하게 분배된 시작 시간을 갖습니다
        float timeOffset = (float(i) / float(snowflakeCount)) * (totalHeight / fallSpeed);
        
        // 눈송이의 현재 수명 계산
        float flakeLifetime = time - timeOffset;
        
        // 음수 수명은 아직 생성되지 않은 눈송이
        if (flakeLifetime < 0.0f)
        {
            continue; // 이 눈송이는 아직 생성되지 않음
        }
        
        // 눈송이 이동 거리 계산
        float fallingDistance = fallSpeed * flakeLifetime;
        
        // 전체 라이프 사이클 거리를 넘어가면 처음부터 다시 시작
        // 하지만 같은 위치에 새 눈송이가 즉시 나타나지 않게 함
        float cycleProgress = fmod(fallingDistance, totalHeight * 2.0f);
        
        // 눈송이가 실제 활성화되어 있는지 확인
        // 총 이동 거리를 넘어가면 비활성화 (두 번째 사이클에 해당)
        if (cycleProgress >= totalHeight)
        {
            continue; // 이 눈송이는 현재 비활성화됨
        }
        
        // 실제 y 위치 계산
        float yPos = startHeight - cycleProgress;
        
        // 눈송이 현재 위치 계산
        float3 offset = float3(
            xPos + sin(time * 0.3f + i * 0.1f) * 0.6f, // 좌우 흔들림
            yPos, // 계산된 높이
            zPos
        );
        
        // 바람 효과 적용
        float windEffect = sin(time * 0.15f) * 0.5f + 0.5f;
        offset += WindDirection * (WindStrength * windEffect * 0.5f);
        
        // 카메라 공간으로 위치 변환
        float3 right = normalize(cross(float3(0, 1, 0), cameraForward));
        float3 up = float3(0, 1, 0);
        float3 snowflakePos = cameraPos + offset.x * right + offset.y * up + offset.z * cameraForward;
        
        // 빌보드 계산
        float3 lookDir = normalize(cameraPos - snowflakePos);
        float3 rightDir = normalize(cross(up, lookDir));
        float3 upDir = normalize(cross(lookDir, rightDir));
        
        rightDir *= snowflakeSize;
        upDir *= snowflakeSize;
        
        // 빌보드 정점 생성
        GS_OUTPUT vertices[4];
        float3 center = snowflakePos;
        
        // 좌하단
        vertices[0].Position = float4(center - rightDir - upDir, 1.0);
        vertices[0].Position = mul(vertices[0].Position, View);
        vertices[0].Position = mul(vertices[0].Position, Projection);
        vertices[0].TexCoord = float2(0, 1);
        
        // 좌상단
        vertices[1].Position = float4(center - rightDir + upDir, 1.0);
        vertices[1].Position = mul(vertices[1].Position, View);
        vertices[1].Position = mul(vertices[1].Position, Projection);
        vertices[1].TexCoord = float2(0, 0);
        
        // 우상단
        vertices[2].Position = float4(center + rightDir + upDir, 1.0);
        vertices[2].Position = mul(vertices[2].Position, View);
        vertices[2].Position = mul(vertices[2].Position, Projection);
        vertices[2].TexCoord = float2(1, 0);
        
        // 우하단
        vertices[3].Position = float4(center + rightDir - upDir, 1.0);
        vertices[3].Position = mul(vertices[3].Position, View);
        vertices[3].Position = mul(vertices[3].Position, Projection);
        vertices[3].TexCoord = float2(1, 1);
        
        // 거리 및 높이에 따른 투명도 계산
        float distToCamera = length(snowflakePos - cameraPos);
        float visibleDistance = snowRadius + forwardDistance;
        float distFade = saturate(1.0 - (distToCamera / visibleDistance));
        
        // 페이드 인/아웃 효과 계산
        float fadeDistance = 15.0f; // 페이드 효과를 적용할 거리
        
        float topFade = 1.0f;
        float bottomFade = 1.0f;
        
        // 상단에서 페이드 인
        if (yPos > (startHeight - fadeDistance))
        {
            topFade = saturate((startHeight - yPos) / fadeDistance);
        }
        
        // 하단에서 페이드 아웃
        if (yPos < (endHeight + fadeDistance))
        {
            bottomFade = saturate((yPos - endHeight) / fadeDistance);
        }
        
        float4 color = float4(SnowColor, SnowOpacity * distFade * topFade * bottomFade);
        
        // 모든 정점에 색상 설정
        for (int v = 0; v < 4; v++)
        {
            vertices[v].Color = color;
        }
        
        // 삼각형 스트립 생성
        outputStream.Append(vertices[0]);
        outputStream.Append(vertices[1]);
        outputStream.Append(vertices[3]);
        outputStream.Append(vertices[2]);
        
        outputStream.RestartStrip();
    }
}
