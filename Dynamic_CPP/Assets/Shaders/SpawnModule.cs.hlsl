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
    float gAccumulatedTime;
    int gEmitterType;
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
    float3 gAcceleration; // 초기 가속도 
    float gPad2; // 패딩
    float gMinVerticalVelocity; // 최소 수직 속도
    float gMaxVerticalVelocity; // 최대 수직 속도
    float gHorizontalVelocityRange; // 수평 속도 범위
    float gPad3; // 패딩
}

// 파티클 버퍼 (register u0에 바인딩)
RWStructuredBuffer<ParticleData> gParticles : register(u0);
// 랜덤 카운터 버퍼 (register u1에 바인딩)
RWStructuredBuffer<uint> gRandomCounter : register(u1);
RWStructuredBuffer<float> gTimeBuffer : register(u2);
// 새로 추가: 스폰 카운터와 필요한 파티클 수를 위한 버퍼
RWStructuredBuffer<uint> gSpawnCounter : register(u3);
RWStructuredBuffer<uint> gActiveParticleCounter : register(u4);

// 난수 생성 함수
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
    return float(state) / 4294967296.0; // 2^32
}

// 파티클 초기화 함수
void InitializeParticle(inout ParticleData particle, uint seed)
{
    // 기본 속성 설정
    particle.age = 0.0f;
    particle.lifeTime = gLifeTime;
    particle.rotation = 0.0f;
    particle.rotatespeed = gRotateSpeed;
    particle.size = gSize;
    particle.color = gColor;
    particle.velocity = gVelocity;
    particle.acceleration = gAcceleration;
    particle.isActive = 1; // 활성화
    
    // 이미터 타입에 따른 위치 설정
    switch (gEmitterType)
    {
        case 0: // point
            particle.position = float3(0.0f, 0.0f, 0.0f);
            break;
            
        case 1: // sphere
        {
                float theta = rand(seed) * 6.28318f; // 0 ~ 2π (방위각)
                float phi = rand(seed) * 3.14159f; // 0 ~ π (고도각)
                float radius = gEmitterRadius; // 반지름
            
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
                float radius = (0.5f + rand(seed) * 0.5f) * gEmitterRadius;
            
                particle.position = float3(
                radius * cos(theta),
                0.0f,
                radius * sin(theta)
            );
                break;
            }
    }
    
    // 초기 속도에 랜덤 변동 추가
    if (gHorizontalVelocityRange > 0.0f || gMaxVerticalVelocity > gMinVerticalVelocity)
    {
        float verticalVelocity = lerp(gMinVerticalVelocity, gMaxVerticalVelocity, rand(seed));
        float angle = rand(seed) * 6.28318f;
        float magnitude = rand(seed) * gHorizontalVelocityRange;
        
        particle.velocity += float3(
            magnitude * cos(angle),
            verticalVelocity,
            magnitude * sin(angle)
        );
    }
}

// 스레드 그룹 크기 정의
#define THREAD_GROUP_SIZE 256

// 메인 컴퓨트 셰이더 함수
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint particleIndex = DTid.x;
    
    // 단계 1: 기존 파티클 업데이트 - 모든 스레드가 담당
    if (particleIndex < gMaxParticles)
    {
        // 현재 파티클이 활성 상태인지 확인
        if (gParticles[particleIndex].isActive == 1)
        {
            // 기존 활성 파티클 업데이트 (나이 증가 등)
            gParticles[particleIndex].age += gDeltaTime;
            
            // 수명이 다했는지 확인
            if (gParticles[particleIndex].age >= gParticles[particleIndex].lifeTime)
            {
                // 파티클 비활성화
                gParticles[particleIndex].isActive = 0;
            }
            else
            {
                // 파티클 위치 업데이트 -> movementmodule로 이동
                //gParticles[particleIndex].velocity += gParticles[particleIndex].acceleration * gDeltaTime;
                //gParticles[particleIndex].position += gParticles[particleIndex].velocity * gDeltaTime;
                //gParticles[particleIndex].rotation += gParticles[particleIndex].rotatespeed * gDeltaTime;
                
                // 활성 파티클 카운터 증가
                InterlockedAdd(gActiveParticleCounter[0], 1);
            }
        }
    }
    
    // 첫 번째 스레드만 이번 프레임에 생성할 파티클 수를 계산
    if (DTid.x == 0)
    {
        // 활성 파티클 카운터 초기화
        gActiveParticleCounter[0] = 0;
    
        // 누적 시간에 현재 델타 타임 추가
        float newAccumulatedTime = gAccumulatedTime + gDeltaTime;
    
        // 기본 방법: 정확한 파티클 수 계산
        float particlesPerSecond = gSpawnRate;
        float particlesThisFrame = particlesPerSecond * gDeltaTime;
    
        // 누적 소수점 처리
        float fractionalPart = frac(gTimeBuffer[0]);
        float totalParticles = particlesThisFrame + fractionalPart;
    
        // 생성할 파티클 수 (정수부)
        uint particlesToSpawn = uint(totalParticles);
    
        // 남은 소수점 부분은 다음 프레임으로
        float remainingFraction = frac(totalParticles);
    
        // 디버깅용 카운터 업데이트
        gSpawnCounter[0] = particlesToSpawn;
        gSpawnCounter[1] = 0; // 이번 프레임에 생성된 카운트는 0으로 초기화
    
        // 남은 소수점 저장
        gTimeBuffer[0] = remainingFraction;
    }
    
    // 그룹 동기화 - 모든 스레드가 이 지점에 도달할 때까지 대기
    GroupMemoryBarrierWithGroupSync();
    
    // 단계 2: 새 파티클 생성 - 병렬로 처리
    if (gSpawnCounter[0] > 0 && gSpawnRate > 0.0f)
    {
        // 각 스레드가 하나의 파티클을 담당
        for (uint i = 0; i < gMaxParticles; i += THREAD_GROUP_SIZE)
        {
            uint idx = i + GTid.x;
            
            if (idx < gMaxParticles && gParticles[idx].isActive == 0)
            {
                // 원자적 연산으로 카운터 증가
                uint spawnIndex;
                InterlockedAdd(gSpawnCounter[1], 1, spawnIndex);
                
                // 아직 생성해야 할 파티클이 남아있는지 확인
                if (spawnIndex < gSpawnCounter[0])
                {
                    // 난수 시드 생성 (시간, 인덱스, 그룹 ID 등을 조합)
                    uint seed = wang_hash(idx + spawnIndex + Gid.x * 1000 + GTid.x);
                    InterlockedAdd(gRandomCounter[0], 1); // 다음 프레임을 위한 시드 증가
                    
                    // 파티클 초기화
                    InitializeParticle(gParticles[idx], seed);
                    
                    // 할당된 파티클을 찾았으므로 루프 종료
                    break;
                }
            }
        }
    }
}