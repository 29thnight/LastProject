// 빌보드용 지오메트리 셰이더
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

struct GSInput
{
    float4 position : SV_POSITION;
    float2 size : TEXCOORD0; // 빌보드 크기
    float4 color : COLOR; // 선택적: 색상 정보
};

struct GSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR; // 선택적: 픽셀 셰이더로 전달할 색상
};

[maxvertexcount(6)]
void main(point GSInput input[1], inout TriangleStream<GSOutput> outputStream)
{
    // 카메라 위치 계산 (View 행렬의 역행렬의 마지막 행)
    float3 cameraPos = float3(
        -dot(View[0].xyz, View[3].xyz),
        -dot(View[1].xyz, View[3].xyz),
        -dot(View[2].xyz, View[3].xyz)
    );
    
    // 빌보드의 중심 위치 (월드 공간)
    float3 worldPos = mul(input[0].position, World).xyz;
    
    // 카메라 기준 좌표계 설정
    float3 up = float3(0, 1, 0);
    float3 look = normalize(cameraPos - worldPos);
    float3 right = normalize(cross(up, look));
    up = normalize(cross(look, right));
    
    // 빌보드 사이즈
    float2 halfSize = input[0].size * 0.5f;
    
    // 사각형의 네 꼭지점 생성 (월드 공간)
    float3 corners[4];
    corners[0] = worldPos - right * halfSize.x - up * halfSize.y; // 좌하단
    corners[1] = worldPos - right * halfSize.x + up * halfSize.y; // 좌상단
    corners[2] = worldPos + right * halfSize.x - up * halfSize.y; // 우하단
    corners[3] = worldPos + right * halfSize.x + up * halfSize.y; // 우상단
    
    // 텍스처 좌표
    float2 texCoords[4];
    texCoords[0] = float2(0, 1); // 좌하단
    texCoords[1] = float2(0, 0); // 좌상단
    texCoords[2] = float2(1, 1); // 우하단
    texCoords[3] = float2(1, 0); // 우상단
    
    // 두 개의 삼각형으로 사각형 생성
    GSOutput output;
    
    // 첫 번째 삼각형 (0-1-2)
    output.color = input[0].color;
    
    output.position = mul(float4(corners[0], 1.0f), View);
    output.position = mul(output.position, Projection);
    output.texCoord = texCoords[0];
    outputStream.Append(output);
    
    output.position = mul(float4(corners[1], 1.0f), View);
    output.position = mul(output.position, Projection);
    output.texCoord = texCoords[1];
    outputStream.Append(output);
    
    output.position = mul(float4(corners[2], 1.0f), View);
    output.position = mul(output.position, Projection);
    output.texCoord = texCoords[2];
    outputStream.Append(output);
    
    outputStream.RestartStrip();
    
    // 두 번째 삼각형 (1-3-2)
    output.position = mul(float4(corners[1], 1.0f), View);
    output.position = mul(output.position, Projection);
    output.texCoord = texCoords[1];
    outputStream.Append(output);
    
    output.position = mul(float4(corners[3], 1.0f), View);
    output.position = mul(output.position, Projection);
    output.texCoord = texCoords[3];
    outputStream.Append(output);
    
    output.position = mul(float4(corners[2], 1.0f), View);
    output.position = mul(output.position, Projection);
    output.texCoord = texCoords[2];
    outputStream.Append(output);
    
    outputStream.RestartStrip();
}