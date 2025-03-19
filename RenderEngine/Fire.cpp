#include "Fire.h"
#include "AssetSystem.h"
#include "Material.h"
#include "Scene.h"
#include "Mesh.h"
#include "ImGuiRegister.h"

FirePass::FirePass()
{
	m_pso = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &AssetsSystems->VertexShaders["BillBoard"];
	m_pso->m_geometryShader = &AssetsSystems->GeometryShaders["BillBoard"];
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["Fire"];
	m_pso->m_computeShader = &AssetsSystems->ComputeShaders["FireCompute"];
	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
		cbDesc.ByteWidth = sizeof(ExplodeParameters);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DeviceState::g_pDevice->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
	}

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
	mmParam->speed = 5.0f;
	mmParam->size = Mathf::Vector2(256.0f, 256.0f);
	mmParam->range = Mathf::Vector2(8.0f, 4.0f);

	SetParameters(mmParam);


	CD3D11_DEPTH_STENCIL_DESC depthDesc{ CD3D11_DEFAULT() };
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthEnable = false;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DeviceState::g_pDevice->CreateDepthStencilState(&depthDesc, &m_pso->m_depthStencilState);

	m_baseFireTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01.dds"));
	m_fireAlphaTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01_a.jpg"));

	m_ModelBuffer = DirectX11::CreateBuffer(
		sizeof(ModelConstantBuffer),
		D3D11_BIND_CONSTANT_BUFFER,
		&modelConst
	);
}

void FirePass::Initialize()
{
	{
		D3D11_BUFFER_DESC paramDesc = {};
		paramDesc.Usage = D3D11_USAGE_DYNAMIC;
		paramDesc.ByteWidth = sizeof(ExplodeParameters);
		paramDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		paramDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DirectX11::ThrowIfFailed(
			DeviceState::g_pDevice->CreateBuffer(&paramDesc, nullptr, m_constantBuffer.GetAddressOf())
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

	CreateBillboardVertexBuffer();
}

void FirePass::CreateBillboardVertexBuffer()
{
	BillboardVertex vertices[] = {
		{ {1.0f, 0.0f, 0.0f, 1.0f}, {10.0f, -10.0f}, {0.0f, 0.0f, 0.0f, 0.0f} }
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(vertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBuffer(&vbDesc, &vbData, m_billboardVertexBuffer.GetAddressOf())
	);
}

void FirePass::LoadTexture(const std::string_view& basePath, const std::string_view& noisePath)
{
	//m_noiseTexture = std::shared_ptr<Texture>(Texture::LoadFormPath(noisePath));
	//m_baseFireTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01.dds"));
	//m_fireAlphaTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01_a.jpg"));
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
			m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource
		)
	);

	memcpy(mappedResource.pData, static_cast<ExplodeParameters*>(mmParam), sizeof(ExplodeParameters));
	DeviceState::g_pDeviceContext->Unmap(m_constantBuffer.Get(), 0);

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
	EffectedObject.push_back(object);
}

void FirePass::Execute(Scene& scene)
{
	// 현재 렌더 상태 저장
	ID3D11DepthStencilState* prevDepthState;
	UINT prevStencilRef;
	ID3D11BlendState* prevBlendState;
	float prevBlendFactor[4];
	UINT prevSampleMask;
	ID3D11RasterizerState* prevRasterizerState;

	auto& deviceContext = DeviceState::g_pDeviceContext;
	deviceContext->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);
	deviceContext->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);
	deviceContext->RSGetState(&prevRasterizerState);

	// 컴퓨트 셰이더 실행 (불 텍스처 생성)
	deviceContext->CSSetShader(m_pso->m_computeShader->GetShader(), nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	ID3D11SamplerState* samplers[] = {
	   m_pso->m_samplers[0].m_SamplerState,
	   m_pso->m_samplers[1].m_SamplerState
	};
	deviceContext->CSSetSamplers(0, 2, samplers);

	ID3D11ShaderResourceView* srvs[]{
		m_baseFireTexture->m_pSRV,
		m_fireAlphaTexture->m_pSRV
	};

	deviceContext->CSSetShaderResources(0, 2, srvs);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &m_resultTexture->m_pUAV, nullptr);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	deviceContext->ClearUnorderedAccessViewFloat(m_resultTexture->m_pUAV, clearColor);

	deviceContext->Dispatch(32, 32, 1);

	// 컴퓨트 셰이더 리소스 해제
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	deviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	ID3D11ShaderResourceView* nullSRV = nullptr;
	deviceContext->CSSetShaderResources(0, 1, &nullSRV);

	// 렌더링 파이프라인 설정
	m_pso->Apply();

	ID3D11RenderTargetView* rtv = m_renderTarget->GetRTV();
	deviceContext->OMSetRenderTargets(1, &rtv, DeviceState::g_pDepthStencilView);
	deviceContext->OMSetBlendState(m_pso->m_blendState, nullptr, 0xFFFFFFFF);

	// 카메라 설정
	scene.UseCamera(scene.m_MainCamera);

	modelConst.world = XMMatrixIdentity();
	modelConst.view = XMMatrixTranspose(scene.m_MainCamera.CalculateView());
	modelConst.projection = XMMatrixTranspose(scene.m_MainCamera.CalculateProjection());

	deviceContext->GSSetConstantBuffers(0, 1, m_ModelBuffer.GetAddressOf());
	DirectX11::UpdateBuffer(m_ModelBuffer.Get(), &modelConst);

	// 픽셀 셰이더 설정
	DirectX11::PSSetConstantBuffer(3, 1, m_constantBuffer.GetAddressOf());
	DirectX11::PSSetShaderResources(0, 1, &m_resultTexture->m_pSRV);

	// 정점 버퍼 설정
	UINT stride = sizeof(BillboardVertex);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, m_billboardVertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// 빌보드 그리기
	deviceContext->Draw(1, 0);

	// 리소스 정리 및 상태 복원
	DirectX11::PSSetShaderResources(0, 1, &nullSRV);
	deviceContext->OMSetDepthStencilState(prevDepthState, prevStencilRef);
	deviceContext->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
	deviceContext->RSSetState(prevRasterizerState);

	deviceContext->GSSetShader(nullptr, nullptr, 0);

	// 포인터 해제
	if (prevDepthState) prevDepthState->Release();
	if (prevBlendState) prevBlendState->Release();
	if (prevRasterizerState) prevRasterizerState->Release();

	DirectX11::UnbindRenderTargets();
	EffectedObject.clear();
}