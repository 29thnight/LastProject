#include "EffectSystem.h"
#include "Camera.h"
#include "ShaderSystem.h"

EffectSystem::EffectSystem(int maxParticles) : m_maxParticles(maxParticles), m_isRunning(false)
{
	m_particleData.resize(maxParticles);
	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}
	m_instanceData.resize(maxParticles);

	// 버퍼 초기화
	InitializeBuffers();

}

EffectSystem::~EffectSystem()
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

	if (m_particlesBufferA) m_particlesBufferA->Release();
	if (m_particlesBufferB) m_particlesBufferB->Release();
	if (m_particlesUAV_A) m_particlesUAV_A->Release();
	if (m_particlesUAV_B) m_particlesUAV_B->Release();
	if (m_particlesSRV_A) m_particlesSRV_A->Release();
	if (m_particlesSRV_B) m_particlesSRV_B->Release();
}

void EffectSystem::Play()
{
	m_isRunning = true;

	m_activeParticleCount = 0;

	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}
}

bool EffectSystem::InitializeBuffers()
{
	// 기존 버퍼 해제
	if (m_particlesBufferA) m_particlesBufferA->Release();
	if (m_particlesBufferB) m_particlesBufferB->Release();
	if (m_particlesUAV_A) m_particlesUAV_A->Release();
	if (m_particlesUAV_B) m_particlesUAV_B->Release();
	if (m_particlesSRV_A) m_particlesSRV_A->Release();
	if (m_particlesSRV_B) m_particlesSRV_B->Release();

	// 파티클 버퍼 생성 설정
	D3D11_BUFFER_DESC particleBufferDesc = {};
	particleBufferDesc.ByteWidth = sizeof(ParticleData) * static_cast<UINT>(m_particleData.size());
	particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	particleBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	particleBufferDesc.CPUAccessFlags = 0;
	particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	particleBufferDesc.StructureByteStride = sizeof(ParticleData);

	D3D11_SUBRESOURCE_DATA particleInitData = {};
	particleInitData.pSysMem = m_particleData.data();

	// A 버퍼 생성
	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&particleBufferDesc, &particleInitData, &m_particlesBufferA);
	if (FAILED(hr))
		return false;

	// B 버퍼 생성
	hr = DeviceState::g_pDevice->CreateBuffer(&particleBufferDesc, &particleInitData, &m_particlesBufferB);
	if (FAILED(hr))
		return false;

	// UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(m_particleData.size());
	uavDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particlesBufferA, &uavDesc, &m_particlesUAV_A);
	if (FAILED(hr))
		return false;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particlesBufferB, &uavDesc, &m_particlesUAV_B);
	if (FAILED(hr))
		return false;

	// SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(m_particleData.size());

	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particlesBufferA, &srvDesc, &m_particlesSRV_A);
	if (FAILED(hr))
		return false;

	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particlesBufferB, &srvDesc, &m_particlesSRV_B);
	if (FAILED(hr))
		return false;

	// 초기 상태는 A 버퍼 사용
	m_usingBufferA = true;

	return true;
}

void EffectSystem::Update(float delta)
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

	// 정상 실행 상태
	int moduleIndex = 0;

	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it, ++moduleIndex)
	{
		ParticleModule& module = *it;

		// 모듈에 따라 다른 버퍼 설정
		if (moduleIndex == 0) {
			// 첫 번째 모듈 (SpawnModule)
			if (m_usingBufferA) {
				// A 버퍼가 입력, B 버퍼가 출력
				module.SetParticlesSRV(m_particlesSRV_A);
				module.SetParticlesUAV(m_particlesUAV_B);
			}
			else {
				// B 버퍼가 입력, A 버퍼가 출력
				module.SetParticlesSRV(m_particlesSRV_B);
				module.SetParticlesUAV(m_particlesUAV_A);
			}
		}
		else {
			// 두 번째 이후 모듈들 - 이전 모듈의 출력이 이 모듈의 입력이 됨
			// 여기서 핑퐁(ping-pong) 패턴을 적용합니다
			if (moduleIndex % 2 == 1) { // 홀수 인덱스 모듈
				if (m_usingBufferA) {
					// B 버퍼가 입력(이전 모듈의 출력), A 버퍼가 출력
					module.SetParticlesSRV(m_particlesSRV_B);
					module.SetParticlesUAV(m_particlesUAV_A);
				}
				else {
					// A 버퍼가 입력, B 버퍼가 출력
					module.SetParticlesSRV(m_particlesSRV_A);
					module.SetParticlesUAV(m_particlesUAV_B);
				}
			}
			else { // 짝수 인덱스 모듈
				if (m_usingBufferA) {
					// A 버퍼가 입력, B 버퍼가 출력
					module.SetParticlesSRV(m_particlesSRV_A);
					module.SetParticlesUAV(m_particlesUAV_B);
				}
				else {
					// B 버퍼가 입력, A 버퍼가 출력
					module.SetParticlesSRV(m_particlesSRV_B);
					module.SetParticlesUAV(m_particlesUAV_A);
				}
			}
		}

		// 모듈 업데이트
		module.Update(delta, m_particleData);
	}

	// 마지막 모듈의 결과를 CPU에 다시 읽어와 m_particleData 업데이트 (필요한 경우)
	// ReadbackParticleBuffer();

	// 다음 프레임을 위해 버퍼 교환
	SwapBuffers();

	// 활성 파티클 카운트 업데이트
	m_activeParticleCount = 0;
	for (const auto& particle : m_particleData)
	{
		if (particle.isActive)
		{
			m_activeParticleCount++;
		}
	}
}

void EffectSystem::Render(RenderScene& scene, Camera& camera)
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
	ID3D11ShaderResourceView* activeSRV = m_usingBufferA ? m_particlesSRV_B : m_particlesSRV_A;

	for (auto* renderModule : m_renderModules)
	{
		renderModule->SetSRV(activeSRV);

		//if (m_activeParticleCount > 0)
		//{
			// PSO 적용
		
		renderModule->GetPSO()->Apply();

		// 렌더링 수행
		renderModule->Render(world, view, projection);
		//}
	}

}

void EffectSystem::SetPosition(const Mathf::Vector3& position)
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

