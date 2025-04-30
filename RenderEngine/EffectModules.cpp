#include "EffectModules.h"
#include "Camera.h"
#include "ShaderSystem.h"

float ParticleModule::ApplyEasing(float normalizedTime)
{
	if (!m_useEasing) return normalizedTime;

	int easingIndex = static_cast<int>(m_easingType);
	if (easingIndex >= 0 && easingIndex < static_cast<int>(EasingEffect::EasingEffectEnd)) 
	{
		return EasingFunction[easingIndex](normalizedTime);
	}

	return normalizedTime;
}


void SpawnModule::Initialize()
{
	m_uniform = std::uniform_real_distribution<float>(0.0f, 1.0f);

	m_particleTemplate.age = 0.0f;
	m_particleTemplate.lifeTime = 1.0f;
	m_particleTemplate.rotation = 0.0f;
	m_particleTemplate.rotatespeed = 1.0f;
	m_particleTemplate.size = float2(1.0f, 1.0f);
	m_particleTemplate.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	m_particleTemplate.velocity = float3(0.0f, 0.0f, 0.0f);
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
	particle = m_particleTemplate;

	switch (m_emitterType)
	{
	case EmitterType::point:
		particle.position = float3(0.0f, 0.0f, 0.0f);
		break;
	case EmitterType::sphere:
	{
		float theta = m_uniform(m_random) * XM_2PI;  // 0 ~ 2π (방위각)
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
		float height = m_uniform(m_random) * 1.0f;
		float radiusAtHeight = 0.5f * (1.0f - height);

		particle.position = Mathf::Vector3(
			radiusAtHeight * cos(theta),
			height,  // y축이 원뿔의 높이 방향
			radiusAtHeight * sin(theta)
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
	}


	// 아래로 가속 (중력)
	particle.acceleration = float3(0.0f, -9.8f, 0.0f);
}

void MovementModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			float normalizedAge = particle.age / particle.lifeTime;

			float easingFactor = 1.0f;
			if (IsEasingEnabled())
			{
				easingFactor = ApplyEasing(normalizedAge);
			}

			if (m_gravity)
			{
				// 중력 적용 시 가속도 추가
				particle.velocity += particle.acceleration * m_gravityStrength * delta * easingFactor;
			}

			particle.position += particle.velocity * delta * easingFactor;

			particle.rotation += particle.rotatespeed * delta * easingFactor;
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

			if (particle.lifeTime > 0.0f && particle.age >= particle.lifeTime)
			{
				particle.isActive = false;
			}
		}
	}
}

// 테스트용 이징 컬러만.
void ColorModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			float normalizedAge = particle.age / particle.lifeTime;

			// 이징 함수 적용
			if (IsEasingEnabled())
			{
				// 부모 클래스의 ApplyEasing 메서드 사용
				normalizedAge = ApplyEasing(normalizedAge);
			}

			// 이징이 적용된 normalizedAge로 색상 계산
			particle.color = EvaluateColor(normalizedAge);
		}
	}
}

Mathf::Vector4 ColorModule::EvaluateColor(float t)
{
	switch (m_transitionMode)
	{
	case ColorTransitionMode::Gradient:
		return EvaluateGradient(t);

	case ColorTransitionMode::Discrete:
		return EvaluateDiscrete(t);

	case ColorTransitionMode::Custom:
		if (m_customEvaluator)
			return m_customEvaluator(t);
		return Mathf::Vector4(1, 1, 1, 1); // 기본값

	default:
		return Mathf::Vector4(1, 1, 1, 1); // 기본값
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

Mathf::Vector4 ColorModule::EvaluateDiscrete(float t)
{
	if (m_discreteColors.empty())
		return Mathf::Vector4(1, 1, 1, 1);

	// t값에 따라 해당 인덱스의 색상 반환
	size_t index = static_cast<size_t>(t * m_discreteColors.size());
	index = std::min(index, m_discreteColors.size() - 1);
	return m_discreteColors[index];
}

void SizeModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			float normalizedAge = particle.age / particle.lifeTime;
			if (IsEasingEnabled())
			{
				normalizedAge = ApplyEasing(normalizedAge);
			}
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

EffectModules::EffectModules(int maxParticles) : m_maxParticles(maxParticles), m_isRunning(false)
{
	m_particleData.resize(maxParticles);
	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}

	m_instanceData.resize(maxParticles);

}

EffectModules::~EffectModules()
{
	//auto it = m_moduleList.begin();
	//ParticleModule* current = nullptr;
	//
	//while (it != m_moduleList.end())
	//{
	//	current = &(*it);
	//	++it;
	//	delete current;
	//}
	//
	//m_moduleList.ClearLink();

	for (auto* module : m_renderModules)
	{
		delete module;
	}
	m_renderModules.clear();
}

void EffectModules::Play()
{
	m_isRunning = true;

	m_activeParticleCount = 0;

	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}
}

