#pragma once
#include "Core.Minimal.h"
#include "random"
#include "Effects.h"
#include "Texture.h"

struct ParticleData		// 아마 공통 cb가 될듯 특정 부분이 필요한 cb는 그 이펙트에서 따로 설정? 아니면 최대한 공통모수를 가지게끔 설정?
{
	Mathf::Vector3 position;
	Mathf::Vector3 velocity;
	Mathf::Vector3 acceleration;
	Mathf::Vector2 size;
	Mathf::Vector4 color;
	float lifeTime;
	float age;
	float rotation;
	float rotatespeed;
	bool isActive;
};

enum class EmitterType
{
	point,
	sphere,
	box,
	cone,
	circle,
};

class ParticleModule
{
public:
	virtual ~ParticleModule() = default;
	virtual void Initialize() {}
	virtual void Update(float delta, std::vector<ParticleData>& particles) = 0;
};

class SpawnModule : public ParticleModule
{
public:
	SpawnModule(int maxParticles, float spawnRate, EmitterType emitterType)
	: m_maxParticles(maxParticles), m_spawnRate(spawnRate), m_emitterType(emitterType),
		m_Time(0.0f), m_random(std::random_device{}()) {}
	 
	void Initialize() override;
	void Update(float delta, std::vector<ParticleData>& particles) override;

	void SetEmitterShape(EmitterType type) { m_emitterType = type; }
	void SetSpawnRate(float rate) { m_spawnRate = rate; }
	void SetMaxParticles(int count) { m_maxParticles = count; }
private:
	void SpawnParticle(std::vector<ParticleData>& particles);
	void InitializeParticle(ParticleData& particle);

private:
	int m_maxParticles;
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

class LifeModule : public ParticleModule
{
public:
	void Update(float delta, std::vector<ParticleData>& particles) override;
private:
};

class ColorModule : public ParticleModule
{
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

	void SetColorGradient(const std::vector<std::pair<float, Mathf::Vector4>>& gradient) { m_colorGradient = gradient; }

private:
	// 색 변화의 중간 보간 함수
	Mathf::Vector4 EvaluateGradient(float t);

	std::vector<std::pair<float, Mathf::Vector4>> m_colorGradient;
};

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

	void SetStartSize(float size) { m_startSize = size; }
	void SetEndSize(float size) { m_endSize = size; }
	void SetSizeOverLifeFunction(std::function<Mathf::Vector2(float)> func) { m_sizeOverLife = func; }

private:
	float m_startSize;
	float m_endSize;
	std::function<Mathf::Vector2(float)> m_sizeOverLife;
};

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

class NiagaraEffectManager : public Effects
{
public:
	NiagaraEffectManager(int maxParticles = 1000);
	~NiagaraEffectManager();

	template<typename T, typename... Args>
	T* AddModule(Args&&... args)
	{
		static_assert(std::is_base_of<ParticleModule, T>::value, "T must derive from ParticleModule");

		T* module = new T(std::forward<Args>(args)...);
		module->Initialize();
		m_modules.push_back(module);
		return module;
	}

	template<typename T>
	T* GetModule()
	{
		static_assert(std::is_base_of<ParticleModule, T>::value, "T must derive from ParticleModule");

		for (auto* module : m_modules)
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

	void Pause() { m_isRunning = false; }

	void Resume() { m_isRunning = true; }

	virtual void Update(float delta) override;

	virtual void Render(Scene& scene, Camera& camera) override;

	void SetParticleTexture(std::shared_ptr<Texture> texture) { m_particleTexture = texture; }

	void SetPixelShader(PixelShader* shader) { m_pso->m_pixelShader = shader; }

	void SetComputeShader(ComputeShader* shader) { m_pso->m_computeShader = shader; }

protected:
	virtual void UpdateInstanceData();

	virtual void PrepareBillboards(); // New method to prepare billboards for rendering
	virtual void SetupRenderState(Camera& camera); // New method to setup render state
	virtual void RenderBillboards(Camera& camera); // New method to render the billboards
	virtual void CleanupRenderState(); // New method to cleanup after rendering

	void SaveRenderState();
	void RestoreRenderState();

	// data members
	bool m_isRunning;
	std::vector<ParticleData> m_particleData;
	std::vector<ParticleModule*> m_modules;
	int m_activeParticleCount = 0;
	int m_maxParticles;
	std::vector<BillBoardInstanceData> m_instanceData;

	// resource
	std::shared_ptr<Texture> m_particleTexture;

	// state methods
	ID3D11DepthStencilState* m_prevDepthState = nullptr;
	UINT m_prevStencilRef = 0;
	ID3D11BlendState* m_prevBlendState = nullptr;
	float m_prevBlendFactor[4] = { 0 };
	UINT m_prevSampleMask = 0;
	ID3D11RasterizerState* m_prevRasterizerState = nullptr;

};

