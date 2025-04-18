#include "RenderModules.h"
#include "ShaderSystem.h"
#include "Renderer.h"

void RenderModules::CleanupRenderState()
{
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DeviceState::g_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

	DeviceState::g_pDeviceContext->GSSetShader(nullptr, nullptr, 0);
}

void RenderModules::SaveRenderState()
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	if (m_prevDepthState) m_prevDepthState->Release();
	if (m_prevBlendState) m_prevBlendState->Release();
	if (m_prevRasterizerState) m_prevRasterizerState->Release();

	deviceContext->OMGetDepthStencilState(&m_prevDepthState, &m_prevStencilRef);
	deviceContext->OMGetBlendState(&m_prevBlendState, m_prevBlendFactor, &m_prevSampleMask);
	deviceContext->RSGetState(&m_prevRasterizerState);
}

void RenderModules::RestoreRenderState()
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	deviceContext->OMSetDepthStencilState(m_prevDepthState, m_prevStencilRef);
	deviceContext->OMSetBlendState(m_prevBlendState, m_prevBlendFactor, m_prevSampleMask);
	deviceContext->RSSetState(m_prevRasterizerState);

	DirectX11::UnbindRenderTargets();
}


void BillboardModule::Initialize()
{
	m_vertices = Quad;
	m_indices = Indices;

    m_pso = std::make_unique<PipelineStateObject>();
    m_BillBoardType = BillBoardType::Basic;
    m_instanceCount = 0;
    m_maxCount = 0;
    m_prevInstanceCount = 0;  // 추가: 이전 인스턴스 수 추적용

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBlendState(&blendDesc, &m_pso->m_blendState)
	);

	CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateRasterizerState(
			&rasterizerDesc,
			&m_pso->m_rasterizerState
		)
	);

	CD3D11_DEPTH_STENCIL_DESC depthDesc{ CD3D11_DEFAULT() };
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DeviceState::g_pDevice->CreateDepthStencilState(&depthDesc, &m_pso->m_depthStencilState);

	m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_pso->m_vertexShader = &ShaderSystem->VertexShaders["BillBoard"];

	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32_UINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateInputLayout(
			vertexLayoutDesc,
			_countof(vertexLayoutDesc),
			m_pso->m_vertexShader->GetBufferPointer(),
			m_pso->m_vertexShader->GetBufferSize(),
			&m_pso->m_inputLayout
		)
	);

	auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	m_pso->m_samplers.push_back(linearSampler);
	m_pso->m_samplers.push_back(pointSampler);

    // Create model constant buffer
    m_ModelBuffer = DirectX11::CreateBuffer(
        sizeof(ModelConstantBuffer),
        D3D11_BIND_CONSTANT_BUFFER,
        &m_ModelConstantBuffer
    );

    // 최적화: 카메라 상수 버퍼 미리 생성
    cameraConstantBuffer = DirectX11::CreateBuffer(
        sizeof(CameraConstants),
        D3D11_BIND_CONSTANT_BUFFER,
        nullptr
    );

    // 최적화: 버퍼 풀 생성을 위한 초기 크기 설정 (예: 100개의 빌보드로 시작)
    CreateBufferPool(100 * 4); // 각 빌보드당 4개의 버텍스
}

// 최적화: 버퍼 풀 생성 메서드 추가
void BillboardModule::CreateBufferPool(UINT vertexCount)
{
    auto& device = DeviceState::g_pDevice;

    // Release previous resources
    billboardComputeBuffer.Reset();
    billboardVertexBuffer.Reset();
    vertexBufferUAV.Reset();

    // First create compute buffer with UAV binding
    D3D11_BUFFER_DESC computeDesc = {};
    computeDesc.Usage = D3D11_USAGE_DEFAULT;
    computeDesc.ByteWidth = sizeof(BillboardVertex) * vertexCount;
    computeDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_VERTEX_BUFFER; // 최적화: 직접 버텍스 버퍼로 사용
    computeDesc.CPUAccessFlags = 0;
    computeDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    computeDesc.StructureByteStride = sizeof(BillboardVertex);

    HRESULT hr = device->CreateBuffer(&computeDesc, nullptr, &billboardComputeBuffer);
    if (FAILED(hr)) {
        char errMsg[256];
        sprintf_s(errMsg, "Failed to create compute buffer. Error: 0x%08X", hr);
        OutputDebugStringA(errMsg);
        return;
    }

    // Create UAV for compute buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = vertexCount;

    hr = device->CreateUnorderedAccessView(
        billboardComputeBuffer.Get(), &uavDesc, &vertexBufferUAV);
    if (FAILED(hr)) {
        char errMsg[256];
        sprintf_s(errMsg, "Failed to create UAV. Error: 0x%08X", hr);
        OutputDebugStringA(errMsg);
        return;
    }

    // 최적화: billboardVertexBuffer를 생성하지 않고 billboardComputeBuffer를 직접 사용

    m_maxCount = vertexCount;
}

