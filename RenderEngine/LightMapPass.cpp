#include "LightMapPass.h"
#include "ShaderSystem.h"
#include "Material.h"
#include "Skeleton.h"
#include "Scene.h"
#include "RenderableComponents.h"
#include "Mesh.h"
#include "LightController.h"
#include "LightProperty.h"

struct alignas(16) CB {
	XMFLOAT2 offset{ 0,0 };
	XMFLOAT2 size{ 0,0 };
	XMFLOAT3 cameraPos{ 0,0,0 };
	int lightmapIndex = -1;
};

LightMapPass::LightMapPass()
{
	m_pso = std::make_unique<PipelineStateObject>();
	m_pso->m_vertexShader = &ShaderSystem->VertexShaders["VertexShader"];
	m_pso->m_pixelShader = &ShaderSystem->PixelShaders["Lightmap"];

	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateInputLayout(
			vertexLayoutDesc,
			_countof(vertexLayoutDesc),
			m_pso->m_vertexShader->GetBufferPointer(),
			m_pso->m_vertexShader->GetBufferSize(),
			&m_pso->m_inputLayout
		)
	);

	CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	m_pso->m_depthStencilState = DeviceState::g_pDepthStencilState;

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);
	m_materialBuffer = DirectX11::CreateBuffer(sizeof(MaterialInfomation), D3D11_BIND_CONSTANT_BUFFER, nullptr);
	m_boneBuffer = DirectX11::CreateBuffer(sizeof(Mathf::xMatrix) * Skeleton::MAX_BONES, D3D11_BIND_CONSTANT_BUFFER, nullptr);
	m_cbuffer = DirectX11::CreateBuffer(sizeof(CB), D3D11_BIND_CONSTANT_BUFFER, nullptr);
	DirectX::SetName(m_cbuffer.Get(), "bind lightmapping data");
	DirectX::SetName(m_materialBuffer.Get(), "materialData");
}

void LightMapPass::Initialize(std::vector<Texture*>& lightmaps, std::vector<Texture*>& directionalmaps)
{
	m_plightmaps = &lightmaps;
	m_pDirectionalMaps = &directionalmaps;
	//int size = lightmaps.size();
	//D3D11_TEXTURE2D_DESC temp;
	//lightmaps[0]->m_pTexture->GetDesc(&temp);

	//D3D11_TEXTURE2D_DESC texArrayDesc = {};
	//texArrayDesc.Width = temp.Width;
	//texArrayDesc.Height = temp.Width;
	//texArrayDesc.MipLevels = 1;
	//texArrayDesc.ArraySize = size; // N개의 텍스처
	//texArrayDesc.Format = temp.Format;
	//texArrayDesc.SampleDesc.Count = 1;
	//texArrayDesc.Usage = D3D11_USAGE_DEFAULT;
	//texArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//texArrayDesc.CPUAccessFlags = 0;

	//ID3D11Texture2D* textureArray = nullptr;
	//auto hr = DeviceState::g_pDevice->CreateTexture2D(&texArrayDesc, nullptr, &textureArray);

	//for (UINT i = 0; i < size; i++) {
	//	DeviceState::g_pDeviceContext->CopySubresourceRegion(
	//		textureArray, D3D11CalcSubresource(0, i, 1), 0, 0, 0, // Dest
	//		lightmaps[i]->m_pTexture, 0, nullptr // Source
	//	);
	//}

	//D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	//srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	//srvDesc.Texture2DArray.MostDetailedMip = 0;
	//srvDesc.Texture2DArray.MipLevels = 1;
	//srvDesc.Texture2DArray.FirstArraySlice = 0;
	//srvDesc.Texture2DArray.ArraySize = size;

	//ID3D11ShaderResourceView* textureArraySRV = nullptr;
	//hr = DeviceState::g_pDevice->CreateShaderResourceView(textureArray, &srvDesc, &textureArraySRV);

	//DirectX11::PSSetShaderResources(14, 1, &textureArraySRV);
}

