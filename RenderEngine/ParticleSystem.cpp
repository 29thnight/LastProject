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
	CreateSharedBuffers();
	InitializeParticleIndices();
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
	ReleaseSharedBuffer();

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
		particle.isActive = 0;
	}
	InitializeParticleIndices();
}

void ParticleSystem::Update(float delta)
{
	if (!m_isRunning || m_isPaused)
		return;

	// 활성 카운터 초기화
	UINT zero = 0;
	DeviceState::g_pDeviceContext->ClearUnorderedAccessViewUint(m_activeCountUAV, &zero);

	// 모듈 순서: 1) SpawnModule 2) LifeModule (순서 변경됨)
	bool currentBuffer = m_usingBufferA;

	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it) {
		ParticleModule& module = *it;

		// 현재 버퍼 상태에 따라 입력/출력 설정
		if (currentBuffer) {
			// A가 입력, B가 출력
			module.SetBuffers(m_particleUAV_A, m_particleSRV_A, m_particleUAV_B, m_particleSRV_B);
		}
		else {
			// B가 입력, A가 출력
			module.SetBuffers(m_particleUAV_B, m_particleSRV_B, m_particleUAV_A, m_particleSRV_A);
		}

		// 모듈 유형에 따라 적절한 버퍼 설정 (동일한 인덱스 버퍼 사용)
		if (auto* lifeModule = dynamic_cast<LifeModuleCS*>(&module)) {
			lifeModule->SetSharedBuffers(m_inactiveIndicesUAV, m_inactiveCountUAV, m_activeCountUAV);
		}
		else if (auto* spawnModule = dynamic_cast<SpawnModuleCS*>(&module)) {
			spawnModule->SetSharedBuffers(m_inactiveIndicesUAV, m_inactiveCountUAV, m_activeCountUAV);
		}

		// 모듈 업데이트 실행
		module.Update(delta, m_particleData);

		// 버퍼 상태 전환
		currentBuffer = !currentBuffer;
	}

	// 최종 버퍼 상태 저장
	m_usingBufferA = currentBuffer;

	// 인덱스 버퍼 교체 작업 제거됨 (더 이상 필요 없음)
	// SwapIndexBuffer();

	// 활성 파티클 수 읽기
	m_activeParticleCount = ReadActiveParticleCount();
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

UINT ParticleSystem::ReadActiveParticleCount()
{
	// GPU 버퍼에서 활성 파티클 수 읽기
	UINT count = 0;

	// 활성 카운터 버퍼에 대한 스테이징 버퍼 생성 (CPU 읽기 가능)
	ID3D11Buffer* stagingBuffer = nullptr;
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(UINT);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.StructureByteStride = sizeof(UINT);

	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&desc, nullptr, &stagingBuffer);
	if (FAILED(hr))
	{
		return 0; // 버퍼 생성 실패 시 0 반환
	}

	// GPU 버퍼에서 스테이징 버퍼로 데이터 복사
	DeviceState::g_pDeviceContext->CopyResource(stagingBuffer, m_activeCountBuffer);

	// 스테이징 버퍼 매핑하여 CPU에서 읽기
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = DeviceState::g_pDeviceContext->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		// 데이터 읽기
		count = *(UINT*)mappedResource.pData;

		// 버퍼 언매핑
		DeviceState::g_pDeviceContext->Unmap(stagingBuffer, 0);
	}

	// 스테이징 버퍼 해제
	if (stagingBuffer)
	{
		stagingBuffer->Release();
	}

	return count;
}

void ParticleSystem::CreateSharedBuffers()
{
	// 비활성 파티클 인덱스 버퍼 (모든 파티클 수만큼)
	D3D11_BUFFER_DESC inactiveIndexDesc = {};
	inactiveIndexDesc.ByteWidth = sizeof(UINT) * m_maxParticles;
	inactiveIndexDesc.Usage = D3D11_USAGE_DEFAULT;
	inactiveIndexDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	inactiveIndexDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	inactiveIndexDesc.StructureByteStride = sizeof(UINT);

	// 초기 인덱스 데이터는 초기화 모듈에서 설정하므로 여기서는 생략 가능
	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&inactiveIndexDesc, nullptr, &m_inactiveIndicesBuffer);
	if (FAILED(hr))
		return; // 오류 처리

	// 비활성 카운터 버퍼
	D3D11_BUFFER_DESC countDesc = {};
	countDesc.ByteWidth = sizeof(UINT);
	countDesc.Usage = D3D11_USAGE_DEFAULT;
	countDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	countDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	countDesc.StructureByteStride = sizeof(UINT);

	hr = DeviceState::g_pDevice->CreateBuffer(&countDesc, nullptr, &m_inactiveCountBuffer);
	if (FAILED(hr))
		return; // 오류 처리

	// 활성 파티클 카운터 버퍼
	hr = DeviceState::g_pDevice->CreateBuffer(&countDesc, nullptr, &m_activeCountBuffer);
	if (FAILED(hr))
		return; // 오류 처리

	// UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC inactiveIndexUAVDesc = {};
	inactiveIndexUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	inactiveIndexUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	inactiveIndexUAVDesc.Buffer.FirstElement = 0;
	inactiveIndexUAVDesc.Buffer.NumElements = m_maxParticles;
	inactiveIndexUAVDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_inactiveIndicesBuffer, &inactiveIndexUAVDesc, &m_inactiveIndicesUAV);
	if (FAILED(hr))
		return; // 오류 처리

	D3D11_UNORDERED_ACCESS_VIEW_DESC countUAVDesc = {};
	countUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	countUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	countUAVDesc.Buffer.FirstElement = 0;
	countUAVDesc.Buffer.NumElements = 1;
	countUAVDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_inactiveCountBuffer, &countUAVDesc, &m_inactiveCountUAV);
	if (FAILED(hr))
		return; // 오류 처리

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_activeCountBuffer, &countUAVDesc, &m_activeCountUAV);
	if (FAILED(hr))
		return; // 오류 처리

}

