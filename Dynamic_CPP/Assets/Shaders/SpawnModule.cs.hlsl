
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
    
    // 초기 속도에 랜덤 변동 추가 (선택 사항)
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
#define THREAD_GROUP_SIZE 1024

// 메인 컴퓨트 셰이더 함수
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    
    // 파티클 인덱스가 유효한 범위인지 확인
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
                // 파티클 위치 업데이트
                gParticles[particleIndex].velocity += gParticles[particleIndex].acceleration * gDeltaTime;
                gParticles[particleIndex].position += gParticles[particleIndex].velocity * gDeltaTime;
                gParticles[particleIndex].rotation += gParticles[particleIndex].rotatespeed * gDeltaTime;
            }
        }
    }
    
    // 첫 번째 스레드만 새 파티클 생성 처리
    if (DTid.x == 0)
    {
        // 누적 시간 업데이트
        float accumulatedTime = gAccumulatedTime + gDeltaTime;
        
        // 스폰 간격 계산
        float spawnInterval = 1.0f / gSpawnRate;
        
        // 디버깅을 위해 임의의 파티클 하나 생성 (테스트용)
        if (gSpawnRate > 0.0f) // 스폰 레이트가 0보다 크면
        {
            for (uint i = 0; i < gMaxParticles; i++)
            {
                if (gParticles[i].isActive == 0)
                {
                    // 난수 시드 생성
                    uint seed = wang_hash(i + gRandomCounter[0]);
                    gRandomCounter[0] = seed; // 다음 프레임을 위해 카운터 업데이트
                    
                    // 파티클 초기화
                    InitializeParticle(gParticles[i], seed);
                    
                    // 하나의 파티클만 생성하고 빠져나감 (테스트용)
                    break;
                }
            }
        }
        
        // 이번 프레임에서 생성해야 할 파티클 수 계산
        while (accumulatedTime >= spawnInterval && gSpawnRate > 0.0f)
        {
            // 비활성 파티클 찾기
            uint particleIndex = 0xFFFFFFFF; // 초기값을 무효한 인덱스로 설정
            
            for (uint i = 0; i < gMaxParticles; i++)
            {
                if (gParticles[i].isActive == 0)
                {
                    particleIndex = i;
                    break;
                }
            }
            
            // 비활성 파티클을 찾았으면 초기화
            if (particleIndex != 0xFFFFFFFF)
            {
                // 난수 시드 생성
                uint seed = wang_hash(particleIndex + gRandomCounter[0]);
                gRandomCounter[0] = seed; // 다음 프레임을 위해 카운터 업데이트
                
                // 파티클 초기화
                InitializeParticle(gParticles[particleIndex], seed);
                
                // 누적 시간 감소
                accumulatedTime -= spawnInterval;
            }
            else
            {
                // 비활성 파티클을 찾지 못했으면 루프 탈출
                break;
            }
        }
        
        // 남은 누적 시간 저장
        gTimeBuffer[0] = accumulatedTime;
    }
}