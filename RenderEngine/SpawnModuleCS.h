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
	int padding[2];
};

// 파티클 템플릿 상수 버퍼
struct ParticleTemplateBuffer
{
	Mathf::Vector3 position;
	Mathf::Vector3 velocity;
	Mathf::Vector3 acceleration;
	Mathf::Vector2 size;
	Mathf::Vector4 color;
	float lifeTime;
	float rotation;
	float rotateSpeed;
	float padding;
};

// 컴퓨트 셰이더를 사용하는 스폰 모듈
class SpawnModuleCS : public ParticleModule
{
public:
	SpawnModuleCS(float spawnRate, EmitterType emitterType, int maxParticles);
	~SpawnModuleCS();

	void Initialize() override;
	void Update(float delta, std::vector<ParticleData>& particles) override;

	void SetEmitterShape(EmitterType type) { m_emitterType = type; }
	void SetSpawnRate(float rate) { m_spawnRate = rate; }

	// 파티클 템플릿 설정 메서드
	void SetParticleTemplate(const ParticleData& templateData) { m_particleTemplate = templateData; }

private:
	void CreateBuffers(int maxParticles);
	void UpdateBuffers(float delta, std::vector<ParticleData>& particles);
	void DispatchCompute();

private:
	float m_spawnRate;
	float m_timeSinceLastSpawn;
	EmitterType m_emitterType;
	int m_maxParticles;

	// 파티클 템플릿 데이터
	ParticleData m_particleTemplate;

	// 컴퓨트 셰이더 리소스
	ComPtr<ID3D11ComputeShader> m_pComputeShader;
	ComPtr<ID3D11Buffer> m_pParamsBuffer;          // 스폰 파라미터 상수 버퍼
	ComPtr<ID3D11Buffer> m_pTemplateBuffer;        // 파티클 템플릿 상수 버퍼
	ComPtr<ID3D11Buffer> m_pParticleBuffer;        // 파티클 데이터 버퍼 (RW)
	ComPtr<ID3D11Buffer> m_pParticleBufferCPU;     // CPU 읽기용 버퍼
	ComPtr<ID3D11Buffer> m_pCounterBuffer;         // 활성 파티클 카운터
	ComPtr<ID3D11Buffer> m_pCounterBufferCPU;      // CPU 읽기용 카운터 버퍼
	ComPtr<ID3D11ShaderResourceView> m_pParticleSRV;
	ComPtr<ID3D11UnorderedAccessView> m_pParticleUAV;
	ComPtr<ID3D11ShaderResourceView> m_pCounterSRV;
	ComPtr<ID3D11UnorderedAccessView> m_pCounterUAV;
};