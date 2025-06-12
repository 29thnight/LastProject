#pragma once
#include "ParticleModule.h"

// colorgradient
class ColorModule : public ParticleModule
{
private:
	using ColorEvaluator = std::function<Mathf::Vector4(float)>;
	ColorEvaluator m_customEvaluator = nullptr;
public:
	ColorModule()
	{
		m_colorGradient = {
			{0.0f, float4(1.0f,1.0f,1.0f,1.0f)},
			{0.3f, float4(1.0f,1.0f,0.0f,0.9f)},
			{0.7f, float4(1.0f,0.0f,0.0f,0.7f)},
			{1.0f, float4(0.5f,0.0f,0.0f,0.0f)},
		};

	}

	void Update(float delta, std::vector<ParticleData>& particles) override;

	// 보간
	void SetColorGradient(const std::vector<std::pair<float, Mathf::Vector4>>& gradient) { m_colorGradient = gradient; }

	// 모드 설정 (보간, 이산, 사용자 정의 함수)
	void SetTransitionMode(ColorTransitionMode mode) { m_transitionMode = mode; }

	// 사용자 정의 함수
	void SetCustomEvaluator(const ColorEvaluator& evaluator) { m_customEvaluator = evaluator; }

	// 이산
	void SetDiscreteColors(const std::vector<Mathf::Vector4>& colors) { m_discreteColors = colors; }
private:
	// 통합 색상 평가 함수
	Mathf::Vector4 EvaluateColor(float t);

	// 보간용 평가 함수
	Mathf::Vector4 EvaluateGradient(float t);

	// 이산용 평가 함수
	Mathf::Vector4 EvaluateDiscrete(float t);

	std::vector<std::pair<float, Mathf::Vector4>> m_colorGradient;

	ColorTransitionMode m_transitionMode = ColorTransitionMode::Gradient;

	std::vector<Mathf::Vector4> m_discreteColors;
};