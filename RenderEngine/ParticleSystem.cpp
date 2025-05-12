#include "ParticleSystem.h"

ParticleSystem::ParticleSystem(int maxParticles) : m_maxParticles(maxParticles), m_isRunning(false)
{
	m_particleData.resize(maxParticles);
	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}

	m_instanceData.resize(maxParticles);
	CreateParticleBuffer(maxParticles);
}

ParticleSystem::~ParticleSystem()
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

	ReleaseBuffers();

	for (auto* module : m_renderModules)
	{
		delete module;
	}
	m_renderModules.clear();
}

void ParticleSystem::Play()
{
	m_isRunning = true;

	m_activeParticleCount = 0;

	for (auto& particle : m_particleData)
	{
		particle.isActive = false;
	}
}

void ParticleSystem::Update(float delta)
{
	if (!m_isRunning || m_isPaused)
		return;

	// 1. 모듈을 순회하기 전에 초기 버퍼 설정
	bool isFirstModule = true;

	// 2. 각 모듈을 순회하면서 버퍼 설정 및 업데이트
	ParticleModule* prevModule = nullptr;
	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it) {
		ParticleModule& module = *it;

		// 3. 현재 모듈의 입출력 버퍼 설정
		ConfigureModuleBuffers(module, isFirstModule);

		// 4. 모듈 업데이트
		module.Update(delta, m_particleData);

		// 5. 다음 모듈을 위해 버퍼 상태 업데이트
		if (!isFirstModule) {
			m_usingBufferA = !m_usingBufferA; // 버퍼 A/B 교대
		}

		isFirstModule = false;
		prevModule = &module;
	}
}

void ParticleSystem::Render(RenderScene& scene, Camera& camera)
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

void ParticleSystem::SetPosition(const Mathf::Vector3& position)
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

void ParticleSystem::CreateParticleBuffer(UINT numParticles)
{
	// 버퍼 설명 초기화
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(ParticleData) * numParticles;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(ParticleData);

	// 초기 데이터 설정
	D3D11_SUBRESOURCE_DATA initialData = {};
	initialData.pSysMem = m_particleData.data();

	// 버퍼 A 생성
	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&bufferDesc, &initialData, &m_particleBufferA);
	if (FAILED(hr)) {
		// 오류 처리
		return;
	}

	// 버퍼 B 생성
	hr = DeviceState::g_pDevice->CreateBuffer(&bufferDesc, &initialData, &m_particleBufferB);
	if (FAILED(hr)) {
		// 오류 처리
		return;
	}

	// UAV 설명 초기화
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numParticles;
	uavDesc.Buffer.Flags = 0;

	// UAV A 생성
	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particleBufferA, &uavDesc, &m_particleUAV_A);
	if (FAILED(hr)) {
		// 오류 처리
		return;
	}

	// UAV B 생성
	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particleBufferB, &uavDesc, &m_particleUAV_B);
	if (FAILED(hr)) {
		// 오류 처리
		return;
	}

	// SRV 설명 초기화
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numParticles;

	// SRV A 생성
	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particleBufferA, &srvDesc, &m_particleSRV_A);
	if (FAILED(hr)) {
		// 오류 처리
		return;
	}

	// SRV B 생성
	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particleBufferB, &srvDesc, &m_particleSRV_B);
	if (FAILED(hr)) {
		// 오류 처리
		return;
	}

	// 초기 상태는 A 버퍼를 사용
	m_usingBufferA = true;
}

