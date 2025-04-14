// NewBillBoard.hlsl
cbuffer ModelConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VSInput
{
    float4 VertexPosition : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float4 InstancePosition : POSITION1;
    float2 InstanceScale : TEXCOORD1;
    uint TexIndex : TEXCOORD2;
    float4 Color : COLOR0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    uint TexIndex : TEXCOORD1;
    float4 Color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // 인스턴스 월드 위치 설정
    float3 worldPos = input.InstancePosition.xyz;
    
    // 카메라 방향 벡터 (더 정확한 빌보드 계산을 위해)
    float3 cameraRight = normalize(float3(View._11, View._21, View._31));
    float3 cameraUp = normalize(float3(View._12, View._22, View._32));
    
    // 기본 정점 위치에 인스턴스 스케일 적용
    float3 vertexPos = input.VertexPosition.x * cameraRight * input.InstanceScale.x +
                       input.VertexPosition.y * cameraUp * input.InstanceScale.y;
    
    // 정점 위치를 인스턴스 위치에 더함
    worldPos += vertexPos;
    
    // 월드-뷰-투영 변환 적용 (순서 주의)
    float4 positionW = mul(float4(worldPos, 1.0f), World);
    float4 positionV = mul(positionW, View);
    float4 positionP = mul(positionV, Projection);
    
    output.Position = positionP;
    output.TexCoord = input.TexCoord;
    output.TexIndex = input.TexIndex;
    output.Color = input.Color;
    
    return output;
}