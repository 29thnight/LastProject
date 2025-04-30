#pragma once
#include "Core.Minimal.h"
#include "random"
#include "Texture.h"
#include "RenderModules.h"
#include "IRenderPass.h"
#include "LinkedListLib.hpp"
#include "EaseInOut.h"

struct alignas(16) EffectParameters		// 공통 effectparams
{
	float time;
	float intensity;
	float speed;
	float pad;
};

struct alignas(16) ParticleData
{
	Mathf::Vector3 position;
	float pad1;

	Mathf::Vector3 velocity;
	float pad2;

	Mathf::Vector3 acceleration;
	float pad3;

	Mathf::Vector2 size;
	float age;
	float lifeTime;

	float rotation;
	float rotatespeed;
	float2 pad4;

	Mathf::Vector4 color;

	UINT isActive;
	float3 pad5;
};

enum class EmitterType
{
	point,
	sphere,
	box,
	cone,
	circle,
};

enum class ColorTransitionMode
{
	Gradient,
	Discrete,
	Custom,
};

class ParticleModule : public LinkProperty<ParticleModule>
{	
public:
	ParticleModule() : LinkProperty<ParticleModule>(this) {}
	virtual ~ParticleModule() = default;
	virtual void Initialize() {}
	virtual void Update(float delta, std::vector<ParticleData>& particles) = 0;

	void SetEasingType(EasingEffect type) 
	{
		m_easingType = type;
		m_useEasing = true;
	}

	void SetAnimationType(StepAnimation type) 
	{
		m_animationType = type;
		m_useEasing = true;
	}

	void SetEasingDuration(float duration) 
	{
		m_easingDuration = duration;
	}

	void EnableEasing(bool enable) 
	{
		m_useEasing = enable;
	}

	bool IsEasingEnabled() const 
	{
		return m_useEasing;
	}

	float ApplyEasing(float normalizedTime);

	EaseInOut CreateEasingObject() 
	{
		return EaseInOut(m_easingType, m_animationType, m_easingDuration);
	}

protected:
	bool m_useEasing;
	EasingEffect m_easingType;
	StepAnimation m_animationType;
	float m_easingDuration;
};

// spawn rate, emittertype
class SpawnModule : public ParticleModule
{
public:
	SpawnModule(float spawnRate, EmitterType emitterType)
	: m_spawnRate(spawnRate), m_emitterType(emitterType),
		m_Time(0.0f), m_random(std::random_device{}()) {}
	 
	void Initialize() override;
	void Update(float delta, std::vector<ParticleData>& particles) override;

	void SetEmitterShape(EmitterType type) { m_emitterType = type; }
	void SetSpawnRate(float rate) { m_spawnRate = rate; }
	void SetMaxParticles(UINT maxParticles);
public:
	ParticleData m_particleTemplate;

private:
	void SpawnParticle(std::vector<ParticleData>& particles);
	void InitializeParticle(ParticleData& particle);

private:
	float m_spawnRate;
	float m_Time;
	EmitterType m_emitterType;
	std::mt19937 m_random;
	std::uniform_real_distribution<float> m_uniform;


	float m_lifeTime;
	float m_rotateSpeed;
	float2 m_initialSize;
	float4 m_initialColor;
	float3 m_initialAcceleration;

