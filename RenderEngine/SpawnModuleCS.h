#pragma once
#include "ParticleModule.h"

enum class EmitterType;
class SpawnModuleCS : public ParticleModule
{
public:
	SpawnModuleCS(float spawnRate, EmitterType emitterType, int maxParticles)
		: m_spawnRate(spawnRate), m_emitterType(emitterType),
		m_Time(0.0f), m_random(std::random_device{}()),
		m_computeShader(nullptr), m_spawnParamsBuffer(nullptr),
		m_templateBuffer(nullptr), m_randomCounterBuffer(nullptr),
		m_randomCounterUAV(nullptr), m_isInitialized(false),
		m_particlesCapacity(maxParticles)
	{
		m_uniform = std::uniform_real_distribution<float>(0.0f, 1.0f);
	}

	~SpawnModuleCS()
	{
		Release();
	}

	// ParticleModule method
	void Initialize() override;
	void Update(float delta, std::vector<ParticleData>& particles) override;

	// SpawnModule type method
	void SetEmitterShape(EmitterType type) { m_emitterType = type; m_paramsDirty = true; }
	void SetSpawnRate(float rate) { m_spawnRate = rate; m_paramsDirty = true; }
	void SetMaxParticles(UINT maxParticles);
	void SetLifeTime(float value) { m_particleTemplate.lifeTime = value; m_templateDirty = true; }
	void SetRotateSpeed(float value) { m_particleTemplate.rotatespeed = value; m_templateDirty = true; }
	void SetSize(float2 value) { m_particleTemplate.size = value; m_templateDirty = true; }
	void SetColor(float4 value) { m_particleTemplate.color = value; m_templateDirty = true; }
	void SetVelocity(float3 value) { m_particleTemplate.velocity = value; m_templateDirty = true; }
	void SetAcceleration(float3 value) { m_particleTemplate.acceleration = value; m_templateDirty = true; }

	// ????
	void SetVerticalVelocity(float min, float max) { m_minVerticalVelocity = min; m_maxVerticalVelocity = max; m_templateDirty = true; }
	void SetHorizontalVelocityRange(float range) { m_horizontalVelocityRange = range; m_templateDirty = true; }

	// compute shader initialize method
	bool InitializeCompute();
	bool TryGetCPUCount(UINT* count);

	// Get method
	UINT GetActiveParticleCount();
	ID3D11Buffer* GetSourceBuffer() { return m_usingBufferA ? m_particlesBufferA : m_particlesBufferB; }
	ID3D11Buffer* GetDestBuffer() { return m_usingBufferA ? m_particlesBufferB : m_particlesBufferA; }
	ID3D11UnorderedAccessView* GetSourceUAV() { return m_usingBufferA ? m_particlesUAV_A : m_particlesUAV_B; }
	ID3D11UnorderedAccessView* GetDestUAV() { return m_usingBufferA ? m_particlesUAV_B : m_particlesUAV_A; }
	ID3D11ShaderResourceView* GetSourceSRV() { return m_usingBufferA ? m_particlesSRV_A.Get() : m_particlesSRV_B.Get(); }
	ID3D11ShaderResourceView* GetDestSRV() { return m_usingBufferA ? m_particlesSRV_B.Get() : m_particlesSRV_A.Get(); }
	ID3D11ShaderResourceView* GetParticlesSRV() const { return m_particlesSRV_A.Get(); }
	UINT GetParticleCount() const { return m_particlesCapacity; }
public:
	ParticleData m_particleTemplate;
	ComPtr<ID3D11ShaderResourceView> m_particlesSRV_A;
	ComPtr<ID3D11ShaderResourceView> m_particlesSRV_B;
private:

	// compute shader method
	bool CreateBuffers(std::vector<ParticleData>& particles);
	void UpdateConstantBuffers(float delta);
	void Release();

	// 스폰 파라미터 구조체 (상수 버퍼)
	struct alignas(16) SpawnParams
	{
		float spawnRate;
		float deltaTime;
		float accumulatedTime;
		int emitterType;

		float3 emitterSize;
		float emitterRadius;

		UINT maxParticles;
		float3 pad;
	};

	// 파티클 템플릿 구조체 (상수 버퍼)
	struct alignas(16) ParticleTemplateParams
	{
		float lifeTime;
		float rotateSpeed;
		float2 size;

		float4 color;

		float3 velocity;
		float pad1;

		float3 acceleration;
		float pad2;

		float minVerticalVelocity;
		float maxVerticalVelocity;
		float horizontalVelocityRange;
		float pad3;
	};

private:
	// 기존 SpawnModule 변수
	float m_spawnRate;
	float m_Time;
	EmitterType m_emitterType;
	std::mt19937 m_random;
	std::uniform_real_distribution<float> m_uniform;
	float2 m_initialSize;
	float4 m_initialColor;
	float3 m_initialAcceleration;
	float m_minVerticalVelocity;
	float m_maxVerticalVelocity;
	float m_horizontalVelocityRange;
	size_t m_particlesCapacity;

	// 컴퓨트 셰이더 관련 변수
	ID3D11Buffer* m_spawnCounterBuffer = nullptr;
	ID3D11UnorderedAccessView* m_spawnCounterUAV = nullptr;
	ID3D11ComputeShader* m_computeShader;
	ID3D11Buffer* m_spawnParamsBuffer;
	ID3D11Buffer* m_templateBuffer;
	ID3D11Buffer* m_randomCounterBuffer;
	ID3D11UnorderedAccessView* m_randomCounterUAV;
	ID3D11Buffer* m_particlesBufferA = nullptr;
	ID3D11Buffer* m_particlesBufferB = nullptr;
	ID3D11UnorderedAccessView* m_particlesUAV_A = nullptr;
	ID3D11UnorderedAccessView* m_particlesUAV_B = nullptr;
	ID3D11Buffer* m_timeBuffer;
	ID3D11UnorderedAccessView* m_timeUAV;

	// 초기화 관련 변수
	bool m_isInitialized;
	bool m_paramsDirty;
	bool m_templateDirty;
	bool m_usingBufferA;
	
	// 파티클 수 추적 함수
	ID3D11Buffer* m_activeCountBuffer;				// 활성 파티클 수를 저장할 버퍼
	ID3D11UnorderedAccessView* m_activeCountUAV;	// 버퍼의 UAV
	ID3D11Buffer* m_activeCountStagingBuffer;		// CPU 읽기용 스테이징 버퍼
	float m_correctionFactor;
	UINT m_actualCount;

};