void EffectModules::Update(float delta)
{
	if (!m_isRunning) // 완전히 정지된 상태
		return;

	if (m_isPaused) // 일시 정지 상태
	{
		m_activeParticleCount = 0;
		for (const auto& particle : m_particleData)
		{
			if (particle.isActive)
			{
				m_activeParticleCount++;
			}
		}
		return;
	}

	//std::cout << m_activeParticleCount << std::endl;

	// 정상 실행 상태
	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it)
	{
		ParticleModule& module = *it;
		module.Update(delta, m_particleData);
	}

	m_activeParticleCount = 0;
	for (const auto& particle : m_particleData)
	{
		if (particle.isActive)
		{
			m_activeParticleCount++;
		}
	}
}

void EffectModules::Render(RenderScene& scene, Camera& camera)
{
	if (!m_isRunning) //|| m_activeParticleCount == 0)
		return;

	auto& deviceContext = DeviceState::g_pDeviceContext;

	//SaveRenderState();

	Mathf::Matrix world = XMMatrixIdentity();
	Mathf::Matrix view = XMMatrixTranspose(camera.CalculateView());
	Mathf::Matrix projection = XMMatrixTranspose(camera.CalculateProjection());

	//for (auto* renderModule : m_renderModules)
	//{
	//	if (auto* billboardModule = dynamic_cast<BillboardModule*>(renderModule))
	//	{
	//		billboardModule->SetupInstancing(m_instanceData.data(), m_activeParticleCount);
	//	}
	//}

	for (auto* renderModule : m_renderModules)
	{
		//if (m_activeParticleCount > 0)
		//{
			// PSO 적용
			renderModule->GetPSO()->Apply();

			// 렌더링 수행
			renderModule->Render(world, view, projection);
		//}
	}

}

void EffectModules::SetPosition(const Mathf::Vector3& position)
{
	m_position = position;
	// SpawnModule은 위치를 직접 저장하지 않으므로, 모든 파티클의 위치를 이동
	for (auto& particle : m_particleData) {
		if (particle.isActive) {
			// 기존 위치에서 새 위치로의 오프셋 적용
			particle.position += position - m_position;
		}
	}
}

void SpawnModuleCS::Initialize()
{
	m_uniform = std::uniform_real_distribution<float>(0.0f, 1.0f);
	m_particleTemplate.age = 0.0f;
	m_particleTemplate.lifeTime = 1.0f;
	m_particleTemplate.rotation = 0.0f;
	m_particleTemplate.rotatespeed = 1.0f;
	m_particleTemplate.size = float2(1.0f, 1.0f);
	m_particleTemplate.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	m_particleTemplate.velocity = float3(0.0f, 0.0f, 0.0f);
	m_particleTemplate.acceleration = float3(0.0f, -9.8f, 0.0f);

	m_lifeTime = 1.0f;
	m_rotateSpeed = 1.0f;
	m_initialSize = float2(1.0f, 1.0f);
	m_initialColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	m_initialAcceleration = float3(0.0f, -9.8f, 0.0f);
	m_minVerticalVelocity = 0.0f;
	m_maxVerticalVelocity = 0.0f;
	m_horizontalVelocityRange = 0.0f;

	m_templateDirty = true;
	m_paramsDirty = true;

	m_computeShader = ShaderSystem->ComputeShaders["SpawnModule"].GetShader();
	InitializeCompute();
}

