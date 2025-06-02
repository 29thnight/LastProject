#include "InitializeModuleCS.h"
#include "ShaderSystem.h"

InitializeModuleCS::InitializeModuleCS()
{
    m_computeShader = ShaderSystem->ComputeShaders["InitializeModule"].GetShader();
    CreateBuffers();
}

InitializeModuleCS::~InitializeModuleCS()
{
}

void InitializeModuleCS::Initialize(ID3D11UnorderedAccessView* particleUAV, ID3D11UnorderedAccessView* inactiveIndicesUAV, ID3D11UnorderedAccessView* inactiveCountUAV, ID3D11UnorderedAccessView* activeCountUAV, UINT maxParticles)
{
    DirectX11::BeginEvent(L"InitializeModuleCS");

    // 파티클 수 저장
    m_particlesCapacity = maxParticles;

    // 상수 버퍼 업데이트
    UpdateConstantBuffer(maxParticles);

    // 컴퓨트 셰이더 설정
    DeviceState::g_pDeviceContext->CSSetShader(m_computeShader, nullptr, 0);

    // 상수 버퍼 설정
    ID3D11Buffer* constantBuffers[] = { m_initParamsBuffer };
    DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 1, constantBuffers);

    // 출력 버퍼 설정
    ID3D11UnorderedAccessView* uavs[] = {
        particleUAV,          // 파티클 데이터 출력 (u0)
        inactiveIndicesUAV,   // 비활성 파티클 인덱스 (u1)
        inactiveCountUAV,     // 비활성 파티클 카운터 (u2)
        activeCountUAV        // 활성 파티클 카운터 (u3)
    };

    UINT initCounts[] = { 0, 0, 0, 0 };
    DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 4, uavs, initCounts);

    // 컴퓨트 셰이더 실행
    UINT numThreadGroups = (std::max<UINT>)(1, (maxParticles + 1023) / 1024); // 올림 계산
    DeviceState::g_pDeviceContext->Dispatch(numThreadGroups, 1, 1);

    // 리소스 해제
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr, nullptr, nullptr };
    DeviceState::g_pDeviceContext->CSSetUnorderedAccessViews(0, 4, nullUAVs, nullptr);
    ID3D11Buffer* nullBuffers[] = { nullptr };
    DeviceState::g_pDeviceContext->CSSetConstantBuffers(0, 1, nullBuffers);
    DeviceState::g_pDeviceContext->CSSetShader(nullptr, nullptr, 0);

    DirectX11::EndEvent();
}

void InitializeModuleCS::CreateBuffers()
{
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(InitParams);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_initParamsBuffer);
    if (FAILED(hr)) {
        // 오류 처리
        MessageBoxA(nullptr, "Failed to create constant buffer", "Buffer Error", MB_OK | MB_ICONERROR);
    }
}

void InitializeModuleCS::UpdateConstantBuffer(UINT maxParticles)
{
    InitParams params;
    params.maxParticles = maxParticles;
    params.pad[0] = params.pad[1] = params.pad[2] = 0.0f;

    // 상수 버퍼 업데이트
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = DeviceState::g_pDeviceContext->Map(m_initParamsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        memcpy(mappedResource.pData, &params, sizeof(InitParams));
        DeviceState::g_pDeviceContext->Unmap(m_initParamsBuffer, 0);
    }
}

