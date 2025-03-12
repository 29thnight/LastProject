#pragma once
#include "IRenderPass.h"
#include "Texture.h"

struct SnowParameters
{
	Mathf::Vector3 windDirection = Mathf::Vector3(0.1f, 0.0f, 0.0f);		// 바람 방향
	float windStrength = 0.5f;												// 바람 세기
	float snowFallSpeed = 1.0f;												// 눈 내리는 속도
	float snowAmount = 0.5f;												// 눈 입자량
	float snowSize = 0.05f;													// 눈 입자 크기
	Mathf::Vector3	snowColor = Mathf::Vector3(1.0f, 1.0f, 1.0f);			// 눈 색깔
	float snowOpacity = 0.7f;												// 눈 불투명도
};

struct SnowCB
{
	Mathf::Matrix View;
	Mathf::Matrix Projection;
	Mathf::Vector4 CameraPostion;
	float Time;
	Mathf::Vector3 pad;
};

struct SnowParamBuffer
{
	Mathf::Vector3 WindDirection;
	float WindStrength;
	float SnowFallSpeed;
	float SnowAmount;
	float SnowSize;
	float time;
	Mathf::Vector3 SnowColor;
	float SnowOpacity;
};

struct EffectVertex
{
	Mathf::Vector3 Position;
};

class SnowPass final : public IRenderPass
{
public:
	SnowPass();

	void Initialize(Texture* renderTarget);
	void LoadTexture(const std::string_view& filePath);
	void Update(float delta);
	void Execute(Scene& scene) override;
	void SetParameters(const SnowParameters& param) { mParams = param; }
private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;  // 단일 점 버퍼
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer; // 카메라, 변환 행렬 등
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_snowParamsBuffer; // 눈 파라미터
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	std::shared_ptr<Texture> m_texture;
	SnowParameters mParams;
	Texture* m_renderTarget;
	float m_delta;
};

