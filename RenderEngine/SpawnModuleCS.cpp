#include "ShaderSystem.h"
#include "SpawnModuleCS.h"

void SpawnModuleCS::Initialize()
{
    m_uniform = std::uniform_real_distribution<float>(0.0f, 1.0f);

    // 파티클 템플릿 한 번만 초기화
    m_particleTemplate.age = 0.0f;
    m_particleTemplate.lifeTime = 1.0f;
    m_particleTemplate.rotation = 0.0f;
    m_particleTemplate.rotatespeed = 0.0f;
    m_particleTemplate.size = float2(0.3f, 0.3f);
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
    DirectX11::BeginEvent(L"SpawnModuleCS");
    m_particlesCapacity = particles.size();

    // 상수 버퍼 업데이트
    UpdateConstantBuffers(delta);

    // 컴퓨트 셰이더 설정
    DeviceState::g_pDeviceContext->CSSetShader(m_computeShader, nullptr, 0);

    // 상수 버퍼 설정
    ID3D11Buffer* constantBuffers[] = { m_spawnParamsBuffer, m_templateBuffer };
    DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 2, constantBuffers);

    // 셰이더에서 출력으로 사용할 버퍼와 기타 UAV 설정
    // 중요: 여기서 공유 버퍼인 m_inactiveIndicesUAV, m_inactiveCountUAV, m_activeCountUAV를 사용
    ID3D11UnorderedAccessView* uavs[] = {
        m_outputUAV,           // 파티클 데이터 출력 버퍼
        m_randomCounterUAV,    // 랜덤 카운터 (스폰 모듈 전용)
        m_timeUAV,             // 시간 버퍼 (스폰 모듈 전용)
        m_spawnCounterUAV,     // 스폰 카운터 (스폰 모듈 전용)
        m_activeCountUAV,      // 활성 파티클 카운터 (공유 버퍼)
        m_inactiveIndicesUAV,  // 비활성 파티클 인덱스 (공유 버퍼)
        m_inactiveCountUAV     // 비활성 카운터 (공유 버퍼)
    };

    UINT initCounts[] = { 0, 0, 0, 0, 0, 0, 0 };

    // 셰이더에서 입력으로 사용할 버퍼 설정
    ID3D11ShaderResourceView* srvs[] = { m_inputSRV };
    DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, srvs);

    DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 7, uavs, initCounts);

    // 컴퓨트 셰이더 실행
    UINT numThreadGroups = (std::max<UINT>)(1, (static_cast<UINT>(particles.size()) + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE);
    DeviceState::g_pDeviceContext->Dispatch(numThreadGroups, 1, 1);

    // 리소스 해제
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 7, nullUAVs, nullptr);

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, nullSRVs);

    ID3D11Buffer* nullBuffers[] = { nullptr, nullptr };
    DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 2, nullBuffers);

    DeviceState::g_pDeviceContext->CSSetShader(nullptr, nullptr, 0);

    DirectX11::EndEvent();
}

void SpawnModuleCS::OnSystemResized(UINT max)
{
    if (max != m_particlesCapacity)
    {
        m_particlesCapacity = max;
        m_paramsDirty = true;  // 파라미터 업데이트가 필요하다고 표시

        // 추가적으로 필요한 처리가 있다면 여기에 구현
        // 예: 버퍼 재생성 등
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

    // CPU 읽기용 스테이징 버퍼 생성
    D3D11_BUFFER_DESC stagingDesc = {};
    stagingDesc.ByteWidth = sizeof(UINT);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    hr = DeviceState::g_pDevice->CreateBuffer(&stagingDesc, nullptr, &m_activeCountStagingBuffer);
    if (FAILED(hr))
        return false;

    // 시간 버퍼 초기화
    float zeroTime = 0.0f;
    DeviceState::g_pDeviceContext->UpdateSubresource(m_timeBuffer, 0, nullptr, &zeroTime, 0, 0);

    // 스폰 카운터 초기화
    UINT zeroCounter[2] = { 0, 0 };
    DeviceState::g_pDeviceContext->UpdateSubresource(m_spawnCounterBuffer, 0, nullptr, zeroCounter, 0, 0);

    m_isInitialized = true;
    return true;
}

void SpawnModuleCS::Release()
{
    // 스폰 모듈 전용 리소스 해제
    if (m_computeShader) m_computeShader->Release();
    if (m_spawnParamsBuffer) m_spawnParamsBuffer->Release();
    if (m_templateBuffer) m_templateBuffer->Release();
    if (m_randomCounterBuffer) m_randomCounterBuffer->Release();
    if (m_randomCounterUAV) m_randomCounterUAV->Release();
    if (m_timeBuffer) m_timeBuffer->Release();
    if (m_timeUAV) m_timeUAV->Release();
    if (m_spawnCounterBuffer) m_spawnCounterBuffer->Release();
    if (m_spawnCounterUAV) m_spawnCounterUAV->Release();
    if (m_activeCountStagingBuffer) m_activeCountStagingBuffer->Release();

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
    m_activeCountStagingBuffer = nullptr;

    // 외부 버퍼 참조 초기화 (공유 버퍼는 여기서 해제하지 않음)
    m_inputUAV = nullptr;
    m_inputSRV = nullptr;
    m_outputUAV = nullptr;
    m_outputSRV = nullptr;
    m_inactiveIndicesUAV = nullptr;
    m_inactiveCountUAV = nullptr;
    m_activeCountUAV = nullptr;

    m_isInitialized = false;
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

bool SpawnModuleCS::TryGetCPUCount(UINT* count)
{
    if (!m_activeCountUAV || !m_activeCountStagingBuffer)
        return false;

    // 활성 카운트 버퍼에서 스테이징 버퍼로 복사
    // 여기서는 ParticleSystem에서 공유받은 m_activeCountBuffer를 사용해야 함
    // 그러나 현재 코드에서는 해당 버퍼의 포인터가 없으므로 이 기능을 사용하려면 추가 수정이 필요
    // 임시로 다음과 같이 구현

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