void SpawnModuleCS::Update(float delta, std::vector<ParticleData>& particles)
{
	m_particlesCapacity = particles.size();

	// 버퍼 크기 확인 및 필요 시 재생성
	D3D11_BUFFER_DESC bufferDesc = {};
	if (m_particlesBufferA)
	{
		m_particlesBufferA->GetDesc(&bufferDesc);
	}

	if (!m_particlesBufferA || particles.size() != bufferDesc.ByteWidth / sizeof(ParticleData))
	{
		// 파티클 버퍼 재생성
		CreateBuffers(particles);
	}

	// 상수 버퍼 업데이트
	UpdateConstantBuffers(delta);

	// 컴퓨트 셰이더 설정
	DeviceState::g_pDeviceContext->CSSetShader(m_computeShader, nullptr, 0);

	// 상수 버퍼 설정
	ID3D11Buffer* constantBuffers[] = { m_spawnParamsBuffer, m_templateBuffer };
	DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 2, constantBuffers);

	// 소스 버퍼(입력)와 대상 버퍼(출력) 설정
	ID3D11UnorderedAccessView* sourceUAV = GetSourceUAV();
	ID3D11UnorderedAccessView* destUAV = GetDestUAV();

	// 셰이더에서 출력으로 사용할 버퍼와 기타 UAV 설정
	ID3D11UnorderedAccessView* uavs[] = { destUAV, m_randomCounterUAV, m_timeUAV, m_spawnCounterUAV };
	UINT initCounts[] = { 0, 0, 0, 0 };

	// 셰이더에서 입력으로 사용할 버퍼 설정 (필요한 경우)
	// ID3D11ShaderResourceView* srvs[] = { GetSourceSRV() };
	// DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, srvs);

	DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 4, uavs, initCounts);

	// 컴퓨트 셰이더 실행
	UINT numThreadGroups = (std::max<UINT>)(1, (static_cast<UINT>(particles.size()) + 255) / 256);
	DeviceState::g_pDeviceContext->Dispatch(numThreadGroups, 1, 1);

	// 리소스 해제
	ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr, nullptr, nullptr };
	DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 4, nullUAVs, nullptr);

	ID3D11Buffer* nullBuffers[] = { nullptr, nullptr };
	DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 2, nullBuffers);

	DeviceState::g_pDeviceContext->CSSetShader(nullptr, nullptr, 0);

	// 버퍼 역할 뒤집기 - 다음 프레임을 위해
	m_usingBufferA = !m_usingBufferA;

	// CPU 읽기는 필요에 따라 디버그 모드에서만 사용
#ifdef _DEBUG
	//ReadBackParticleBuffer(particles);
#endif
}

bool SpawnModuleCS::CreateBuffers(std::vector<ParticleData>& particles)
{
	// 기존 버퍼 해제
	if (m_particlesBufferA) m_particlesBufferA->Release();
	if (m_particlesBufferB) m_particlesBufferB->Release();
	if (m_particlesUAV_A) m_particlesUAV_A->Release();
	if (m_particlesUAV_B) m_particlesUAV_B->Release();
	if (m_particlesStagingBuffer) m_particlesStagingBuffer->Release();
	if (m_particlesSRV_A) m_particlesSRV_A.Reset();
	if (m_particlesSRV_B) m_particlesSRV_B.Reset();

	// 모든 파티클을 비활성 상태로 초기화
	for (auto& particle : particles) {
		particle.isActive = 0;
	}

	// 파티클 버퍼 생성 설정
	D3D11_BUFFER_DESC particleBufferDesc = {};
	particleBufferDesc.ByteWidth = sizeof(ParticleData) * static_cast<UINT>(particles.size());
	particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	particleBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	particleBufferDesc.CPUAccessFlags = 0;
	particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	particleBufferDesc.StructureByteStride = sizeof(ParticleData);

	D3D11_SUBRESOURCE_DATA particleInitData = {};
	particleInitData.pSysMem = particles.data();

	// A 버퍼 생성 (초기 데이터로)
	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&particleBufferDesc, &particleInitData, &m_particlesBufferA);
	if (FAILED(hr))
		return false;

	// B 버퍼 생성 (동일한 초기 데이터로)
	hr = DeviceState::g_pDevice->CreateBuffer(&particleBufferDesc, &particleInitData, &m_particlesBufferB);
	if (FAILED(hr))
		return false;

	// A 버퍼 UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(particles.size());
	uavDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particlesBufferA, &uavDesc, &m_particlesUAV_A);
	if (FAILED(hr))
		return false;

	// B 버퍼 UAV 생성
	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particlesBufferB, &uavDesc, &m_particlesUAV_B);
	if (FAILED(hr))
		return false;

	// A 버퍼 SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(particles.size());

	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particlesBufferA, &srvDesc, m_particlesSRV_A.GetAddressOf());
	if (FAILED(hr))
		return false;

	// B 버퍼 SRV 생성
	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particlesBufferB, &srvDesc, m_particlesSRV_B.GetAddressOf());
	if (FAILED(hr))
		return false;

	// 디버깅 용도를 위한 스테이징 버퍼
	D3D11_BUFFER_DESC stagingDesc = {};
	stagingDesc.ByteWidth = sizeof(ParticleData) * static_cast<UINT>(particles.size());
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	stagingDesc.StructureByteStride = sizeof(ParticleData);

	hr = DeviceState::g_pDevice->CreateBuffer(&stagingDesc, nullptr, &m_particlesStagingBuffer);
	if (FAILED(hr))
		return false;

	// 초기 상태는 A 버퍼 사용
	m_usingBufferA = true;

	return true;
}

