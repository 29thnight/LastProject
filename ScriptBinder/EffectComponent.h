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
		return std::string("EffectComponent");
	}

	bool IsEnabled() const override
	{
		return m_IsEnabled;
	}

	void SetEnabled(bool able) override
	{
		m_IsEnabled = able;
	}

	virtual void Update(float tick) override;

	ReflectionField(EffectComponent, PropertyOnly)
	{
		PropertyField
		({
			meta_property(m_IsEnabled)
		});
		ReturnReflectionPropertyOnly(EffectComponent)
	};
private:
	bool m_IsEnabled = true;
};

