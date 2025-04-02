#include "NiagaraEffectManager.h"
#include "Camera.h"
#include "AssetSystem.h"

void SpawnModule::Initialize()
{
	m_uniform = std::uniform_real_distribution<float>(0.0f, 1.0f);
}

void SpawnModule::Update(float delta, std::vector<ParticleData>& particles)
{
	m_Time += delta;

	float spawnInterval = 1.0f / m_spawnRate;
	while (m_Time >= spawnInterval)
	{
		SpawnParticle(particles);
		m_Time -= spawnInterval;
	}

}

void SpawnModule::SpawnParticle(std::vector<ParticleData>& particles)
{
	// 비활성 파티클 재활성화
	for (auto& particle : particles)
	{
		if (!particle.isActive)
		{
			InitializeParticle(particle);
			particle.isActive = true;
			break;
		}
	}
}

void SpawnModule::InitializeParticle(ParticleData& particle)
{
	// 기본 초기화
	particle.age = 0.0f;
	particle.lifeTime = 1.0f; // 1초 수명
	particle.rotation = 0.0f;
	particle.rotatespeed = 1.0f;
	particle.size = float2(1.0f, 1.0f);
	particle.color = float4(1.0f, 1.0f, 1.0f, 1.0f);

	switch (m_emitterType)
	{
	case EmitterType::point:
		particle.position = float3(0.0f, 0.0f, 0.0f);
		break;
	case EmitterType::sphere:
	{
		float theta = m_uniform(m_random) * 6.28f;  // 0 ~ 2π (방위각)
		float phi = m_uniform(m_random) * 3.14f;    // 0 ~ π (고도각)
		float radius = 1.0f;                        // 반지름
		particle.position = float3(
			radius * sin(phi) * cos(theta),
			radius * sin(phi) * sin(theta),  // 수정된 부분
			radius * cos(phi)
		);
		break;
	}
	case EmitterType::box:
	{
		particle.position = Mathf::Vector3(
			(m_uniform(m_random) - 0.5f) * 2.0f,
			(m_uniform(m_random) - 0.5f) * 2.0f,
			(m_uniform(m_random) - 0.5f) * 2.0f
		);
		break;
	}
	case EmitterType::cone:
	{
		float theta = m_uniform(m_random) * 6.28f;
		float radius = m_uniform(m_random) * 0.5f;
		particle.position = Mathf::Vector3(
			radius * cos(theta),
			0.0f,
			radius * sin(theta)
		);
		break;
	}
	case EmitterType::circle:
	{
		float theta = m_uniform(m_random) * 6.28f;
		float radius = 0.5f + m_uniform(m_random) * 0.5f;
		particle.position = Mathf::Vector3(
			radius * cos(theta),
			0.0f,
			radius * sin(theta)
		);
		break;
	}
	} // switch 문 끝

	// 모든 emitterType에 대해 실행되는 코드
	particle.velocity = float3(
		(m_uniform(m_random) - 0.5f) * 2.0f,
		1.0f + m_uniform(m_random) * 2.0f,
		(m_uniform(m_random) - 0.5f) * 2.0f
	);

	// 아래로 가속 (중력)
	particle.acceleration = float3(0.0f, -9.8f, 0.0f);
}

void MovementModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			if (m_gravity)
			{
				// 중력 적용 시 가속도 추가
				particle.velocity += particle.acceleration * m_gravityStrength * delta;
			}

			particle.position += particle.velocity * delta;

			particle.rotation += particle.rotatespeed * delta;
		}
	}
}

void LifeModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			particle.age += delta;

			if (particle.age >= particle.lifeTime)
			{
				particle.isActive = false;
			}
		}
	}
}

void ColorModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			float normalizedAge = particle.age / particle.lifeTime;
			particle.color = EvaluateGradient(normalizedAge);
		}
	}
}

Mathf::Vector4 ColorModule::EvaluateGradient(float t)
{
	// 그라데이션에 적절한 색 평가
	if (t <= m_colorGradient[0].first)
		return m_colorGradient[0].second;

	if (t >= m_colorGradient.back().first)
		return m_colorGradient.back().second;

	for (size_t i = 0; i < m_colorGradient.size() - 1; i++)
	{
		if (t >= m_colorGradient[i].first && t <= m_colorGradient[i + 1].first)
		{
			float localT = (t - m_colorGradient[i].first) /
				(m_colorGradient[i + 1].first - m_colorGradient[i].first);

			return Mathf::Vector4::Lerp(
				m_colorGradient[i].second,
				m_colorGradient[i + 1].second,
				localT
			);
		}
	}
	return m_colorGradient[0].second;
}

void SizeModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			float normalizedAge = particle.age / particle.lifeTime;
			particle.size = m_sizeOverLife(normalizedAge);
		}
	}
}

void CollisionModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			// 충돌 검사
			if (particle.position.y < m_floorHeight && particle.velocity.y < 0.0f)
			{
				// 반사
				particle.position.y = m_floorHeight;
				particle.velocity.y = -particle.velocity.y * m_bounceFactor;

				// 마찰력
				particle.velocity.x *= 0.9f;
				particle.velocity.z *= 0.9f;
			}
		}
	}
}

NiagaraEffectManager::NiagaraEffectManager(int maxParticles) : m_maxParticles(maxParticles), m_isRunning(false)
{
	m_particleData.resize(maxParticles);
	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}

	BillboardVertex vertex;
	vertex.Position = Mathf::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	CreateBillboard(&vertex, BillBoardType::Basic);

	m_instanceData.resize(maxParticles);

	// 기본 ps
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["ParticleDefault"];
}

