#include "SpawnModuleCS.h"

SpawnModuleCS::SpawnModuleCS(float spawnRate, EmitterType emitterType, int maxParticles)
    : m_spawnRate(spawnRate)
    , m_emitterType(emitterType)
    , m_maxParticles(maxParticles)
    , m_timeSinceLastSpawn(0.0f)
{
}

SpawnModuleCS::~SpawnModuleCS()
{
    
}

void SpawnModuleCS::Initialize()
{
    // 파티클 템플릿 초기화
    m_particleTemplate.age = 0.0f;
    m_particleTemplate.lifeTime = 1.0f;
    m_particleTemplate.rotation = 0.0f;
    m_particleTemplate.rotatespeed = 1.0f;
    m_particleTemplate.size = float2(1.0f, 1.0f);
    m_particleTemplate.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    m_particleTemplate.velocity = float3(0.0f, 0.0f, 0.0f);
    m_particleTemplate.acceleration = float3(0.0f, -9.8f, 0.0f);

    m_pComputeShader = ShaderSystem->ComputeShaders["SpawnModule"].GetShader();

    CreateBuffers(m_maxParticles);
}

void SpawnModuleCS::CreateBuffers(int maxParticles)
{
    auto& device = DeviceState::g_pDevice;

    // 1. 파라미터 상수 버퍼
    D3D11_BUFFER_DESC paramsBufferDesc = {};
    paramsBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    paramsBufferDesc.ByteWidth = sizeof(SpawnModuleParamsBuffer);
    paramsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    paramsBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&paramsBufferDesc, nullptr, &m_pParamsBuffer);

    // 2. 파티클 템플릿 상수 버퍼
    D3D11_BUFFER_DESC templateBufferDesc = {};
    templateBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    templateBufferDesc.ByteWidth = sizeof(ParticleTemplateBuffer);
    templateBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    templateBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&templateBufferDesc, nullptr, &m_pTemplateBuffer);

    // 3. 파티클 데이터 버퍼 (RW)
    D3D11_BUFFER_DESC particleBufferDesc = {};
    particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    particleBufferDesc.ByteWidth = sizeof(ParticleData) * maxParticles;
    particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    particleBufferDesc.StructureByteStride = sizeof(ParticleData);
    device->CreateBuffer(&particleBufferDesc, nullptr, &m_pParticleBuffer);

    // 4. CPU 읽기용 파티클 버퍼
    D3D11_BUFFER_DESC cpuBufferDesc = {};
    cpuBufferDesc.Usage = D3D11_USAGE_STAGING;
    cpuBufferDesc.ByteWidth = sizeof(ParticleData) * maxParticles;
    cpuBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    cpuBufferDesc.StructureByteStride = sizeof(ParticleData);
    device->CreateBuffer(&cpuBufferDesc, nullptr, &m_pParticleBufferCPU);

    // 5. 활성 파티클 카운터 버퍼
    D3D11_BUFFER_DESC counterBufferDesc = {};
    counterBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    counterBufferDesc.ByteWidth = sizeof(UINT);
    counterBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    counterBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    counterBufferDesc.StructureByteStride = sizeof(UINT);
    device->CreateBuffer(&counterBufferDesc, nullptr, &m_pCounterBuffer);

    // 6. CPU 읽기용 카운터 버퍼
    D3D11_BUFFER_DESC cpuCounterDesc = {};
    cpuCounterDesc.Usage = D3D11_USAGE_STAGING;
    cpuCounterDesc.ByteWidth = sizeof(UINT);
    cpuCounterDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    cpuCounterDesc.StructureByteStride = sizeof(UINT);
    device->CreateBuffer(&cpuCounterDesc, nullptr, &m_pCounterBufferCPU);

    // 7. SRV / UAV 생성
    // 파티클 버퍼 SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc = {};
    particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    particleSRVDesc.Buffer.FirstElement = 0;
    particleSRVDesc.Buffer.NumElements = maxParticles;
    device->CreateShaderResourceView(m_pParticleBuffer.Get(), &particleSRVDesc, &m_pParticleSRV);

    // 파티클 버퍼 UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc = {};
    particleUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    particleUAVDesc.Buffer.FirstElement = 0;
    particleUAVDesc.Buffer.NumElements = maxParticles;
    device->CreateUnorderedAccessView(m_pParticleBuffer.Get(), &particleUAVDesc, &m_pParticleUAV);

    // 카운터 버퍼 SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC counterSRVDesc = {};
    counterSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    counterSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    counterSRVDesc.Buffer.FirstElement = 0;
    counterSRVDesc.Buffer.NumElements = 1;
    device->CreateShaderResourceView(m_pCounterBuffer.Get(), &counterSRVDesc, &m_pCounterSRV);

    // 카운터 버퍼 UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC counterUAVDesc = {};
    counterUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    counterUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    counterUAVDesc.Buffer.FirstElement = 0;
    counterUAVDesc.Buffer.NumElements = 1;
    device->CreateUnorderedAccessView(m_pCounterBuffer.Get(), &counterUAVDesc, &m_pCounterUAV);

    // 초기화: 모든 파티클을 비활성화
    std::vector<ParticleData> initialParticles(maxParticles);
    for (auto& particle : initialParticles)
    {
        particle.isActive = false;
    }

    auto& deviceContext = DeviceState::g_pDeviceContext;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    deviceContext->Map(m_pParticleBufferCPU.Get(), 0, D3D11_MAP_WRITE, 0, &mappedResource);
    memcpy(mappedResource.pData, initialParticles.data(), sizeof(ParticleData) * maxParticles);
    deviceContext->Unmap(m_pParticleBufferCPU.Get(), 0);

    deviceContext->CopyResource(m_pParticleBuffer.Get(), m_pParticleBufferCPU.Get());

    // 카운터 초기화
    UINT initialCounter = 0;
    deviceContext->Map(m_pCounterBufferCPU.Get(), 0, D3D11_MAP_WRITE, 0, &mappedResource);
    memcpy(mappedResource.pData, &initialCounter, sizeof(UINT));
    deviceContext->Unmap(m_pCounterBufferCPU.Get(), 0);

    deviceContext->CopyResource(m_pCounterBuffer.Get(), m_pCounterBufferCPU.Get());
}

