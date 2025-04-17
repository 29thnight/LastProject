#pragma once
#include "EffectModules.h"
#include "ShaderSystem.h"
#include "Effects.h"

class SparkleEffect : public EffectModules
{
public:
    SparkleEffect(const Mathf::Vector3& position, int maxParticles = 100);

    ~SparkleEffect();

    void InitializeModules();

    void Update(float delta) override;

    void Render(RenderScene& scene, Camera& camera) override;

    void SpawnSparklesBurst(int count);

    void UpdateInstanceData();
private:
    BillboardModule* m_billboardModule;

    float m_delta;
    Texture* m_sparkleTexture;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    SpawnModule* m_spawnModule;
};