	float m_minVerticalVelocity;
	float m_maxVerticalVelocity;
	float m_horizontalVelocityRange;
};

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

	// ParticleModule 인터페이스 구현
	void Initialize() override;
	void Update(float delta, std::vector<ParticleData>& particles) override;

	// SpawnModule 인터페이스 구현
	void SetEmitterShape(EmitterType type) { m_emitterType = type; m_paramsDirty = true; }
	void SetSpawnRate(float rate) { m_spawnRate = rate; m_paramsDirty = true; }
	void SetMaxParticles(UINT maxParticles);

	// 컴퓨트 셰이더 초기화 메서드
	bool InitializeCompute();

	ID3D11ShaderResourceView* GetParticlesSRV() const { return m_particlesSRV_A.Get(); }
	UINT GetParticleCount() const { return m_particlesCapacity; }
	UINT GetActiveParticleCount() const;

	ID3D11Buffer* GetSourceBuffer() { return m_usingBufferA ? m_particlesBufferA : m_particlesBufferB; }
	ID3D11Buffer* GetDestBuffer() { return m_usingBufferA ? m_particlesBufferB : m_particlesBufferA; }
	ID3D11UnorderedAccessView* GetSourceUAV() { return m_usingBufferA ? m_particlesUAV_A : m_particlesUAV_B; }
	ID3D11UnorderedAccessView* GetDestUAV() { return m_usingBufferA ? m_particlesUAV_B : m_particlesUAV_A; }
	ID3D11ShaderResourceView* GetSourceSRV() { return m_usingBufferA ? m_particlesSRV_A.Get() : m_particlesSRV_B.Get(); }
	ID3D11ShaderResourceView* GetDestSRV() { return m_usingBufferA ? m_particlesSRV_B.Get() : m_particlesSRV_A.Get(); }

public:
	ParticleData m_particleTemplate;
	ComPtr<ID3D11ShaderResourceView> m_particlesSRV_A;
	ComPtr<ID3D11ShaderResourceView> m_particlesSRV_B;
private:
	// 기존 메서드 (CPU 폴백용)
	void SpawnParticle(std::vector<ParticleData>& particles);
	void InitializeParticle(ParticleData& particle);

	// 컴퓨트 셰이더 관련 메서드
	bool CreateBuffers(std::vector<ParticleData>& particles);
	void UpdateConstantBuffers(float delta);
	void UpdateParticleBuffer(std::vector<ParticleData>& particles);
	void ReadBackParticleBuffer(std::vector<ParticleData>& particles);
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
	float m_lifeTime;
	float m_rotateSpeed;
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
	ID3D11Buffer* m_particlesStagingBuffer;
	ID3D11Buffer* m_timeBuffer;
	ID3D11UnorderedAccessView* m_timeUAV;

	bool m_isInitialized;
	bool m_paramsDirty;
	bool m_templateDirty;
	bool m_usingBufferA;
};

// gravity, gravity strength
class MovementModule : public ParticleModule
{
public:
	MovementModule() : m_gravity(true), m_gravityStrength(1.0f) {}

	void Update(float delta, std::vector<ParticleData>& particles) override;

	void SetUseGravity(bool use) { m_gravity = use; }

	void SetGravityStrength(float strength) { m_gravityStrength = strength; }
private:
	bool m_gravity;
	float m_gravityStrength;
};

// none
class LifeModule : public ParticleModule
{
public:
	void Update(float delta, std::vector<ParticleData>& particles) override;
private:
};

// colorgradient
class ColorModule : public ParticleModule
{
private:
	using ColorEvaluator = std::function<Mathf::Vector4(float)>;
	ColorEvaluator m_customEvaluator = nullptr;
public:
	ColorModule()
	{
		m_colorGradient = {
			{0.0f, float4(1.0f,1.0f,1.0f,1.0f)},
			{0.3f, float4(1.0f,1.0f,0.0f,0.9f)},
			{0.7f, float4(1.0f,0.0f,0.0f,0.7f)},
			{1.0f, float4(0.5f,0.0f,0.0f,0.0f)},
		};

	}

	void Update(float delta, std::vector<ParticleData>& particles) override;

	// 보간
	void SetColorGradient(const std::vector<std::pair<float, Mathf::Vector4>>& gradient) { m_colorGradient = gradient; }

	// 모드 설정 (보간, 이산, 사용자 정의 함수)
	void SetTransitionMode(ColorTransitionMode mode) { m_transitionMode = mode; }

	// 사용자 정의 함수
	void SetCustomEvaluator(const ColorEvaluator& evaluator) { m_customEvaluator = evaluator; }

	// 이산
	void SetDiscreteColors(const std::vector<Mathf::Vector4>& colors) { m_discreteColors = colors; }
private:
	// 통합 색상 평가 함수
	Mathf::Vector4 EvaluateColor(float t);

	// 보간용 평가 함수
	Mathf::Vector4 EvaluateGradient(float t);

