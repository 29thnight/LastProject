#include "TestEffect.h"

TestEffect::TestEffect(const Mathf::Vector3& position, int maxParticles)
{
    m_position = position;

    // 3. SpawnModuleCS 생성 및 설정
    m_spawnModule = AddModule<SpawnModuleCS>(10000.0f, EmitterType::box, 1000000);

    m_billboardModule = AddRenderModule<BillboardModule>();
    m_billboardModule->Initialize();

    m_billboardModule->GetPSO()->m_pixelShader = &ShaderSystem->PixelShaders["Sparkle"];
    m_billboardModule->InitializeInstance(m_maxParticles);

    Play();
}


void TestEffect::Update(float delta)
{
    if (!m_isRunning)
        return;

    // 시간 업데이트
    m_delta += delta;
    if (m_delta > 10000.0f) {
        m_delta = 0.0f;
    }

    //UpdateInstanceBuffer();

    // 기본 업데이트 호출 (모듈 업데이트 포함)
    EffectSystem::Update(delta);
}

void TestEffect::Render(RenderScene& scene, Camera& camera)
{
    if (!m_isRunning) //|| m_activeParticleCount == 0)
        return;

    auto& deviceContext = DeviceState::g_pDeviceContext;

    // 렌더 스테이트 저장
    if (m_renderModules.size() > 0) {
        m_renderModules[0]->SaveRenderState();
    }

    // 렌더 타겟 설정
    ID3D11RenderTargetView* rtv = camera.m_renderTarget->GetRTV();
    deviceContext->OMSetRenderTargets(1, &rtv, camera.m_depthStencil->m_pDSV);

    // ParticleData를 BillBoardInstanceData로 변환
    int instanceIndex = 0;
    for (const auto& particle : m_particleData)
    {
        if (particle.isActive)
        {
            // ParticleData를 BillBoardInstanceData로 변환
            m_instanceData[instanceIndex].Position = Mathf::Vector4(
                particle.position.x,
                particle.position.y,
                particle.position.z,
                particle.rotation  // 회전값을 w 성분에 저장 (빌보드 셰이더에서 사용)
            );

            // 크기 정보를 텍스처 좌표에 저장 (일반적인 패턴)
            m_instanceData[instanceIndex].TexCoord = Mathf::Vector2(
                particle.size.x,
                particle.size.y
            );

            // 텍스처 인덱스 (기본적으로 0 또는 특정 값)
            m_instanceData[instanceIndex].TexIndex = 0;

            // 색상 정보 복사
            m_instanceData[instanceIndex].Color = particle.color;

            instanceIndex++;
        }
    }

    // 빌보드 모듈이 있다면 인스턴스 데이터 설정
    if (m_billboardModule && m_activeParticleCount > 0) {
        // 변환된 인스턴스 데이터를 빌보드 모듈에 전달
        m_billboardModule->SetupInstancing(m_instanceData.data(), m_activeParticleCount);
    }

    // 기존 EffectModules의 Render 메서드 호출
    EffectSystem::Render(scene, camera);

    // 렌더 스테이트 정리 및 복원
    if (m_renderModules.size() > 0) {
        m_renderModules[0]->CleanupRenderState();
        m_renderModules[0]->RestoreRenderState();
    }
}

void TestEffect::UpdateInstanceBuffer(const std::vector<ParticleData>& particles)
{
}
