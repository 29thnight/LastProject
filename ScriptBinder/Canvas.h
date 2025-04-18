#pragma once
#include "../Utility_Framework/Core.Minimal.h"
#include "Component.h"
#include "IRenderable.h"
#include "IUpdatable.h"

class Canvas : public Component, public IRenderable, public IUpdatable, public Meta::IReflectable<Canvas>
{
public:
	Canvas();
	~Canvas() = default;

	bool IsEnabled() const override
	{
		return m_IsEnabled;
	}

	void SetEnabled(bool able) override
	{
		m_IsEnabled = able;
	}

	void AddUIObject(GameObject* obj);
	virtual void Update(float tick) override;


	ReflectionField(Canvas)
	{
		PropertyField
		({
			meta_property(m_IsEnabled)
			meta_property(CanvasOrder)
			});

		FieldEnd(Canvas, PropertyOnly)
	};
	bool m_IsEnabled = true;
	int PreCanvasOrder = 0;
	int CanvasOrder = 0;
	std::vector<GameObject*> UIObjs;

	//현재 선택중인 UI
	GameObject* SelectUI = nullptr;
};

