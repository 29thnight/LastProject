// SpawnModule.hlsl

struct SpawnModuleParamsBuffer
{
    float time;
    float spawnRate;
    float deltaTime;
    float timeSinceLastSpawn;
    int emitterType;
    int maxParticles;
};

struct ParticleTemplateBuffer
{
    float3 position;
    float3 velocity;
    float3 acceleration;
    float2 size;
    float4 color;
    float lifeTime;
    float rotation;
    float rotateSpeed;
};

struct ParticleData
{
    bool isActive;
    float age;
    float lifeTime;
    float rotation;
    float rotatespeed;
    float2 size;
    float4 color;
    float3 position;
    float3 velocity;
    float3 acceleration;
};

ConstantBuffer<SpawnModuleParamsBuffer> g_Params : register(b0);
ConstantBuffer<ParticleTemplateBuffer> g_Template : register(b1);

StructuredBuffer<ParticleData> g_ParticleBuffer : register(t0);
RWStructuredBuffer<ParticleData> rw_ParticleBuffer : register(u0);
RWStructuredBuffer<uint> rw_CounterBuffer : register(u1);
StructuredBuffer<uint> g_SeedBuffer : register(t2);

uint xorshift32(inout uint state)
{
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}

// 0.0 ~ 1.0 사이의 float 랜덤 값 생성 (결정적)
float rand_deterministic(uint threadID, int offset)
{
    uint seed = g_SeedBuffer[threadID];
    uint currentState = seed + offset; // 각 랜덤 호출에 대해 약간 다른 시드 사용
    return (xorshift32(currentState) & 0xFFFFFF) / float(0xFFFFFF);
}

float3 RandomDirection_Deterministic(uint threadID)
{
    float u = rand_deterministic(threadID, 0);
    float v = rand_deterministic(threadID, 1);
    float theta = 2.0f * 3.14159265359f * u;
    float phi = acos(2.0f * v - 1.0f);
    float x = sin(phi) * cos(theta);
    float y = cos(phi);
    float z = sin(phi) * sin(theta);
    return float3(x, y, z);
}

float3 RandomPositionInSphere_Deterministic(float radius, uint threadID)
{
    float u = rand_deterministic(threadID, 2);
    float v = rand_deterministic(threadID, 3);
    float theta = 2.0f * 3.14159265359f * u;
    float phi = acos(2.0f * v - 1.0f);
    float r = radius * pow(rand_deterministic(threadID, 4), 1.0f / 3.0f);
    float x = r * sin(phi) * cos(theta);
    float y = r * cos(phi);
    float z = r * sin(phi) * sin(theta);
    return float3(x, y, z);
}

float3 RandomPositionInBox_Deterministic(float boxSize, uint threadID)
{
    float x = (rand_deterministic(threadID, 10) - 0.5f) * 2.0f * boxSize;
    float y = (rand_deterministic(threadID, 11) - 0.5f) * 2.0f * boxSize;
    float z = (rand_deterministic(threadID, 12) - 0.5f) * 2.0f * boxSize;
    return float3(x, y, z);
}

float3 RandomPositionInCircle_Deterministic(float radius, uint threadID)
{
    float theta = rand_deterministic(threadID, 13) * 6.28f; // 0 ~ 2π
    float r = radius * (0.5f + rand_deterministic(threadID, 14) * 0.5f); // 반지름의 50% ~ 100%
    float x = r * cos(theta);
    float z = r * sin(theta);
    return float3(x, 0.0f, z); // y축이 0인 원형 (XZ 평면)
}

float3 RandomPositionInCone_Deterministic(float height, uint threadID)
{
    float theta = rand_deterministic(threadID, 15) * 6.28f; // 0 ~ 2π
    float h = rand_deterministic(threadID, 16) * height; // 높이
    float radiusAtHeight = 0.5f * (1.0f - h / height); // 높이에 따른 반지름 계산
    float x = radiusAtHeight * cos(theta);
    float z = radiusAtHeight * sin(theta);
    return float3(x, h, z); // y축이 원뿔의 높이 방향
}

