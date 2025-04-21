// BillboardCompute.hlsl
struct BillboardInput
{
    float3 Position;
    float Padding1;
    float2 Scale;
    uint TexIndex;
    float Padding2;
    float4 Color;
};

struct VertexOutput
{
    float4 Position;
    float2 TexCoord;
    uint TexIndex;
    float Padding;
    float4 Color;
};

cbuffer CameraConstants : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    float3 CameraRight;
    float Padding;
    float3 CameraUp;
    uint BillboardCount;
};

// 빌보드 입력 데이터
StructuredBuffer<BillboardInput> InputBillboards : register(t0);
// 출력 버퍼 (변환된 정점)
RWStructuredBuffer<VertexOutput> OutputVertices : register(u0);

// 빌보드 테이블 (각 정점의 로컬 좌표와 텍스처 좌표)
static const float2 QuadPositions[4] =
{
    float2(-1, -1), // 좌하단 (0)
    float2(1, -1), // 우하단 (1)
    float2(1, 1), // 우상단 (2)
    float2(-1, 1) // 좌상단 (3)
};

static const float2 QuadTexCoords[4] =
{
    float2(0, 1), // 좌하단
    float2(1, 1), // 우하단
    float2(1, 0), // 우상단
    float2(0, 0) // 좌상단
};

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint billboardIndex = DTid.x;
    
    // 인덱스가 범위를 벗어나면 종료
    if (billboardIndex >= BillboardCount)
        return;
    
    // 빌보드 데이터 가져오기
    BillboardInput billboard = InputBillboards[billboardIndex];
    
    // 4개의 정점 처리 (빌보드 당 쿼드)
    for (int i = 0; i < 4; i++)
    {
        // 쿼드의 로컬 좌표 가져오기
        float2 localPos = QuadPositions[i];
        
        // 중요: 월드 공간에서 빌보드 위치 계산 - 이 부분이 핵심
        float3 billboardPos = billboard.Position; // 빌보드의 기본 위치
        
        // 월드 공간의 빌보드 기준점에 월드 행렬 적용
        float3 worldPos = mul(float4(billboardPos, 1.0f), World).xyz;
        
        // 카메라 방향 벡터를 사용하여 빌보드 면을 카메라를 향하게 함
        // 중요: offset을 적용하기 전에 worldPos 값을 저장
        float3 localWorldPos = worldPos;
        
        // 로컬 좌표에 따른 오프셋 계산 (카메라 방향 벡터 기준)
        float3 offset = CameraRight * localPos.x * billboard.Scale.x +
                        CameraUp * localPos.y * billboard.Scale.y;
        
        // 오프셋을 적용하여 최종 월드 위치 계산
        localWorldPos += offset;
        
        // 뷰 및 투영 변환 적용
        float4 clipPos = mul(mul(float4(localWorldPos, 1.0f), View), Projection);
        
        // 결과 저장 (빌보드 당 4개의 정점)
        uint vertexIndex = billboardIndex * 4 + i;
        
        VertexOutput vertex;
        vertex.Position = clipPos;
        vertex.TexCoord = QuadTexCoords[i];
        vertex.Padding = 0;
        vertex.TexIndex = billboard.TexIndex;
        vertex.Color = billboard.Color;
        
        OutputVertices[vertexIndex] = vertex;
    }
}