NiagaraEffectManager::~NiagaraEffectManager()
{
	for (auto& module : m_modules)
	{
		delete module;
	}
	m_modules.clear();
}

void NiagaraEffectManager::Play()
{
	m_isRunning = true;

	m_activeParticleCount = 0;

	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}
}

void NiagaraEffectManager::Update(float delta)
{
	if (!m_isRunning)
		return;

	for (auto* module : m_modules)
	{
		module->Update(delta, m_particleData);
	}

	m_activeParticleCount = 0;
	for (const auto& particle : m_particleData)
	{
		if (particle.isActive)
		{
			m_activeParticleCount++;
		}
	}

	UpdateInstanceData();
}

void NiagaraEffectManager::Render(Scene& scene, Camera& camera)
{
	if (!m_isRunning || m_activeParticleCount == 0)
		return;

	SetupBillBoardInstancing(m_instanceData.data(), m_activeParticleCount);

	auto& deviceContext = DeviceState::g_pDeviceContext;
	deviceContext->OMSetBlendState(m_pso->m_blendState, nullptr, 0xffffffff);
	deviceContext->RSSetState(m_pso->m_rasterizerState);
	deviceContext->OMSetDepthStencilState(m_pso->m_depthStencilState, 0);
	deviceContext->IASetPrimitiveTopology(m_pso->m_primitiveTopology);
	deviceContext->IASetInputLayout(m_pso->m_inputLayout);
	deviceContext->VSSetShader(m_pso->m_vertexShader->GetShader(), nullptr, 0);
	deviceContext->GSSetShader(m_pso->m_geometryShader->GetShader(), nullptr, 0);
	deviceContext->PSSetShader(m_pso->m_pixelShader->GetShader(), nullptr, 0);

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

	deviceContext->PSSetShaderResources(0, 1, &m_particleTexture->m_pSRV);

	DrawBillboard(XMMatrixIdentity(),
		XMMatrixTranspose(camera.CalculateView()),
		XMMatrixTranspose(camera.CalculateProjection()));
}

void NiagaraEffectManager::UpdateInstanceData()
{
	m_instanceData.resize(m_activeParticleCount);

	int instanceIndex = 0;
	for (const auto& particle : m_particleData)
	{
		if (particle.isActive)
		{
			m_instanceData[instanceIndex].position = Mathf::Vector4(
				particle.position.x,
				particle.position.y,
				particle.position.z,
				1.0
			);

			m_instanceData[instanceIndex].size = particle.size;

			m_instanceData[instanceIndex].id = static_cast<UINT>(particle.age / particle.lifeTime * 10) % 10;

			instanceIndex++;
		}
	}

	 if (m_activeParticleCount > 0)
    {
        SetupBillBoardInstancing(m_instanceData.data(), m_activeParticleCount);
    }
}

void NiagaraEffectManager::PrepareBillboards()
{
	if (m_activeParticleCount > 0)
	{
		SetupBillBoardInstancing(m_instanceData.data(), m_activeParticleCount);
	}
}

void NiagaraEffectManager::SetupRenderState(Camera& camera)
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	deviceContext->OMSetBlendState(m_pso->m_blendState, nullptr, 0xffffffff);
	deviceContext->RSSetState(m_pso->m_rasterizerState);
	deviceContext->OMSetDepthStencilState(m_pso->m_depthStencilState, 0);

	deviceContext->IASetPrimitiveTopology(m_pso->m_primitiveTopology);
	deviceContext->IASetInputLayout(m_pso->m_inputLayout);

	deviceContext->VSSetShader(m_pso->m_vertexShader->GetShader(), nullptr, 0);
	deviceContext->GSSetShader(m_pso->m_geometryShader->GetShader(), nullptr, 0);
	deviceContext->PSSetShader(m_pso->m_pixelShader->GetShader(), nullptr, 0);

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

	deviceContext->PSSetShaderResources(0, 1, &m_particleTexture->m_pSRV);
}

void NiagaraEffectManager::RenderBillboards(Camera& camera)
{
	if (m_activeParticleCount > 0)
	{
		DrawBillboard(
			XMMatrixIdentity(),
			XMMatrixTranspose(camera.CalculateView()),
			XMMatrixTranspose(camera.CalculateProjection())
		);
	}
}

void NiagaraEffectManager::CleanupRenderState()
{
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DeviceState::g_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

	DeviceState::g_pDeviceContext->GSSetShader(nullptr, nullptr, 0);
}

void NiagaraEffectManager::SaveRenderState()
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	if (m_prevDepthState) m_prevDepthState->Release();
	if (m_prevBlendState) m_prevBlendState->Release();
	if (m_prevRasterizerState) m_prevRasterizerState->Release();

	deviceContext->OMGetDepthStencilState(&m_prevDepthState, &m_prevStencilRef);
	deviceContext->OMGetBlendState(&m_prevBlendState, m_prevBlendFactor, &m_prevSampleMask);
	deviceContext->RSGetState(&m_prevRasterizerState);
}

void NiagaraEffectManager::RestoreRenderState()
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	deviceContext->OMSetDepthStencilState(m_prevDepthState, m_prevStencilRef);
	deviceContext->OMSetBlendState(m_prevBlendState, m_prevBlendFactor, m_prevSampleMask);
	deviceContext->RSSetState(m_prevRasterizerState);

	DirectX11::UnbindRenderTargets();
}
