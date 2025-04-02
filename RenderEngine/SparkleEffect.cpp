#include "SparkleEffect.h"
#include "ImGuiRegister.h"
#include "Camera.h"

SparkleEffect::SparkleEffect(const Mathf::Vector3& position, int maxParticles) : NiagaraEffectManager(maxParticles), m_delta(0.0f), m_position(0.0f, 0.0f, 0.0f)
{
	m_pso->m_pixelShader = &AssetsSystems->PixelShaders["Sparkle"];

    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.ByteWidth = sizeof(SparkleParameters);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        DeviceState::g_pDevice->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
    }

    m_sparkleParams = new SparkleParameters;
    m_sparkleParams->time = 0.0f;
    m_sparkleParams->intensity = 1.5f;
    m_sparkleParams->speed = 3.0f;
    m_sparkleParams->color = Mathf::Vector4(0.8f, 0.9f, 1.0f, 1.0f); // 약간 푸른 빛
    m_sparkleParams->size = Mathf::Vector2(3.0f, 3.0f);
    m_sparkleParams->range = Mathf::Vector2(4.0f, 2.0f);
    SetParameters(m_sparkleParams);

    // 실제 텍스처 사진
    m_sparkleTexture = std::shared_ptr<Texture>(Texture::LoadFormPath("star.png"));

    m_particleTexture = m_sparkleTexture;

    {
        ImGui::ContextRegister("Sparkle Effect", [&]()
            {
                ImGui::SliderFloat("Intensity", &m_sparkleParams->intensity, 0.0f, 5.0f);
                ImGui::SliderFloat("Speed", &m_sparkleParams->speed, 0.1f, 10.0f);
                ImGui::ColorEdit3("Color", &m_sparkleParams->color.x);
                ImGui::SliderFloat2("Size", &m_sparkleParams->size.x, 0.1f, 10.0f);

                // 위치 조정 UI
                ImGui::Text("Position");
                ImGui::SliderFloat("X", &m_position.x, -50.0f, 50.0f);
                ImGui::SliderFloat("Y", &m_position.y, -50.0f, 50.0f);
                ImGui::SliderFloat("Z", &m_position.z, -50.0f, 50.0f);

                if (ImGui::Button("Spawn Burst"))
                {
                    SpawnSparklesBurst(20);
                }
            });
    }

    BillboardVertex vertex;
    vertex.Position = { 0.0f, 0.0f, 0.0f, 0.0f };
    CreateBillboard(&vertex, BillBoardType::Basic);

    InitializeModules();

    Play();
}

SparkleEffect::~SparkleEffect()
{
    if (m_sparkleParams)
    {
        delete m_sparkleParams;
        m_sparkleParams = nullptr;
    }
}

void SparkleEffect::InitializeModules()
{
    // 스폰 모듈 추가 (이펙트의 위치는 m_position)
    m_spawnModule = AddModule<SpawnModule>(m_maxParticles, 20.0f, EmitterType::cone);

    // 수명 모듈 추가
    AddModule<LifeModule>();

    // 움직임 모듈 추가 (중력 없음, 자유롭게 움직이는 반짝임)
    auto movementModule = AddModule<MovementModule>();
    movementModule->SetUseGravity(false);

    // 색상 모듈 추가 (반짝이는 효과를 위한 투명도 변화)
    auto colorModule = AddModule<ColorModule>();
    colorModule->SetColorGradient({
        {0.0f, Mathf::Vector4(m_sparkleParams->color.x, m_sparkleParams->color.y, m_sparkleParams->color.z, 0.0f)},
        {0.1f, Mathf::Vector4(m_sparkleParams->color.x, m_sparkleParams->color.y, m_sparkleParams->color.z, 0.9f)},
        {0.4f, Mathf::Vector4(1.0f, 1.0f, 1.0f, 1.0f)},
        {0.7f, Mathf::Vector4(m_sparkleParams->color.x, m_sparkleParams->color.y, m_sparkleParams->color.z, 0.7f)},
        {1.0f, Mathf::Vector4(m_sparkleParams->color.x * 0.5f, m_sparkleParams->color.y * 0.5f, m_sparkleParams->color.z * 0.5f, 0.0f)}
        });

    // 크기 모듈 추가 (깜빡이는 효과)
    auto sizeModule = AddModule<SizeModule>();
    sizeModule->SetStartSize(0.2f);
    sizeModule->SetEndSize(0.05f);
    sizeModule->SetSizeOverLifeFunction([this](float t) {
        // 반짝이는 효과를 위한 사인 파동
        float pulse = 0.7f + 0.3f * sin(t * m_sparkleParams->speed * 10.0f);
        float baseSize = 0.2f + t * (0.05f - 0.2f);
        return Mathf::Vector2(baseSize * pulse, baseSize * pulse);
       });
}

