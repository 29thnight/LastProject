#include "SSAOPass.h"
#include "ShaderSystem.h"
#include "../EngineEntry/RenderPassSettings.h"
#include "Scene.h"
#include <random>

SSAOPass::SSAOPass()
{
    m_pso = std::make_unique<PipelineStateObject>();
    m_pso->m_vertexShader = &ShaderSystem->VertexShaders["Fullscreen"];
    m_pso->m_pixelShader = &ShaderSystem->PixelShaders["SSAO"];
    m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    m_Buffer = DirectX11::CreateBuffer(sizeof(SSAOBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);

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
    CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };

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
}

SSAOPass::~SSAOPass()
{
}

void SSAOPass::Initialize(Texture* renderTarget, ID3D11ShaderResourceView* depth, Texture* normal, Texture* diffuse)
{
	m_DepthSRV = depth;
	m_NormalTexture = normal;
	m_RenderTarget = renderTarget;
    m_DiffuseTexture = diffuse;

	
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between 0.0 - 1.0
    std::default_random_engine generator;

    for (int i = 0; i < 64; ++i)
    {
        XMVECTOR sample = XMVectorSet(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator),
            0
        );
        sample = XMVector3Normalize(sample);
        float scale = (float)i / 64.0;
        // to lerp
        XMVectorScale(sample, scale);

        XMStoreFloat4(&m_SSAOBuffer.m_SampleKernel[i], sample);
    }

    using namespace DirectX::PackedVector;
    std::vector<XMBYTEN4> rotation;
    rotation.reserve(16);
    for (int i = 0; i < 16; ++i)
    {
        XMVECTOR rot = XMVectorSet(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0,
            0
        );
        XMBYTEN4 rotbyte;
        XMStoreByteN4(&rotbyte, rot);
        rotation.push_back(rotbyte);
    }

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = rotation.data();
    data.SysMemPitch = 4 * 4; // 4 pixels width, 4 bytes (32 bits)
    Texture* tex = Texture::Create(4, 4, "Noise Tex", DXGI_FORMAT_R8G8B8A8_SNORM, D3D11_BIND_SHADER_RESOURCE, &data);
    tex->CreateSRV(DXGI_FORMAT_R8G8B8A8_SNORM);
    m_NoiseTexture = MakeUniqueTexturePtr(tex);
}

void SSAOPass::ReloadDSV(ID3D11ShaderResourceView* depth)
{
    m_DepthSRV = depth;
}

void SSAOPass::Execute(RenderScene& scene, Camera& camera)
{
    if (!RenderPassData::VaildCheck(&camera)) return;
    auto renderData = RenderPassData::GetData(&camera);

    m_pso->Apply();

	DirectX11::ClearRenderTargetView(m_RenderTarget->GetRTV(), Colors::Transparent);

	ID3D11RenderTargetView* rtv = m_RenderTarget->GetRTV();
    DirectX11::OMSetRenderTargets(1, &rtv, nullptr);

	Mathf::xMatrix view = camera.CalculateView();
	Mathf::xMatrix proj = camera.CalculateProjection();
    m_SSAOBuffer.m_ViewProjection = XMMatrixMultiply(view, proj);
    m_SSAOBuffer.m_InverseViewProjection = XMMatrixInverse(nullptr, m_SSAOBuffer.m_ViewProjection);
    m_SSAOBuffer.m_InverseProjection = camera.CalculateInverseProjection();
    m_SSAOBuffer.m_CameraPosition = camera.m_eyePosition;
    m_SSAOBuffer.m_Radius = radius;
    m_SSAOBuffer.m_Thickness = thickness;
    m_SSAOBuffer.m_windowSize = { (float)DeviceState::g_ClientRect.width, (float)DeviceState::g_ClientRect.height };
    m_SSAOBuffer.m_frameIndex = Time->GetFrameCount();

    DirectX11::UpdateBuffer(m_Buffer.Get(), &m_SSAOBuffer);

    DirectX11::PSSetConstantBuffer(3, 1, m_Buffer.GetAddressOf());

    ID3D11ShaderResourceView* srvs[4] = { 
        renderData->m_depthStencil->m_pSRV, 
        m_NormalTexture->m_pSRV, 
        m_NoiseTexture->m_pSRV, 
        m_DiffuseTexture->m_pSRV 
    };
    DirectX11::PSSetShaderResources(0, 4, srvs);

    DirectX11::Draw(4, 0);

    ID3D11ShaderResourceView* nullSRV[4] = { nullptr, nullptr, nullptr, nullptr };
    DirectX11::PSSetShaderResources(0, 4, nullSRV);
    DirectX11::UnbindRenderTargets();
}

void SSAOPass::ControlPanel()
{
    ImGui::PushID(this);
    ImGui::Text("SSAO");
    ImGui::SliderFloat("Radius", &radius, 0.0f, 1.0f);
    ImGui::SliderFloat("Thickness", &thickness, 0.0f, 1.0f);
    ImGui::PopID();
}

void SSAOPass::Resize(uint32_t width, uint32_t height)
{
}

void SSAOPass::ApplySettings(const SSAOPassSetting& setting)
{
    radius = setting.radius;
    thickness = setting.thickness;
}
