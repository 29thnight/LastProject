#include "AtlasEffect.h"

AtlasEffect::AtlasEffect(const float3& position, int maxParticles)
{
	m_position = position;
	{
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.ByteWidth = sizeof(AtlasParameters);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DeviceState::g_pDevice->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
	}

	m_atlasParams = new AtlasParameters;
	m_atlasParams->time = 0.0f;
	m_atlasParams->intensity = 1.0f;
	m_atlasParams->speed = 5.0f;
	m_atlasParams->color = float4(1, 1, 1, 1);
	m_atlasParams->size = float2(256, 256);
	m_atlasParams->range = float2(8, 4);
	SetParameters(m_atlasParams);

	m_atlasTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("파일이름"));
	m_atlasAlphaTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("파일알파"));

}

AtlasEffect::~AtlasEffect()
{
}

void AtlasEffect::InitializeModules()
{
	// 단순 텍스처 렌더하는건데 모듈이 필요한가?
}

void AtlasEffect::Update(float delta)
{
}

void AtlasEffect::Render(RenderScene& scene, Camera& camera)
{
}

void AtlasEffect::UpdateConstantBuffer()
{
}

void AtlasEffect::UpdateInstanceData()
{
}

void AtlasEffect::SetParameters(AtlasParameters* param)
{
}
