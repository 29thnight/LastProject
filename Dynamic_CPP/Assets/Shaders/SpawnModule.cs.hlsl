// SpawnModule.hlsl

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

cbuffer SpawnParams : register(b0)
{
    float gSpawnRate;
    float gDeltaTime;
    int gEmitterType;
    float pad;
    float3 gEmitterSize;
    float gEmitterRadius;
    uint gMaxParticles;
    float3 gPad;
}

cbuffer ParticleTemplateParams : register(b1)
{
    float gLifeTime;
    float gRotateSpeed;
    float2 gSize;
    float4 gColor;
    float3 gVelocity;
    float gPad1;
    float3 gAcceleration;
    float gPad2;
    float gMinVerticalVelocity;
    float gMaxVerticalVelocity;
    float gHorizontalVelocityRange;
    float gPad3;
}

StructuredBuffer<ParticleData> ParticlesInput : register(t0);
RWStructuredBuffer<ParticleData> ParticlesOutput : register(u0);
RWStructuredBuffer<uint> gRandomCounter : register(u1);
RWStructuredBuffer<float> gTimeBuffer : register(u2);
RWStructuredBuffer<uint> gSpawnCounter : register(u3);
RWStructuredBuffer<uint> gInactiveParticleIndices : register(u4);
RWStructuredBuffer<uint> gInactiveParticleCount : register(u5);
RWStructuredBuffer<uint> gActiveParticleCounter : register(u6);

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float rand(inout uint state)
{
    state = wang_hash(state);
    return float(state) / 4294967296.0;
}

void InitializeParticle(inout ParticleData particle, inout uint seed)
{
    particle.age = 0.0f;
    particle.lifeTime = gLifeTime;
    particle.rotation = 0.0f;
    particle.rotatespeed = gRotateSpeed;
    particle.size = gSize;
    particle.color = gColor;
    particle.isActive = 1;
    particle.acceleration = gAcceleration;
    
    // 이미터 타입에 따른 위치 설정
    switch (gEmitterType)
    {
        case 0: // point
            particle.position = float3(0.0f, 0.0f, 0.0f);
            break;
            
        case 1: // sphere
        {
                float theta = rand(seed) * 6.28318f;
                float phi = rand(seed) * 3.14159f;
                float radius = gEmitterRadius * pow(rand(seed), 1.0f / 3.0f);
            
                particle.position = float3(
                radius * sin(phi) * cos(theta),
                radius * sin(phi) * sin(theta),
                radius * cos(phi)
            );
                break;
            }
        
        case 2: // box
        {
                particle.position = float3(
                (rand(seed) - 0.5f) * 2.0f * gEmitterSize.x,
                (rand(seed) - 0.5f) * 2.0f * gEmitterSize.y,
                (rand(seed) - 0.5f) * 2.0f * gEmitterSize.z
            );
                break;
            }
        
        case 3: // cone
        {
                float theta = rand(seed) * 6.28318f;
                float height = rand(seed) * gEmitterSize.y;
                float radiusAtHeight = gEmitterRadius * (1.0f - height / gEmitterSize.y);
            
                particle.position = float3(
                radiusAtHeight * cos(theta),
                height,
                radiusAtHeight * sin(theta)
            );
                break;
            }
        
        case 4: // circle
        {
                float theta = rand(seed) * 6.28318f;
                float radius = sqrt(rand(seed)) * gEmitterRadius;
            
                particle.position = float3(
                radius * cos(theta),
                0.0f,
                radius * sin(theta)
            );
                break;
            }
    }
    
    // 초기 속도 설정
    float3 baseVelocity = gVelocity;
    float3 randomVelocity = float3(0, 0, 0);

    if (gHorizontalVelocityRange > 0.0f || gMaxVerticalVelocity > gMinVerticalVelocity)
    {
        float verticalVelocity = lerp(gMinVerticalVelocity, gMaxVerticalVelocity, rand(seed));
        float angle = rand(seed) * 6.28318f;
        float magnitude = rand(seed) * gHorizontalVelocityRange;
    
        randomVelocity = float3(
            magnitude * cos(angle),
            verticalVelocity,
            magnitude * sin(angle)
        );
    }
    
    particle.velocity = baseVelocity + randomVelocity;
}

#define THREAD_GROUP_SIZE 1024

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    
    // 모든 파티클에 대해 기본적으로 입력을 출력으로 복사
    if (particleIndex < gMaxParticles)
    {
        ParticlesOutput[particleIndex] = ParticlesInput[particleIndex];
    }
    
    // 첫 번째 스레드만 스폰 계산 수행
    if (DTid.x == 0)
    {
        uint inactiveCount = gInactiveParticleCount[0];
        
        // 생성할 파티클 수 계산
        float particlesPerSecond = gSpawnRate;
        float particlesThisFrame = particlesPerSecond * gDeltaTime;
        
        // 누적 소수점 처리
        float fractionalPart = gTimeBuffer[0];
        float totalParticles = particlesThisFrame + fractionalPart;
        
        // 실제 생성할 파티클 수
        uint particlesToSpawn = min(uint(totalParticles), inactiveCount);
        
        // 다음 프레임을 위한 소수점 부분 저장
        gTimeBuffer[0] = frac(totalParticles);
        
        // 생성할 파티클 수 저장
        gSpawnCounter[0] = particlesToSpawn;
    }
    
    // 그룹 동기화
    GroupMemoryBarrierWithGroupSync();
    
    // 파티클 생성 처리
    uint globalIndex = DTid.x;
    uint spawnCount = gSpawnCounter[0];
    
    if (globalIndex < spawnCount)
    {
        uint inactiveSlot;
        InterlockedAdd(gInactiveParticleCount[0], -1, inactiveSlot);
        
        // 범위 체크: inactiveSlot이 1 이상이어야 유효 (0이 되면 빈 상태)
        if (inactiveSlot > 0)
        {
            // 스택처럼 뒤에서부터 인덱스 가져오기
            uint particleIndex = gInactiveParticleIndices[inactiveSlot - 1];
            
            if (particleIndex < gMaxParticles)
            {
                // 난수 시드 생성
                uint seed = wang_hash(particleIndex + DTid.x + gRandomCounter[0]);
                InterlockedAdd(gRandomCounter[0], 1);
                
                // 파티클 초기화
                ParticleData particle = (ParticleData) 0;
                InitializeParticle(particle, seed);
                
                // GPU 버퍼에 저장
                ParticlesOutput[particleIndex] = particle;
                
                InterlockedAdd(gActiveParticleCounter[0], 1);
            }
        }
        else
        {
            // 실제로 가져올 인덱스가 없었다면 카운트 복구
            InterlockedAdd(gInactiveParticleCount[0], 1);
        }
    }
}