void SparkleEffect::Update(float delta)
{
    if (!m_isRunning)
        return;

    // 시간 업데이트
    m_delta += delta;
    if (m_delta > 10000.0f) {
        m_delta = 0.0f;
    }

    m_sparkleParams->time = m_delta;

    std::cout << m_activeParticleCount << std::endl;

    // 상수 버퍼 업데이트
    UpdateSparkleConstantBuffer();

    // 기본 업데이트 호출 (모듈 업데이트 포함)
    NiagaraEffectManager::Update(delta);
}

void SparkleEffect::Render(Scene& scene, Camera& camera)
{
    if (!m_isRunning || m_activeParticleCount == 0)
        return;

    SaveRenderState();

    // Custom pre-rendering code here

    PrepareBillboards();
    SetupRenderState(camera);
    RenderBillboards(camera);

    // Custom post-rendering code here

    CleanupRenderState();

    RestoreRenderState();
}

void SparkleEffect::SetPosition(const Mathf::Vector3& position)
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

void SparkleEffect::SpawnSparklesBurst(int count)
{

    // 보유한 파티클 중 비활성 상태인 것을 찾아 활성화
    int spawned = 0;

    for (auto& particle : m_particleData) {
        if (!particle.isActive && spawned < count) {
            // 비활성 파티클을 찾아 초기화
            particle.position = m_position;
            particle.velocity = Mathf::Vector3(
                (rand() / (float)RAND_MAX - 0.5f) * 4.0f,  // x: -2 ~ 2
                (rand() / (float)RAND_MAX - 0.5f) * 4.0f,  // y: -2 ~ 2
                (rand() / (float)RAND_MAX - 0.5f) * 4.0f   // z: -2 ~ 2
            );
            particle.acceleration = Mathf::Vector3(0.0f, 0.0f, 0.0f);
            particle.age = 0.0f;
            particle.lifeTime = 0.5f + (rand() / (float)RAND_MAX) * 1.0f; // 0.5 ~ 1.5초
            particle.rotation = rand() / (float)RAND_MAX * 6.28f;  // 0 ~ 2π
            particle.rotatespeed = (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
            particle.size = Mathf::Vector2(0.2f, 0.2f);
            particle.color = Mathf::Vector4(
                m_sparkleParams->color.x,
                m_sparkleParams->color.y,
                m_sparkleParams->color.z,
                1.0f
            );
            particle.isActive = true;

            spawned++;
        }
    }
}

void SparkleEffect::UpdateSparkleConstantBuffer()
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    DirectX11::ThrowIfFailed(
        DeviceState::g_pDeviceContext->Map(
            m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource
        )
    );
    memcpy(mappedResource.pData, m_sparkleParams, sizeof(SparkleParameters));
    DeviceState::g_pDeviceContext->Unmap(m_constantBuffer.Get(), 0);
}

void SparkleEffect::UpdateInstanceData()
{
    m_instanceData.resize(m_activeParticleCount);
    int instanceIndex = 0;

    for (const auto& particle : m_particleData)
    {
        if (particle.isActive)
        {
            m_instanceData[instanceIndex].position = Mathf::Vector4(
                particle.position.x,
                particle.position.y,
                particle.position.z,
                1.0f
            );
            m_instanceData[instanceIndex].size = particle.size * m_sparkleParams->intensity;
            m_instanceData[instanceIndex].id = instanceIndex % 4;
            instanceIndex++;
        }
    }
}

void SparkleEffect::SetupRenderState(Camera& camera)
{
    auto& deviceContext = DeviceState::g_pDeviceContext;

    // Apply PSO
    m_pso->Apply();

    // Set render target
    ID3D11RenderTargetView* rtv = camera.m_renderTarget->GetRTV();
    deviceContext->OMSetRenderTargets(1, &rtv, DeviceState::g_pDepthStencilView);

    // Set blend state
    deviceContext->OMSetBlendState(m_pso->m_blendState, nullptr, 0xFFFFFFFF);

    // Set pipeline state
    deviceContext->IASetPrimitiveTopology(m_pso->m_primitiveTopology);
    deviceContext->IASetInputLayout(m_pso->m_inputLayout);

    // Set shaders
    deviceContext->VSSetShader(m_pso->m_vertexShader->GetShader(), nullptr, 0);
    deviceContext->GSSetShader(m_pso->m_geometryShader->GetShader(), nullptr, 0);
    deviceContext->PSSetShader(m_pso->m_pixelShader->GetShader(), nullptr, 0);

    // Set sparkle-specific constant buffer
    DirectX11::PSSetConstantBuffer(3, 1, m_constantBuffer.GetAddressOf());

    // Set texture
    ID3D11ShaderResourceView* srv = m_sparkleTexture->m_pSRV;
    DirectX11::PSSetShaderResources(0, 1, &srv);
}

void SparkleEffect::SetParameters(SparkleParameters* params)
{
    if (params) {
        *m_sparkleParams = *params;
        UpdateSparkleConstantBuffer();
    }
}
