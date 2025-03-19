#pragma once
#include "Texture.h"
#include "SceneObject.h"
#include "IEffect.h"

struct alignas(16) ExplodeParameters : public EffectParameters
{
	Mathf::Vector2 size;
	Mathf::Vector2 range;
};

struct BillboardVertex {
	DirectX::XMFLOAT4 Position;  // 중심 위치
	DirectX::XMFLOAT2 Size;      // 빌보드 크기
	DirectX::XMFLOAT4 Color;     // 색상
};

struct alignas(16) ModelConstantBuffer 
{
	Mathf::Matrix world;
	Mathf::Matrix view;
	Mathf::Matrix projection;
};


class IEffect;
class FirePass : public IEffect
{
public:
	FirePass();

	void LoadTexture(const std::string_view& basePath, const std::string_view& noisePath);

	void Execute(Scene& scene) override;

	void Update(float delta);

	void SetRenderTarget(Texture* renderTargetView);

	void PushFireObject(SceneObject* object);

	void Initialize();

	void CreateBillboardVertexBuffer();
private:
	ComPtr<ID3D11Buffer> m_constantBuffer;
	ComPtr<ID3D11Buffer> m_billboardVertexBuffer;

	ComPtr<ID3D11Buffer> m_ModelBuffer;

	std::shared_ptr<Texture> m_baseFireTexture;	// 기본 불 텍스처
	std::shared_ptr<Texture> m_noiseTexture;	// 노이즈 텍스처
	std::shared_ptr<Texture> m_fireAlphaTexture;// 알파 텍스처
	std::shared_ptr<Texture> m_resultTexture;	// 결과 텍스처

	// unordered access view -> 셰이더 프로그램 안에서 자원을 읽음과 동시에 쓰기도 가능 출력이 정해지지 않아서 셰이더 프로그램안에서 임의의 위치에서 scatter연산이 가능함
	//std::shared_ptr<Texture> m_texture;			// 기본 불 텍스처
	ExplodeParameters* mmParam;

	Texture* m_renderTarget = nullptr;
	float m_delta;
	std::vector<SceneObject*> EffectedObject;
	ModelConstantBuffer modelConst{};
};

