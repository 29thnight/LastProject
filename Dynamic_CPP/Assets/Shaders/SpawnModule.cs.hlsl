// SpawnModule.hlsl

#define MAX_PARTICLES 1024 // C++ 코드의 m_maxParticles와 일치해야 합니다.

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

float3 RandomDirection()
{
    float u = rand();
    float v = rand();
    float theta = 2.0f * 3.14159265359f * u;
    float phi = acos(2.0f * v - 1.0f);
    float x = sin(phi) * cos(theta);
    float y = cos(phi);
    float z = sin(phi) * sin(theta);
    return float3(x, y, z);
}

float3 RandomPositionInSphere(float radius)
{
    float u = rand();
    float v = rand();
    float theta = 2.0f * 3.14159265359f * u;
    float phi = acos(2.0f * v - 1.0f);
    float r = radius * pow(rand(), 1.0f / 3.0f); // 균등한 체적 샘플링
    float x = r * sin(phi) * cos(theta);
    float y = r * cos(phi);
    float z = r * sin(phi) * sin(theta);
    return float3(x, y, z);
}

float3 RandomPositionInHemisphere(float radius, float3 normal)
{
    float u = rand();
    float v = rand();
    float theta = 2.0f * 3.14159265359f * u;
    float z = rand();
    float r = radius * sqrt(1.0f - z * z);
    float x = r * cos(theta);
    float y = r * sin(theta);
    float3 localDir = normalize(float3(x, y, z));

    // 노멀을 기준으로 로컬 방향을 월드 공간으로 회전
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

float3 RandomVelocityInCone(float angleRadians, float3 direction)
{
    float u = rand();
    float v = rand();
    float r = sqrt(u) * tan(angleRadians / 2.0f);
    float phi = 2.0f * 3.14159265359f * v;
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = 1.0f;
    float3 localDir = normalize(float3(x, y, z));

    // 방향 벡터를 기준으로 로컬 방향을 월드 공간으로 회전
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
        // 파티클 업데이트 로직 (수명 감소, 속도/가속도 적용 등)은 다른 컴퓨트 셰이더에서 처리하는 것이 일반적입니다.
        // 이 셰이더는 주로 새로운 파티클 생성을 담당합니다.
        return;
    }
    else
    {
        // 새로운 파티클 생성 시도
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

                // Emitter 타입에 따른 초기 위치 및 속도 설정
                if (g_Params.emitterType == 0) // Point
                {
                    particle.position = g_Template.position;
                    particle.velocity = g_Template.velocity;
                }
                else if (g_Params.emitterType == 1) // Sphere
                {
                    particle.position = RandomPositionInSphere(1.0f) + g_Template.position; // 반지름 1.0f의 구
                    particle.velocity = normalize(particle.position - g_Template.position) * length(g_Template.velocity);
                }
                else if (g_Params.emitterType == 2) // Hemisphere
                {
                    particle.position = RandomPositionInHemisphere(1.0f, float3(0, 1, 0)) + g_Template.position; // 위쪽 Hemishpere
                    particle.velocity = normalize(particle.position - g_Template.position) * length(g_Template.velocity);
                }
                else if (g_Params.emitterType == 3) // Cone
                {
                    particle.position = g_Template.position;
                    particle.velocity = RandomVelocityInCone(radians(30.0f), normalize(float3(0, 1, 0))) * length(g_Template.velocity); // 30도 콘
                }
                else // 기본 Point Emitter
                {
                    particle.position = g_Template.position;
                    particle.velocity = g_Template.velocity;
                }

                rw_ParticleBuffer[DTid.x] = particle;
            }
            else
            {
                // 최대 파티클 수 도달
            }
        }
    }
}