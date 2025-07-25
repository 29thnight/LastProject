#include "GBufferPass.h"
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
#include "Terrain.h"

ID3D11ShaderResourceView* nullSRVs[5]{
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};

GBufferPass::GBufferPass()
{
	m_pso = std::make_unique<PipelineStateObject>();
	m_instancePSO = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &ShaderSystem->VertexShaders["VertexShader"];
	m_pso->m_pixelShader = &ShaderSystem->PixelShaders["GBuffer"];
	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_instancePSO->m_vertexShader = &ShaderSystem->VertexShaders["InstancedVertexShader"];
	m_instancePSO->m_pixelShader = &ShaderSystem->PixelShaders["GBuffer"];
	m_instancePSO->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

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

	InputLayOutContainer instanceVertexLayoutDesc = {
		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	m_instancePSO->CreateInputLayout(std::move(instanceVertexLayoutDesc));

	CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_instancePSO->m_rasterizerState
		)
	);

	m_pso->m_depthStencilState = DeviceState::g_pDepthStencilState;
	m_instancePSO->m_depthStencilState = DeviceState::g_pDepthStencilState;

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

	m_instancePSO->m_samplers.push_back(linearSampler);
	m_instancePSO->m_samplers.push_back(pointSampler);

	m_materialBuffer = DirectX11::CreateBuffer(sizeof(MaterialInfomation), D3D11_BIND_CONSTANT_BUFFER, nullptr);
	m_boneBuffer = DirectX11::CreateBuffer(sizeof(Mathf::xMatrix) * Skeleton::MAX_BONES, D3D11_BIND_CONSTANT_BUFFER, nullptr);

	for (uint32 i = 0; i < MAX_BONES; i++)
	{
		InitialMatrix[i] = XMMatrixIdentity();
	}

	// Create a dynamic structured buffer for instance data (world matrices)
   // This buffer is created once and updated each frame.
	constexpr uint32 MAX_INSTANCES = 2048; // Max number of instances per draw call
	m_maxInstanceCount = MAX_INSTANCES;

	D3D11_BUFFER_DESC instanceBufferDesc{};
	instanceBufferDesc.ByteWidth = sizeof(Mathf::xMatrix) * MAX_INSTANCES;
	instanceBufferDesc.Usage = D3D11_USAGE_DEFAULT; // Changed for UpdateSubresource
	instanceBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	instanceBufferDesc.CPUAccessFlags = 0; // No direct CPU access needed
	instanceBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	instanceBufferDesc.StructureByteStride = sizeof(Mathf::xMatrix);

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBuffer(&instanceBufferDesc, nullptr, &m_instanceBuffer)
	);

	// Create a shader resource view for the instance buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MAX_INSTANCES;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateShaderResourceView(m_instanceBuffer.Get(), &srvDesc, &m_instanceBufferSRV)
	);
}

GBufferPass::~GBufferPass()
{
}

void GBufferPass::SetRenderTargetViews(ID3D11RenderTargetView** renderTargetViews, uint32 size)
{
	for (uint32 i = 0; i < size; i++)
	{
		m_renderTargetViews[i] = renderTargetViews[i];
	}
}

void GBufferPass::Execute(RenderScene& scene, Camera& camera)
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

