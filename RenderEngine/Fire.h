#pragma once
#include "Texture.h"
#include "SceneObject.h"
#include "IEffect.h"

struct alignas(16) FireParameters : public EffectParameters
{
	float colorShift;
	float noiseScale;
	float verticalFactor;
	float flamePower;

	Mathf::Vector4 firePosition1;
	Mathf::Vector4 firePosition2;
	Mathf::Vector4 firePosition3;
	Mathf::Vector4 firePosition4;

	float detailScale;
	float patternChangeSpeed;
	int numFireEffects;
	int pad;

	float timeOffset1;
	float timeOffset2;
	float timeOffset3;
	float timeOffset4;

	Mathf::Vector4 Color;
};


struct alignas(16) ExplodeParameters : public EffectParameters
{
	Mathf::Vector2 size;
	Mathf::Vector2 range;
};

class IEffect;
class FirePass : public IEffect
{
public:
	FirePass();

	//void SetParameters(const FireParameters& param) { mParam = param; }

	void LoadTexture(const std::string_view& basePath, const std::string_view& noisePath);

	void Execute(Scene& scene) override;

	void Update(float delta);

	void SetRenderTarget(Texture* renderTargetView);

	void PushFireObject(SceneObject* object);

	void Initialize();
private:

	ComPtr<ID3D11ComputeShader> m_computeShader;
	ComPtr<ID3D11Buffer> m_fireParamsBuffer;
	ComPtr<ID3D11Buffer> m_constantBuffer;

	std::shared_ptr<Texture> m_baseFireTexture;	// 기본 불 텍스처
	std::shared_ptr<Texture> m_noiseTexture;	// 노이즈 텍스처
	std::shared_ptr<Texture> m_fireAlphaTexture;// 알파 텍스처
	std::shared_ptr<Texture> m_resultTexture;	// 결과 텍스처

	// unordered access view -> 셰이더 프로그램 안에서 자원을 읽음과 동시에 쓰기도 가능 출력이 정해지지 않아서 셰이더 프로그램안에서 임의의 위치에서 scatter연산이 가능함
	//std::shared_ptr<Texture> m_texture;			// 기본 불 텍스처
	FireParameters* mParam;

	ExplodeParameters* mmParam;

	Texture* m_renderTarget = nullptr;
	float m_delta;
	std::vector<SceneObject*> m_fireObjects;
};