void ParticleSystem::ResizeParticleSystem(UINT newMaxParticles)
{
	if (newMaxParticles == m_maxParticles)
		return; // 크기가 같으면 변경 필요 없음

	// 1. 기존 파티클 데이터 백업 (필요시)
	std::vector<ParticleData> oldParticleData;
	if (newMaxParticles > m_maxParticles) {
		// 확장하는 경우: 기존 데이터 보존
		oldParticleData = m_particleData;
	}

	// 2. 기존 리소스 정리
	ReleaseBuffers();

	// 3. 파티클 데이터 벡터 크기 조정
	m_maxParticles = newMaxParticles;
	m_particleData.resize(newMaxParticles);
	m_instanceData.resize(newMaxParticles);

	// 4. 파티클 데이터 초기화
	for (auto& particle : m_particleData) {
		particle.isActive = false;
	}

	// 5. 기존 데이터 복원 (크기 확장 시)
	if (!oldParticleData.empty()) {
		// 기존 파티클 데이터 복사 (활성 파티클만)
		size_t copyCount = std::min(oldParticleData.size(), m_particleData.size());
		for (size_t i = 0; i < copyCount; ++i) {
			if (oldParticleData[i].isActive) {
				m_particleData[i] = oldParticleData[i];
			}
		}
	}

	// 6. 새 버퍼 생성
	CreateParticleBuffer(m_maxParticles);

	// 7. 모듈에 새 크기 알림 (필요시)
	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it) {
		ParticleModule& module = *it;
		module.OnSystemResized(m_maxParticles);
	}

	// 8. 활성 파티클 수 업데이트
	m_activeParticleCount = 0;
	for (const auto& particle : m_particleData) {
		if (particle.isActive) {
			m_activeParticleCount++;
		}
	}
}

void ParticleSystem::ReleaseBuffers()
{
	// 버퍼 A 해제
	if (m_particleBufferA) {
		m_particleBufferA->Release();
		m_particleBufferA = nullptr;
	}

	// 버퍼 B 해제
	if (m_particleBufferB) {
		m_particleBufferB->Release();
		m_particleBufferB = nullptr;
	}

	// UAV A 해제
	if (m_particleUAV_A) {
		m_particleUAV_A->Release();
		m_particleUAV_A = nullptr;
	}

	// UAV B 해제
	if (m_particleUAV_B) {
		m_particleUAV_B->Release();
		m_particleUAV_B = nullptr;
	}

	// SRV A 해제
	if (m_particleSRV_A) {
		m_particleSRV_A->Release();
		m_particleSRV_A = nullptr;
	}

	// SRV B 해제
	if (m_particleSRV_B) {
		m_particleSRV_B->Release();
		m_particleSRV_B = nullptr;
	}
}

void ParticleSystem::ConfigureModuleBuffers(ParticleModule& module, bool isFirstModule)
{
	if (isFirstModule) {
		// 첫 번째 모듈: 이전 프레임의 결과를 입력으로 사용
		// (현재 m_usingBufferA 상태에 따라)
		if (m_usingBufferA) {
			// A가 입력(읽기), B가 출력(쓰기) 버퍼
			module.SetBuffers(
				m_particleUAV_A, m_particleSRV_A,  // 입력
				m_particleUAV_B, m_particleSRV_B   // 출력
			);
		}
		else {
			// B가 입력(읽기), A가 출력(쓰기) 버퍼
			module.SetBuffers(
				m_particleUAV_B, m_particleSRV_B,  // 입력
				m_particleUAV_A, m_particleSRV_A   // 출력
			);
		}
	}
	else {
		// 두 번째 이후 모듈: 이전 모듈의 출력을 입력으로 사용
		// (현재 m_usingBufferA 상태의 반대)
		if (m_usingBufferA) {
			// B가 입력(읽기), A가 출력(쓰기) 버퍼
			module.SetBuffers(
				m_particleUAV_B, m_particleSRV_B,  // 입력
				m_particleUAV_A, m_particleSRV_A   // 출력
			);
		}
		else {
			// A가 입력(읽기), B가 출력(쓰기) 버퍼
			module.SetBuffers(
				m_particleUAV_A, m_particleSRV_A,  // 입력
				m_particleUAV_B, m_particleSRV_B   // 출력
			);
		}
	}
}
