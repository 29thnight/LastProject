#pragma once
#include "IRenderPass.h"
#include "Texture.h"
#include "GameObject.h"

class Camera;
class GBufferPass final : public IRenderPass
{
public:
	GBufferPass();
	~GBufferPass();

	void SetRenderTargetViews(ID3D11RenderTargetView** renderTargetViews, uint32 size);
	void Execute(RenderScene& scene, Camera& camera) override;
	//NOTICE : after execute TerrainRenderCommandList this Function
	void CreateRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera) override;
	void TerrainRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera);
	virtual void Resize(uint32_t width, uint32_t height) override;

private:
	std::unique_ptr<PipelineStateObject> m_instancePSO;
	uint32 m_maxInstanceCount{};
	ComPtr<ID3D11Buffer> m_materialBuffer;
	ComPtr<ID3D11Buffer> m_boneBuffer;
	ComPtr<ID3D11Buffer> m_instanceBuffer;
	ComPtr<ID3D11ShaderResourceView> m_instanceBufferSRV;
	ID3D11RenderTargetView* m_renderTargetViews[RTV_TypeMax]{}; //0: diffuse, 1: metalRough, 2: normal, 3: emissive
};