	// 이산용 평가 함수
	Mathf::Vector4 EvaluateDiscrete(float t);

	std::vector<std::pair<float, Mathf::Vector4>> m_colorGradient;

	ColorTransitionMode m_transitionMode = ColorTransitionMode::Gradient;

	std::vector<Mathf::Vector4> m_discreteColors;
};

// startsize, endsize, sizefunction
class SizeModule : public ParticleModule
{
public:
	SizeModule() : m_startSize(0.1f), m_endSize(1.0f), m_sizeOverLife([this](float t) {return Mathf::Vector2::Lerp(
		Mathf::Vector2(m_startSize, m_startSize),
		Mathf::Vector2(m_endSize, m_endSize),
		t
	);
		}) {}

	void Update(float delta, std::vector<ParticleData>& particles) override;

	// m_sizeoverlife function을 설정 안했을때의 기본으로 쓰이는 크기
	void SetStartSize(float size) { m_startSize = size; }
	void SetEndSize(float size) { m_endSize = size; }

	void SetSizeOverLifeFunction(std::function<Mathf::Vector2(float)> func) { m_sizeOverLife = func; }

private:
	float m_startSize;
	float m_endSize;
	std::function<Mathf::Vector2(float)> m_sizeOverLife;
};

// floorheight, bouncefactor 
class CollisionModule : public ParticleModule
{
public:
	CollisionModule() : m_floorHeight(0.0f), m_bounceFactor(0.5f) {}

	void Update(float delta, std::vector<ParticleData>& particles) override;

	void SetFloorHeight(float height) { m_floorHeight = height; }

	void SetBounceFactor(float factor) { m_bounceFactor = factor; }

private:
	float m_floorHeight;
	float m_bounceFactor;
};

// maxparticles
class EffectModules
{
public:
	EffectModules(int maxParticles = 1000);
	~EffectModules();

	template<typename T, typename... Args>
	T* AddModule(Args&&... args)
	{
		static_assert(std::is_base_of<ParticleModule, T>::value, "T must derive from ParticleModule");

		T* module = new T(std::forward<Args>(args)...);
		module->Initialize();
		m_moduleList.Link(module);
		return module;
	}

	template<typename T>
	T* GetModule()
	{
		static_assert(std::is_base_of<ParticleModule, T>::value, "T must derive from ParticleModule");

		for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it)
		{
			ParticleModule& module = *it;
			if (T* t = dynamic_cast<T*>(&module))
			{
				return t;
			}
		}
		return nullptr;
	}

	template<typename T, typename... Args>
	T* AddRenderModule(Args&&... args) {
		static_assert(std::is_base_of<RenderModules, T>::value,
			"T must be derived from RenderModules");

		T* module = new T(std::forward<Args>(args)...);
		module->Initialize();
		m_renderModules.push_back(module);
		return module;
	}

	template<typename T>
	T* GetRenderModule()
	{
		static_assert(std::is_base_of<RenderModules, T>::value, "T must derive from RenderModules");

		for (auto* module : m_renderModules)
		{
			if (T* t = dynamic_cast<T*>(module))
			{
				return t;
			}
		}
		return nullptr;
	}

	void Play();

	void Stop() { m_isRunning = false; }

	void Pause() { m_isPaused = true; }

	void Resume() { m_isPaused = false; }

	virtual void Update(float delta);

	virtual void Render(RenderScene& scene, Camera& camera);

	void SetPosition(const Mathf::Vector3& position);

	void SetMaxParticles(int max) { m_particleData.resize(max); m_instanceData.resize(max); }
protected:
	// 렌더 초기화 메소드는 rendermodule에서 정의.

	// data members
	bool m_isRunning;
	bool m_isPaused;
	std::vector<ParticleData> m_particleData;
	LinkedList<ParticleModule> m_moduleList;
	int m_activeParticleCount = 0;
	int m_maxParticles;
	std::vector<BillBoardInstanceData> m_instanceData;
	Mathf::Vector3 m_position;
	std::vector<RenderModules*> m_renderModules;
};

