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

	// ⭐ 활성 카운터 초기화를 여기서 하지 않음! LifeModule에서 처리

	// 현재 읽기/쓰기 버퍼 결정
	ID3D11UnorderedAccessView* inputUAV = m_usingBufferA ? m_particleUAV_A : m_particleUAV_B;
	ID3D11ShaderResourceView* inputSRV = m_usingBufferA ? m_particleSRV_A : m_particleSRV_B;
	ID3D11UnorderedAccessView* outputUAV = m_usingBufferA ? m_particleUAV_B : m_particleUAV_A;
	ID3D11ShaderResourceView* outputSRV = m_usingBufferA ? m_particleSRV_B : m_particleSRV_A;

	// 1단계: LifeModule 먼저 실행 (기존 파티클 업데이트 + 활성 카운터 계산)
	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it)
	{
		ParticleModule& module = *it;

		if (auto* lifeModule = dynamic_cast<LifeModuleCS*>(&module))
		{
			// ⭐ LifeModule이 카운터 초기화부터 계산까지 모두 담당
			lifeModule->SetBuffers(inputUAV, inputSRV, outputUAV, outputSRV);
			lifeModule->SetSharedBuffers(m_inactiveIndicesUAV, m_inactiveCountUAV, m_activeCountUAV);
			lifeModule->Update(delta, m_particleData);
			break; // LifeModule은 하나만
		}
	}

	// 버퍼 스왑 (LifeModule 결과가 이제 입력이 됨)
	m_usingBufferA = !m_usingBufferA;
	inputUAV = m_usingBufferA ? m_particleUAV_A : m_particleUAV_B;
	inputSRV = m_usingBufferA ? m_particleSRV_A : m_particleSRV_B;
	outputUAV = m_usingBufferA ? m_particleUAV_B : m_particleUAV_A;
	outputSRV = m_usingBufferA ? m_particleSRV_B : m_particleSRV_A;

	// 2단계: SpawnModule 실행 (새 파티클 추가 + 카운터에 추가)
	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it)
	{
		ParticleModule& module = *it;

		if (auto* spawnModule = dynamic_cast<SpawnModuleCS*>(&module))
		{
			// ⭐ SpawnModule은 기존 카운터에 새 파티클 개수만 추가
			spawnModule->SetBuffers(inputUAV, inputSRV, outputUAV, outputSRV);
			spawnModule->SetSharedBuffers(m_inactiveIndicesUAV, m_inactiveCountUAV, m_activeCountUAV);
			spawnModule->Update(delta, m_particleData);
			break; // SpawnModule은 하나만
		}
	}

	// 최종 버퍼 스왑
	m_usingBufferA = !m_usingBufferA;

	DeviceState::g_pDeviceContext->Flush();
	m_activeParticleCount = ReadActiveParticleCount();

#ifdef _DEBUG
	static int frameCount = 0;
	if (++frameCount % 60 == 0) {
		UINT inactiveCount = ReadInactiveParticleCount();
		std::cout << "Active particles: " << m_activeParticleCount
			<< ", Inactive particles: " << inactiveCount
			<< ", Total: " << (m_activeParticleCount + inactiveCount) << std::endl;
	}
#endif
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
	// 기본 D3D11_USAGE_DEFAULT로 충분 (GPU만 사용)
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(ParticleData) * numParticles;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;  // CPU 접근 불필요
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(ParticleData);

	// 초기 데이터 없이 빈 버퍼 생성 (GPU에서 초기화할 예정)
	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_particleBufferA);
	if (FAILED(hr)) return;

	hr = DeviceState::g_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_particleBufferB);
	if (FAILED(hr)) return;

	// UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numParticles;
	uavDesc.Buffer.Flags = 0;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particleBufferA, &uavDesc, &m_particleUAV_A);
	if (FAILED(hr)) return;

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_particleBufferB, &uavDesc, &m_particleUAV_B);
	if (FAILED(hr)) return;

	// SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numParticles;

	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particleBufferA, &srvDesc, &m_particleSRV_A);
	if (FAILED(hr)) return;

	hr = DeviceState::g_pDevice->CreateShaderResourceView(m_particleBufferB, &srvDesc, &m_particleSRV_B);
	if (FAILED(hr)) return;

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

