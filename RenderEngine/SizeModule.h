#pragma once
#include "ParticleModule.h"

// startsize, endsize, sizefunction
class SizeModule : public ParticleModule
{
public:
	SizeModule() : m_startSize(0.1f), m_endSize(1.0f), m_sizeOverLife([this](float t) {return Mathf::Vector2::Lerp(
		Mathf::Vector2(m_startSize, m_startSize),
		Mathf::Vector2(m_endSize, m_endSize),
		t
	);
		}) {
	}

	void Update(float delta, std::vector<ParticleData>& particles) override;

	// m_sizeoverlife function을 설정 안했을때의 기본으로 쓰이는 크기
	void SetStartSize(float size) { m_startSize = size; }
	void SetEndSize(float size) { m_endSize = size; }

	void SetSizeOverLifeFunction(std::function<Mathf::Vector2(float)> func) { m_sizeOverLife = func; }

private:
	float m_startSize;
	float m_endSize;
	std::function<Mathf::Vector2(float)> m_sizeOverLife;
};
