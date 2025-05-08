#include "MovementModuleCS.h"
#include "ShaderSystem.h"

void MovementModuleCS::Initialize()
{
	m_useGravity = true;
	m_gravityStrength = 1.0f;
	
	m_computeShader = ShaderSystem->ComputeShaders["MovementModule"].GetShader();
	InitializeCompute();
}

void MovementModuleCS::Update(float delta, std::vector<ParticleData>& particles)
{
    // 버퍼 참조가 유효한지 확인
    if (!m_particlesSRV || !m_particlesUAV) {
        return;
    }

    // 상수 버퍼 업데이트
    UpdateConstantBuffers(delta);

    // 컴퓨트 셰이더 설정
    DeviceState::g_pDeviceContext->CSSetShader(m_computeShader, nullptr, 0);

    // 상수 버퍼 설정
    ID3D11Buffer* constantBuffers[] = { m_movementParamsBuffer };
    DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 1, constantBuffers);

    // 중요: UAV를 설정하기 전에 SRV를 설정
    ID3D11ShaderResourceView* srvs[] = { m_particlesSRV };
    DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, srvs);

    // 출력 UAV 설정
    ID3D11UnorderedAccessView* uavs[] = { m_particlesUAV };
    UINT initCounts[] = { 0 };
    DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 1, uavs, initCounts);

    // 컴퓨트 셰이더 실행
    UINT numThreadGroups = (std::max<UINT>)(1, (static_cast<UINT>(particles.size()) + 255) / 256);
    DeviceState::g_pDeviceContext->Dispatch(numThreadGroups, 1, 1);

    // 중요: 리소스 해제 (순서 중요!)
    // 먼저 UAV 해제
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    // 그 다음 SRV 해제
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    DeviceState::g_pDeviceContext->CSSetShaderResources(0, 1, nullSRVs);

    // 마지막으로 상수 버퍼 해제
    ID3D11Buffer* nullBuffers[] = { nullptr };
    DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 1, nullBuffers);

    DeviceState::g_pDeviceContext->CSSetShader(nullptr, nullptr, 0);
}

bool MovementModuleCS::InitializeCompute()
{
    D3D11_BUFFER_DESC movementParamsDesc = {};
    movementParamsDesc.ByteWidth = sizeof(MovementParams);
    movementParamsDesc.Usage = D3D11_USAGE_DYNAMIC;
    movementParamsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    movementParamsDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

    HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&movementParamsDesc, nullptr, &m_movementParamsBuffer);
    if (FAILED(hr))
        return false;

    m_isInitialized = true;
    return true;
}

// 여따가 easing 구현
void MovementModuleCS::UpdateConstantBuffers(float delta)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = DeviceState::g_pDeviceContext->Map(m_movementParamsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

    if (SUCCEEDED(hr))
    {
        MovementParams* params = reinterpret_cast<MovementParams*>(mappedResource.pData);
        params->deltaTime = delta;
        params->useGravity = m_useGravity ? 1 : 0;
        params->gravityStrength = m_gravityStrength;
        params->useEasing = IsEasingEnabled() ? 1 : 0;
        params->easingType = static_cast<int>(m_easingType);
        params->animationType = static_cast<int>(m_animationType);
        params->easingDuration = m_easingDuration;

        DeviceState::g_pDeviceContext->Unmap(m_movementParamsBuffer, 0);
    }
}

void MovementModuleCS::Release()
{
    if (m_computeShader) m_computeShader->Release();
    if (m_movementParamsBuffer) m_movementParamsBuffer->Release();

    m_computeShader = nullptr;
    m_movementParamsBuffer = nullptr;
    m_isInitialized = false;
}
