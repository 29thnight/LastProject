#pragma once
#include "IRenderPass.h"

struct alignas(16) EffectParameters
{
	float time;
	float intensity;
	float speed;
	float pad;

	Mathf::Vector4 color;
};

struct alignas(16) BillboardVertex {
	Mathf::Vector4 Position;
	Mathf::Vector2 Size;
};

struct alignas(16) ModelConstantBuffer
{
	Mathf::Matrix world;
	Mathf::Matrix view;
	Mathf::Matrix projection;
};


class Effects : public IRenderPass
{
public:

	virtual ~Effects() {}

	void CreateBillboardVertex(BillboardVertex* vertex);
	
	void SetParameters(EffectParameters* param);

	// 유의할점 shader가 설정되고 맨 마지막에 Geometry shader의 constant buffer가 설정되고 난뒤 그릴것
	// 대부분 맨 마지막에 그리는것이 제일 좋음
	void DrawBillboard(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection);

	virtual void Execute(Scene& scene) abstract;

	EffectParameters* mParam;

	BillboardVertex* mVertex;

	ID3D11Buffer* billboardVertexBuffer;

	ComPtr<ID3D11Buffer> m_ModelBuffer;			// world view proj전용
	ModelConstantBuffer m_ModelConstantBuffer{};
};




