#pragma once
#include "IRenderPass.h"

struct EffectParameters
{
	float time;
	float intensity;
	float speed;
	float pad;
};

struct BillboardVertex {
	Mathf::Vector4 Position;
	Mathf::Vector2 Size;
	Mathf::Vector4 Color;
};

class IEffect : public IRenderPass
{
public:

	virtual ~IEffect() {}

	void CreateBillboardVertex(BillboardVertex* vertex);
	
	void SetParameters(EffectParameters* param);

	virtual void Execute(Scene& scene) abstract;

	EffectParameters* mParam;

	BillboardVertex* mVertex;

	ID3D11Buffer* billboardVertexBuffer;
};




