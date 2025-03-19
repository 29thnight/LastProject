#include "Fire.h"
#include "AssetSystem.h"
#include "Material.h"
#include "Scene.h"
#include "Mesh.h"
#include "ImGuiRegister.h"

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

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
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

	m_pso->m_samplers.emplace_back(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	m_pso->m_samplers.emplace_back(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	// 유니크 쓰지 말기
	//mParam = new FireParameters;
	//mParam->time = 1.0f;
	//mParam->intensity = 1.0f;
	//mParam->speed = 1.0f;
	//mParam->colorShift = 0.5f;
	//mParam->noiseScale = 4.0f;
	//mParam->verticalFactor = 2.0f;
	//mParam->flamePower = 1.2f;
	//mParam->detailScale = 3.0f;
	//mParam->patternChangeSpeed = 5.0f;
	//mParam->firePosition1 = Mathf::Vector4(0.1f, 0.1f, 0.3f, 0.3f);	// 좌측 하단
	//mParam->firePosition2 = Mathf::Vector4(0.6f, 0.1f, 0.3f, 0.3f);	// 우측 하단
	//mParam->firePosition3 = Mathf::Vector4(0.1f, 0.6f, 0.3f, 0.3f);	// 좌측 상단
	//mParam->firePosition4 = Mathf::Vector4(0.6f, 0.6f, 0.3f, 0.3f);	// 우측 상단
	//mParam->timeOffset1 = 0.0f;  // 첫 번째 불은 기본 시간
	//mParam->timeOffset2 = 1.57f; // 약 π/2 (90도 위상차)
	//mParam->timeOffset3 = 3.14f; // 약 π (180도 위상차)
	//mParam->timeOffset4 = 4.17f; // 약 3π/2 (270도 위상차)
	//mParam->numFireEffects = 4;
	//mParam->Color = Mathf::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	//
	//SetParameters(mParam);

	mmParam = new ExplodeParameters;
	mmParam->time = 0.0f;
	mmParam->intensity = 1.0f;
	mmParam->speed = 1.0f;
	mmParam->size = Mathf::Vector2(256.0f, 256.0f);
	mmParam->range = Mathf::Vector2(8.0f, 4.0f);

	SetParameters(mmParam);


	CD3D11_DEPTH_STENCIL_DESC depthDesc{ CD3D11_DEFAULT() };
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DeviceState::g_pDevice->CreateDepthStencilState(&depthDesc, &m_pso->m_depthStencilState);
}

void FirePass::Initialize()
{
	auto computeShaderBlob = AssetsSystems->ComputeShaders["Explode"];

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

	{
		//ImGui::ContextRegister("Fire Effect", [&]()
		//	{
		//		
		//
		//		ImGui::SliderFloat("Color Shift", &mParam->colorShift, -100, 100);
		//		ImGui::SliderFloat("Noise Scale", &mParam->noiseScale, -10.0f, 10.0f);
		//		ImGui::SliderFloat("Scale", &scale.x, 0.1f, 10);
		//	});
	}


}

void FirePass::LoadTexture(const std::string_view& basePath, const std::string_view& noisePath)
{
	m_noiseTexture = std::shared_ptr<Texture>(Texture::LoadFormPath(noisePath));
	m_baseFireTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01.dds"));
	m_fireAlphaTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01_a.jpg"));
}

void FirePass::Update(float delta)
{
	m_delta += delta;

	if (m_delta > 10000.0f) {
		m_delta = 0.0f;
	}
	
	mmParam->time = m_delta;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDeviceContext->Map(
			m_fireParamsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource
		)
	);

	memcpy(mappedResource.pData, static_cast<ExplodeParameters*>(mmParam), sizeof(ExplodeParameters));
	DeviceState::g_pDeviceContext->Unmap(m_fireParamsBuffer.Get(), 0);

	//mParam->time = m_delta;
	//
	//D3D11_MAPPED_SUBRESOURCE mappedResource;
	//DirectX11::ThrowIfFailed(
	//	DeviceState::g_pDeviceContext->Map(
	//		m_fireParamsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource
	//	)
	//);
	//
	//memcpy(mappedResource.pData, static_cast<FireParameters*>(mParam), sizeof(FireParameters));
	//DeviceState::g_pDeviceContext->Unmap(m_fireParamsBuffer.Get(), 0);

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

	ID3D11DepthStencilState* prevDepthState;
	UINT prevStencilRef;
	DeviceState::g_pDeviceContext->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);

	auto& deviceContext = DeviceState::g_pDeviceContext;

	deviceContext->CSSetShader(m_computeShader.Get(), nullptr, 0);

	deviceContext->CSSetConstantBuffers(0, 1, m_fireParamsBuffer.GetAddressOf());

	ID3D11SamplerState* samplers[] = {
	   m_pso->m_samplers[0].m_SamplerState,  // LinearSampler
	   m_pso->m_samplers[1].m_SamplerState   // WrapSampler
	};
	deviceContext->CSSetSamplers(0, 2, samplers);

	//ID3D11ShaderResourceView* srvs[] = {
	//  m_baseFireTexture->m_pSRV,
	//  m_noiseTexture->m_pSRV,
	//   m_fireAlphaTexture->m_pSRV
	//};
	ID3D11ShaderResourceView* srvs = m_baseFireTexture->m_pSRV;
	deviceContext->CSSetShaderResources(0, 1, &srvs);

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
	scene.UseModel();

	DirectX11::PSSetConstantBuffer(3, 1, m_fireParamsBuffer.GetAddressOf());

	DirectX11::PSSetShaderResources(0, 1, &m_resultTexture->m_pSRV);
	
	struct SimpleVertex {
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexCoord;
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT3 Binormal;
		DirectX::XMFLOAT4 BlendIndices;
		DirectX::XMFLOAT4 BlendWeights;
	};

	// 정점 데이터 생성
	SimpleVertex vertices[] = {
		// Position                  Normal              TexCoord       Tangent            Binormal          BlendIndices      BlendWeights
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
		{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
		{{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
		{{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}}
	};

	// 인덱스 데이터
	UINT indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	// 정점 버퍼 생성
	ID3D11Buffer* vertexBuffer = nullptr;
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(vertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;

	DeviceState::g_pDevice->CreateBuffer(&vbDesc, &vbData, &vertexBuffer);

	// 인덱스 버퍼 생성
	ID3D11Buffer* indexBuffer = nullptr;
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;

	DeviceState::g_pDevice->CreateBuffer(&ibDesc, &ibData, &indexBuffer);

	// 월드 행렬 설정 - 카메라 앞에 위치하도록 조정
	DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixIdentity();
	worldMatrix = DirectX::XMMatrixScaling(2.0f, 2.0f, 1.0f) *
		DirectX::XMMatrixTranslation(0.0f, 0.0f, 5.0f); // 카메라 앞쪽에 위치

	scene.UpdateModel(worldMatrix);

	// 버퍼 설정 및 그리기
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 그리기
	deviceContext->DrawIndexed(6, 0, 0);

	// 리소스 해제
	if (vertexBuffer) vertexBuffer->Release();
	if (indexBuffer) indexBuffer->Release();


	//for (auto& sceneObejct : m_fireObjects)
	//{
	//	if (!sceneObejct->m_meshRenderer.m_IsEnabled)
	//		continue;
	//
	//	scene.UpdateModel(sceneObejct->m_transform.GetWorldMatrix());
	//
	//	sceneObejct->m_meshRenderer.m_Mesh->Draw();
	//}

	DirectX11::PSSetShaderResources(0, 1, &nullSRV);

	deviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

	DeviceState::g_pDeviceContext->OMSetDepthStencilState(prevDepthState, prevStencilRef);
	if (prevDepthState) prevDepthState->Release();

	DirectX11::UnbindRenderTargets();

	m_fireObjects.clear();

}
