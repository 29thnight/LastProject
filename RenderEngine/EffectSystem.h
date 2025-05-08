#pragma once
#include "Texture.h"
#include "RenderModules.h"
#include "IRenderPass.h"
#include "SpawnModuleCS.h"
#include "MovementModuleCS.h"

class EffectSystem
{
public:
	EffectSystem(int maxParticles = 1000);
	~EffectSystem();

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

	bool InitializeBuffers();

	void SwapBuffers() { m_usingBufferA = !m_usingBufferA; }

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

	// double buffer members
	ID3D11Buffer* m_particlesBufferA = nullptr;
	ID3D11Buffer* m_particlesBufferB = nullptr;
	ID3D11UnorderedAccessView* m_particlesUAV_A = nullptr;
	ID3D11UnorderedAccessView* m_particlesUAV_B = nullptr;
	ID3D11ShaderResourceView* m_particlesSRV_A = nullptr;
	ID3D11ShaderResourceView* m_particlesSRV_B = nullptr;
	bool m_usingBufferA = true;

};
