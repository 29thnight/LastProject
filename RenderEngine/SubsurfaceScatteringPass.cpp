#include "SubsurfaceScatteringPass.h"
#include "ShaderSystem.h"
#include "Scene.h"
#include "RenderScene.h"
#include "LightController.h"
#include "TimeSystem.h"

struct alignas(16) SubsurfaceScatteringBuffer
{
	float2 direction;
	float strength;
	float width;
	float CameraFOV;
};

SubsurfaceScatteringPass::SubsurfaceScatteringPass()
{
	m_pso = std::make_unique<PipelineStateObject>();

	m_pso->m_vertexShader = &ShaderSystem->VertexShaders["Fullscreen"];
	m_pso->m_pixelShader = &ShaderSystem->PixelShaders["SSS"];
	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

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

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);
	m_Buffer = DirectX11::CreateBuffer(sizeof(SubsurfaceScatteringBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);
}

SubsurfaceScatteringPass::~SubsurfaceScatteringPass()
{
}

void SubsurfaceScatteringPass::Initialize(Texture* diffuse, Texture* metalRough)
{
	m_DiffuseTexture = diffuse;
	m_MetalRoughTexture = metalRough;

	m_CopiedTexture = Texture::Create(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"CopiedTexture",
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET
	);
	m_CopiedTexture->CreateRTV(DXGI_FORMAT_R16G16B16A16_FLOAT);
	m_CopiedTexture->CreateSRV(DXGI_FORMAT_R16G16B16A16_FLOAT);
}

void SubsurfaceScatteringPass::Execute(RenderScene& scene, Camera& camera)
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

void SubsurfaceScatteringPass::CreateRenderCommandList(ID3D11DeviceContext* defferdContext, RenderScene& scene, Camera& camera)
{
	if (!isOn) return;
	if (!RenderPassData::VaildCheck(&camera)) return;
	auto renderData = RenderPassData::GetData(&camera);

	ID3D11DeviceContext* defferdPtr = defferdContext;

	m_pso->Apply(defferdPtr);
	ID3D11RenderTargetView* view = renderData->m_renderTarget->GetRTV();
	DirectX11::OMSetRenderTargets(defferdPtr, 1, &view, nullptr);
	DirectX11::RSSetViewports(defferdPtr, 1, &DeviceState::g_Viewport);
	DirectX11::PSSetConstantBuffer(defferdPtr, 0, 1, m_Buffer.GetAddressOf());

	camera.UpdateBuffer(defferdPtr);

	SubsurfaceScatteringBuffer buffer{};
	buffer.CameraFOV	= camera.m_fov;
	buffer.strength		= strength;
	buffer.width		= width;
	buffer.direction	= direction;

	DirectX11::UpdateBuffer(defferdPtr, m_Buffer.Get(), &buffer);

	DirectX11::CopyResource(defferdPtr, m_CopiedTexture->m_pTexture, renderData->m_renderTarget->m_pTexture);

	ID3D11ShaderResourceView* srvs[3] = {
		renderData->m_depthStencil->m_pSRV,
		m_CopiedTexture->m_pSRV,
		m_MetalRoughTexture->m_pSRV,
	};
	DirectX11::PSSetShaderResources(defferdPtr, 0, 3, srvs);

	DirectX11::Draw(defferdPtr, 4, 0);

	ID3D11ShaderResourceView* nullSRV[3] = {
		nullptr,
		nullptr,
		nullptr
	};
	DirectX11::PSSetShaderResources(defferdPtr, 0, 3, nullSRV);
	DirectX11::UnbindRenderTargets(defferdPtr);

	ID3D11CommandList* commandList{};
	defferdPtr->FinishCommandList(false, &commandList);
	PushQueue(camera.m_cameraIndex, commandList);
}

void SubsurfaceScatteringPass::ControlPanel()
{
	ImGui::PushID(this);
	ImGui::Checkbox("Enable Subsurface Scattering", &isOn);
	ImGui::SliderFloat2("Direction", &direction.x, -1.f, 1.f);
	ImGui::SliderFloat("Strength", &strength, 0.f, 1.f);
	ImGui::SliderFloat("Width", &width, 0.f, 1.f);
	ImGui::PopID();
}