void LightMapPass::Execute(RenderScene& scene, Camera& camera)
{
	//if (!RenderPassData::VaildCheck(&camera)) return;
	//auto renderData = RenderPassData::GetData(&camera);

	//m_pso->Apply();

	//auto& deviceContext = DeviceState::g_pDeviceContext;
	////DirectX11::ClearRenderTargetView(camera.m_renderTarget->GetRTV(), Colors::White);
	//DirectX11::ClearDepthStencilView(renderData->m_depthStencil->m_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	//ID3D11RenderTargetView* rtv = renderData->m_renderTarget->GetRTV();
	//DirectX11::OMSetRenderTargets(1, &rtv, renderData->m_depthStencil->m_pDSV); //뎁스를 사용 안하면 라이트맵은 나오지만 사용 시 뒤에 객체가 보이고, 사용하면 라이트맵이 안나오고

	//camera.UpdateBuffer();
	//scene.UseModel();

	//Animator* currentAnimator = nullptr;
	//Scene* activeScene = scene.GetScene();
	//for (auto& renderer : activeScene->GetMeshRenderers())
	//{
	//	if (!renderer || !renderer->IsEnabled()) continue;

	//	CB buf{};
	//	buf.offset = renderer->m_LightMapping.lightmapOffset;
	//	buf.size = renderer->m_LightMapping.lightmapTiling;
	//	buf.cameraPos = XMFLOAT3(camera.m_eyePosition.m128_f32[0], camera.m_eyePosition.m128_f32[1], camera.m_eyePosition.m128_f32[2]);
	//	buf.lightmapIndex = renderer->m_LightMapping.lightmapIndex;
	//	DirectX11::UpdateBuffer(m_cbuffer.Get(), &buf);
	//	DirectX11::PSSetConstantBuffer(1, 1, m_cbuffer.GetAddressOf());

	//	auto obj = renderer->GetOwner();
	//	scene.UpdateModel(obj->m_transform.GetWorldMatrix());
	//	Animator* animator = scene.GetScene()->m_SceneObjects[obj->m_parentIndex]->GetComponent<Animator>();
	//	if (nullptr != animator && animator->IsEnabled())
	//	{
	//		if (animator != currentAnimator)
	//		{
	//			DirectX11::UpdateBuffer(m_boneBuffer.Get(), animator->m_FinalTransforms);
	//			currentAnimator = animator;
	//		}
	//	}

	//	Material* mat = renderer->m_Material;
	//	DirectX11::UpdateBuffer(m_materialBuffer.Get(), &mat->m_materialInfo);
	//	DirectX11::PSSetConstantBuffer(0, 1, m_materialBuffer.GetAddressOf());
	//	
	//	if (renderer->m_LightMapping.lightmapIndex >= 0) // 또는 라이트맵이 생성되고 있다면 취소하는 방식으로
	//	{
	//		if(m_plightmaps != nullptr && (*m_plightmaps).size() > renderer->m_LightMapping.lightmapIndex)
	//			DirectX11::PSSetShaderResources(14, 1, &(*m_plightmaps)[renderer->m_LightMapping.lightmapIndex]->m_pSRV);
	//		if(m_pDirectionalMaps != nullptr && (*m_pDirectionalMaps).size() > renderer->m_LightMapping.lightmapIndex)
	//			DirectX11::PSSetShaderResources(15, 1, &(*m_pDirectionalMaps)[renderer->m_LightMapping.lightmapIndex]->m_pSRV);
	//	}
	//	
	//	if (mat->m_pBaseColor)
	//	{
	//		DirectX11::PSSetShaderResources(0, 1, &mat->m_pBaseColor->m_pSRV);
	//	}
	//	if (mat->m_pNormal)
	//	{
	//		DirectX11::PSSetShaderResources(1, 1, &mat->m_pNormal->m_pSRV);
	//	}
	//	if (mat->m_pOccRoughMetal)
	//	{
	//		DirectX11::PSSetShaderResources(2, 1, &mat->m_pOccRoughMetal->m_pSRV);
	//	}
	//	if (mat->m_AOMap)
	//	{
	//		DirectX11::PSSetShaderResources(3, 1, &mat->m_AOMap->m_pSRV);
	//	}
	//	if (mat->m_pEmissive)
	//	{
	//		DirectX11::PSSetShaderResources(5, 1, &mat->m_pEmissive->m_pSRV);
	//	}

	//	renderer->m_Mesh->Draw();
	//}

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

void LightMapPass::CreateRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera)
{
	if (!RenderPassData::VaildCheck(&camera)) return;
	auto renderData = RenderPassData::GetData(&camera);

	ID3D11DeviceContext* deferredPtr = deferredContext;

	m_pso->Apply(deferredPtr);

	//DirectX11::ClearRenderTargetView(camera.m_renderTarget->GetRTV(), Colors::White);
	DirectX11::ClearDepthStencilView(deferredPtr, renderData->m_depthStencil->m_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	ID3D11RenderTargetView* rtv = renderData->m_renderTarget->GetRTV();
	DirectX11::OMSetRenderTargets(deferredPtr, 1, &rtv, renderData->m_depthStencil->m_pDSV); //뎁스를 사용 안하면 라이트맵은 나오지만 사용 시 뒤에 객체가 보이고, 사용하면 라이트맵이 안나오고

	camera.UpdateBuffer(deferredPtr);
	scene.UseModel(deferredPtr);

	Animator* currentAnimator = nullptr;
	Scene* activeScene = scene.GetScene();
	for (auto& renderer : activeScene->GetMeshRenderers())
	{
		if (!renderer || !renderer->IsEnabled()) continue;

		CB buf{};
		buf.offset = renderer->m_LightMapping.lightmapOffset;
		buf.size = renderer->m_LightMapping.lightmapTiling;
		buf.cameraPos = XMFLOAT3(camera.m_eyePosition.m128_f32[0], camera.m_eyePosition.m128_f32[1], camera.m_eyePosition.m128_f32[2]);
		buf.lightmapIndex = renderer->m_LightMapping.lightmapIndex;
		DirectX11::UpdateBuffer(deferredPtr, m_cbuffer.Get(), &buf);
		DirectX11::PSSetConstantBuffer(deferredPtr, 1, 1, m_cbuffer.GetAddressOf());

		auto obj = renderer->GetOwner();
		scene.UpdateModel(obj->m_transform.GetWorldMatrix(), deferredPtr);
		Animator* animator = scene.GetScene()->m_SceneObjects[obj->m_parentIndex]->GetComponent<Animator>();
		if (nullptr != animator && animator->IsEnabled())
		{
			if (animator != currentAnimator)
			{
				DirectX11::UpdateBuffer(deferredPtr, m_boneBuffer.Get(), animator->m_FinalTransforms);
				currentAnimator = animator;
			}
		}

		Material* mat = renderer->m_Material;
		DirectX11::UpdateBuffer(deferredPtr, m_materialBuffer.Get(), &mat->m_materialInfo);
		DirectX11::PSSetConstantBuffer(deferredPtr, 0, 1, m_materialBuffer.GetAddressOf());

		if (renderer->m_LightMapping.lightmapIndex >= 0) // 또는 라이트맵이 생성되고 있다면 취소하는 방식으로
		{
			if (m_plightmaps != nullptr && (*m_plightmaps).size() > renderer->m_LightMapping.lightmapIndex)
				DirectX11::PSSetShaderResources(deferredPtr, 14, 1, &(*m_plightmaps)[renderer->m_LightMapping.lightmapIndex]->m_pSRV);
			if (m_pDirectionalMaps != nullptr && (*m_pDirectionalMaps).size() > renderer->m_LightMapping.lightmapIndex)
				DirectX11::PSSetShaderResources(deferredPtr, 15, 1, &(*m_pDirectionalMaps)[renderer->m_LightMapping.lightmapIndex]->m_pSRV);
		}

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

		renderer->m_Mesh->Draw(deferredPtr);
	}

	ID3D11CommandList* commandList{};
	deferredPtr->FinishCommandList(false, &commandList);
	PushQueue(camera.m_cameraIndex, commandList);
}


void LightMapPass::Resize(uint32_t width, uint32_t height)
{
}
