#include "ShaderSystem.h"
#include "SpawnModuleCS.h"

void SpawnModuleCS::Initialize()
{
	m_uniform = std::uniform_real_distribution<float>(0.0f, 1.0f);

	// 파티클 템플릿 한 번만 초기화
	m_particleTemplate.age = 0.0f;
	m_particleTemplate.lifeTime = 1.0f;
	m_particleTemplate.rotation = 0.0f;
	m_particleTemplate.rotatespeed = 1.0f;
	m_particleTemplate.size = float2(1.0f, 1.0f);
	m_particleTemplate.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	m_particleTemplate.velocity = float3(0.0f, 0.0f, 0.0f);
	m_particleTemplate.acceleration = float3(0.0f, -9.8f, 0.0f);

	// 추가 속성 초기화 (템플릿에는 없는 속성)
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

	// 상수 버퍼 업데이트
	UpdateConstantBuffers(delta);

	// 컴퓨트 셰이더 설정
	DeviceState::g_pDeviceContext->CSSetShader(m_computeShader, nullptr, 0);

	// 상수 버퍼 설정
	ID3D11Buffer* constantBuffers[] = { m_spawnParamsBuffer, m_templateBuffer };
	DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 2, constantBuffers);

	// 활성 카운트 리셋
	UINT initialCount = 0;
	DeviceState::g_pDeviceContext->UpdateSubresource(m_activeCountBuffer, 0, nullptr, &initialCount, 0, 0);

	// 셰이더에서 출력으로 사용할 버퍼와 기타 UAV 설정
	ID3D11UnorderedAccessView* uavs[] = { m_outputUAV, m_randomCounterUAV, m_timeUAV, m_spawnCounterUAV, m_activeCountUAV };
	UINT initCounts[] = { 0, 0, 0, 0, 0 };

	// 셰이더에서 입력으로 사용할 버퍼 설정 (필요한 경우)
	ID3D11ShaderResourceView* srvs[] = { m_inputSRV };
	DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, srvs);

	DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 5, uavs, initCounts);

	// 컴퓨트 셰이더 실행
	UINT numThreadGroups = (std::max<UINT>)(1, (static_cast<UINT>(particles.size()) + 255) / 256);
	DeviceState::g_pDeviceContext->Dispatch(numThreadGroups, 1, 1);

	// 리소스 해제
	ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 5, nullUAVs, nullptr);

	ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
	DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, nullSRVs);

	ID3D11Buffer* nullBuffers[] = { nullptr, nullptr };
	DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 2, nullBuffers);

	DeviceState::g_pDeviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SpawnModuleCS::OnSystemResized(UINT max)
{
	if (max != m_particlesCapacity)
	{
		m_particlesCapacity = max;
		m_paramsDirty = true;
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

	// 활성 파티클 카운터 버퍼 생성
	D3D11_BUFFER_DESC activeCounterDesc = {};
	activeCounterDesc.ByteWidth = sizeof(UINT);
	activeCounterDesc.Usage = D3D11_USAGE_DEFAULT;
	activeCounterDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	activeCounterDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	activeCounterDesc.StructureByteStride = sizeof(UINT);

	UINT initialCount = 0;
	D3D11_SUBRESOURCE_DATA initialData1 = {};
	initialData1.pSysMem = &initialCount;

	hr = DeviceState::g_pDevice->CreateBuffer(&activeCounterDesc, &initialData1, &m_activeCountBuffer);
	if (FAILED(hr))
		return false;

	// 활성 카운터 UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC activeCounterUavDesc = {};
	activeCounterUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	activeCounterUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	activeCounterUavDesc.Buffer.FirstElement = 0;
	activeCounterUavDesc.Buffer.NumElements = 1;
	activeCounterUavDesc.Buffer.Flags = 0; // 일반 UAV (InterlockedAdd 사용)

	hr = DeviceState::g_pDevice->CreateUnorderedAccessView(m_activeCountBuffer, &activeCounterUavDesc, &m_activeCountUAV);
	if (FAILED(hr))
		return false;

	// CPU 읽기용 스테이징 버퍼 생성
	D3D11_BUFFER_DESC stagingDesc = {};
	stagingDesc.ByteWidth = sizeof(UINT);
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	hr = DeviceState::g_pDevice->CreateBuffer(&stagingDesc, nullptr, &m_activeCountStagingBuffer);
	if (FAILED(hr))
		return false;

	m_isInitialized = true;

	return true;
}

bool SpawnModuleCS::TryGetCPUCount(UINT* count)
{
	if (!m_activeCountBuffer || !m_activeCountStagingBuffer)
		return false;

	// 활성 카운트 버퍼에서 스테이징 버퍼로 복사
	DeviceState::g_pDeviceContext->CopyResource(m_activeCountStagingBuffer, m_activeCountBuffer);

	// 스테이징 버퍼에서 CPU로 데이터 가져오기
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = DeviceState::g_pDeviceContext->Map(m_activeCountStagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

	if (SUCCEEDED(hr))
	{
		// 카운트 값 읽기
		*count = *reinterpret_cast<UINT*>(mappedResource.pData);
		DeviceState::g_pDeviceContext->Unmap(m_activeCountStagingBuffer, 0);
		return true;
	}

	return false;
}

UINT SpawnModuleCS::GetActiveParticleCount()
{
	static UINT frameCount = 0;
	frameCount++;

	// PID 컨트롤러 매개변수 (한 번만 초기화되도록 static으로 선언)
	static float kP = 0.5f;  // 비례 게인
	static float kI = 0.1f;  // 적분 게인
	static float kD = 0.05f; // 미분 게인

	// 오차 항 (함수 호출 간에 값이 유지되도록 static으로 선언)
	static float errorIntegral = 0.0f;
	static float previousError = 0.0f;

	// 기본 예측
	float avgLifetime = m_particleTemplate.lifeTime;
	float spawnRate = m_spawnRate;
	UINT baseEstimate = static_cast<UINT>(spawnRate * avgLifetime);
	baseEstimate = (std::min)(baseEstimate, static_cast<UINT>(m_particlesCapacity));

	// 주기적으로 GPU에서 실제 카운트 가져오기 (30프레임마다)
	if (frameCount % 30 == 0) {  // 더 자주 업데이트하여 정확도 향상
		UINT actualCount;
		if (TryGetCPUCount(&actualCount)) {
			// 오차 계산
			float error = static_cast<float>(actualCount) - static_cast<float>(baseEstimate);

			// PID 제어
			errorIntegral += error;
			float derivative = error - previousError;

			// 너무 큰 적분 오차 방지 (anti-windup)
			errorIntegral = (std::max)(-1000.0f, (std::min)(errorIntegral, 1000.0f));

			// 보정 계산
			float correction = kP * error + kI * errorIntegral + kD * derivative;
			m_correctionFactor = 1.0f + correction / (std::max)(1.0f, static_cast<float>(baseEstimate));

			// 보정 계수가 너무 극단적이지 않도록 제한
			m_correctionFactor = (std::max)(0.2f, (std::min)(m_correctionFactor, 5.0f));

			// 이전 오차 업데이트
			previousError = error;

			// 디버깅 용도로 실제 값 저장
			m_actualCount = actualCount;
		}
	}

	// 보정된 예측값 반환
	return static_cast<UINT>(baseEstimate * m_correctionFactor);
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

			// 파티클 템플릿에서 값 직접 복사
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

void SpawnModuleCS::Release()
{
	// 리소스 해제
	if (m_computeShader) m_computeShader->Release();
	if (m_spawnParamsBuffer) m_spawnParamsBuffer->Release();
	if (m_templateBuffer) m_templateBuffer->Release();
	if (m_randomCounterBuffer) m_randomCounterBuffer->Release();
	if (m_randomCounterUAV) m_randomCounterUAV->Release();
	if (m_activeCountBuffer) m_activeCountBuffer->Release();
	if (m_activeCountUAV) m_activeCountUAV->Release();
	if (m_activeCountStagingBuffer) m_activeCountStagingBuffer->Release();

	// 기타 리소스 해제
	if (m_timeBuffer) m_timeBuffer->Release();
	if (m_timeUAV) m_timeUAV->Release();
	if (m_spawnCounterBuffer) m_spawnCounterBuffer->Release();
	if (m_spawnCounterUAV) m_spawnCounterUAV->Release();

	// 모든 포인터 초기화
	m_computeShader = nullptr;
	m_spawnParamsBuffer = nullptr;
	m_templateBuffer = nullptr;
	m_randomCounterBuffer = nullptr;
	m_randomCounterUAV = nullptr;
	m_timeBuffer = nullptr;
	m_timeUAV = nullptr;
	m_spawnCounterBuffer = nullptr;
	m_spawnCounterUAV = nullptr;
	m_activeCountBuffer = nullptr;
	m_activeCountUAV = nullptr;
	m_activeCountStagingBuffer = nullptr;

	// 외부 버퍼 참조 초기화
	m_inputUAV = nullptr;
	m_inputSRV = nullptr;
	m_outputUAV = nullptr;
	m_outputSRV = nullptr;

	m_isInitialized = false;
}