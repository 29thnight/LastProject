// InitializeModule.hlsl
// 파티클 시스템 초기화용 컴퓨트 셰이더

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

// 초기화 파라미터 상수 버퍼
cbuffer InitParams : register(b0)
{
    uint gMaxParticles;
    float3 gPad;
}

// 출력 및 공유 버퍼
RWStructuredBuffer<ParticleData> ParticlesOutput : register(u0);
RWStructuredBuffer<uint> gInactiveParticleIndices : register(u1);
RWStructuredBuffer<uint> gInactiveParticleCount : register(u2);
RWStructuredBuffer<uint> gActiveParticleCounter : register(u3);

#define THREAD_GROUP_SIZE 1024

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    
    // 첫 번째 스레드만 카운터 초기화
    if (DTid.x == 0)
    {
        // 비활성 카운터를 maxParticles로 설정
        gInactiveParticleCount[0] = gMaxParticles;
        
        // 활성 카운터를 0으로 설정
        gActiveParticleCounter[0] = 0;
    }
    
    // 모든 스레드가 초기화를 기다림
    GroupMemoryBarrierWithGroupSync();
    
    // 유효한 인덱스에 대해서만 처리
    if (particleIndex < gMaxParticles)
    {
        // 파티클 데이터 초기화
        ParticleData particle;
        particle.position = float3(0, 0, 0);
        particle.pad1 = 0.0f;
        particle.velocity = float3(0, 0, 0);
        particle.pad2 = 0.0f;
        particle.acceleration = float3(0, 0, 0);
        particle.pad3 = 0.0f;
        particle.size = float2(0, 0);
        particle.age = 0.0f;
        particle.lifeTime = 0.0f;
        particle.rotation = 0.0f;
        particle.rotatespeed = 0.0f;
        particle.pad4 = float2(0, 0);
        particle.color = float4(0, 0, 0, 0);
        particle.isActive = 0; // 비활성 상태로 설정
        particle.pad5 = float3(0, 0, 0);
        
        // 파티클 데이터를 출력 버퍼에 저장
        ParticlesOutput[particleIndex] = particle;
        
        // 인덱스를 비활성 인덱스 목록에 추가
        gInactiveParticleIndices[particleIndex] = particleIndex;
    }
}