UINT ParticleSystem::ReadInactiveParticleCount()
{
	UINT count = 0;

	ID3D11Buffer* stagingBuffer = nullptr;
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(UINT);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.StructureByteStride = sizeof(UINT);

	HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&desc, nullptr, &stagingBuffer);
	if (SUCCEEDED(hr))
	{
		DeviceState::g_pDeviceContext->CopyResource(stagingBuffer, m_inactiveCountBuffer);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		hr = DeviceState::g_pDeviceContext->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
		if (SUCCEEDED(hr))
		{
			count = *(UINT*)mappedResource.pData;
			DeviceState::g_pDeviceContext->Unmap(stagingBuffer, 0);
		}

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
	 // CPU 측 파티클 데이터 비활성화
    for (auto& particle : m_particleData) {
        particle.isActive = 0;
    }
    m_activeParticleCount = 0;

    // GPU 카운터 초기화
    UINT zero = 0;
    UINT maxCount = m_maxParticles;
    
    DeviceState::g_pDeviceContext->ClearUnorderedAccessViewUint(m_activeCountUAV, &zero);
    DeviceState::g_pDeviceContext->ClearUnorderedAccessViewUint(m_inactiveCountUAV, &maxCount);

    // 현재 사용 중인 버퍼 선택
    ID3D11UnorderedAccessView* particleUAV = m_usingBufferA ? m_particleUAV_A : m_particleUAV_B;




    // 초기화 모듈 실행
    m_initializeModule.Initialize(
        particleUAV,
        m_inactiveIndicesUAV,
        m_inactiveCountUAV,
        m_activeCountUAV,
        m_maxParticles
    );
    
    // 초기화 완료 후 GPU 동기화
    DeviceState::g_pDeviceContext->Flush();
}


void ParticleSystem::ResizeParticleSystem(UINT newMaxParticles)
{
	if (newMaxParticles == m_maxParticles)
		return;

	// 1. 파티클 시스템 일시 정지 및 GPU 작업 완료 대기
	bool wasRunning = m_isRunning;
	m_isRunning = false;
	DeviceState::g_pDeviceContext->Flush();

	// 2. 기존 GPU 리소스 정리
	ReleaseParticleBuffers();
	ReleaseSharedBuffer();

	// 3. 새로운 크기로 CPU 데이터 구조 업데이트 (디버깅/백업용)
	m_maxParticles = newMaxParticles;
	m_particleData.resize(newMaxParticles);
	m_instanceData.resize(newMaxParticles);

	// CPU 데이터는 단순히 크기만 맞춰주고 초기화는 GPU에서 담당
	for (auto& particle : m_particleData) {
		particle.isActive = 0;
	}

	// 4. 새 GPU 버퍼 생성 (빈 버퍼로 생성)
	CreateParticleBuffer(m_maxParticles);
	CreateSharedBuffers();

	// 5. GPU에서 모든 초기화 수행
	InitializeParticleIndices();

	// 6. 모듈들에게 크기 변경 알림
	for (auto it = m_moduleList.begin(); it != m_moduleList.end(); ++it) {
		ParticleModule& module = *it;
		module.OnSystemResized(m_maxParticles);
	}

	// 7. 상태 복원
	m_activeParticleCount = 0;
	m_usingBufferA = true;
	m_isRunning = wasRunning;
}


void ParticleSystem::ReleaseBuffers()
{
	ReleaseParticleBuffers();
}

void ParticleSystem::ReleaseParticleBuffers()
{
	// 파티클 버퍼만 해제 (공유 버퍼는 건드리지 않음)
	if (m_particleBufferA) { m_particleBufferA->Release(); m_particleBufferA = nullptr; }
	if (m_particleBufferB) { m_particleBufferB->Release(); m_particleBufferB = nullptr; }
	if (m_particleUAV_A) { m_particleUAV_A->Release(); m_particleUAV_A = nullptr; }
	if (m_particleUAV_B) { m_particleUAV_B->Release(); m_particleUAV_B = nullptr; }
	if (m_particleSRV_A) { m_particleSRV_A->Release(); m_particleSRV_A = nullptr; }
	if (m_particleSRV_B) { m_particleSRV_B->Release(); m_particleSRV_B = nullptr; }
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
