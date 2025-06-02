#pragma once
#include "Core.Minimal.h"
#include "ParticleModule.h"
#include "DeviceState.h"

struct InitParams {
	UINT maxParticles;
	float pad[3];
};


class InitializeModuleCS : public ParticleModule
{
private:
	ID3D11ComputeShader* m_computeShader = nullptr;
	ID3D11Buffer* m_initParamsBuffer = nullptr;

	UINT m_particlesCapacity = 0;

public:
	InitializeModuleCS();

	~InitializeModuleCS();

	void Initialize(
		ID3D11UnorderedAccessView* particleUAV,
		ID3D11UnorderedAccessView* inactiveIndicesUAV,
		ID3D11UnorderedAccessView* inactiveCountUAV,
		ID3D11UnorderedAccessView* activeCountUAV,
		UINT maxParticles);

	void CreateBuffers();

	void UpdateConstantBuffer(UINT maxParticles);

};

	