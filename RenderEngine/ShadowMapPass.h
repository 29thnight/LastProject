#pragma once
#include "../Utility_Framework/Core.Minimal.h"
#include "IRenderPass.h"
#include "Camera.h"

class Texture;
class Scene;
class LightController;

//struct alignas(16) ShadowMapConstant
//{
//	float m_shadowMapWidth{};
//	float m_shadowMapHeight{};
//	Mathf::xMatrix m_lightViewProjection{};
//	
//	//3°³Â¥¸® cascade shadow 
//	//Mathf::xMatrix m_lightViewProjection[3]{};
//};
//
//struct ShadowMapRenderDesc
//{
//	Mathf::xVector m_eyePosition{};
//	Mathf::xVector m_lookAt{};
//	float m_nearPlane{ 0.1f };
//	float m_farPlane{ 200.f };
//	float m_viewWidth{ 1.f };
//	float m_viewHeight{ 1.f };
//	float m_textureWidth{ 8192.f };
//	float m_textureHeight{ 8192.f };
//
//	//8192  4096 1024.f
//};


class ShadowMapPass final : public IRenderPass
{
public:
	ShadowMapPass();
	~ShadowMapPass() {};

	void Initialize(uint32 width, uint32 height);
	void Execute(RenderScene& scene, Camera& camera) override;
	void ControlPanel() override;
	Camera m_shadowCamera{};

	std::unique_ptr<Texture> m_shadowMapTexture{};
	ID3D11DepthStencilView* m_shadowMapDSV{ nullptr };



	ComPtr<ID3D11Buffer> m_shadowBuffer;
	ComPtr<ID3D11Buffer> m_shadowgsBuffer;
	D3D11_VIEWPORT shadowViewport;
	//ID3D11ShaderResourceView* m_shadowMapSRV[3]{};
	//ID3D11DepthStencilView* m_shadowMapDSV[3]{};
};