// 최적화: 인스턴스 버퍼 생성 분리
void BillboardModule::CreateInstanceBuffer(const std::vector<BillboardInstance>& instances)
{
    auto& device = DeviceState::g_pDevice;

    // 인스턴스 버퍼가 이미 충분한 크기인지 확인
    if (instanceBuffer && m_prevInstanceCount >= instances.size()) {
        // 기존 버퍼 재사용, 데이터만 업데이트
        DeviceState::g_pDeviceContext->UpdateSubresource(
            instanceBuffer.Get(), 0, nullptr, instances.data(), 0, 0);
        return;
    }

    // 새 버퍼 필요
    instanceBuffer.Reset();
    instanceSRV.Reset();

    // 버퍼 크기를 약간 더 크게 설정 (미래 확장 고려)
    UINT bufferSize = static_cast<UINT>(instances.size() * 1.5f);
    bufferSize = std::max(bufferSize, 16U); // 최소 사이즈 보장

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = sizeof(BillboardInstance) * bufferSize;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(BillboardInstance);

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBuffer(&vbDesc, &vbData, &billboardVertexBuffer)
	);

    HRESULT hr = device->CreateBuffer(&desc, &initData, &instanceBuffer);
    if (FAILED(hr)) {
        char errMsg[256];
        sprintf_s(errMsg, "Failed to create instance buffer. Error: 0x%08X", hr);
        OutputDebugStringA(errMsg);
        return;
    }

	DirectX11::ThrowIfFailed(
		DeviceState::g_pDevice->CreateBuffer(&ibDesc, &ibData, &billboardIndexBuffer)
	);

    hr = device->CreateShaderResourceView(
        instanceBuffer.Get(), &srvDesc, &instanceSRV);
    if (FAILED(hr)) {
        char errMsg[256];
        sprintf_s(errMsg, "Failed to create instance SRV. Error: 0x%08X", hr);
        OutputDebugStringA(errMsg);
        return;
    }

    m_prevInstanceCount = bufferSize;
}