void SpawnModuleCS::SetMaxParticles(UINT maxParticles)
{
	if (maxParticles != m_particlesCapacity)
	{
		// 이전 버퍼 정리는 CreateBuffers에서 처리되므로 여기서는 불필요
		// 단순히 새 파티클 벡터 생성 및 초기화
		m_particlesCapacity = maxParticles;
		m_paramsDirty = true;

		// 새 파티클 벡터 생성 및 초기화
		std::vector<ParticleData> newParticles(maxParticles);
		for (auto& particle : newParticles) {
			particle.isActive = 0; // 비활성 상태로 초기화
		}

		// 새 버퍼 생성 - CreateBuffers 내에서 기존 버퍼 해제 및 재생성
		CreateBuffers(newParticles);
	}
}

bool SpawnModuleCS::InitializeCompute()
{
	// 상수 버퍼 생성
	D3D11_BUFFER_DESC spawnParamsDesc = {};
	spawnParamsDesc.ByteWidth = sizeof(SpawnParams);
	spawnParamsDesc.Usage = D3D11_USAGE_DYNAMIC;
	spawnParamsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	spawnParamsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&spawnParamsDesc, nullptr, &m_spawnParamsBuffer);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC templateDesc = {};
	templateDesc.ByteWidth = sizeof(ParticleTemplateParams);
	templateDesc.Usage = D3D11_USAGE_DYNAMIC;
	templateDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	templateDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = DeviceState::g_pDevice->CreateBuffer(&templateDesc, nullptr, &m_templateBuffer);
	if (FAILED(hr))
		return false;

	// 랜덤 카운터 버퍼 생성
	D3D11_BUFFER_DESC counterDesc = {};
	counterDesc.ByteWidth = sizeof(UINT);
	counterDesc.Usage = D3D11_USAGE_DEFAULT;
	counterDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	counterDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	counterDesc.StructureByteStride = sizeof(UINT);

	std::random_device rd;
	UINT initialSeed = rd();
	D3D11_SUBRESOURCE_DATA initialData = {};
	initialData.pSysMem = &initialSeed;

	hr = DeviceState::g_pDevice->CreateBuffer(&counterDesc, &initialData, &m_randomCounterBuffer);
	if (FAILED(hr))
		return false;

	// 랜덤 카운터 UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_randomCounterBuffer, &uavDesc, &m_randomCounterUAV);
	if (FAILED(hr))
		return false;

	// 시간 버퍼 생성 (누적 시간 저장용)
	D3D11_BUFFER_DESC timeDesc = {};
	timeDesc.ByteWidth = sizeof(float);
	timeDesc.Usage = D3D11_USAGE_DEFAULT;
	timeDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	timeDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	timeDesc.StructureByteStride = sizeof(float);

	// 초기 값으로 현재 m_Time 설정
	float initialTime = m_Time;
	D3D11_SUBRESOURCE_DATA timeData = {};
	timeData.pSysMem = &initialTime;

	hr = DeviceState::g_pDevice->CreateBuffer(&timeDesc, &timeData, &m_timeBuffer);
	if (FAILED(hr))
		return false;

	// 시간 버퍼의 UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC timeUAVDesc = {};
	timeUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	timeUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	timeUAVDesc.Buffer.FirstElement = 0;
	timeUAVDesc.Buffer.NumElements = 1;
	timeUAVDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_timeBuffer, &timeUAVDesc, &m_timeUAV);
	if (FAILED(hr))
		return false;


	// 스폰 카운터 버퍼 생성
	D3D11_BUFFER_DESC spawnCounterDesc = {};
	spawnCounterDesc.ByteWidth = sizeof(UINT) * 2; // [0] = 생성할 파티클 수, [1] = 현재까지 생성된 파티클 수
	spawnCounterDesc.Usage = D3D11_USAGE_DEFAULT;
	spawnCounterDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	spawnCounterDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	spawnCounterDesc.StructureByteStride = sizeof(UINT);

	UINT initialSpawnCounter[2] = { 0, 0 };
	D3D11_SUBRESOURCE_DATA spawnCounterData = {};
	spawnCounterData.pSysMem = initialSpawnCounter;

	hr = DeviceState::g_pDevice->CreateBuffer(&spawnCounterDesc, &spawnCounterData, &m_spawnCounterBuffer);
	if (FAILED(hr))
		return false;

	// 스폰 카운터 UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC spawnCounterUAVDesc = {};
	spawnCounterUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	spawnCounterUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	spawnCounterUAVDesc.Buffer.FirstElement = 0;
	spawnCounterUAVDesc.Buffer.NumElements = 2;
	spawnCounterUAVDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_spawnCounterBuffer, &spawnCounterUAVDesc, &m_spawnCounterUAV);
	if (FAILED(hr))
		return false;

	m_isInitialized = true;
	return true;
}