void ParticleSystem::ReleaseSharedBuffer()
{
	if (m_inactiveIndicesBuffer) { m_inactiveIndicesBuffer->Release(); m_inactiveIndicesBuffer = nullptr; }
	if (m_inactiveCountBuffer) { m_inactiveCountBuffer->Release(); m_inactiveCountBuffer = nullptr; }
	if (m_activeCountBuffer) { m_activeCountBuffer->Release(); m_activeCountBuffer = nullptr; }

	if (m_inactiveIndicesUAV) { m_inactiveIndicesUAV->Release(); m_inactiveIndicesUAV = nullptr; }
	if (m_inactiveCountUAV) { m_inactiveCountUAV->Release(); m_inactiveCountUAV = nullptr; }
	if (m_activeCountUAV) { m_activeCountUAV->Release(); m_activeCountUAV = nullptr; }
}

void ParticleSystem::InitializeParticleIndices()
{
	// GPU에서 초기화 수행
	// 현재 사용 중인 버퍼를 기준으로 UAV 선택
	ID3D11UnorderedAccessView* particleUAV = m_usingBufferA ? m_particleUAV_A : m_particleUAV_B;

	// 초기화 모듈 실행
	m_initializeModule.Initialize(
		particleUAV,
		m_inactiveIndicesUAV,
		m_inactiveCountUAV,
		m_activeCountUAV,
		m_maxParticles
	);

	// CPU 측 파티클 데이터도 비활성화
	for (auto& particle : m_particleData) {
		particle.isActive = 0;
	}

	// CPU 측 활성 파티클 카운트도 초기화
	m_activeParticleCount = 0;
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
	ReleaseSharedBuffer();

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
	CreateSharedBuffers();

	InitializeParticleIndices();

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
	if (m_inactiveIndicesBuffer) { m_inactiveIndicesBuffer->Release(); m_inactiveIndicesBuffer = nullptr; }
	if (m_inactiveCountBuffer) { m_inactiveCountBuffer->Release(); m_inactiveCountBuffer = nullptr; }
	if (m_activeCountBuffer) { m_activeCountBuffer->Release(); m_activeCountBuffer = nullptr; }

	if (m_inactiveIndicesUAV) { m_inactiveIndicesUAV->Release(); m_inactiveIndicesUAV = nullptr; }
	if (m_inactiveCountUAV) { m_inactiveCountUAV->Release(); m_inactiveCountUAV = nullptr; }
	if (m_activeCountUAV) { m_activeCountUAV->Release(); m_activeCountUAV = nullptr; }
}

ID3D11ShaderResourceView* ParticleSystem::GetCurrentRenderingSRV() const
{
	bool finalIsBufferA = m_usingBufferA;
	if (m_moduleList.size() % 2 == 1) {
		// 모듈 개수가 홀수이면 반전
		finalIsBufferA = !finalIsBufferA;
	}

	return finalIsBufferA ? m_particleSRV_A : m_particleSRV_B;
}

void ParticleSystem::ConfigureModuleBuffers(ParticleModule& module, bool isFirstModule)
{
	// 간소화된 로직: 현재 사용 중인 버퍼가 입력, 다른 버퍼가 출력
	if (m_usingBufferA) {
		// A가 입력(읽기), B가 출력(쓰기) 버퍼
		module.SetBuffers(
			m_particleUAV_A, m_particleSRV_A,  // 입력
			m_particleUAV_B, m_particleSRV_B   // 출력
		);
		//std::cout << "  Module using: Buffer A → Buffer B" << std::endl;
	}
	else {
		// B가 입력(읽기), A가 출력(쓰기) 버퍼
		module.SetBuffers(
			m_particleUAV_B, m_particleSRV_B,  // 입력
			m_particleUAV_A, m_particleSRV_A   // 출력
		);
		//std::cout << "  Module using: Buffer B → Buffer A" << std::endl;
	}
}
