#pragma once
#include "ParticleModule.h"

class LifeModuleCS : public ParticleModule
{
public:
    LifeModuleCS()
        : m_computeShader(nullptr), m_lifeParamsBuffer(nullptr),
        m_isInitialized(false), m_paramsDirty(true),
        m_inactiveIndicesUAV(nullptr), m_inactiveCountUAV(nullptr)
    {
        SetStage(ModuleStage::LIFE); 
    }

    ~LifeModuleCS()
    {
        Release();
    }

    void Initialize() override;
    void Update(float delta, std::vector<ParticleData>& particles) override;
    void OnSystemResized(UINT max) override;

    bool InitializeCompute();

    void SetInactiveBuffers(ID3D11UnorderedAccessView* inactiveIndicesUAV, ID3D11UnorderedAccessView* inactiveCountUAV)
    {
        m_inactiveIndicesUAV = inactiveIndicesUAV;
        m_inactiveCountUAV = inactiveCountUAV;
    }

    void SetActiveCounterBuffer(ID3D11UnorderedAccessView* activeCounterUAV)
    {
        m_activeCounterUAV = activeCounterUAV;
    }

    void SetInputOutput(ID3D11ShaderResourceView* inputSRV, ID3D11UnorderedAccessView* outputUAV)
    {
        m_inputSRV = inputSRV;
        m_outputUAV = outputUAV;
    }

private:
    void UpdateConstantBuffers(float delta);
    void Release();

    struct alignas(16) LifeParams
    {
        float deltaTime;
        uint32_t maxParticles;
        uint32_t resetInactiveCounter;
        float pad;
    };

private:
    ID3D11ComputeShader* m_computeShader;
    ID3D11Buffer* m_lifeParamsBuffer;

    ID3D11UnorderedAccessView* m_inactiveIndicesUAV;
    ID3D11UnorderedAccessView* m_inactiveCountUAV;
    ID3D11UnorderedAccessView* m_activeCounterUAV;

    bool m_isInitialized;
    bool m_paramsDirty;
    UINT m_particlesCapacity = 0;
};

