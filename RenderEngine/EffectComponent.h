#pragma once
#include "ParticleSystem.h"`
#include "EffectComponent.generated.h"

class EffectComponent : public Component, public IAwakable, public IUpdatable, public IOnDistroy
{
public:
   ReflectEffectComponent
	[[Serializable(Inheritance:Component)]]
	GENERATED_BODY(EffectComponent)


	void Awake() override;
	void Update(float tick) override;
	void OnDistroy() override;


    [[Property]]
	int num{};

	// string int bool float만 매개변수로 접근 가능
	void Update();

	// IMGUI로 버튼 만들기
	ParticleSystem* AddEmiiter(ParticleDataType type, UINT maxParticles);

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

	void MakeEditor();
private:
	std::vector<std::shared_ptr<ParticleSystem>> m_emitters;
};