UINT SpawnModuleCS::GetActiveParticleCount() const
{
	return NULL;

	if (!m_particlesBufferA || !m_particlesBufferB || !m_particlesStagingBuffer || m_particlesCapacity == 0)
		return 0; // 유효한 버퍼가 없으면 0 반환

	// 현재 소스 버퍼 가져오기
	ID3D11Buffer* currentBuffer = m_usingBufferA ? m_particlesBufferA : m_particlesBufferB;

	// 벡터를 m_particlesCapacity 크기로 제한
	UINT safeCapacity = m_particlesCapacity;
	std::vector<ParticleData> debugParticles(safeCapacity);
	UINT activeCount = 0;

	// GPU에서 파티클 데이터 읽어오기
	DeviceState::g_pDeviceContext->CopyResource(m_particlesStagingBuffer, currentBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = DeviceState::g_pDeviceContext->Map(m_particlesStagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

	if (SUCCEEDED(hr))
	{
		// 버퍼 크기 확인
		D3D11_BUFFER_DESC stagingDesc;
		m_particlesStagingBuffer->GetDesc(&stagingDesc);
		UINT bufferSize = stagingDesc.ByteWidth;
		UINT elementsInBuffer = bufferSize / sizeof(ParticleData);

		// 복사할 크기 계산 (최소값 사용)
		UINT elementsToRead = std::min(safeCapacity, elementsInBuffer);
		UINT bytesToRead = elementsToRead * sizeof(ParticleData);

		// 안전하게 복사
		if (bytesToRead > 0) {
			memcpy(debugParticles.data(), mappedResource.pData, bytesToRead);
		}

		DeviceState::g_pDeviceContext->Unmap(m_particlesStagingBuffer, 0);

		// 활성 파티클 수 계산
		for (size_t i = 0; i < elementsToRead; ++i)
		{
			if (debugParticles[i].isActive != 0)
			{
				activeCount++;
			}
		}
	}

	return activeCount;
}

void SpawnModuleCS::UpdateConstantBuffers(float delta)
{
	// 스폰 파라미터 업데이트
	if (m_paramsDirty)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = DeviceState::g_pDeviceContext->Map(m_spawnParamsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		if (SUCCEEDED(hr))
		{
			SpawnParams* params = reinterpret_cast<SpawnParams*>(mappedResource.pData);
			params->spawnRate = m_spawnRate; 
			params->deltaTime = delta;
			params->accumulatedTime = m_Time;
			params->emitterType = static_cast<int>(m_emitterType);

			// 이미터 크기 파라미터 설정 (기본값)
			params->emitterSize = float3(1.0f, 1.0f, 1.0f);
			params->emitterRadius = 1.0f;
			params->maxParticles = static_cast<UINT>(m_particlesCapacity);

			DeviceState::g_pDeviceContext->Unmap(m_spawnParamsBuffer, 0);
			m_paramsDirty = false;
		}
	}

	// 파티클 템플릿 업데이트
	if (m_templateDirty)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = DeviceState::g_pDeviceContext->Map(m_templateBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		if (SUCCEEDED(hr))
		{
			ParticleTemplateParams* templateParams = reinterpret_cast<ParticleTemplateParams*>(mappedResource.pData);

			// 파티클 템플릿에서 값 복사
			templateParams->lifeTime = m_particleTemplate.lifeTime;
			templateParams->rotateSpeed = m_particleTemplate.rotatespeed;
			templateParams->size = m_particleTemplate.size;
			templateParams->color = m_particleTemplate.color;
			templateParams->velocity = m_particleTemplate.velocity;
			templateParams->acceleration = m_particleTemplate.acceleration;

			// 추가 속성
			templateParams->minVerticalVelocity = m_minVerticalVelocity;
			templateParams->maxVerticalVelocity = m_maxVerticalVelocity;
			templateParams->horizontalVelocityRange = m_horizontalVelocityRange;

			DeviceState::g_pDeviceContext->Unmap(m_templateBuffer, 0);
			m_templateDirty = false;
		}
	}
}

void SpawnModuleCS::UpdateParticleBuffer(std::vector<ParticleData>& particles)
{
	// 파티클 데이터를 GPU 버퍼로 업데이트
	D3D11_BOX box = {};
	box.left = 0;
	box.right = sizeof(ParticleData) * static_cast<UINT>(particles.size());
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;

	DeviceState::g_pDeviceContext->UpdateSubresource(m_particlesBufferA, 0, &box, particles.data(), 0, 0);
}

void SpawnModuleCS::ReadBackParticleBuffer(std::vector<ParticleData>& particles)
{
	// 현재 소스 버퍼(읽기 버퍼)에서 데이터 읽기
	ID3D11Buffer* currentBuffer = GetSourceBuffer();

	// GPU에서 파티클 데이터 읽어오기
	DeviceState::g_pDeviceContext->CopyResource(m_particlesStagingBuffer, currentBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = DeviceState::g_pDeviceContext->Map(m_particlesStagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

	if (SUCCEEDED(hr))
	{
		// 버퍼 크기 확인
		D3D11_BUFFER_DESC stagingDesc;
		m_particlesStagingBuffer->GetDesc(&stagingDesc);
		UINT bufferSize = stagingDesc.ByteWidth;
		UINT elementsInBuffer = bufferSize / sizeof(ParticleData);

		// 복사할 크기 계산 (최소값 사용)
		UINT elementsToRead = std::min(static_cast<UINT>(particles.size()), elementsInBuffer);
		UINT bytesToRead = elementsToRead * sizeof(ParticleData);

		// 안전하게 복사
		if (bytesToRead > 0) {
			memcpy(particles.data(), mappedResource.pData, bytesToRead);
		}

		DeviceState::g_pDeviceContext->Unmap(m_particlesStagingBuffer, 0);
	}
}

void SpawnModuleCS::Release()
{
	// 리소스 해제
	if (m_computeShader) m_computeShader->Release();
	if (m_spawnParamsBuffer) m_spawnParamsBuffer->Release();
	if (m_templateBuffer) m_templateBuffer->Release();
	if (m_randomCounterBuffer) m_randomCounterBuffer->Release();
	if (m_randomCounterUAV) m_randomCounterUAV->Release();

	// 더블 버퍼링 리소스 해제
	if (m_particlesBufferA) m_particlesBufferA->Release();
	if (m_particlesBufferB) m_particlesBufferB->Release();
	if (m_particlesUAV_A) m_particlesUAV_A->Release();
	if (m_particlesUAV_B) m_particlesUAV_B->Release();
	if (m_particlesStagingBuffer) m_particlesStagingBuffer->Release();
	if (m_particlesSRV_A) m_particlesSRV_A.Reset();
	if (m_particlesSRV_B) m_particlesSRV_B.Reset();

	// 모든 포인터 초기화
	m_computeShader = nullptr;
	m_spawnParamsBuffer = nullptr;
	m_templateBuffer = nullptr;
	m_randomCounterBuffer = nullptr;
	m_randomCounterUAV = nullptr;
	m_particlesBufferA = nullptr;
	m_particlesBufferB = nullptr;
	m_particlesUAV_A = nullptr;
	m_particlesUAV_B = nullptr;
	m_particlesStagingBuffer = nullptr;

	// 기타 리소스 해제
	if (m_timeBuffer) m_timeBuffer->Release();
	if (m_timeUAV) m_timeUAV->Release();
	if (m_spawnCounterBuffer) m_spawnCounterBuffer->Release();
	if (m_spawnCounterUAV) m_spawnCounterUAV->Release();
	m_timeBuffer = nullptr;
	m_timeUAV = nullptr;
	m_spawnCounterBuffer = nullptr;
	m_spawnCounterUAV = nullptr;

	m_isInitialized = false;
}