void GBufferPass::CreateRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera)
{
	if (!RenderPassData::VaildCheck(&camera)) return;
	auto data = RenderPassData::GetData(&camera);

	ID3D11DeviceContext* deferredPtr = deferredContext;

	// --- 1. CLASSIFY RENDER PROXIES ---
	// Grouping key is now a pair of (Material GUID, Mesh GUID) to ensure
	// that only objects with the exact same mesh and material are instanced together.
	using InstanceGroupKey = PrimitiveRenderProxy::ProxyFilter;
	std::vector<PrimitiveRenderProxy*> animatedProxies;
	std::map<InstanceGroupKey, std::vector<PrimitiveRenderProxy*>> instanceGroups;

	for (auto& proxy : data->m_deferredQueue)
	{
		if (proxy->m_isAnimationEnabled && HashedGuid::INVAILD_ID != proxy->m_animatorGuid)
		{
			animatedProxies.push_back(proxy);
		}
		else
		{
			// Assuming PrimitiveRenderProxy has a pointer to a Mesh object which contains the hashingMesh GUID.
			// Based on operator==, the mesh guid is proxy->m_mesh->m_hashingMesh.
			InstanceGroupKey key
			{ 
				proxy->m_materialGuid, 
				proxy->m_Mesh->m_hashingMesh,
				proxy->m_EnableLOD,
				proxy->GetLODLevel(&camera),
			};
			instanceGroups[key].push_back(std::move(proxy));
		}
	}

	// --- INITIAL PSO AND RENDER TARGET SETUP ---
	DirectX11::OMSetRenderTargets(deferredPtr, RTV_TypeMax, m_renderTargetViews, data->m_depthStencil->m_pDSV);
	camera.UpdateBuffer(deferredPtr);
	scene.UseModel(deferredPtr);
	DirectX11::RSSetViewports(deferredPtr, 1, &DeviceState::g_Viewport);
	DirectX11::PSSetConstantBuffer(deferredPtr, 1, 1, &scene.m_LightController->m_pLightBuffer);

	// --- 2. RENDER ANIMATED OBJECTS (INDIVIDUALLY) ---
	m_pso->Apply(deferredPtr);
	DirectX11::VSSetConstantBuffer(deferredPtr, 3, 1, m_boneBuffer.GetAddressOf());
	DirectX11::PSSetConstantBuffer(deferredPtr, 0, 1, m_materialBuffer.GetAddressOf());

	HashedGuid currentAnimatorGuid{};
	HashedGuid currentMaterialGuid{};

	for (auto& proxy : animatedProxies)
	{
		scene.UpdateModel(proxy->m_worldMatrix, deferredPtr);

		if (proxy->m_finalTransforms && proxy->m_animatorGuid != currentAnimatorGuid)
		{
			DirectX11::UpdateBuffer(deferredPtr, m_boneBuffer.Get(), proxy->m_finalTransforms);
			currentAnimatorGuid = proxy->m_animatorGuid;
		}

		if (proxy->m_materialGuid != currentMaterialGuid)
		{
			Material* mat = proxy->m_Material;
			auto matinfo = mat->m_materialInfo;
			matinfo.m_bitflag |= proxy->m_isShadowRecive ? MaterialInfomation::USE_SHADOW_RECIVE : 0;

			DirectX11::UpdateBuffer(deferredPtr, m_materialBuffer.Get(), &matinfo);
			if (mat->m_pBaseColor) DirectX11::PSSetShaderResources(deferredPtr, 0, 1, &mat->m_pBaseColor->m_pSRV);
			if (mat->m_pNormal) DirectX11::PSSetShaderResources(deferredPtr, 1, 1, &mat->m_pNormal->m_pSRV);
			if (mat->m_pOccRoughMetal) DirectX11::PSSetShaderResources(deferredPtr, 2, 1, &mat->m_pOccRoughMetal->m_pSRV);
			if (mat->m_AOMap) DirectX11::PSSetShaderResources(deferredPtr, 3, 1, &mat->m_AOMap->m_pSRV);
			if (mat->m_pEmissive) DirectX11::PSSetShaderResources(deferredPtr, 5, 1, &mat->m_pEmissive->m_pSRV);
			currentMaterialGuid = proxy->m_materialGuid;
		}

		proxy->Draw(deferredPtr);
	}

	// --- 3. RENDER STATIC OBJECTS (INSTANCED) ---
	m_instancePSO->Apply(deferredPtr);

	// Bind the pre-created instance buffer SRV to the vertex shader once.
	DirectX11::VSSetShaderResources(deferredPtr, 0, 1, m_instanceBufferSRV.GetAddressOf());

	for (auto const& [groupKey, proxies] : instanceGroups)
	{
		if (proxies.empty()) continue;
		assert(proxies.size() <= m_maxInstanceCount && "Exceeded maximum instance count!");

		const auto& groupMaterialGuid = groupKey.materialGuid;
		auto firstProxy = proxies.front();

		// *** THE KEY OPTIMIZATION IS HERE ***
		// --- Set material once per group ---
		// Only update material state if it has changed from the previous group.
		if (groupMaterialGuid != currentMaterialGuid)
		{
			Material* mat = firstProxy->m_Material;
			auto matinfo = mat->m_materialInfo;
			matinfo.m_bitflag |= firstProxy->m_isShadowRecive ? MaterialInfomation::USE_SHADOW_RECIVE : 0;

			DirectX11::UpdateBuffer(deferredPtr, m_materialBuffer.Get(), &matinfo);
			if (mat->m_pBaseColor) DirectX11::PSSetShaderResources(deferredPtr, 0, 1, &mat->m_pBaseColor->m_pSRV);
			if (mat->m_pNormal) DirectX11::PSSetShaderResources(deferredPtr, 1, 1, &mat->m_pNormal->m_pSRV);
			if (mat->m_pOccRoughMetal) DirectX11::PSSetShaderResources(deferredPtr, 2, 1, &mat->m_pOccRoughMetal->m_pSRV);
			if (mat->m_AOMap) DirectX11::PSSetShaderResources(deferredPtr, 3, 1, &mat->m_AOMap->m_pSRV);
			if (mat->m_pEmissive) DirectX11::PSSetShaderResources(deferredPtr, 5, 1, &mat->m_pEmissive->m_pSRV);
		}

		// --- Update the instance data buffer using UpdateSubresource ---
		// This is safer for deferred contexts than Map/Unmap.
		std::vector<Mathf::xMatrix> instanceMatrices;
		instanceMatrices.reserve(proxies.size());
		for (const auto& proxy : proxies)
		{
			instanceMatrices.push_back(proxy->m_worldMatrix);
		}

		// Define the destination box to specify the exact region to update.
		// This prevents reading past the end of the source data.
		D3D11_BOX destBox;
		destBox.left = 0;
		destBox.right = proxies.size() * sizeof(Mathf::xMatrix);
		destBox.top = 0;
		destBox.bottom = 1;
		destBox.front = 0;
		destBox.back = 1;

		deferredPtr->UpdateSubresource(m_instanceBuffer.Get(), 0, &destBox, instanceMatrices.data(), 0, 0);

		// --- Draw all instances in one call ---
		firstProxy->DrawInstanced(deferredPtr, proxies.size());
	}

	// --- 4. CLEANUP AND FINISH COMMAND LIST ---
	ID3D11ShaderResourceView* nullSRV[] = { nullptr };
	DirectX11::VSSetShaderResources(deferredPtr, 0, 1, nullSRV); // Unbind instance buffer

	ID3D11Buffer* nullBuffer[] = { nullptr };
	DirectX11::PSSetShaderResources(deferredPtr, 0, 5, nullSRVs);
	DirectX11::VSSetConstantBuffer(deferredPtr, 3, 1, nullBuffer);

	ID3D11RenderTargetView* nullRTV[RTV_TypeMax]{};
	deferredPtr->OMSetRenderTargets(RTV_TypeMax, nullRTV, nullptr);

	ID3D11CommandList* commandList{};
	deferredPtr->FinishCommandList(false, &commandList);
	PushQueue(camera.m_cameraIndex, commandList);
}

