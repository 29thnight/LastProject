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

// 입력 파티클 버퍼 (읽기 전용)
StructuredBuffer<ParticleData> ParticlesInput : register(t0);
// 출력 파티클 버퍼 (쓰기 전용)
RWStructuredBuffer<ParticleData> ParticlesOutput : register(u0);
// 비활성 파티클 인덱스 버퍼
RWStructuredBuffer<uint> gInactiveParticleIndices : register(u1);
// 비활성 파티클 카운터
RWStructuredBuffer<uint> gInactiveParticleCount : register(u2);
// 활성 파티클 카운터
RWStructuredBuffer<uint> gActiveParticleCounter : register(u3);

#define THREAD_GROUP_SIZE 1024

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 첫 번째 스레드는 카운터 초기화
    if (DTid.x == 0)
    {
        gActiveParticleCounter[0] = 0; // 활성 카운터를 0으로 리셋
    }
    
    // 모든 스레드가 초기화를 기다림
    GroupMemoryBarrierWithGroupSync();
    
    uint particleIndex = DTid.x;
    
    if (particleIndex < gMaxParticles)
    {
        ParticleData particle = ParticlesInput[particleIndex];
        
        // 기본적으로 입력을 출력에 복사
        ParticlesOutput[particleIndex] = particle;
        
        // 활성 파티클만 처리
        if (particle.isActive == 1)
        {
            // 파티클 나이 증가 (이 부분이 누락되어 있었음)
            particle.age += gDeltaTime;
            
            if (particle.age >= particle.lifeTime)
            {
                // 수명이 다한 파티클 비활성화
                particle.isActive = 0;
                
                // 비활성 인덱스 목록에 추가
                uint inactiveIdx;
                InterlockedAdd(gInactiveParticleCount[0], 1, inactiveIdx);
                
                if (inactiveIdx < gMaxParticles)
                {
                    gInactiveParticleIndices[inactiveIdx] = particleIndex;
                }
            }
            else
            {
                // 파티클이 여전히 활성 상태일 경우 카운터 증가
                InterlockedAdd(gActiveParticleCounter[0], 1);
            }
            
            // 업데이트된 파티클 데이터 저장
            ParticlesOutput[particleIndex] = particle;
            
        }
    }
}
