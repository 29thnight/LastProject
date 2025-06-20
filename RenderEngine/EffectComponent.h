#pragma once
#include "ParticleSystem.h"`
#include "EffectComponent.generated.h"

enum class EffectModuleType
{
	SpawnModule,
	MeshSpawnModule,
	MovementModule,
	ColorModule,
	SizeModule,
};

enum class RenderType
{
	Billboard,
	Mesh,
};

class EffectComponent : public Component, public IAwakable, public IUpdatable, public IOnDistroy
{
public:
   ReflectEffectComponent
	[[Serializable(Inheritance:Component)]]
	GENERATED_BODY(EffectComponent)


	void Awake() override;
	void Update(float tick) override;
	void OnDistroy() override;

	void SetTexture(Texture* tex);


    [[Property]]
	int num{};

	// string int bool float만 매개변수로 접근 가능

	// IMGUI로 버튼 만들기

	[[Method]]
	void RemoveEmitter(int index);

	[[Method]]
	void PlayAll();

	[[Method]]
	void StopAll();

	[[Method]]
	void PlayEmitter(int index);

	[[Method]]
	void StopEmitter(int index);

private:
	std::vector<std::shared_ptr<ParticleSystem>> m_emitters;

	std::shared_ptr<ParticleSystem> m_tempParticleSystem = nullptr;
	bool m_isEditingEmitter = false;
	int m_selectedModuleIndex = 0;
	int m_selectedRenderIndex = 0;

	std::vector<Texture*> m_textures;

	struct ModuleInfo {
		const char* name;
		EffectModuleType type;
	};

	struct RenderInfo {
		const char* name;
		RenderType type;
	};

	std::vector<ModuleInfo> m_availableModules = {
	   {"Spawn Module", EffectModuleType::SpawnModule},
	   {"Mesh Spawn Module", EffectModuleType::MeshSpawnModule},
	   {"Movement Module", EffectModuleType::MovementModule},
	   {"Color Module", EffectModuleType::ColorModule},
	   {"Size Module", EffectModuleType::SizeModule},
	};

	std::vector<RenderInfo> m_availableRenders = {
		{"Billboard Render", RenderType::Billboard},
		{"Mesh Render", RenderType::Mesh},
	};

	void RenderMainEditor();
	void RenderEmitterEditor();
	void AddSelectedModule();
	void AddSelectedRender();
	void StartCreateEmitter();
	void SaveCurrentEmitter();
	void CancelEditing();
	void RenderExistingModules();
};