void GBufferPass::TerrainRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera)
{
	if (!RenderPassData::VaildCheck(&camera)) return;
	auto data = RenderPassData::GetData(&camera);

	ID3D11DeviceContext* deferredPtr = deferredContext;
	m_pso->Apply(deferredPtr);

	for (auto& RTV : m_renderTargetViews)
	{
		deferredPtr->ClearRenderTargetView(RTV, Colors::Transparent);
	}

	DirectX11::OMSetRenderTargets(deferredPtr, RTV_TypeMax, m_renderTargetViews, data->m_depthStencil->m_pDSV);

	camera.UpdateBuffer(deferredPtr);
	scene.UseModel(deferredPtr);
	DirectX11::RSSetViewports(deferredPtr, 1, &DeviceState::g_Viewport);
	DirectX11::PSSetConstantBuffer(deferredPtr, 1, 1, &scene.m_LightController->m_pLightBuffer);

	for (auto& terrainProxy : data->m_terrainQueue) {

		auto terrainMesh = terrainProxy->m_terrainMesh;
		auto terrainMaterial = terrainProxy->m_terrainMaterial;

		if (terrainMesh && terrainMaterial)
		{
			DirectX11::PSSetConstantBuffer(deferredPtr, 12, 1, terrainMaterial->GetLayerBuffer());
			scene.UpdateModel(terrainProxy->m_worldMatrix, deferredPtr);

			DirectX11::PSSetShaderResources(deferredPtr, 6, 1, terrainMaterial->GetLayerSRV());
			DirectX11::PSSetShaderResources(deferredPtr, 7, 1, terrainMaterial->GetSplatMapSRV());
			terrainMesh->Draw(deferredPtr);
		}
	}

	ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
	DirectX11::PSSetShaderResources(deferredPtr, 6, 2, nullSRV);

	DirectX11::PSSetShaderResources(deferredPtr, 0, 5, nullSRVs);

	ID3D11RenderTargetView* nullRTV[RTV_TypeMax]{};
	ZeroMemory(nullRTV, sizeof(nullRTV));
	deferredPtr->OMSetRenderTargets(RTV_TypeMax, nullRTV, nullptr);

	ID3D11CommandList* commandList{};
	deferredPtr->FinishCommandList(false, &commandList);
	PushQueue(camera.m_cameraIndex, commandList);
}

void GBufferPass::Resize(uint32_t width, uint32_t height)
{
}
