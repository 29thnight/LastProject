#pragma once
#include "../Texture.h"
#include "RenderModules.h"
#include "../IRenderPass.h"
#include "SpawnModuleCS.h"


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
