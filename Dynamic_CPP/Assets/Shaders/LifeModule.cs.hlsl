// LifeModule.hlsl

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

StructuredBuffer<ParticleData> ParticlesInput : register(t0);
RWStructuredBuffer<ParticleData> ParticlesOutput : register(u0);
RWStructuredBuffer<uint> gInactiveParticleIndices : register(u1);
RWStructuredBuffer<uint> gInactiveParticleCount : register(u2);
RWStructuredBuffer<uint> gActiveParticleCounter : register(u3);

#define THREAD_GROUP_SIZE 1024

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    
    if (DTid.x == 0)
    {
        gActiveParticleCounter[0] = 0;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (particleIndex < gMaxParticles)
    {
        ParticleData particle = ParticlesInput[particleIndex];
        
        // 활성 파티클만 처리
        if (particle.isActive == 1)
        {
            // 파티클 나이 증가
            particle.age += gDeltaTime;
            
            // 수명 체크
            if (particle.age >= particle.lifeTime)
            {
                // 수명이 다한 파티클 비활성화
                particle.isActive = 0;
                particle.age = 0.0f;
                
                uint inactiveSlot;
                InterlockedAdd(gInactiveParticleCount[0], 1, inactiveSlot);
                
                // 범위 체크
                if (inactiveSlot < gMaxParticles)
                {
                    // 스택처럼 현재 카운트 위치에 인덱스 추가
                    gInactiveParticleIndices[inactiveSlot] = particleIndex;
                }
            }
            else
            {
                // 파티클이 여전히 활성 상태 - 활성 카운터 증가
                InterlockedAdd(gActiveParticleCounter[0], 1);
            }
        }
        
        // 업데이트된 파티클 데이터를 출력 버퍼에 저장
        ParticlesOutput[particleIndex] = particle;
    }
}