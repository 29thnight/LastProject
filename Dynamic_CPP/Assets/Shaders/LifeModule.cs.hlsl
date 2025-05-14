// LifeModule.hlsl
// 생명주기 관리 컴퓨트 셰이더

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

cbuffer TimeParams : register(b0)
{
    float gDeltaTime;
    uint gMaxParticles;
    float2 gPad;
}

// 파티클 버퍼
RWStructuredBuffer<ParticleData> gParticles : register(u0);
// 활성 파티클 카운터
RWStructuredBuffer<uint> gActiveParticleCounter : register(u1);
// 비활성 파티클 인덱스 버퍼 (재활용 가능한 파티클 인덱스 저장)
RWStructuredBuffer<uint> gInactiveParticleIndices : register(u2);
RWStructuredBuffer<uint> gInactiveParticleCount : register(u3);

#define THREAD_GROUP_SIZE 1024

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 첫 번째 스레드는 카운터 초기화
    if (DTid.x == 0)
    {
        gActiveParticleCounter[0] = 0;
        //gInactiveParticleCount[0] = 0;
    }
    
    // 모든 스레드가 초기화를 기다림
    GroupMemoryBarrierWithGroupSync();
    
    uint particleIndex = DTid.x;
    uint maxParticles = gMaxParticles; // 상수 버퍼에서 가져오거나 하드코딩
    
    
    if (particleIndex < maxParticles)
    {
        ParticleData particle = gParticles[particleIndex];
        
        if (particle.isActive == 1)
        {
            // 나이 증가
            particle.age += gDeltaTime;
            
            // 수명 검사
            if (particle.age >= particle.lifeTime)
            {
                // 파티클 비활성화
                particle.isActive = 0;
                gParticles[particleIndex] = particle;
                
                // 비활성화된 파티클 인덱스 저장 (스폰에 재사용)
                uint idx;
                InterlockedAdd(gInactiveParticleCount[0], 1, idx);
                if (idx < maxParticles) // 버퍼 범위 체크
                {
                    gInactiveParticleIndices[idx] = particleIndex;
                }
            }
            else
            {
                // 파티클이 여전히 활성 상태
                gParticles[particleIndex] = particle;
                
                // 활성 파티클 카운터 증가
                InterlockedAdd(gActiveParticleCounter[0], 1);
            }
        }
    }
}