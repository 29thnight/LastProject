#pragma once
#include "IRenderPass.h"

struct EffectParameters
{
	float time;
	float intensity;
	float speed;
	float pad;
};

class IEffect : public IRenderPass
{
public:

	virtual ~IEffect() {}

	void SetParameters(EffectParameters* param);

	virtual void Execute(Scene& scene) abstract;

	EffectParameters* mParam;

};




