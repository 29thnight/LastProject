#include "ShadowMapPass.h"
#include "AssetSystem.h"
#include "Scene.h"
#include "Mesh.h"
#include "Sampler.h"
#include "Renderer.h"
#include "Light.h"

ShadowMapPass::ShadowMapPass()
{
	m_pso = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &AssetsSystems->VertexShaders["Shadow"];
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["ShadowMap"];

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

	CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };

	//rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	//rasterizerDesc.CullMode = D3D11_CULL_BACK;
	//rasterizerDesc.DepthBias = 1500;              
	//rasterizerDesc.SlopeScaledDepthBias = 2.0f;   
	//rasterizerDesc.DepthBiasClamp = 0.0f;         

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
 	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);
	

	shadowViewport.TopLeftX = 0;
	shadowViewport.TopLeftY = 0;
	shadowViewport.Width = 1024;
	shadowViewport.Height = 1024;
	shadowViewport.MinDepth = 0.0f;
	shadowViewport.MaxDepth = 1.0f;

	m_shadowBuffer = DirectX11::CreateBuffer(sizeof(ShadowInfo), D3D11_BIND_CONSTANT_BUFFER, nullptr);
}

void ShadowMapPass::Initialize(uint32 width, uint32 height)
{
	Texture* shadowMapTexture = Texture::Create(width, height, "Shadow Map",
		DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
	//shadowMapTexture->CreateRTV(DXGI_FORMAT_R32_FLOAT);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateDepthStencilView(
			shadowMapTexture->m_pTexture,
			&depthStencilViewDesc,
			&shadowMapTexture->m_pDSV
		)
	);
	shadowMapTexture->CreateSRV(DXGI_FORMAT_R32_FLOAT);
	m_shadowMapTexture = std::unique_ptr<Texture>(shadowMapTexture);

	//m_shadowMapDSV = m_shadowMapTexture->m_pDSV;

	Texture* shadowMapTexture2 = Texture::Create(width, height, "Shadow Map2",
		DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
	//shadowMapTexture->CreateRTV(DXGI_FORMAT_R32_FLOAT);
	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc2{};
	depthStencilViewDesc2.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilViewDesc2.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateDepthStencilView(
			shadowMapTexture2->m_pTexture,
			&depthStencilViewDesc2,
			&shadowMapTexture2->m_pDSV
		)
	);
	shadowMapTexture2->CreateSRV(DXGI_FORMAT_R32_FLOAT);

	m_shadowMapTexture2 = std::unique_ptr<Texture>(shadowMapTexture2);
	//m_shadowMapDSV = m_shadowMapTexture->m_pDSV;

	


	m_shadowCamera.m_isOrthographic = true;
	m_shadowCamera2.m_isOrthographic = true;
}

