#include "Fire.h"
#include "AssetSystem.h"
#include "Material.h"
#include "Scene.h"
#include "Mesh.h"

FirePass::FirePass()
{
	m_pso = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &AssetsSystems->VertexShaders["VertexShader"];

	// 불효과를 ps에서만 해도 되나?
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["Fire"];

	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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

	{
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.ByteWidth = sizeof(FireParameters);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DeviceState::g_pDevice->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
	}

	//{
	//	D3D11_BUFFER_DESC fireParams = {};
	//	fireParams.Usage = D3D11_USAGE_DYNAMIC;
	//	fireParams.ByteWidth = sizeof(FireParameters);
	//	fireParams.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	//	fireParams.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//	DeviceState::g_pDevice->CreateBuffer(&fireParams, nullptr, m_fireParamsBuffer.GetAddressOf());
	//}


	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBlendState(&blendDesc, &m_pso->m_blendState)
	);

	CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	m_pso->m_depthStencilState = DeviceState::g_pDepthStencilState;


	m_pso->m_samplers.emplace_back(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	m_pso->m_samplers.emplace_back(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	mParam = {};
	mParam.time = 0.0f;
	mParam.intensity = 1.0f;
	mParam.speed = 1.0f;
	mParam.colorShift = 0.5f;
	mParam.noiseScale = 4.0f;
	mParam.verticalFactor = 2.0f;
	mParam.flamePower = 1.2f;
	mParam.detailScale = 3.0f;
}

void FirePass::Initialize()
{
	auto computeShaderBlob = AssetsSystems->ComputeShaders["FireCompute"];

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateComputeShader(
			computeShaderBlob.GetBufferPointer(),
			computeShaderBlob.GetBufferSize(),
			nullptr,
			m_computeShader.GetAddressOf()
		)
	);

	{
		D3D11_BUFFER_DESC paramDesc = {};
		paramDesc.Usage = D3D11_USAGE_DYNAMIC;
		paramDesc.ByteWidth = sizeof(FireParameters);
		paramDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		paramDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DirectX11::ThrowIfFailed(
			DeviceState::g_pDevice->CreateBuffer(&paramDesc, nullptr, m_fireParamsBuffer.GetAddressOf())
		);
	}

	m_resultTexture = std::shared_ptr<Texture>(Texture::Create(
		512,  // Width
		512,  // Height
		"FireEffectTexture",
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
	));

	m_resultTexture->CreateSRV(DXGI_FORMAT_R8G8B8A8_UNORM);
	m_resultTexture->CreateUAV(DXGI_FORMAT_R8G8B8A8_UNORM);
}

void FirePass::LoadTexture(const std::string_view& basePath, const std::string_view& noisePath)
{
	m_noiseTexture = std::shared_ptr<Texture>(Texture::LoadFormPath(noisePath));
	m_baseFireTexture = std::shared_ptr<Texture>(Texture::LoadFormPath(basePath));
}

void FirePass::Update(float delta)
{
	m_delta += delta;

	if (m_delta > 10000.0f) {
		m_delta = 0.0f;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDeviceContext->Map(
			m_fireParamsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource
		)
	);

	memcpy(mappedResource.pData, &mParam, sizeof(FireParameters));
	DeviceState::g_pDeviceContext->Unmap(m_fireParamsBuffer.Get(), 0);

}

void FirePass::SetRenderTarget(Texture* renderTargetView)
{
	m_renderTarget = renderTargetView;
}

// 옵젝 붙이는거 고민중..
void FirePass::PushFireObject(SceneObject* object)
{
	m_fireObjects.push_back(object);
}



void FirePass::Execute(Scene& scene)
{
	if (!m_baseFireTexture || !m_noiseTexture) {
		return;
	}

	auto& deviceContext = DeviceState::g_pDeviceContext;

	deviceContext->CSSetShader(m_computeShader.Get(), nullptr, 0);

	deviceContext->CSSetConstantBuffers(0, 1, m_fireParamsBuffer.GetAddressOf());

	ID3D11ShaderResourceView* srvs[] = {
	  m_baseFireTexture->m_pSRV,
	  m_noiseTexture->m_pSRV
	};

	deviceContext->CSSetShaderResources(0, 2, srvs);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &m_resultTexture->m_pUAV, nullptr);

	deviceContext->Dispatch(32, 32, 1);

	ID3D11UnorderedAccessView* nullUAV = nullptr;
	deviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	deviceContext->CSSetShaderResources(0, 1, &nullSRV);

	m_pso->Apply();

	ID3D11RenderTargetView* rtv = m_renderTarget->GetRTV();
	deviceContext->OMSetRenderTargets(1, &rtv, DeviceState::g_pDepthStencilView);

	float blendFactor[] = { 0.0f, 0.0f, 0.0f,0.0f };
	deviceContext->OMSetBlendState(m_pso->m_blendState, blendFactor, 0xFFFFFFFF);

	scene.UseCamera(scene.m_MainCamera);
	DirectX11::PSSetConstantBuffer(1, 1, &scene.m_LightController.m_pLightBuffer);
	
	DirectX11::PSSetConstantBuffer(2, 1, m_fireParamsBuffer.GetAddressOf());

	DirectX11::PSSetShaderResources(0, 1, &m_resultTexture->m_pSRV);

	for (auto& sceneObejct : m_fireObjects)
	{
		if (!sceneObejct->m_meshRenderer.m_IsEnabled)
			continue;

		scene.UpdateModel(sceneObejct->m_transform.GetWorldMatrix());

		sceneObejct->m_meshRenderer.m_Mesh->Draw();
	}

	m_fireObjects.clear();

	DirectX11::PSSetShaderResources(0, 1, &nullSRV);

	deviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

	DirectX11::UnbindRenderTargets();

}
