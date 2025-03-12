#include "Snow.h"
#include "AssetSystem.h"
#include "Scene.h"

SnowPass::SnowPass()
{
	m_pso = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &AssetsSystems->VertexShaders["SnowVS"];
	m_pso->m_geometryShader = &AssetsSystems->GeometryShaders["SnowGS"];
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["SnowPS"];

	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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

	CD3D11_RASTERIZER_DESC rasterizerDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	rasterizerDesc.CullMode = D3D11_CULL_NONE;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	EffectVertex vertices[] = { {Mathf::Vector3(0.0f,0.0f,0.0f)} };
	{
		UINT size = sizeof(EffectVertex);
		D3D11_SUBRESOURCE_DATA vbData = {};
		vbData.pSysMem = vertices;

		m_vertexBuffer = DirectX11::CreateBuffer(size, D3D11_BIND_VERTEX_BUFFER, &vbData);
	}

	{
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.ByteWidth = sizeof(SnowCB);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DeviceState::g_pDevice->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
	}
	
	{
		D3D11_BUFFER_DESC snowParamsDesc = {};
		snowParamsDesc.Usage = D3D11_USAGE_DYNAMIC;
		snowParamsDesc.ByteWidth = sizeof(SnowParamBuffer);
		snowParamsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		snowParamsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DeviceState::g_pDevice->CreateBuffer(&snowParamsDesc, nullptr, m_snowParamsBuffer.GetAddressOf());
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

	DeviceState::g_pDevice->CreateBlendState(&blendDesc, &m_pso->m_blendState);

	CD3D11_DEPTH_STENCIL_DESC depthDesc{ CD3D11_DEFAULT() };
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

	DeviceState::g_pDevice->CreateDepthStencilState(&depthDesc, &m_pso->m_depthStencilState);

	//DirectX11::ThrowIfFailed(
	//	DirectX11::IASetVertexBuffers(0,1, m_pso->);
	//);
}

void SnowPass::Initialize(Texture* renderTarget)
{
	m_renderTarget = renderTarget;

	// 렌더 타겟 텍스처에서 크기 정보 가져오기
	D3D11_TEXTURE2D_DESC rtDesc;
	m_renderTarget->m_pTexture->GetDesc(&rtDesc);

	// 깊이 스텐실 텍스처 생성
	D3D11_TEXTURE2D_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(depthDesc));
	depthDesc.Width = rtDesc.Width;
	depthDesc.Height = rtDesc.Height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc = rtDesc.SampleDesc; // 렌더 타겟과 동일한 멀티샘플링 설정
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ID3D11Texture2D* depthStencilTexture = nullptr;
	HRESULT hr = DeviceState::g_pDevice->CreateTexture2D(&depthDesc, nullptr, &depthStencilTexture);
	if (SUCCEEDED(hr))
	{
		// 깊이 스텐실 뷰 생성
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		ZeroMemory(&dsvDesc, sizeof(dsvDesc));
		dsvDesc.Format = depthDesc.Format;
		dsvDesc.ViewDimension = rtDesc.SampleDesc.Count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		hr = DeviceState::g_pDevice->CreateDepthStencilView(depthStencilTexture, &dsvDesc, &m_depthStencilView);
		depthStencilTexture->Release(); // 텍스처에 대한 참조 해제
	}
}

void SnowPass::LoadTexture(const std::string_view& filePath)
{
	//m_texture = std::make_shared<Texture>(Texture::LoadFormPath(filePath));
}

void SnowPass::Update(float delta)
{
	m_delta += delta;
	// time 값이 너무 커지는 것 방지
	if (m_delta > 10000.0f) {
		m_delta = 0.0f;
	}
}

void SnowPass::Execute(Scene& scene)
{
	ID3D11DepthStencilState* prevDepthState;
	UINT prevStencilRef;
	DeviceState::g_pDeviceContext->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);

	ID3D11RenderTargetView* rtv = m_renderTarget->GetRTV();
	DirectX11::OMSetRenderTargets(1, &rtv, m_depthStencilView.Get());

	ID3D11RasterizerState* prevRasterizerState;
	DeviceState::g_pDeviceContext->RSGetState(&prevRasterizerState);

	m_pso->Apply();

	UINT stride = sizeof(EffectVertex);
	UINT offset = 0;
	DirectX11::IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDeviceContext->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)
	);
	Mathf::Vector4 pos;
	pos = scene.m_MainCamera.m_eyePosition;

	SnowCB* cbData = reinterpret_cast<SnowCB*>(mappedResource.pData);
	cbData->View = XMMatrixTranspose(scene.m_MainCamera.CalculateView());
	cbData->Projection = XMMatrixTranspose(scene.m_MainCamera.CalculateProjection() );
	cbData->CameraPostion = pos;
	cbData->Time = m_delta;
	DeviceState::g_pDeviceContext->Unmap(m_constantBuffer.Get(), 0);

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDeviceContext->Map(m_snowParamsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)
	);

	SnowParamBuffer* snowParams = reinterpret_cast<SnowParamBuffer*>(mappedResource.pData);
	snowParams->WindDirection = mParams.windDirection;
	snowParams->WindStrength = mParams.windStrength;
	snowParams->SnowFallSpeed = mParams.snowFallSpeed;
	snowParams->SnowAmount = mParams.snowAmount;
	snowParams->SnowSize = mParams.snowSize;
	snowParams->time = m_delta;
	snowParams->SnowColor = mParams.snowColor;
	snowParams->SnowOpacity = mParams.snowOpacity;
	DeviceState::g_pDeviceContext->Unmap(m_snowParamsBuffer.Get(), 0);

	ID3D11Buffer* constantBuffer = m_constantBuffer.Get();
	ID3D11Buffer* snowParamsBuffer = m_snowParamsBuffer.Get();
	DeviceState::g_pDeviceContext->VSSetConstantBuffers(0, 1, &constantBuffer);
	DeviceState::g_pDeviceContext->GSSetConstantBuffers(0, 1, &constantBuffer);
	DeviceState::g_pDeviceContext->PSSetConstantBuffers(0, 1, &constantBuffer);

	DeviceState::g_pDeviceContext->VSSetConstantBuffers(1, 1, &snowParamsBuffer);
	DeviceState::g_pDeviceContext->GSSetConstantBuffers(1, 1, &snowParamsBuffer);
	DeviceState::g_pDeviceContext->PSSetConstantBuffers(1, 1, &snowParamsBuffer);

	DeviceState::g_pDeviceContext->Draw(1, 0);

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DeviceState::g_pDeviceContext->RSSetState(prevRasterizerState);
	if (prevRasterizerState) prevRasterizerState->Release();
	DeviceState::g_pDeviceContext->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
	DeviceState::g_pDeviceContext->GSSetShader(nullptr, nullptr, 0);
	DeviceState::g_pDeviceContext->OMSetDepthStencilState(prevDepthState, prevStencilRef);
	if (prevDepthState) prevDepthState->Release();

	DirectX11::UnbindRenderTargets();
}
