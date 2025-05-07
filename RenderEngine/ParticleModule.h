#pragma once
#include "Core.Minimal.h"
#include "random"
#include "LinkedListLib.hpp"
#include "EaseInOut.h"
#include "BaseEffectStruct.h"

// easing 위치 고민 현재는 그냥 모든 module에 넣을수 있는 구조

class ParticleModule : public LinkProperty<ParticleModule>
{
public:
	ParticleModule() : LinkProperty<ParticleModule>(this) {}
	virtual ~ParticleModule() = default;
	virtual void Initialize() {}
	virtual void Update(float delta, std::vector<ParticleData>& particles) = 0;

	void SetEasingType(EasingEffect type)
	{
		m_easingType = type;
		m_useEasing = true;
	}

	void SetAnimationType(StepAnimation type)
	{
		m_animationType = type;
		m_useEasing = true;
	}

	void SetEasingDuration(float duration)
	{
		m_easingDuration = duration;
	}

	void EnableEasing(bool enable)
	{
		m_useEasing = enable;
	}

	bool IsEasingEnabled() const
	{
		return m_useEasing;
	}

	float ApplyEasing(float normalizedTime);

	EaseInOut CreateEasingObject()
	{
		return EaseInOut(m_easingType, m_animationType, m_easingDuration);
	}

	ID3D11ShaderResourceView* GetParticlesSRV() { return m_particlesSRV; }
	ID3D11UnorderedAccessView* GetParticlesUAV() { return m_particlesUAV; }

protected:
	bool m_useEasing;
	EasingEffect m_easingType;
	StepAnimation m_animationType;
	float m_easingDuration;

	// 버퍼 참조
	ID3D11UnorderedAccessView* m_particlesUAV = nullptr;
	ID3D11ShaderResourceView* m_particlesSRV = nullptr;
};