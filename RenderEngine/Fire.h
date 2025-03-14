#pragma once
#include "IRenderPass.h"
#include "Texture.h"
#include "SceneObject.h"

struct FireParameters
{
	float time;
	float intensity;
	float speed;
	float colorShift;
	float noiseScale;
	float verticalFactor;
	float flamePower;
	float detailScale;
};

class FirePass final : public IRenderPass
{
public:
	FirePass();

	void SetParameters(const FireParameters& param) { mParam = param; }

	void LoadTexture(const std::string_view& basePath, const std::string_view& noisePath);

	void Execute(Scene& scene) override;

	void Update(float delta);

	void SetRenderTarget(Texture* renderTargetView);

	void PushFireObject(SceneObject* object);
private:

	void Initialize();

	ComPtr<ID3D11ComputeShader> m_computeShader;
	ComPtr<ID3D11Buffer> m_fireParamsBuffer;
	ComPtr<ID3D11Buffer> m_constantBuffer;

	std::shared_ptr<Texture> m_baseFireTexture;	// 기본 불 텍스처
	std::shared_ptr<Texture> m_noiseTexture;	// 노이즈 텍스처
	std::shared_ptr<Texture> m_resultTexture;	// 결과 텍스처

	// unordered access view -> 셰이더 프로그램 안에서 자원을 읽음과 동시에 쓰기도 가능 출력이 정해지지 않아서 셰이더 프로그램안에서 임의의 위치에서 scatter연산이 가능함
	//std::shared_ptr<Texture> m_texture;			// 기본 불 텍스처
	FireParameters mParam;
	Texture* m_renderTarget = nullptr;
	float m_delta;
	std::vector<SceneObject*> m_fireObjects;
};

