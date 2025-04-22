// ParticleSpawn.hlsl - SpawnModule에 대응하는 컴퓨트 셰이더

// 파티클 데이터 구조체 - C++ 구조체와 정확히 일치해야 함
struct ParticleData
{
    float3 position;
    float3 velocity;
    float3 acceleration;
    float2 size;
    float age;
    float lifeTime;
    float rotation;
    float rotatespeed;
    float4 color;
    uint isActive;
};

// 스폰 모듈 파라미터 상수 버퍼 (register b0에 바인딩)
cbuffer SpawnParams : register(b0)
{
    float gSpawnRate; // 초당 스폰할 파티클 수
    float gDeltaTime; // 델타 타임
    float gAccumulatedTime; // 누적 시간
    int gEmitterType; // 이미터 타입 (0:point, 1:sphere, 2:box, 3:cone, 4:circle)
    float gEmitterSizeX; // 이미터 X 크기
    float gEmitterSizeY; // 이미터 Y 크기
    float gEmitterSizeZ; // 이미터 Z 크기
    float gEmitterRadius; // 이미터 반지름
    uint gMaxParticles; // 최대 파티클 수
    float3 gPad; // 패딩
}

// 파티클 템플릿 상수 버퍼 (register b1에 바인딩)
cbuffer ParticleTemplateParams : register(b1)
{
    float gLifeTime; // 파티클 수명
    float gRotateSpeed; // 회전 속도
    float gSizeX; // 초기 크기 X
    float gSizeY; // 초기 크기 Y
    float gColorR; // 초기 색상 R
    float gColorG; // 초기 색상 G
    float gColorB; // 초기 색상 B
    float gColorA; // 초기 색상 A
    float gVelocityX; // 초기 속도 X
    float gVelocityY; // 초기 속도 Y
    float gVelocityZ; // 초기 속도 Z
    float gPad1; // 패딩
    float gAccelerationX; // 초기 가속도 X
    float gAccelerationY; // 초기 가속도 Y
    float gAccelerationZ; // 초기 가속도 Z
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
    particle.size = float2(gSizeX, gSizeY);
    particle.color = float4(gColorR, gColorG, gColorB, gColorA);
    particle.velocity = float3(gVelocityX, gVelocityY, gVelocityZ);
    particle.acceleration = float3(gAccelerationX, gAccelerationY, gAccelerationZ);
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
                (rand(seed) - 0.5f) * 2.0f * gEmitterSizeX,
                (rand(seed) - 0.5f) * 2.0f * gEmitterSizeY,
                (rand(seed) - 0.5f) * 2.0f * gEmitterSizeZ
            );
                break;
            }
        
        case 3: // cone
        {
                float theta = rand(seed) * 6.28318f;
                float height = rand(seed) * gEmitterSizeY;
                float radiusAtHeight = gEmitterRadius * (1.0f - height / gEmitterSizeY);
            
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
#define THREAD_GROUP_SIZE 256

// 메인 컴퓨트 셰이더 함수
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void CS_SpawnParticles(uint3 DTid : SV_DispatchThreadID)
{
    // 첫 번째 스레드만 스폰 로직 처리 (스레드 0)
    if (DTid.x == 0)
    {
        // 누적 시간 업데이트
        float accumulatedTime = gAccumulatedTime + gDeltaTime;
        
        // 스폰 간격 계산
        float spawnInterval = 1.0f / gSpawnRate;
        
        // 이번 프레임에서 생성해야 할 파티클 수 계산
        while (accumulatedTime >= spawnInterval)
        {
            // 비활성 파티클 찾기
            uint particleIndex = 0xFFFFFFFF; // 초기값을 무효한 인덱스로 설정
            
            for (uint i = 0; i < gMaxParticles
                ; i++)
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
        gAccumulatedTime = accumulatedTime;
    }
}