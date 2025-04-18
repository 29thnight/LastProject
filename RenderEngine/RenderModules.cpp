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

    // Blend state setup
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

    // Rasterizer state setup
    CD3D11_RASTERIZER_DESC rasterizerDesc{ CD3D11_DEFAULT() };
    DirectX11::ThrowIfFailed(
        DeviceState::g_pDevice->CreateRasterizerState(
            &rasterizerDesc,
            &m_pso->m_rasterizerState
        )
    );

    // Depth stencil state setup
    CD3D11_DEPTH_STENCIL_DESC depthDesc{ CD3D11_DEFAULT() };
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthEnable = true;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DeviceState::g_pDevice->CreateDepthStencilState(&depthDesc, &m_pso->m_depthStencilState);

    m_pso->m_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_pso->m_vertexShader = &ShaderSystem->VertexShaders["BillBoard2"];
    m_pso->m_computeShader = &ShaderSystem->ComputeShaders["BillBoard"];

    // Input layout setup - updated to match the BillboardVertex structure
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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

    // Samplers setup
    auto linearSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
    auto pointSampler = std::make_shared<Sampler>(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    m_pso->m_samplers.push_back(linearSampler);
    m_pso->m_samplers.push_back(pointSampler);

    // Create index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(uint32) * m_indices.size();
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = m_indices.data();

    DirectX11::ThrowIfFailed(
        DeviceState::g_pDevice->CreateBuffer(&ibDesc, &ibData, &billboardIndexBuffer)
    );

    // Create model constant buffer
    m_ModelBuffer = DirectX11::CreateBuffer(
        sizeof(ModelConstantBuffer),
        D3D11_BIND_CONSTANT_BUFFER,
        &m_ModelConstantBuffer
    );
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

    // 1. Create instance buffer and SRV
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = sizeof(BillboardInstance) * instances.size();
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(BillboardInstance);

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = instances.data();

    // Release previous resources first
    instanceBuffer.Reset();
    instanceSRV.Reset();

    // Create new buffer
    HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&desc, &initData, &instanceBuffer);
    if (FAILED(hr)) {
        char errMsg[256];
        sprintf_s(errMsg, "Failed to create instance buffer. Error: 0x%08X", hr);
        OutputDebugStringA(errMsg);
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = instances.size();

    hr = DeviceState::g_pDevice->CreateShaderResourceView(
        instanceBuffer.Get(), &srvDesc, &instanceSRV);
    if (FAILED(hr)) {
        char errMsg[256];
        sprintf_s(errMsg, "Failed to create instance SRV. Error: 0x%08X", hr);
        OutputDebugStringA(errMsg);
        return;
    }

    // 2. Create UAV buffer for processing
    UINT totalVertices = instances.size() * 4; // 4 vertices per billboard

    if (m_maxCount < totalVertices || !billboardComputeBuffer || !billboardVertexBuffer) {
        // Release previous resources
        billboardComputeBuffer.Reset();
        billboardVertexBuffer.Reset();
        vertexBufferUAV.Reset();

        // First create compute buffer with UAV binding
        D3D11_BUFFER_DESC computeDesc = {};
        computeDesc.Usage = D3D11_USAGE_DEFAULT;
        computeDesc.ByteWidth = sizeof(BillboardVertex) * totalVertices;
        computeDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        computeDesc.CPUAccessFlags = 0;
        computeDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        computeDesc.StructureByteStride = sizeof(BillboardVertex);

        hr = DeviceState::g_pDevice->CreateBuffer(&computeDesc, nullptr, &billboardComputeBuffer);
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
        uavDesc.Buffer.NumElements = totalVertices;

        hr = DeviceState::g_pDevice->CreateUnorderedAccessView(
            billboardComputeBuffer.Get(), &uavDesc, &vertexBufferUAV);
        if (FAILED(hr)) {
            char errMsg[256];
            sprintf_s(errMsg, "Failed to create UAV. Error: 0x%08X", hr);
            OutputDebugStringA(errMsg);
            return;
        }

        if (vertexBufferUAV)
        {
            return;
        }

        // Now create a separate vertex buffer
        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(BillboardVertex) * totalVertices;
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexDesc.CPUAccessFlags = 0;
        vertexDesc.MiscFlags = 0;

        hr = DeviceState::g_pDevice->CreateBuffer(&vertexDesc, nullptr, &billboardVertexBuffer);
        if (FAILED(hr)) {
            char errMsg[256];
            sprintf_s(errMsg, "Failed to create vertex buffer. Error: 0x%08X", hr);
            OutputDebugStringA(errMsg);
            return;
        }

        m_maxCount = totalVertices;
    }

    // 3. Setup camera data for compute shader
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

    // Update or create camera constant buffer
    if (cameraConstantBuffer) {
        // Update existing buffer
        deviceContext->UpdateSubresource(cameraConstantBuffer.Get(), 0, nullptr, &cameraData, 0, 0);
    }
    else {
        // Create new buffer
        cameraConstantBuffer = DirectX11::CreateBuffer(
            sizeof(CameraConstants),
            D3D11_BIND_CONSTANT_BUFFER,
            &cameraData
        );

        if (!cameraConstantBuffer) {
            OutputDebugStringA("Failed to create camera constant buffer");
            return;
        }
    }

    // 4. Run compute shader
    deviceContext->CSSetShader(m_pso->m_computeShader->GetShader(), nullptr, 0);
    deviceContext->CSSetConstantBuffers(0, 1, cameraConstantBuffer.GetAddressOf());
    deviceContext->CSSetShaderResources(0, 1, instanceSRV.GetAddressOf());
    deviceContext->CSSetUnorderedAccessViews(0, 1, vertexBufferUAV.GetAddressOf(), nullptr);

    // Dispatch with appropriate thread group count
    UINT numThreadGroups = (instances.size() + 63) / 64;
    deviceContext->Dispatch(numThreadGroups, 1, 1);

    // 5. Cleanup compute shader resources (unbind them, don't destroy)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    deviceContext->CSSetShaderResources(0, 1, &nullSRV);
    deviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    deviceContext->CSSetShader(nullptr, nullptr, 0);

    // 6. Copy data from compute buffer to vertex buffer
    deviceContext->CopyResource(billboardVertexBuffer.Get(), billboardComputeBuffer.Get());

    DebugPrintComputeBufferContent();
}

