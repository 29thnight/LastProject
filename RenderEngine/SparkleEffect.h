#pragma once
#include "NiagaraEffectManager.h"
#include "AssetSystem.h"
#include "Effects.h"

struct SparkleParameters : public EffectParameters
{
    Mathf::Vector2 size;
    Mathf::Vector2 range;
};

class SparkleEffect : public NiagaraEffectManager
{
public:
    SparkleEffect(const Mathf::Vector3& position, int maxParticles = 100);

    ~SparkleEffect();

    void InitializeModules();

    void Update(float delta) override;

    void Render(Scene& scene, Camera& camera) override;

    void SetPosition(const Mathf::Vector3& position);

    void SpawnSparklesBurst(int count);

    void UpdateSparkleConstantBuffer();

    void UpdateInstanceData() override;

    void SetupRenderState(Camera& camera) override;

    void SetParameters(SparkleParameters* params);

private:
    float m_delta;
    Mathf::Vector3 m_position;
    SparkleParameters* m_sparkleParams;
    std::shared_ptr<Texture> m_sparkleTexture;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    SpawnModule* m_spawnModule;
};