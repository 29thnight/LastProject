#pragma once
#include "ParticleSystem.h"`
#include "EffectComponent.generated.h"

class EffectComponent : public ParticleSystem
{
   ReflectEffectComponent
	[[Serializable]]
	EffectComponent();
	~EffectComponent();

	[[Property]]
	std::string m_name{};

	[[Method]]
	void Update();
};