void BillboardModule::SetupInstancing(BillboardInstance* instanceData, UINT count)
{
    if (count == 0) {
        m_instanceCount = 0;
        return;
    }

    std::vector<BillboardInstance> instances(instanceData, instanceData + count);
    ProcessBillboards(instances);
}

void BillboardModule::Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection)
{
    if (m_instanceCount == 0 || !billboardVertexBuffer || !billboardIndexBuffer) {
        return;
    }

    auto& deviceContext = DeviceState::g_pDeviceContext;


    // Update model constant buffer
    m_ModelConstantBuffer.world = world;
    m_ModelConstantBuffer.view = view;
    m_ModelConstantBuffer.projection = projection;

    deviceContext->VSSetConstantBuffers(0, 1, m_ModelBuffer.GetAddressOf());
    DirectX11::UpdateBuffer(m_ModelBuffer.Get(), &m_ModelConstantBuffer);

    // Set vertex and index buffers
    UINT stride = sizeof(BillboardVertex);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, billboardVertexBuffer.GetAddressOf(), &stride, &offset);
    deviceContext->IASetIndexBuffer(billboardIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    // Draw - each billboard consists of 2 triangles (6 indices)
    deviceContext->DrawIndexed(m_indices.size(), 0, 0);

    // Cleanup
    DirectX11::UnbindRenderTargets();
}

void MeshModule::Initialize()
{
	//m_gameObject->GetComponent<MeshRenderer>().
}

void MeshModule::Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection)
{
}

void MeshModule::SetupInstancing(void* instanceData, UINT count)
{
}


void BillboardModule::DebugPrintComputeBufferContent()
{
    if (!billboardComputeBuffer || m_instanceCount == 0)
        return;

    auto& deviceContext = DeviceState::g_pDeviceContext;
    UINT totalVertices = m_instanceCount * 4;

    // 스테이징 버퍼 생성 (GPU -> CPU 복사용)
    ID3D11Buffer* stagingBuffer = nullptr;
    D3D11_BUFFER_DESC stagingDesc = {};
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.ByteWidth = sizeof(BillboardVertex) * totalVertices;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.StructureByteStride = sizeof(BillboardVertex);

    HRESULT hr = DeviceState::g_pDevice->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
    if (FAILED(hr)) {
        std::cout << "Failed to create staging buffer for debug output" << std::endl;
        return;
    }

    // 컴퓨트 버퍼에서 스테이징 버퍼로 복사
    deviceContext->CopyResource(stagingBuffer, billboardComputeBuffer.Get());

    // 데이터 매핑 및 출력
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = deviceContext->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        BillboardVertex* vertices = static_cast<BillboardVertex*>(mappedResource.pData);

        // 처음 몇 개의 버텍스만 출력 (전체를 출력하면 너무 많음)
        int printCount = std::min(totalVertices, 16u); // 최대 16개까지만 출력

        std::cout << "ComputeBuffer Content (first " << printCount << " vertices):" << std::endl;
        for (UINT i = 0; i < printCount; i++) {
            std::cout << "Vertex " << i << ":" << std::endl;
            std::cout << "  Position: ("
                << vertices[i].Position.x << ", "
                << vertices[i].Position.y << ", "
                << vertices[i].Position.z << ", "
                << vertices[i].Position.w << ")" << std::endl;
            std::cout << "  TexCoord: ("
                << vertices[i].TexCoord.x << ", "
                << vertices[i].TexCoord.y << ")" << std::endl;
            std::cout << "  TexIndex: " << vertices[i].TexIndex << std::endl;
            std::cout << "  Color: ("
                << vertices[i].Color.x << ", "
                << vertices[i].Color.y << ", "
                << vertices[i].Color.z << ", "
                << vertices[i].Color.w << ")" << std::endl;
        }

        deviceContext->Unmap(stagingBuffer, 0);
    }
    else {
        std::cout << "Failed to map staging buffer for reading" << std::endl;
    }

    // 스테이징 버퍼 해제
    if (stagingBuffer) {
        stagingBuffer->Release();
    }
}