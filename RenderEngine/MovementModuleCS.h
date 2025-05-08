#pragma once
#include "ParticleModule.h"

class MovementModuleCS : public ParticleModule
{
public:
	MovementModuleCS() {}
	
	~MovementModuleCS()
	{
		Release();
	}

	void Initialize() override;
	void Update(float delta, std::vector<ParticleData>& particles) override;

	void SetUseGravity(bool use) { m_useGravity = use; }
	void SetGravityStrength(float strength) { m_gravityStrength = strength; }

private:

	struct MovementParams
	{
		float deltaTime;
		int useGravity;
		float gravityStrength;
		int useEasing;
		int easingType;
		int animationType;
		float easingDuration;
		float padding[1];
	};

	bool InitializeCompute();
	void UpdateConstantBuffers(float delta);
	void Release();


	ID3D11ComputeShader* m_computeShader;
	ID3D11Buffer* m_movementParamsBuffer;

	bool m_useGravity;
	float m_gravityStrength;
	bool m_isInitialized;

};

