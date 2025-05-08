// MovementModule.hlsl
struct ParticleData
{
    float3 position;
    float pad1;
    float3 velocity;
    float pad2;
    float3 acceleration;
    float pad3;
    float2 size;
    float age;
    float lifeTime;
    float rotation;
    float rotatespeed;
    float2 pad4;
    float4 color;
    uint isActive;
    float3 pad5;
};

cbuffer MovementParams : register(b0)
{
    float deltaTime;
    int useGravity;
    float gravityStrength;
    int useEasing;
    int easingType;
    int animationType;
    float easingDuration;
    float padding[1];
};

// 이징 함수들 - ApplyEasing 함수와 동일한 구현 필요
float ApplyEasing(float normalizedTime, int easingType, int animationType, float duration)
{
    // SpawnModuleCS에서와 동일한 이징 함수 구현
    // (여기에 구현 필요)
    return 1.0f; // 기본 구현
}

// 스레드 그룹 크기 정의
#define THREAD_GROUP_SIZE 256

// 셰이더 리소스 뷰와 UAV 정의
StructuredBuffer<ParticleData> ParticlesSRV : register(t0);
RWStructuredBuffer<ParticleData> ParticlesUAV : register(u0);

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    
    // 활성 파티클만 처리
    if (ParticlesSRV[particleIndex].isActive > 0)
    {
        // 파티클 데이터 가져오기
        ParticleData particle = ParticlesSRV[particleIndex];
        
        // 정규화된 나이 계산
        float normalizedAge = particle.age / particle.lifeTime;
        
        // 이징 적용
        float easingFactor = 1.0f;
        if (useEasing > 0)
        {
            easingFactor = ApplyEasing(normalizedAge, easingType, animationType, easingDuration);
        }
        
        // 중력 적용
        if (useGravity > 0)
        {
            particle.velocity += particle.acceleration * gravityStrength * deltaTime * easingFactor;
        }
        
        // 위치 및 회전 업데이트
        particle.position += particle.velocity * deltaTime * easingFactor;
        particle.rotation += particle.rotatespeed * deltaTime * easingFactor;
        
        // 업데이트된 파티클 저장
        ParticlesUAV[particleIndex] = particle;
    }
}