#pragma once
#include "EffectModules.h"
#include "Texture.h"

struct alignas(16) AtlasParameters : public EffectParameters
{
	Mathf::Vector2 size;
	Mathf::Vector2 range;
};

class AtlasEffect : public EffectModules
{
public:
	AtlasEffect(const float3& position, int maxParticles = 100);

	~AtlasEffect();

	void InitializeModules();

	void Update(float delta) override;

	void Render(RenderScene& scene, Camera& camera) override;

	void UpdateConstantBuffer();

	void UpdateInstanceData();

	void SetParameters(AtlasParameters* param);

private:
	BillboardModule* m_billboardModule;
	AtlasParameters* m_atlasParams;
	std::shared_ptr<Texture> m_atlasTexture;
	std::shared_ptr<Texture> m_atlasAlphaTexture;
	ComPtr<ID3D11Buffer> m_constantBuffer;
	SpawnModule* m_spawnModule;
};