float3 RandomPositionInHemisphere_Deterministic(float radius, float3 normal, uint threadID)
{
    float u = rand_deterministic(threadID, 5);
    float v = rand_deterministic(threadID, 6);
    float theta = 2.0f * 3.14159265359f * u;
    float z = rand_deterministic(threadID, 7);
    float r = radius * sqrt(1.0f - z * z);
    float x = r * cos(theta);
    float y = r * sin(theta);
    float3 localDir = normalize(float3(x, y, z));

    float3 tangent, binormal;
    if (abs(normal.x) > abs(normal.y))
    {
        tangent = normalize(float3(-normal.z, 0.0f, normal.x));
    }
    else
    {
        tangent = normalize(float3(0.0f, -normal.z, normal.y));
    }
    binormal = cross(normal, tangent);

    return g_Template.position + tangent * localDir.x + binormal * localDir.y + normal * localDir.z * radius;
}

float3 RandomVelocityInCone_Deterministic(float angleRadians, float3 direction, uint threadID)
{
    float u = rand_deterministic(threadID, 8);
    float v = rand_deterministic(threadID, 9);
    float r = sqrt(u) * tan(angleRadians / 2.0f);
    float phi = 2.0f * 3.14159265359f * v;
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = 1.0f;
    float3 localDir = normalize(float3(x, y, z));

    float3 tangent, binormal;
    if (abs(direction.x) > abs(direction.y))
    {
        tangent = normalize(float3(-direction.z, 0.0f, direction.x));
    }
    else
    {
        tangent = normalize(float3(0.0f, -direction.z, direction.y));
    }
    binormal = cross(direction, tangent);

    return normalize(tangent * localDir.x + binormal * localDir.y + direction * localDir.z);
}

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_Params.maxParticles)
    {
        return;
    }

    ParticleData particle = rw_ParticleBuffer[DTid.x];

    if (particle.isActive)
    {
        return;
    }
    else
    {
        float spawnInterval = 1.0f / g_Params.spawnRate;
        if (g_Params.timeSinceLastSpawn >= spawnInterval)
        {
            uint currentCount;
            uint maxCount = g_Params.maxParticles;
            InterlockedAdd(rw_CounterBuffer[0], 1, currentCount);

            if (currentCount < maxCount)
            {
                particle.isActive = true;
                particle.age = 0.0f;
                particle.lifeTime = g_Template.lifeTime;
                particle.rotation = g_Template.rotation;
                particle.rotatespeed = g_Template.rotateSpeed;
                particle.size = g_Template.size;
                particle.color = g_Template.color;
                particle.acceleration = g_Template.acceleration;

                // 이미터 타입에 따른 처리
                // 0: Point, 1: Sphere, 2: Box, 3: Cone, 4: Circle
                if (g_Params.emitterType == 0) // Point
                {
                    particle.position = g_Template.position;
                    particle.velocity = g_Template.velocity;
                }
                else if (g_Params.emitterType == 1) // Sphere
                {
                    particle.position = RandomPositionInSphere_Deterministic(1.0f, DTid.x) + g_Template.position;
                    particle.velocity = normalize(particle.position - g_Template.position) * length(g_Template.velocity);
                }
                else if (g_Params.emitterType == 2) // Box
                {
                    float height = 1.0f; // 원뿔 높이
                    particle.position = RandomPositionInCone_Deterministic(height, DTid.x) + g_Template.position;
                    particle.velocity = float3(0, 1, 0) * length(g_Template.velocity); // 위쪽 방향으로 속도 
                }
                else if (g_Params.emitterType == 3) // Cone
                {
                    float boxSize = 1.0f; // 상자 크기
                    particle.position = RandomPositionInBox_Deterministic(boxSize, DTid.x) + g_Template.position;
                    particle.velocity = g_Template.velocity;
                }
                else if (g_Params.emitterType == 4) // Circle
                {
                    float radius = 1.0f; // 원 반지름
                    particle.position = RandomPositionInCircle_Deterministic(radius, DTid.x) + g_Template.position;
                    particle.velocity = g_Template.velocity;
                }
                else // 기본 Point Emitter
                {
                    particle.position = g_Template.position;
                    particle.velocity = g_Template.velocity;
                }

                rw_ParticleBuffer[DTid.x] = particle;
            }
        }
    }
}