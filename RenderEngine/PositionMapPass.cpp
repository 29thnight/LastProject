#include "PositionMapPass.h"
#include "ShaderSystem.h"
#include "Scene.h"
#include "Mesh.h"
#include "Sampler.h"
#include "RenderableComponents.h"
#include "ResourceAllocator.h"
struct alignas(16) PositionMapBuffer
{
	int positionMapWidth;
	int positionMapheight;
};

PositionMapPass::PositionMapPass()
{
	m_pso = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &ShaderSystem->VertexShaders["PositionMap"];
	m_pso->m_pixelShader = &ShaderSystem->PixelShaders["PositionMap"];

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

	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = false;
	//rasterizerDesc.MultisampleEnable = true;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

	m_Buffer = DirectX11::CreateBuffer(sizeof(PositionMapBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);

	m_edgeComputeShader = &ShaderSystem->ComputeShaders["NeighborSampling"];
	m_edgeCoverComputeShader = &ShaderSystem->ComputeShaders["LightmapEdgeCover"];

	CreateTempTexture();
}

void PositionMapPass::Initialize(uint32 width, uint32 height)
{
}

void PositionMapPass::Execute(RenderScene& scene, Camera& camera)
{
	ClearTextures();

	if (tempTexture == nullptr) {
		CreateTempTexture();
	}

	m_pso->Apply();

	PositionMapBuffer posBuf = { posNormMapSize, posNormMapSize };
	DirectX11::UpdateBuffer(m_Buffer.Get(), &posBuf);
	DirectX11::VSSetConstantBuffer(0, 1, m_Buffer.GetAddressOf());

	auto pre = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		posNormMapSize,
		posNormMapSize
	);

	DeviceState::g_pDeviceContext->RSSetViewports(1, &pre);

	int i = 0;
	std::vector<MeshRenderer*> renderers = scene.GetScene()->GetMeshRenderers();
	for (auto& renderer : renderers)
	{
		if (!renderer->IsEnabled()) continue;
		auto meshName = renderer->m_Mesh->GetName();
		if (m_positionMapTextures.find(meshName) == m_positionMapTextures.end()) {
			if (m_positionMapTextures[meshName] != nullptr) continue;
			// 모델의 positionMap 생성
			m_positionMapTextures[meshName] = Texture::Create(posNormMapSize, posNormMapSize, "Position Map",
				DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
			m_positionMapTextures[meshName]->m_textureType = TextureType::ImageTexture;
			m_positionMapTextures[meshName]->CreateRTV(DXGI_FORMAT_R32G32B32A32_FLOAT);
			m_positionMapTextures[meshName]->CreateSRV(DXGI_FORMAT_R32G32B32A32_FLOAT);
			m_positionMapTextures[meshName]->CreateUAV(DXGI_FORMAT_R32G32B32A32_FLOAT);

			m_normalMapTextures[meshName] = Texture::Create(posNormMapSize, posNormMapSize, "Normal Map",
				DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
			m_normalMapTextures[meshName]->m_textureType = TextureType::ImageTexture;
			m_normalMapTextures[meshName]->CreateRTV(DXGI_FORMAT_R32G32B32A32_FLOAT);
			m_normalMapTextures[meshName]->CreateSRV(DXGI_FORMAT_R32G32B32A32_FLOAT);
			m_normalMapTextures[meshName]->CreateUAV(DXGI_FORMAT_R32G32B32A32_FLOAT);

			float nullColor[4] = { 0,0,0,0 };

			DirectX11::ClearRenderTargetView(m_positionMapTextures[meshName]->GetRTV(), nullColor);
			DirectX11::ClearRenderTargetView(m_normalMapTextures[meshName]->GetRTV(), nullColor);

			ID3D11RenderTargetView* rtv[2] = {
				m_positionMapTextures[meshName]->GetRTV(),
				m_normalMapTextures[meshName]->GetRTV()
			};
			DeviceState::g_pDeviceContext->OMSetRenderTargets(2, rtv, nullptr);

			renderer->m_Mesh->Draw();

			//DirectX::ScratchImage image;
			//HRESULT hr = DirectX::CaptureTexture(DeviceState::g_pDevice, DeviceState::g_pDeviceContext, m_positionMapTextures[meshName]->m_pTexture, image);
			//
			//std::wstring a = std::to_wstring(i++);
			//a += L"Lightmap.png";
			//DirectX::SaveToWICFile(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
			//	GUID_ContainerFormatPng, a.c_str());

			ID3D11RenderTargetView* nullRTV[2] = { nullptr, nullptr };
			DeviceState::g_pDeviceContext->OMSetRenderTargets(2, nullRTV, nullptr);
		}
	}

	if (isDilateOn == false) return;

	ID3D11UnorderedAccessView* nullUAV[2] = { nullptr, nullptr };
	
	for (int i = 0; i < posNormDilateCount; i++) {
		for (auto& [name, texture] : m_positionMapTextures)
		{
			if (texture != nullptr)
			{
				DirectX11::CSSetShader(m_edgeComputeShader->GetShader(), nullptr, 0);
				DeviceState::g_pDeviceContext->ClearUnorderedAccessViewFloat(tempTexture->m_pUAV, Colors::Transparent);
				DirectX11::CSSetUnorderedAccessViews(0, 1, &tempTexture->m_pUAV, nullptr);
				DirectX11::CSSetShaderResources(0, 1, &texture->m_pSRV);
				DirectX11::Dispatch(posNormMapSize / 16.f, posNormMapSize / 16.f, 1);

				DirectX11::CSSetUnorderedAccessViews(0, 2, nullUAV, nullptr);

				// 엣지로 덮어쓰기.
				DirectX11::CSSetShader(m_edgeCoverComputeShader->GetShader(), nullptr, 0);
				DirectX11::CSSetUnorderedAccessViews(0, 1, &texture->m_pUAV, nullptr);
				DirectX11::CSSetShaderResources(0, 1, &tempTexture->m_pSRV);
				DirectX11::Dispatch(posNormMapSize / 16.f, posNormMapSize / 16.f, 1);

				DirectX11::CSSetUnorderedAccessViews(0, 2, nullUAV, nullptr);
			}
		}
		for (auto& [name, texture] : m_normalMapTextures)
		{
			if (texture != nullptr)
			{
				DirectX11::CSSetShader(m_edgeComputeShader->GetShader(), nullptr, 0);
				DeviceState::g_pDeviceContext->ClearUnorderedAccessViewFloat(tempTexture->m_pUAV, Colors::Transparent);
				DirectX11::CSSetUnorderedAccessViews(0, 1, &tempTexture->m_pUAV, nullptr);
				DirectX11::CSSetShaderResources(0, 1, &texture->m_pSRV);
				DirectX11::Dispatch(posNormMapSize / 16.f, posNormMapSize / 16.f, 1);

				DirectX11::CSSetUnorderedAccessViews(0, 2, nullUAV, nullptr);

				// 엣지로 덮어쓰기.
				DirectX11::CSSetShader(m_edgeCoverComputeShader->GetShader(), nullptr, 0);
				DirectX11::CSSetUnorderedAccessViews(0, 1, &texture->m_pUAV, nullptr);
				DirectX11::CSSetShaderResources(0, 1, &tempTexture->m_pSRV);
				DirectX11::Dispatch(posNormMapSize / 16.f, posNormMapSize / 16.f, 1);

				DirectX11::CSSetUnorderedAccessViews(0, 2, nullUAV, nullptr);
			}
		}
	}

	DeviceState::g_pDeviceContext->RSSetViewports(1, &DeviceState::g_Viewport);
}

void PositionMapPass::ClearTextures()
{
	for (auto& texture : m_positionMapTextures)
	{
		DeallocateResource(texture.second);
	}
	m_positionMapTextures.clear();
	for (auto& texture : m_normalMapTextures)
	{
		DeallocateResource(texture.second);
	}
	m_normalMapTextures.clear();
	
	DeallocateResource(tempTexture);
	tempTexture = nullptr;
}

void PositionMapPass::ControlPanel()
{
}

void PositionMapPass::Resize(uint32_t width, uint32_t height)
{
}

void PositionMapPass::CreateTempTexture()
{
	tempTexture = Texture::Create(
		posNormMapSize,
		posNormMapSize,
		"tempTexture",
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE
	);
	tempTexture->m_textureType = TextureType::ImageTexture;
	tempTexture->CreateUAV(DXGI_FORMAT_R32G32B32A32_FLOAT);
	tempTexture->CreateSRV(DXGI_FORMAT_R32G32B32A32_FLOAT);
}

/*
 - 권용우
메쉬별로 positionMap 생성.
픽셀셰이더를 사용하므로 viewport와 positionMap의 width, height가 일치하는지 확인하기.
(이거 때문에 사이즈 달라져서 개고생함.)

추가 개선 가능 여부
1. 같은 UV를 사용하는 메쉬의 경우 해당 텍스쳐에서 작업할 수 있도록 수정 할 수 있을거같음.
*/