#pragma once
#include "IRenderPass.h"

// 해야할것 : 빌보드 종류, 인스턴싱 사용해서 빌보드 그리기

enum class BillBoardType
{
	Basic,
	YAxs,
	AxisAligned,
};

struct alignas(16) BillBoardInstanceData
{
	Mathf::Vector4 position;
	Mathf::Vector2 size;
	UINT id;
};

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
};

struct alignas(16) ModelConstantBuffer
{
	Mathf::Matrix world;
	Mathf::Matrix view;
	Mathf::Matrix projection;
};

enum class Effect
{
	Explode,
};

class Effects : public IRenderPass
{
public:
	Effects();

	virtual ~Effects() {}

	// 빌보드의 위치와 사이즈 설정
	void CreateBillboard(BillboardVertex* vertex, BillBoardType type = BillBoardType::Basic);

	void SetParameters(EffectParameters* param);

	// 유의할점 shader가 설정되고 맨 마지막에 Geometry shader의 constant buffer가 설정되고 난뒤 그릴것
	// 대부분 맨 마지막에 그리는것이 제일 좋음
	void DrawBillboard(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection);

	virtual void Execute(Scene& scene, Camera& camera);

	virtual void Render(Scene& scene, Camera& camera) {};

	void SetupBillBoardInstancing(BillBoardInstanceData* instance, UINT count);

	virtual void Update(float delta) {};

	void UpdateEffects(float delta);

	void MakeEffects(Effect type, std::string_view name);

	Effects* GetEffect(std::string_view name);

	bool RemoveEffect(std::string_view name);
protected:
	ComPtr<ID3D11Buffer> m_constantBuffer{};
private:

	void CreateBillBoardMatrix(const Mathf::Matrix& view, const Mathf::Matrix& world);

	EffectParameters* mParam = nullptr;

	BillboardVertex* mVertex = nullptr;

	ID3D11Buffer* billboardVertexBuffer = nullptr;

	UINT m_instanceCount;

	BillBoardType m_BillBoardType = BillBoardType::Basic;

	ComPtr<ID3D11Buffer> m_InstanceBuffer;
	ComPtr<ID3D11Buffer> m_ModelBuffer;			// world view proj전용
	ModelConstantBuffer m_ModelConstantBuffer{};

	std::unordered_map<std::string, std::unique_ptr<Effects>> effects;

};




