#pragma once
#include "Core.Minimal.h"
#include "EffectModules.h"
#include "ShaderSystem.h"

// 컴퓨트 셰이더용 파라미터 상수 버퍼
struct SpawnModuleParamsBuffer
{
	float time;
	float spawnRate;
	float deltaTime;
	float timeSinceLastSpawn;
	int emitterType;
	int maxParticles;
	float padding[2]; // 16바이트 정렬을 위한 패딩
};

struct ParticleTemplateBuffer
{
	float3 position;
	float padding1; // 16바이트 정렬
	float3 velocity;
	float padding2; // 16바이트 정렬
	float3 acceleration;
	float padding3; // 16바이트 정렬
	float2 size;
	float lifeTime;
	float rotation;
	float4 color;
	float rotateSpeed;
	float padding4[3]; // 16바이트 정렬
};

class SpawnModuleCS : public ParticleModule
{
public:
    SpawnModuleCS(float spawnRate, EmitterType emitterType, int maxParticles);
    virtual ~SpawnModuleCS();

    // ParticleModule 인터페이스 구현
    void Initialize() override;
    void Update(float delta, std::vector<ParticleData>& particles) override;

    // SpawnModule과 동일한 인터페이스 제공
    void SetEmitterShape(EmitterType type) { m_emitterType = type; }
    void SetSpawnRate(float rate) { m_spawnRate = rate; }

    // 템플릿 접근자
    ParticleData& GetParticleTemplate() { return m_particleTemplate; }

    // 기존 CPU 버전과 동일한 이름으로 함수 제공
    void SetLifeTime(float lifeTime) { m_particleTemplate.lifeTime = lifeTime; }
    void SetRotation(float rotation) { m_particleTemplate.rotation = rotation; }
    void SetRotateSpeed(float rotateSpeed) { m_particleTemplate.rotatespeed = rotateSpeed; }
    void SetSize(float2 size) { m_particleTemplate.size = size; }
    void SetColor(float4 color) { m_particleTemplate.color = color; }
    void SetVelocity(float3 velocity) { m_particleTemplate.velocity = velocity; }
    void SetAcceleration(float3 acceleration) { m_particleTemplate.acceleration = acceleration; }
    void SetPosition(float3 position) { m_particleTemplate.position = position; }

private:
    void CreateBuffers(int maxParticles);
    void UpdateBuffers(float delta, std::vector<ParticleData>& particles);
    void DispatchCompute();

private:
    float m_spawnRate;
    EmitterType m_emitterType;
    int m_maxParticles;
    float m_timeSinceLastSpawn;

    // 파티클 템플릿 (기존 SpawnModule과 동일)
    ParticleData m_particleTemplate;

    // 컴퓨트 셰이더 관련 멤버
    ComPtr<ID3D11ComputeShader> m_pComputeShader;

    // 버퍼
    ComPtr<ID3D11Buffer> m_pParamsBuffer;          // 파라미터
    ComPtr<ID3D11Buffer> m_pTemplateBuffer;        // 파티클 템플릿
    ComPtr<ID3D11Buffer> m_pParticleBuffer;        // 파티클 데이터
    ComPtr<ID3D11Buffer> m_pParticleBufferCPU;     // CPU 읽기용
    ComPtr<ID3D11Buffer> m_pCounterBuffer;         // 활성 파티클 카운터
    ComPtr<ID3D11Buffer> m_pCounterBufferCPU;      // CPU 읽기용
    ComPtr<ID3D11Buffer> m_pSeedBuffer;            // 랜덤 시드값

    // 뷰
    ComPtr<ID3D11ShaderResourceView> m_pParticleSRV;
    ComPtr<ID3D11UnorderedAccessView> m_pParticleUAV;
    ComPtr<ID3D11ShaderResourceView> m_pCounterSRV;
    ComPtr<ID3D11UnorderedAccessView> m_pCounterUAV;
    ComPtr<ID3D11ShaderResourceView> m_pSeedSRV;
};