void ShadowMapPass::Execute(RenderScene& scene, Camera& camera)
{


	m_pso->Apply();

	//ID3D11RenderTargetView* rtv = m_shadowMapTexture->GetRTV();
	if (!m_abled)
	{
		DirectX11::ClearDepthStencilView(m_shadowMapTexture->m_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
		return;
	}
	for (int i = 0; i < 2; i++)
	{
		if (i == 0)
		{
			DirectX11::ClearDepthStencilView(m_shadowMapTexture->m_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			DirectX11::OMSetRenderTargets(0, nullptr, m_shadowMapTexture->m_pDSV);
			auto desc = scene.m_LightController->m_shadowMapRenderDesc;
			m_shadowCamera.m_eyePosition = XMLoadFloat4(&(scene.m_LightController->GetLight(0).m_direction)) * -50;
			m_shadowCamera.m_lookAt = desc.m_lookAt;
			m_shadowCamera.m_viewHeight = 50;
			m_shadowCamera.m_viewWidth = 50;
			m_shadowCamera.m_nearPlane = 0.1f;
			m_shadowCamera.m_farPlane =  100.f;
			DeviceState::g_pDeviceContext->RSSetViewports(1, &shadowViewport);
			DirectX11::PSSetShader(NULL, NULL, 0);
			auto& shadowMapConstant = scene.m_LightController->m_shadowMapConstant;

			shadowMapConstant.m_shadowMapWidth = desc.m_textureWidth;
			shadowMapConstant.m_shadowMapHeight = desc.m_textureHeight;
			
			shadowMapConstant.m_lightViewProjection[0] = m_shadowCamera.CalculateView() * m_shadowCamera.CalculateProjection(true);
			scene.m_LightController->m_shadowMapConstant.m_lightViewProjection[0] = shadowMapConstant.m_lightViewProjection[0];
			shadowMapConstant.m_lightViewProjection[1] = scene.m_LightController->m_shadowMapConstant.m_lightViewProjection[1];

			DirectX11::UpdateBuffer(scene.m_LightController->m_shadowMapBuffer, &shadowMapConstant);

			m_shadowCamera.UpdateBuffer(true);
		}
		else if(i == 1)
		{
			DirectX11::ClearDepthStencilView(m_shadowMapTexture2->m_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			DirectX11::OMSetRenderTargets(0, nullptr, m_shadowMapTexture2->m_pDSV);
			auto desc = scene.m_LightController->m_shadowMapRenderDesc;
			m_shadowCamera.m_eyePosition = XMLoadFloat4(&(scene.m_LightController->GetLight(0).m_direction)) * -50;
			m_shadowCamera.m_lookAt = desc.m_lookAt;
			m_shadowCamera.m_viewHeight = 50;
			m_shadowCamera.m_viewWidth = 50;
			m_shadowCamera.m_nearPlane = 0.1f;
			m_shadowCamera.m_farPlane = 200.0f;
			DeviceState::g_pDeviceContext->RSSetViewports(1, &shadowViewport);

			auto& shadowMapConstant = scene.m_LightController->m_shadowMapConstant;

			shadowMapConstant.m_shadowMapWidth = desc.m_textureWidth;
			shadowMapConstant.m_shadowMapHeight = desc.m_textureHeight;
			shadowMapConstant.m_lightViewProjection[1] = m_shadowCamera.CalculateView() * m_shadowCamera.CalculateProjection(true);
			scene.m_LightController->m_shadowMapConstant.m_lightViewProjection[1] = shadowMapConstant.m_lightViewProjection[1];
			shadowMapConstant.m_lightViewProjection[0] = scene.m_LightController->m_shadowMapConstant.m_lightViewProjection[0];
			
			DirectX11::UpdateBuffer(scene.m_LightController->m_shadowMapBuffer, &shadowMapConstant);
	
			m_shadowCamera.UpdateBuffer(true);
		}
		
	
		scene.UseModel();
		//camera.GetFrustum().Intersects();
		for (auto& obj : scene.GetScene()->m_SceneObjects)
		{

			if (obj->ToString() == "Cube")
				continue;
			MeshRenderer* meshRenderer = obj->GetComponent<MeshRenderer>();
			if (nullptr == meshRenderer) continue;
			if (!meshRenderer->IsEnabled()) continue;

			scene.UpdateModel(obj->m_transform.GetWorldMatrix());

			meshRenderer->m_Mesh->Draw();
		}

		//DirectX11::ClearRenderTargetView(rtv, Colors::Transparent);
		DeviceState::g_pDeviceContext->RSSetViewports(1, &DeviceState::g_Viewport);
		DirectX11::UnbindRenderTargets();
	}
}

void ShadowMapPass::ControlPanel()
{
	ImGui::Text("ShadowPass");
	ImGui::Checkbox("Enable2", &m_abled);
	ImGui::Image((ImTextureID)m_shadowMapTexture->m_pSRV, ImVec2(512, 512));
	ImGui::Image((ImTextureID)m_shadowMapTexture2->m_pSRV, ImVec2(512, 512));
}
