#include "Fire.h"
#include "AssetSystem.h"
#include "Material.h"
#include "Scene.h"
#include "Mesh.h"
#include "ImGuiRegister.h"

FirePass::FirePass()
{
	// 특정 셰이더마다 설정
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["Fire"];
	m_pso->m_computeShader = &AssetsSystems->ComputeShaders["FireCompute"];	

	// cb는 이펙트 클래스마다 달라지므로 따로 설정
	{
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.ByteWidth = sizeof(ExplodeParameters);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		DeviceState::g_pDevice->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
	}

	mmParam = new ExplodeParameters;
	mmParam->time = 0.0f;
	mmParam->intensity = 1.0f;
	mmParam->speed = 5.0f;
	mmParam->color = Mathf::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	mmParam->size = Mathf::Vector2(256.0f, 256.0f);
	mmParam->range = Mathf::Vector2(8.0f, 4.0f);
	SetParameters(mmParam);

	m_baseFireTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01.dds"));
	m_fireAlphaTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("Explosion_01_a.jpg"));

	m_resultTexture = std::shared_ptr<Texture>(Texture::Create(
		512,  // Width
		512,  // Height
		"FireEffectTexture",
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
	));

	m_resultTexture->CreateSRV(DXGI_FORMAT_R8G8B8A8_UNORM);
	m_resultTexture->CreateUAV(DXGI_FORMAT_R8G8B8A8_UNORM);

	{
		ImGui::ContextRegister("Fire Effect", [&]()
			{
				ImGui::SliderFloat("Intensity", &mmParam->intensity, 0.0f, 10.0f);
				ImGui::SliderFloat("Speed", &mmParam->speed, 1.0f, 100.0f);
				ImGui::ColorEdit3("Color", &mmParam->color.x);
			});
	}

	BillboardVertex vertex;
	vertex.Position = { 0.0f, 0.0f, 0.0f, 0.0f };

	CreateBillboard(&vertex);
}

void FirePass::Render(Scene& scene, Camera& camera)
{
	// 현재 렌더 상태 저장
	ID3D11DepthStencilState* prevDepthState;
	UINT prevStencilRef;
	ID3D11BlendState* prevBlendState;
	float prevBlendFactor[4];
	UINT prevSampleMask;
	ID3D11RasterizerState* prevRasterizerState;

	auto& deviceContext = DeviceState::g_pDeviceContext;
	deviceContext->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);
	deviceContext->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);
	deviceContext->RSGetState(&prevRasterizerState);

	// 컴퓨트 셰이더 실행
	deviceContext->CSSetShader(m_pso->m_computeShader->GetShader(), nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	ID3D11ShaderResourceView* srvs[]{
		m_baseFireTexture->m_pSRV,
		m_fireAlphaTexture->m_pSRV
	};

	deviceContext->CSSetShaderResources(0, 2, srvs);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &m_resultTexture->m_pUAV, nullptr);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	deviceContext->ClearUnorderedAccessViewFloat(m_resultTexture->m_pUAV, clearColor);

	deviceContext->Dispatch(32, 32, 1);

	// 컴퓨트 셰이더 리소스 해제
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	deviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	ID3D11ShaderResourceView* nullSRV = nullptr;
	deviceContext->CSSetShaderResources(0, 1, &nullSRV);

	// 렌더링 파이프라인 설정
	m_pso->Apply();

	ID3D11RenderTargetView* rtv = camera.m_renderTarget->GetRTV();
	deviceContext->OMSetRenderTargets(1, &rtv, DeviceState::g_pDepthStencilView);
	deviceContext->OMSetBlendState(m_pso->m_blendState, nullptr, 0xFFFFFFFF);


	// 픽셀 셰이더 설정
	DirectX11::PSSetConstantBuffer(3, 1, m_constantBuffer.GetAddressOf());
	DirectX11::PSSetShaderResources(0, 1, &m_resultTexture->m_pSRV);

	// 빌보드 그리기
	// 이펙트를 적용할 오브젝트 기반
	//if (!EffectedObject.empty()) {
	//	BillBoardInstanceData* instances = new BillBoardInstanceData[EffectedObject.size()];
	//
	//	for (size_t i = 0; i < EffectedObject.size(); i++) {
	//		Mathf::Vector3 pos = EffectedObject[i]->GetTransform()->GetPosition();
	//		instances[i].position = Mathf::Vector4(pos.x, pos.y, pos.z, 1.0f);
	//		instances[i].size = mmParam->size;
	//	}
	//
	//	// Effects 클래스의 메서드 호출하여 인스턴스 버퍼 설정
	//	SetupBillBoardInstancing(instances, static_cast<UINT>(EffectedObject.size()));
	//
	//	delete[] instances;
	//}

	//int numBillboards = 2;  // 빌보드 두 개
	//BillBoardInstanceData* instances = new BillBoardInstanceData[numBillboards];
	//
	//// 첫 번째 빌보드 (기본 위치)
	//instances[0].position = Mathf::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	//instances[0].size = Mathf::Vector2(10.0f, 10.0f);
	//
	//// 두 번째 빌보드
	//instances[1].position = Mathf::Vector4(10.0f, 0.0f, 0.0f, 1.0f);
	//instances[1].size = Mathf::Vector2(10.0f, 10.0f);
	//
	//// 인스턴스 버퍼 설정 및 그리기
	//SetupBillBoardInstancing(instances, numBillboards);
	//delete[] instances;


	int gridSize = 4; // 4x4 그리드
	int numBillboards = gridSize * gridSize;
	BillBoardInstanceData* instances = new BillBoardInstanceData[numBillboards];
	for (int x = 0; x < gridSize; x++) {
		for (int z = 0; z < gridSize; z++) {
			int index = x * gridSize + z;
			float spacing = 5.0f;
			instances[index].position = Mathf::Vector4(
				(x - gridSize / 2) * spacing,
				0.0f,
				(z - gridSize / 2) * spacing,
				1.0f
			);
			instances[index].size = Mathf::Vector2(10.0f, 10.0f);
			instances[index].id = index;
		}
	}
	SetupBillBoardInstancing(instances, numBillboards);
	delete[] instances;
	DrawBillboard(
		XMMatrixIdentity(),
		XMMatrixTranspose(camera.CalculateView()),
		XMMatrixTranspose(camera.CalculateProjection()));

	// 리소스 정리 및 상태 복원
	DirectX11::PSSetShaderResources(0, 1, &nullSRV);
	deviceContext->OMSetDepthStencilState(prevDepthState, prevStencilRef);
	deviceContext->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
	deviceContext->RSSetState(prevRasterizerState);

	deviceContext->GSSetShader(nullptr, nullptr, 0);

	// 포인터 해제
	if (prevDepthState) prevDepthState->Release();
	if (prevBlendState) prevBlendState->Release();
	if (prevRasterizerState) prevRasterizerState->Release();

	DirectX11::UnbindRenderTargets();
	EffectedObject.clear();
}

void FirePass::Update(float delta)
{
	m_delta += delta;

	if (m_delta > 10000.0f) {
		m_delta = 0.0f;
	}
	
	mmParam->time = m_delta;

	// 아마 update마다 cb가 다를테니 계속 써야할지도
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DirectX11::ThrowIfFailed(
		DeviceState::g_pDeviceContext->Map(
			m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource
		)
	);
	memcpy(mappedResource.pData, static_cast<ExplodeParameters*>(mmParam), sizeof(ExplodeParameters));
	DeviceState::g_pDeviceContext->Unmap(m_constantBuffer.Get(), 0);
}