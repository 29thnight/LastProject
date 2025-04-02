#include "Effects.h"
#include "AssetSystem.h"
#include "Fire.h"
#include "SparkleEffect.h"

Effects::Effects()
{
	m_pso = std::make_unique<PipelineStateObject>();
	m_BillBoardType = BillBoardType::Basic;
	m_instanceCount = 0;
	mParam = nullptr;

	// 공통 블렌드 상태 설정
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

	// 공통 래스터라이저 상태 설정
	CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	// 공통 뎁스 스텐실 상태 설정
	CD3D11_DEPTH_STENCIL_DESC depthDesc{ CD3D11_DEFAULT() };
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DeviceState::g_pDevice->CreateDepthStencilState(&depthDesc, &m_pso->m_depthStencilState);

	// 공통 프리미티브 토폴로지 설정
	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

	m_pso->m_vertexShader = &AssetsSystems->VertexShaders["BillBoard"];
	m_pso->m_geometryShader = &AssetsSystems->GeometryShaders["BillBoard"];

	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
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

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

}

void Effects::CreateBillboard(BillboardVertex* vertex, BillBoardType type)
{
	mVertex = vertex;

	m_BillBoardType = type;

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(BillboardVertex);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = mVertex;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBuffer(&vbDesc, &vbData, &billboardVertexBuffer)
	);

	m_ModelBuffer = DirectX11::CreateBuffer(
		sizeof(ModelConstantBuffer),
		D3D11_BIND_CONSTANT_BUFFER,
		&m_ModelConstantBuffer
	);
}

void Effects::SetParameters(EffectParameters* param)
{
	mParam = param;
}

void Effects::DrawBillboard(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection)
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	m_ModelConstantBuffer.world = world;
	m_ModelConstantBuffer.view = view;
	m_ModelConstantBuffer.projection = projection;

	deviceContext->GSSetConstantBuffers(0, 1, m_ModelBuffer.GetAddressOf());
	DirectX11::UpdateBuffer(m_ModelBuffer.Get(), &m_ModelConstantBuffer);

	// 정점 버퍼 설정
	ID3D11Buffer* buffers[2] = { billboardVertexBuffer, m_InstanceBuffer.Get() };
	UINT strides[2] = { sizeof(BillboardVertex), sizeof(BillBoardInstanceData) };
	UINT offsets[2] = { 0, 0 };

	deviceContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// 빌보드 그리기
	//deviceContext->Draw(1, 0);
	deviceContext->DrawInstanced(1, m_instanceCount, 0, 0);
}

void Effects::Execute(Scene& scene, Camera& camera)
{
	for (auto& [key, effect] : effects) {
		effect->Render(scene, camera);
	}
}

void Effects::SetupBillBoardInstancing(BillBoardInstanceData* instance, UINT count)
{
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(BillBoardInstanceData) * count;
	ibDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = instance;

	ID3D11Buffer* instanceBuffer;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBuffer(&ibDesc, &ibData, &instanceBuffer)
	);
	
	m_InstanceBuffer.Attach(instanceBuffer);
	m_instanceCount = count;
}

void Effects::UpdateEffects(float delta)
{
	for (auto& [key, effect] : effects) {
		effect->Update(delta);
	}
}

void Effects::MakeEffects(Effect type, std::string_view name)
{
	switch (type)
	{
	case Effect::Explode:
		effects[name.data()] = std::make_unique<FirePass>();
		break;
	case Effect::Sparkle:
		effects[name.data()] = std::make_unique<SparkleEffect>(Mathf::Vector3(-100.0f,-100.0f,0.0f));
		break;
	}
}

Effects* Effects::GetEffect(std::string_view name)
{
	auto it = effects.find(name.data());
	if (it != effects.end()) {
		return it->second.get();
	}
	return nullptr;
}

bool Effects::RemoveEffect(std::string_view name)
{
	return effects.erase(name.data()) > 0;
}

void Effects::CreateBillBoardMatrix(const Mathf::Matrix& view, const Mathf::Matrix& world)
{
	Mathf::Vector3 pos = world.Translation();
	Mathf::Matrix matrix;

	switch (m_BillBoardType)
	{
	case BillBoardType::Basic:
		break;
	case BillBoardType::YAxs:
		break;
	case BillBoardType::AxisAligned:
		break;
	default:
		break;
	}
}