void BillboardModule::ProcessBillboards(const std::vector<BillboardInstance>& instances)
{
    // Return if no instances
    if (instances.empty()) {
        m_instanceCount = 0;
        return;
    }

    m_instanceCount = static_cast<UINT>(instances.size());
    auto& deviceContext = DeviceState::g_pDeviceContext;

    // 1. 최적화: 인스턴스 버퍼 생성 또는 업데이트
    CreateInstanceBuffer(instances);

    // 2. 최적화: 필요한 경우에만 버텍스 버퍼 풀 크기 조정
    UINT totalVertices = instances.size() * 4; // 4 vertices per billboard
    if (m_maxCount < totalVertices) {
        CreateBufferPool(totalVertices);
    }

    // 3. 카메라 데이터 설정
    Mathf::Vector3 cameraRight(m_ModelConstantBuffer.view._11,
        m_ModelConstantBuffer.view._21,
        m_ModelConstantBuffer.view._31);

    Mathf::Vector3 cameraUp(m_ModelConstantBuffer.view._12,
        m_ModelConstantBuffer.view._22,
        m_ModelConstantBuffer.view._32);

    cameraRight.Normalize();
    cameraUp.Normalize();

    CameraConstants cameraData = {
        m_ModelConstantBuffer.view,
        m_ModelConstantBuffer.projection,
        cameraRight,
        0.0f, // padding
        cameraUp,
        static_cast<UINT>(instances.size())
    };

    // 최적화: 버퍼 업데이트 - 항상 존재하는 버퍼 사용
    deviceContext->UpdateSubresource(cameraConstantBuffer.Get(), 0, nullptr, &cameraData, 0, 0);

    // 4. 컴퓨트 셰이더 실행
    deviceContext->CSSetShader(m_pso->m_computeShader->GetShader(), nullptr, 0);
    deviceContext->CSSetConstantBuffers(0, 1, cameraConstantBuffer.GetAddressOf());
    deviceContext->CSSetShaderResources(0, 1, instanceSRV.GetAddressOf());
    deviceContext->CSSetUnorderedAccessViews(0, 1, vertexBufferUAV.GetAddressOf(), nullptr);

    // 최적화: 스레드 그룹 계산 개선
    UINT numThreadGroups = (instances.size() + 63) / 64;
    deviceContext->Dispatch(numThreadGroups, 1, 1);

    // 5. 최적화: 컴퓨트 셰이더 리소스 정리 (메모리 배리어 사용)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    deviceContext->CSSetShaderResources(0, 1, &nullSRV);
    deviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void BillboardModule::InitializeInstance(UINT count)
{
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage = D3D11_USAGE_DYNAMIC;
	ibDesc.ByteWidth = sizeof(BillBoardInstanceData) * count;
	ibDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // CPU에서 쓰기 가능하도록
	ibDesc.MiscFlags = 0;

    // 최적화: 벡터 리사이즈로 메모리 할당 최소화
    static std::vector<BillboardInstance> instances;
    instances.resize(count);
    memcpy(instances.data(), instanceData, count * sizeof(BillboardInstance));

    ProcessBillboards(instances);
}

void BillboardModule::Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection)
{
    if (m_instanceCount == 0 || !billboardComputeBuffer || !billboardIndexBuffer) {
        return;
    }

	m_ModelConstantBuffer.world = world;
	m_ModelConstantBuffer.view = view;
	m_ModelConstantBuffer.projection = projection;

    // 최적화: 모델 상수 버퍼 업데이트
    if (m_ModelConstantBuffer.world != world ||
        m_ModelConstantBuffer.view != view ||
        m_ModelConstantBuffer.projection != projection) {

        m_ModelConstantBuffer.world = world;
        m_ModelConstantBuffer.view = view;
        m_ModelConstantBuffer.projection = projection;

        DirectX11::UpdateBuffer(m_ModelBuffer.Get(), &m_ModelConstantBuffer);
    }

    deviceContext->VSSetConstantBuffers(0, 1, m_ModelBuffer.GetAddressOf());

    // 최적화: 직접 UAV 버퍼를 버텍스 버퍼로 사용
    UINT stride = sizeof(BillboardVertex);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, billboardComputeBuffer.GetAddressOf(), &stride, &offset);
    deviceContext->IASetIndexBuffer(billboardIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(m_pso->m_primitiveTopology);

    // 최적화: 파이프라인 상태 설정
    deviceContext->VSSetShader(m_pso->m_vertexShader->GetShader(), nullptr, 0);
    deviceContext->RSSetState(m_pso->m_rasterizerState.Get());
    deviceContext->OMSetBlendState(m_pso->m_blendState.Get(), nullptr, 0xffffffff);
    deviceContext->OMSetDepthStencilState(m_pso->m_depthStencilState.Get(), 0);

    // 샘플러 설정
    if (!m_pso->m_samplers.empty()) {
        std::vector<ID3D11SamplerState*> samplers;
        for (auto& sampler : m_pso->m_samplers) {
            samplers.push_back(sampler->GetSamplerState());
        }
        deviceContext->PSSetSamplers(0, samplers.size(), samplers.data());
    }

    // 인덱스 개수 계산 (각 빌보드는 2개의 삼각형, 6개의 인덱스)
    UINT indexCount = m_instanceCount * 6;
    indexCount = std::min(indexCount, static_cast<UINT>(m_indices.size()));

    // 인덱스 기반 드로우 콜
    deviceContext->DrawIndexed(indexCount, 0, 0);
}


void MeshModule::Initialize()
{
	m_gameObject->GetComponent<meshrenderer
}

void MeshModule::Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection)
{
}

void MeshModule::SetupInstancing(void* instanceData, UINT count)
{
}
