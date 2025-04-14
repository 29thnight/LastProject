#pragma once
#include "../Utility_Framework/Core.Minimal.h"
#include "Component.h"
#include "IRenderable.h"
#include "IUpdatable.h"

class EffectComponent : public Component, public IRenderable, public IUpdatable<EffectComponent>, public Meta::IReflectable<EffectComponent>
{
public:
	EffectComponent();
	~EffectComponent() = default;

	void Load(std::string_view filepath);

	std::string ToString() const override
	{

	}

};

