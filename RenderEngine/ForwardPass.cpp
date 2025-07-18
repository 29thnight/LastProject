#include "ForwardPass.h"
#include "ShaderSystem.h"
#include "Material.h"
#include "Skeleton.h"
#include "Scene.h"
#include "RenderableComponents.h"
#include "Mesh.h"
#include "LightController.h"
#include "LightProperty.h"
#include "Benchmark.hpp"
#include "MeshRendererProxy.h"

struct alignas(16) ForwardBuffer
{
	bool32 m_useEnvironmentMap{};
	float m_envMapIntensity{ 1.f };
};

ForwardPass::ForwardPass()
{
	m_pso = std::make_unique<PipelineStateObject>();
	m_pso->m_vertexShader = &ShaderSystem->VertexShaders["VertexShader"];
	m_pso->m_pixelShader = &ShaderSystem->PixelShaders["Forward"];
	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	InputLayOutContainer vertexLayoutDesc = {
		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	m_pso->CreateInputLayout(std::move(vertexLayoutDesc));

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

	m_Buffer = DirectX11::CreateBuffer(sizeof(ForwardBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);
	m_materialBuffer = DirectX11::CreateBuffer(sizeof(MaterialInfomation), D3D11_BIND_CONSTANT_BUFFER, nullptr);
	m_boneBuffer = DirectX11::CreateBuffer(sizeof(Mathf::xMatrix) * Skeleton::MAX_BONES, D3D11_BIND_CONSTANT_BUFFER, nullptr);
}

ForwardPass::~ForwardPass()
{
}

void ForwardPass::UseEnvironmentMap(Texture* envMap, Texture* preFilter, Texture* brdfLut)
{
	m_EnvironmentMap = envMap;
	m_PreFilter = preFilter;
	m_BrdfLut = brdfLut;
}

void ForwardPass::Execute(RenderScene& scene, Camera& camera)
{
	auto cmdQueuePtr = GetCommandQueue(camera.m_cameraIndex);

	if (nullptr != cmdQueuePtr)
	{
		while (!cmdQueuePtr->empty())
		{
			ID3D11CommandList* CommandJob;
			if (cmdQueuePtr->try_pop(CommandJob))
			{
				DirectX11::ExecuteCommandList(CommandJob, true);
				Memory::SafeDelete(CommandJob);
			}
		}
	}
}

void ForwardPass::CreateRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera)
{
	if (!RenderPassData::VaildCheck(&camera)) return;
	auto renderData = RenderPassData::GetData(&camera);

	ID3D11DeviceContext* deferredPtr = deferredContext;

	m_pso->Apply(deferredPtr);

	ForwardBuffer buffer{};
	buffer.m_envMapIntensity = m_envMapIntensity;
	buffer.m_useEnvironmentMap = m_UseEnvironmentMap;
	DirectX11::UpdateBuffer(deferredPtr, m_Buffer.Get(), &buffer);

	ID3D11RenderTargetView* view = renderData->m_renderTarget->GetRTV();
	DirectX11::OMSetRenderTargets(deferredPtr, 1, &view, renderData->m_depthStencil->m_pDSV);
	DirectX11::RSSetViewports(deferredPtr, 1, &DeviceState::g_Viewport);
	scene.UseModel(deferredPtr);
	DirectX11::OMSetDepthStencilState(deferredPtr, DeviceState::g_pDepthStencilState, 1);
	DirectX11::OMSetBlendState(deferredPtr, DeviceState::g_pBlendState, nullptr, 0xFFFFFFFF);
	DirectX11::PSSetConstantBuffer(deferredPtr, 1, 1, &scene.m_LightController->m_pLightBuffer);
	DirectX11::VSSetConstantBuffer(deferredPtr, 3, 1, m_boneBuffer.GetAddressOf());
	DirectX11::PSSetConstantBuffer(deferredPtr, 0, 1, m_materialBuffer.GetAddressOf());
	DirectX11::PSSetConstantBuffer(deferredPtr, 3, 1, m_Buffer.GetAddressOf());

	ID3D11ShaderResourceView* envSRVs[3] = {
		m_UseEnvironmentMap ? m_EnvironmentMap->m_pSRV : nullptr,
		m_UseEnvironmentMap ? m_PreFilter->m_pSRV : nullptr,
		m_UseEnvironmentMap ? m_BrdfLut->m_pSRV : nullptr
	};
	DirectX11::PSSetShaderResources(deferredPtr, 6, 3, envSRVs);

	auto& lightManager = scene.m_LightController;
	if (lightManager->hasLightWithShadows) {
		DirectX11::PSSetShaderResources(deferredPtr, 4, 1, &renderData->m_shadowMapTexture->m_pSRV);
		DirectX11::PSSetConstantBuffer(deferredPtr, 2, 1, &lightManager->m_shadowMapBuffer);
		lightManager->PSBindCloudShadowMap(deferredPtr);
	}

	camera.UpdateBuffer(deferredPtr);
	HashedGuid currentAnimatorGuid{};
	//TODO : Change deferredContext Render
	for (auto& PrimitiveRenderProxy : renderData->m_forwardQueue)
	{
		scene.UpdateModel(PrimitiveRenderProxy->m_worldMatrix, deferredPtr);

		HashedGuid animatorGuid = PrimitiveRenderProxy->m_animatorGuid;
		if (PrimitiveRenderProxy->m_isAnimationEnabled && HashedGuid::INVAILD_ID != animatorGuid)
		{
			if (animatorGuid != currentAnimatorGuid)
			{
				DirectX11::UpdateBuffer(deferredPtr, m_boneBuffer.Get(), PrimitiveRenderProxy->m_finalTransforms);
				currentAnimatorGuid = PrimitiveRenderProxy->m_animatorGuid;
			}
		}

		Material* mat = PrimitiveRenderProxy->m_Material;
		DirectX11::UpdateBuffer(deferredPtr, m_materialBuffer.Get(), &mat->m_materialInfo);

		if (mat->m_pBaseColor)
		{
			DirectX11::PSSetShaderResources(deferredPtr, 0, 1, &mat->m_pBaseColor->m_pSRV);
		}
		if (mat->m_pNormal)
		{
			DirectX11::PSSetShaderResources(deferredPtr, 1, 1, &mat->m_pNormal->m_pSRV);
		}
		if (mat->m_pOccRoughMetal)
		{
			DirectX11::PSSetShaderResources(deferredPtr, 2, 1, &mat->m_pOccRoughMetal->m_pSRV);
		}
		if (mat->m_AOMap)
		{
			DirectX11::PSSetShaderResources(deferredPtr, 3, 1, &mat->m_AOMap->m_pSRV);
		}
		if (mat->m_pEmissive)
		{
			DirectX11::PSSetShaderResources(deferredPtr, 5, 1, &mat->m_pEmissive->m_pSRV);
		}

		PrimitiveRenderProxy->Draw(deferredPtr);
	}

	ID3D11DepthStencilState* nullDepthStencilState = nullptr;
	ID3D11BlendState* nullBlendState = nullptr;

	DirectX11::OMSetDepthStencilState(deferredPtr, nullDepthStencilState, 1);
	DirectX11::OMSetBlendState(deferredPtr, nullBlendState, nullptr, 0xFFFFFFFF);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	DirectX11::PSSetShaderResources(deferredPtr, 0, 1, &nullSRV);
	DirectX11::UnbindRenderTargets(deferredPtr);

	ID3D11CommandList* commandList{};
	deferredPtr->FinishCommandList(false, &commandList);
	PushQueue(camera.m_cameraIndex, commandList);
}

void ForwardPass::ControlPanel()
{
}
