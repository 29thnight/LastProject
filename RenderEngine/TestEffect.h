#pragma once
#include "EffectModules.h"
#include "ShaderSystem.h"
#include "Effects.h"

class TestEffect : public EffectModules
{
public:
	TestEffect(const Mathf::Vector3& position, int maxParticles = 100);

	void Update(float delta) override;

	void Render(RenderScene& scene, Camera& camera) override;

	void UpdateInstanceBuffer(const std::vector<ParticleData>& particles);

private:
	BillboardModule* m_billboardModule;
	SpawnModuleCS* m_spawnModule;
	float m_delta;
};