void SpawnModuleCS::Update(float delta, std::vector<ParticleData>& particles)
{
    m_timeSinceLastSpawn += delta;

    // 스폰 업데이트: 컴퓨트 셰이더 실행
    UpdateBuffers(delta, particles);
    DispatchCompute();

    // 결과를 CPU로 가져오기
    auto& deviceContext = DeviceState::g_pDeviceContext;

    // 파티클 데이터 복사
    deviceContext->CopyResource(m_pParticleBufferCPU.Get(), m_pParticleBuffer.Get());

    // 파티클 데이터 매핑
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    deviceContext->Map(m_pParticleBufferCPU.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    memcpy(particles.data(), mappedResource.pData, sizeof(ParticleData) * m_maxParticles);
    deviceContext->Unmap(m_pParticleBufferCPU.Get(), 0);

    // 스폰했으면 타이머 리셋
    float spawnInterval = 1.0f / m_spawnRate;
    if (m_timeSinceLastSpawn >= spawnInterval)
    {
        m_timeSinceLastSpawn = 0.0f;
    }
}

void SpawnModuleCS::UpdateBuffers(float delta, std::vector<ParticleData>& particles)
{
    auto& deviceContext = DeviceState::g_pDeviceContext;

    // 1. 파라미터 상수 버퍼 업데이트
    SpawnModuleParamsBuffer params;
    params.time = static_cast<float>(GetTickCount64()) / 1000.0f; // 현재 시간
    params.spawnRate = m_spawnRate;
    params.deltaTime = delta;
    params.timeSinceLastSpawn = m_timeSinceLastSpawn;
    params.emitterType = static_cast<int>(m_emitterType);
    params.maxParticles = m_maxParticles;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    deviceContext->Map(m_pParamsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &params, sizeof(SpawnModuleParamsBuffer));
    deviceContext->Unmap(m_pParamsBuffer.Get(), 0);

    // 2. 파티클 템플릿 상수 버퍼 업데이트
    ParticleTemplateBuffer templateData;
    templateData.position = m_particleTemplate.position;
    templateData.velocity = m_particleTemplate.velocity;
    templateData.acceleration = m_particleTemplate.acceleration;
    templateData.size = m_particleTemplate.size;
    templateData.color = m_particleTemplate.color;
    templateData.lifeTime = m_particleTemplate.lifeTime;
    templateData.rotation = m_particleTemplate.rotation;
    templateData.rotateSpeed = m_particleTemplate.rotatespeed;

    deviceContext->Map(m_pTemplateBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &templateData, sizeof(ParticleTemplateBuffer));
    deviceContext->Unmap(m_pTemplateBuffer.Get(), 0);

    // 3. 파티클 데이터 업데이트 (변경된 파티클을 GPU로)
    deviceContext->Map(m_pParticleBufferCPU.Get(), 0, D3D11_MAP_WRITE, 0, &mappedResource);
    memcpy(mappedResource.pData, particles.data(), sizeof(ParticleData) * m_maxParticles);
    deviceContext->Unmap(m_pParticleBufferCPU.Get(), 0);

    deviceContext->CopyResource(m_pParticleBuffer.Get(), m_pParticleBufferCPU.Get());
}

void SpawnModuleCS::DispatchCompute()
{
    auto& deviceContext = DeviceState::g_pDeviceContext;

    // 컴퓨트 셰이더 설정
    deviceContext->CSSetShader(m_pComputeShader.Get(), nullptr, 0);

    // 상수 버퍼 설정
    deviceContext->CSSetConstantBuffers(0, 1, &m_pParamsBuffer);
    deviceContext->CSSetConstantBuffers(1, 1, &m_pTemplateBuffer);

    // UAV 설정
    ID3D11UnorderedAccessView* uavs[] = { m_pParticleUAV.Get(), m_pCounterUAV.Get() };
    deviceContext->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

    // 컴퓨트 셰이더 실행
    // 스레드 그룹 수 계산: 64개 스레드 기준 (64 * 1 * 1)
    UINT threadGroupSize = 64;
    UINT numThreadGroups = (m_maxParticles + threadGroupSize - 1) / threadGroupSize;
    deviceContext->Dispatch(numThreadGroups, 1, 1);

    // 리소스 해제
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    deviceContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);

    ID3D11Buffer* nullBuffers[] = { nullptr, nullptr };
    deviceContext->CSSetConstantBuffers(0, 2, nullBuffers);

    deviceContext->CSSetShader(nullptr, nullptr, 0);
}