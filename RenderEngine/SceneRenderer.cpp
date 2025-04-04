#include "SceneRenderer.h"
#include "SceneRenderer.h"
#include "DeviceState.h"
#include "AssetSystem.h"
#include "Scene.h"
#include "ImGuiRegister.h"
#include "Banchmark.hpp"
#include "RenderScene.h"
#include "../ScriptBinder/Scene.h"
#include "../ScriptBinder/Renderer.h"

#include "IconsFontAwesome6.h"
#include "fa.h"

using namespace lm;
#pragma region ImGuizmo
#include "ImGuizmo.h"

static const float identityMatrix[16] = { 
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f 
};
bool useWindow = true;
bool editWindow = true;
int gizmoCount = 1;
float camDistance = 8.f;
static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);


void SceneRenderer::EditTransform(float* cameraView, float* cameraProjection, float* matrix, bool editTransformDecomposition, GameObject* obj, Camera* cam)
{
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
	static bool useSnap = false;
	static float snap[3] = { 1.f, 1.f, 1.f };
	static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
	static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
	static bool boundSizing = false;
	static bool boundSizingSnap = false;

	ImGuizmo::SetOrthographic(m_pEditorCamera->m_isOrthographic);
	ImGuizmo::BeginFrame();

	if (editTransformDecomposition)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_T))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_G)) // r Key
			mCurrentGizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_F))
			useSnap = !useSnap;
	}

	ImGuiIO& io = ImGui::GetIO();
	float viewManipulateRight = io.DisplaySize.x;
	float viewManipulateTop = 0;
	static ImGuiWindowFlags gizmoWindowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (useWindow)
	{
		ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImVec4)ImColor(0.f, 0.f, 0.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::Begin(ICON_FA_USERS_VIEWFINDER "  Scene      ", 0, gizmoWindowFlags);
		ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());
		ImGuizmo::SetDrawlist();

		float windowWidth = (float)ImGui::GetWindowWidth();
		float windowHeight = (float)ImGui::GetWindowHeight();
		ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);
		viewManipulateRight = ImGui::GetWindowPos().x + windowWidth;
		viewManipulateTop = ImGui::GetWindowPos().y;
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		gizmoWindowFlags |= ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(window->InnerRect.Min, window->InnerRect.Max) ? ImGuiWindowFlags_NoMove : 0;

		float x = windowWidth;//window->InnerRect.Max.x - window->InnerRect.Min.x;
		float y = windowHeight;//window->InnerRect.Max.y - window->InnerRect.Min.y;


		ImGui::Image((ImTextureID)cam->m_renderTarget->m_pSRV, ImVec2(x, y));

        ImVec2 imagePos = ImGui::GetItemRectMin();
        ImGui::SetCursorScreenPos(ImVec2(imagePos.x + 5, imagePos.y + 5));

		if (ImGui::Button(ICON_FA_BARS " Grid"))
		{
			m_bShowGridSettings = true;
		}

		ImGui::SameLine();

		ImVec2 currentPos = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(ImVec2(currentPos.x + 5, currentPos.y));

		if (ImGui::Button(m_pEditorCamera->m_isOrthographic ? ICON_FA_EYE " Orthographic" : ICON_FA_EYE " Perspective"))
		{
			m_pEditorCamera->m_isOrthographic = !m_pEditorCamera->m_isOrthographic;
		}

		currentPos = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(ImVec2(currentPos.x + 5, currentPos.y));

		if (ImGui::Button(ICON_FA_CAMERA " Camera"))
		{
			m_pEditorCamera->m_isOrthographic = !m_pEditorCamera->m_isOrthographic;
		}

		ImGui::PopStyleVar(2);
	}
	else
	{
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	}

	if (obj)
	{
		// 기즈모로 변환 후 오브젝트에 적용.
		ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

		auto parentMat = m_currentScene->GetGameObject(obj->m_parentIndex)->m_transform.GetWorldMatrix();
		XMMATRIX parentWorldInverse = XMMatrixInverse(nullptr, parentMat);

		XMMATRIX newLocalMatrix = XMMatrixMultiply(XMMATRIX(matrix), parentWorldInverse);
		obj->m_transform.SetLocalMatrix(newLocalMatrix);
	}

	ImGuizmo::ViewManipulate(cameraView, camDistance, ImVec2(viewManipulateRight - 128, viewManipulateTop + 30), ImVec2(128, 128), 0x10101010);

	{
		// 기즈모로 변환된 카메라 위치, 회전 적용
		XMVECTOR poss;
		XMVECTOR rots;
		XMVECTOR scales;
		XMMatrixDecompose(&scales, &rots, &poss, XMMatrixInverse(nullptr, XMMATRIX(cameraView)));
		cam->m_eyePosition = poss;
		cam->m_rotation = rots;

		XMVECTOR rotDir = XMVector3Rotate(cam->FORWARD, rots);

		cam->m_forward = rotDir;
	}

	if (useWindow)
	{
		ImGui::End();
		ImGui::PopStyleColor(1);
	}
}
#pragma endregion

SceneRenderer::SceneRenderer(const std::shared_ptr<DirectX11::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources)
{
    InitializeDeviceState();
    InitializeImGui();

	//sampler 생성
	m_linearSampler = new Sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	m_pointSampler = new Sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

	AssetsSystems->LoadShaders();

	InitializeTextures();

	Texture* ao = Texture::Create(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"AmbientOcclusion",
		DXGI_FORMAT_R16_UNORM,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
	);
	ao->CreateRTV(DXGI_FORMAT_R16_UNORM);
	ao->CreateSRV(DXGI_FORMAT_R16_UNORM);
	m_ambientOcclusionTexture = std::unique_ptr<Texture>(std::move(ao));

	//Buffer 생성
	XMMATRIX identity = XMMatrixIdentity();

	m_ModelBuffer = DirectX11::CreateBuffer(sizeof(Mathf::xMatrix), D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER, &Mathf::xMatrixIdentity);
	DirectX::SetName(m_ModelBuffer.Get(), "ModelBuffer");

	m_pEditorCamera = std::make_unique<Camera>();
	m_pEditorCamera->RegisterContainer();
	m_pEditorCamera->m_applyRenderPipelinePass.m_GridPass = true;

    //pass 생성
    //shadowMapPass 는 RenderScene의 맴버
    //gBufferPass
    m_pGBufferPass = std::make_unique<GBufferPass>();
	ID3D11RenderTargetView* views[]{
		m_diffuseTexture->GetRTV(),
		m_metalRoughTexture->GetRTV(),
		m_normalTexture->GetRTV(),
		m_emissiveTexture->GetRTV()
	};
	m_pGBufferPass->SetRenderTargetViews(views, ARRAYSIZE(views));

    //ssaoPass
    m_pSSAOPass = std::make_unique<SSAOPass>();
    m_pSSAOPass->Initialize(
        ao,
        m_deviceResources->GetDepthStencilViewSRV(),
        m_normalTexture.get()
    );

    //deferredPass
    m_pDeferredPass = std::make_unique<DeferredPass>();
    m_pDeferredPass->Initialize(
        m_diffuseTexture.get(),
        m_metalRoughTexture.get(),
        m_normalTexture.get(),
        m_emissiveTexture.get()
    );

	//skyBoxPass
	m_pSkyBoxPass = std::make_unique<SkyBoxPass>();
	m_pSkyBoxPass->Initialize(PathFinder::Relative("HDR/rosendal_park_sunset_puresky_4k.hdr").string());
	

	//toneMapPass
	m_pToneMapPass = std::make_unique<ToneMapPass>();
	m_pToneMapPass->Initialize(
		m_toneMappedColourTexture.get()
	);

	//spritePass
	m_pSpritePass = std::make_unique<SpritePass>();
	m_pSpritePass->Initialize(m_toneMappedColourTexture.get());

	//blitPass
	m_pBlitPass = std::make_unique<BlitPass>();
	m_pBlitPass->Initialize(m_deviceResources->GetBackBufferRenderTargetView());

	//WireFramePass
	m_pWireFramePass = std::make_unique<WireFramePass>();

	//GridPass
    m_pGridPass = std::make_unique<GridPass>();

	//LightmapShadowPass
	m_pLightmapShadowPass = std::make_unique<LightmapShadowPass>();
	//m_pLightmapShadowPass->Initialize(8192, 8192);

	//PositionMapPass
	m_pPositionMapPass = std::make_unique<PositionMapPass>();
	m_pNormalMapPass = std::make_unique<NormalMapPass>();

	m_pUIPass = std::make_unique<UIPass>();
	m_pUIPass->Initialize(m_toneMappedColourTexture.get());

	//AAPass
	m_pAAPass = std::make_unique<AAPass>();

	m_pPostProcessingPass = std::make_unique<PostProcessingPass>();

	//lightmapPass
	m_pLightMapPass = std::make_unique<LightMapPass>();

	m_renderScene = new RenderScene();
}


void SceneRenderer::InitializeDeviceState()
{
    DeviceState::g_pDevice = m_deviceResources->GetD3DDevice();
    DeviceState::g_pDeviceContext = m_deviceResources->GetD3DDeviceContext();
    DeviceState::g_pDepthStencilView = m_deviceResources->GetDepthStencilView();
    DeviceState::g_pDepthStencilState = m_deviceResources->GetDepthStencilState();
    DeviceState::g_pRasterizerState = m_deviceResources->GetRasterizerState();
    DeviceState::g_pBlendState = m_deviceResources->GetBlendState();
    DeviceState::g_Viewport = m_deviceResources->GetScreenViewport();
    DeviceState::g_backBufferRTV = m_deviceResources->GetBackBufferRenderTargetView();
    DeviceState::g_depthStancilSRV = m_deviceResources->GetDepthStencilViewSRV();
    DeviceState::g_ClientRect = m_deviceResources->GetOutputSize();
    DeviceState::g_aspectRatio = m_deviceResources->GetAspectRatio();
}

void SceneRenderer::InitializeImGui()
{
    static int lightIndex = 0;

	ImGui::ContextRegister("RenderPass", true, [&]()
	{
		if (ImGui::CollapsingHeader("ShadowPass"))
		{
			m_renderScene->m_LightController->m_shadowMapPass->ControlPanel();

			ImGui::Image(
				(ImTextureID)m_renderScene->m_LightController->m_shadowMapPass->m_shadowMapTexture->m_pSRV,
				ImVec2(512, 512));
			ImGui::Image(
				(ImTextureID)m_pLightmapShadowPass->m_shadowmapTextures[0]->m_pSRV,
				ImVec2(512, 512));
			ImGui::Image(
				(ImTextureID)m_pLightmapShadowPass->m_shadowmapTextures[1]->m_pSRV,
				ImVec2(512, 512));
			//ImGui::EndTabItem();
		}

		if (ImGui::CollapsingHeader("SSAOPass"))
		{
			m_pSSAOPass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("DeferredPass"))
		{
			m_pDeferredPass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("SkyBoxPass"))
		{
			m_pSkyBoxPass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("ToneMapPass"))
		{
			m_pToneMapPass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("SpritePass"))
		{
			m_pSpritePass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("AAPass"))
		{
			m_pAAPass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("BlitPass"))
		{
			m_pBlitPass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("WireFramePass"))
		{
			m_pWireFramePass->ControlPanel();
		}

		if (ImGui::CollapsingHeader("GridPass"))
		{
			m_pGridPass->ControlPanel();
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("PostProcessPass"))
		{
			m_pPostProcessingPass->ControlPanel();
		}
	});

	ImGui::GetContext("RenderPass").Close();

    ImGui::ContextRegister("Light", true, [&]()
    {
        ImGui::Text("Light Index : %d", lightIndex);
        if (ImGui::Button("Add Light")) {
            Light light;
            light.m_color = XMFLOAT4(1, 1, 1, 1);
            light.m_position = XMFLOAT4(0, 0, 0, 0);
            light.m_lightType = LightType::PointLight;

            m_renderScene->m_LightController->AddLight(light);
        }
        if (ImGui::Button("Light index + ")) {
            lightIndex++;
            if (lightIndex >= MAX_LIGHTS) lightIndex = MAX_LIGHTS - 1;
        }
        if (ImGui::Button("Light index - ")) {
            lightIndex--;
            if (lightIndex < 0) lightIndex = 0;
        }
        if (ImGui::Button("Light On")) {
			m_renderScene->m_LightController->GetLight(lightIndex).m_lightStatus = LightStatus::Enabled;
        }
        if (ImGui::Button("StaticShadow On")) {
			m_renderScene->m_LightController->GetLight(lightIndex).m_lightStatus = LightStatus::StaticShadows;
        }
        if (ImGui::Button("Light Off")) {
			m_renderScene->m_LightController->GetLight(lightIndex).m_lightStatus = LightStatus::Disabled;
        }

        ImGui::DragFloat3("Light Pos", &m_renderScene->m_LightController->GetLight(lightIndex).m_position.x, 0.1f, -10, 10);
        ImGui::DragFloat3("Light Dir", &m_renderScene->m_LightController->GetLight(lightIndex).m_direction.x, 0.1f, -1, 1);
        ImGui::DragFloat("Light colorX", &m_renderScene->m_LightController->GetLight(lightIndex).m_color.x, 0.1f, 0, 1);
        ImGui::DragFloat("Light colorY", &m_renderScene->m_LightController->GetLight(lightIndex).m_color.y, 0.1f, 0, 1);
        ImGui::DragFloat("Light colorZ", &m_renderScene->m_LightController->GetLight(lightIndex).m_color.z, 0.1f, 0, 1);
    });
}

void SceneRenderer::InitializeTextures()
{
	m_diffuseTexture = TextureHelper::CreateRenderTexture(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"DiffuseRTV",
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);

	m_metalRoughTexture = TextureHelper::CreateRenderTexture(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"MetalRoughRTV",
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);

	m_normalTexture = TextureHelper::CreateRenderTexture(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"NormalRTV",
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);

	m_emissiveTexture = TextureHelper::CreateRenderTexture(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"EmissiveRTV",
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);

	m_toneMappedColourTexture = TextureHelper::CreateRenderTexture(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"ToneMappedColourRTV",
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);

	m_gridTexture = TextureHelper::CreateRenderTexture(
		DeviceState::g_ClientRect.width,
		DeviceState::g_ClientRect.height,
		"GridRTV",
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);
}

void SceneRenderer::Initialize(Scene* _pScene)
{
	if (!_pScene)
	{
		m_currentScene = Scene::CreateNewScene();
		m_renderScene->SetScene(m_currentScene);
		m_renderScene->Initialize();

		auto lightColour = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		
		Light pointLight;
		pointLight.m_color = XMFLOAT4(1, 1, 0, 0);
		pointLight.m_position = XMFLOAT4(4, 3, 0, 0);
		pointLight.m_direction = XMFLOAT4(1, -1, 0, 0);
		pointLight.m_lightType = LightType::DirectionalLight;

		Light dirLight;
		dirLight.m_color = lightColour;
		dirLight.m_direction = XMFLOAT4(-1, -1, 1, 0);
		dirLight.m_lightType = LightType::DirectionalLight;

		Light spotLight;
		spotLight.m_color = XMFLOAT4(Colors::Magenta);
		spotLight.m_direction = XMFLOAT4(0, -1, 0, 0);
		spotLight.m_position = XMFLOAT4(3, 2, 0, 0);
		spotLight.m_lightType = LightType::SpotLight;
		spotLight.m_spotLightAngle = 3.142 / 4.0;

		m_renderScene->m_LightController
			->AddLight(dirLight)
			.AddLight(pointLight)
			.AddLight(spotLight)
			.SetGlobalAmbient(XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f));

		ShadowMapRenderDesc desc;
		desc.m_eyePosition = XMLoadFloat4(&(m_renderScene->m_LightController->GetLight(0).m_direction)) * -5.f;
		desc.m_lookAt = XMVectorSet(0, 0, 0, 1);
		desc.m_viewWidth = 16;
		desc.m_viewHeight = 12;
		desc.m_nearPlane = 1.f;
		desc.m_farPlane = 20.f;
		desc.m_textureWidth = 1920;
		desc.m_textureHeight = 1080;

		m_renderScene->m_LightController->Initialize();
		m_renderScene->m_LightController->SetLightWithShadows(0, desc);

		model = Model::LoadModel("plane.fbx");
		Model::LoadModelToScene(model, *m_currentScene);
		model = Model::LoadModel("Model/FlightHelmet.gltf");

		ImGui::ContextRegister("Test UI", true, [&]()
		{
			if (ImGui::Button("Add UI"))
			{
				m_pUIPass->pushUI(&testUI);
			}
			ImGui::SliderFloat3("pos", &testUI.trans.x, -1, 1);
			ImGui::SliderFloat3("scale", &testUI.scale.x, -1, 1);

		});

		ImGui::ContextRegister("Test Add Model", true, [&]()
		{
			if (ImGui::Button("Add Model"))
			{
				Model::LoadModelToScene(model, *m_currentScene);
			}
		});
	}
	else
	{
		m_currentScene = _pScene;
		m_renderScene->SetScene(m_currentScene);
	}

	m_renderScene->SetBuffers(m_ModelBuffer.Get());

	DeviceState::g_pDeviceContext->PSSetSamplers(0, 1, &m_linearSampler->m_SamplerState);
	DeviceState::g_pDeviceContext->PSSetSamplers(1, 1, &m_pointSampler->m_SamplerState);

	m_pSkyBoxPass->GenerateCubeMap(*m_renderScene);
	Texture* envMap = m_pSkyBoxPass->GenerateEnvironmentMap(*m_renderScene);
	Texture* preFilter = m_pSkyBoxPass->GeneratePrefilteredMap(*m_renderScene);
	Texture* brdfLUT = m_pSkyBoxPass->GenerateBRDFLUT(*m_renderScene);

	m_pDeferredPass->UseEnvironmentMap(envMap, preFilter, brdfLUT);
}

void SceneRenderer::OnWillRenderObject(float deltaTime)
{
	m_renderScene->Update(deltaTime);
	m_pEditorCamera->HandleMovement(deltaTime);
	PrepareRender();
	m_pUIPass->Update(deltaTime);
}

void SceneRenderer::SceneRendering()
{
	DirectX11::ResetCallCount();


	//Camera c{};
	//// 메쉬별로 positionMap 생성
	//m_pPositionMapPass->Execute(*m_renderScene, c);
	//m_pNormalMapPass->Execute(*m_renderScene, c);
	//// lightMap에 사용할 shadowMap 생성
	//m_pLightmapShadowPass->Execute(*m_renderScene, c);
	//// lightMap 생성
	//lightMap.GenerateLightMap(m_renderScene, m_pLightmapShadowPass, m_pPositionMapPass, m_pNormalMapPass);



	for(auto& camera : CameraManagement->m_cameras)
	{
		if (nullptr == camera) continue;
		//[1] ShadowMapPass
		{
			static int count = 0;
			Banchmark banch;
			camera->ClearRenderTarget();
			m_renderScene->ShadowStage(*camera);
			Clear(DirectX::Colors::Transparent, 1.0f, 0);
			UnbindRenderTargets();
			//Debug->Log("ShadowMapPass : " + std::to_string(banch.GetElapsedTime()));
		}

		if(!useTestLightmap){
			//[2] GBufferPass
			{
				//Banchmark banch;
				m_pGBufferPass->Execute(*m_renderScene, *camera);

				//std::cout << "GBufferPass : " << banch.GetElapsedTime() << std::endl;
			}

			//[3] SSAOPass
			{
				//Banchmark banch;
				m_pSSAOPass->Execute(*m_renderScene, *camera);
				//std::cout << "GBufferPass : " << banch.GetElapsedTime() << std::endl;
			}

			//[4] DeferredPass
			{
				Banchmark banch;
				m_pDeferredPass->UseAmbientOcclusion(m_ambientOcclusionTexture.get());
				m_pDeferredPass->Execute(*m_renderScene, *camera);
				Debug->Log("DeferredPass : " + std::to_string(banch.GetElapsedTime()));
			}
		}
		else {
			m_pLightMapPass->Execute(*m_renderScene, *camera);
		}

		//[*] WireFramePass
		if (useWireFrame)
		{
			//Banchmark banch;
			m_pWireFramePass->Execute(*m_renderScene, *camera);

			//std::cout << "WireFramePass : " << banch.GetElapsedTime() << std::endl;
		}

		//[5] skyBoxPass
		{
			//Banchmark banch;
			m_pSkyBoxPass->Execute(*m_renderScene, *camera);

			//std::cout << "SkyBoxPass : " << banch.GetElapsedTime() << std::endl;
		}

		//[8] AAPass
		{
			//Banchmark banch;
			m_pAAPass->Execute(*m_renderScene, *camera);
			//std::cout << "AAPass : " << banch.GetElapsedTime() << std::endl;
		}

		//[*] PostProcessPass
		{
			m_pPostProcessingPass->Execute(*m_renderScene, *camera);
		}

		//[6] ToneMapPass
		{
			//Banchmark banch;
			m_pToneMapPass->Execute(*m_renderScene, *camera);

			//std::cout << "ToneMapPass : " << banch.GetElapsedTime() << std::endl;
		}

		//[*] GridPass
		{
			//Banchmark banch;
			m_pGridPass->Execute(*m_renderScene, *camera);

			//std::cout << "GridPass : " << banch.GetElapsedTime() << std::endl;
		}

		//[7] SpritePass
		{
			//Banchmark banch;
			m_pSpritePass->Execute(*m_renderScene, *camera);

			//std::cout << "SpritePass : " << banch.GetElapsedTime() << std::endl;
		}

		//[]  UIPass
		{
			m_pUIPass->Execute(*m_renderScene, *camera);
		}

		//[8] BlitPass
		{
			//Banchmark banch;
			m_pBlitPass->Execute(*m_renderScene, *camera);
			//std::cout << "BlitPass : " << banch.GetElapsedTime() << std::endl;
		}
	}

	//Debug->Log("Draw Call Count : " + std::to_string(DirectX11::GetDrawCallCount()));
	m_pGBufferPass->ClearDeferredQueue();
}

void SceneRenderer::PrepareRender()
{
	for (auto& obj : m_currentScene->m_SceneObjects)
	{
		MeshRenderer* meshRenderer = obj->GetComponent<MeshRenderer>();
		if (nullptr == meshRenderer) continue;
		if (false == meshRenderer->IsEnabled()) continue;

		Material* mat = meshRenderer->m_Material;

		switch (mat->m_renderingMode)
		{
		case Material::RenderingMode::Opaque:
			m_pGBufferPass->PushDeferredQueue(obj.get());

			break;
		case Material::RenderingMode::Transparent:
			break;
		}
	}
}

void SceneRenderer::Clear(const float color[4], float depth, uint8_t stencil)
{
	DirectX11::ClearRenderTargetView(
		m_deviceResources->GetBackBufferRenderTargetView(),
		DirectX::Colors::Transparent
	);

	DirectX11::ClearDepthStencilView(
		m_deviceResources->GetDepthStencilView(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0
	);
}

void SceneRenderer::SetRenderTargets(Texture& texture, bool enableDepthTest)
{
	ID3D11DepthStencilView* dsv = enableDepthTest ? m_deviceResources->GetDepthStencilView() : nullptr;
	ID3D11RenderTargetView* rtv = texture.GetRTV();

	DirectX11::OMSetRenderTargets(1, &rtv, dsv);
}

void SceneRenderer::UnbindRenderTargets()
{
	ID3D11RenderTargetView* nullRTV = nullptr;
	ID3D11DepthStencilView* nullDSV = nullptr;
	DirectX11::OMSetRenderTargets(1, &nullRTV, nullDSV);
}

void SceneRenderer::EditorView()
{
    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    float height = ImGui::GetFrameHeight();

    if (ImGui::BeginViewportSideBar("##MainMenuBar", viewport, ImGuiDir_Up, height, window_flags))
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit"))
                {
                    // Exit action
                    PostQuitMessage(0);
                }
                ImGui::EndMenu();

            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Pipeline Setting"))
                {
                    if (!ImGui::GetContext("RenderPass").IsOpened())
                    {
                        ImGui::GetContext("RenderPass").Open();
                    }
                }

                ImGui::EndMenu();
            }

			if (ImGui::BeginMenu("Bake Lightmap"))
			{
				if (ImGui::MenuItem("Bake"))
				{
					Camera c{};
					// 메쉬별로 positionMap 생성
					m_pPositionMapPass->Execute(*m_renderScene, c);
					m_pNormalMapPass->Execute(*m_renderScene, c);
					// lightMap에 사용할 shadowMap 생성
					m_pLightmapShadowPass->Execute(*m_renderScene, c);
					// lightMap 생성
					lightMap.GenerateLightMap(m_renderScene, m_pLightmapShadowPass, m_pPositionMapPass, m_pNormalMapPass);

					m_pLightMapPass->Initialize(lightMap.lightmaps);


					if (lightMap.imgSRV) {
						ImGui::ContextUnregister("Baked LightMap");

						ImGui::ContextRegister("Baked LightMap", true, [&]() {
							ImGui::Image((ImTextureID)lightMap.imgSRV, ImVec2(512, 512));
							//ImGui::Image((ImTextureID)lightMap.structuredBufferSRV, ImVec2(512, 512));
							for(int i = 0; i < m_pLightmapShadowPass->m_shadowmapTextures.size(); i++)
								ImGui::Image((ImTextureID)m_pLightmapShadowPass->m_shadowmapTextures[i]->m_pSRV,
									ImVec2(512, 512));

							/*for (auto& [name, tex] : m_pPositionMapPass->m_positionMapTextures)
							{
								ImGui::Image((ImTextureID)tex->m_pSRV, ImVec2(512, 512));
							}*/
							});
					}
				}
				ImGui::EndMenu();
			}
            ImGui::EndMainMenuBar();
        }
        ImGui::End();
    }

    if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags)) {
        if (ImGui::BeginMenuBar()) {

			ImGui::Button(ICON_FA_FOLDER_TREE " Content Drawer");

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_CODE " Output Log :"))
			{
				m_bShowLogWindow = true;
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 255, 255));
			ImVec2 pos = ImGui::GetCursorScreenPos();
			ImVec2 size = ImGui::CalcTextSize(Debug->GetBackLogMessage().c_str());

			if (0 == size.x)
			{
				size.x = 10.f;
			}

			if (ImGui::InvisibleButton("##last_log_btn", size)) 
			{
				m_bShowLogWindow = true;
			}
			bool hovered = ImGui::IsItemHovered();
			bool clicked = ImGui::IsItemClicked();

			if (hovered) 
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}

			// 텍스트를 그리기
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			pos.y += 5;
			drawList->AddText(pos, IM_COL32(255, 255, 255, 255), Debug->GetBackLogMessage().c_str());

			//ImGui::Text();
			ImGui::PopStyleColor();

            ImGui::EndMenuBar();
        }
        ImGui::End();
    }

	if (m_bShowLogWindow)
	{
		ShowLogWindow();
	}

	if (m_bShowGridSettings)
	{
		ShowGridSettings();
	}

	auto obj = m_renderScene->GetSelectSceneObject();
	if (obj) 
	{
		auto mat = obj->m_transform.GetWorldMatrix();
		XMFLOAT4X4 objMat;
		XMStoreFloat4x4(&objMat, mat);
		auto view = m_pEditorCamera->CalculateView();
		XMFLOAT4X4 floatMatrix;
		XMStoreFloat4x4(&floatMatrix, view);
		auto proj = m_pEditorCamera->CalculateProjection();
		XMFLOAT4X4 projMatrix;
		XMStoreFloat4x4(&projMatrix, proj);

		EditTransform(&floatMatrix.m[0][0], &projMatrix.m[0][0], &objMat.m[0][0], true, obj, m_pEditorCamera.get());

	}
	else
	{
		auto view = m_pEditorCamera->CalculateView();
		XMFLOAT4X4 floatMatrix;
		XMStoreFloat4x4(&floatMatrix, view);
		auto proj = m_pEditorCamera->CalculateProjection();
		XMFLOAT4X4 projMatrix;
		XMStoreFloat4x4(&projMatrix, proj);
		XMFLOAT4X4 identityMatrix;
		XMStoreFloat4x4(&identityMatrix, XMMatrixIdentity());

		EditTransform(&floatMatrix.m[0][0], &projMatrix.m[0][0], &identityMatrix.m[0][0], false, nullptr, m_pEditorCamera.get());
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(ICON_FA_GAMEPAD "  Game        ", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());
	{
		ImVec2 availRegion = ImGui::GetContentRegionAvail();

		// 이미지의 높이를 영역의 높이로 설정하고, 가로는 aspect ratio를 반영하여 계산
		float imageHeight = availRegion.y;
		float imageWidth = imageHeight * DeviceState::g_aspectRatio;

		if (imageWidth > availRegion.x) {
			imageWidth = availRegion.x;
			imageHeight = imageWidth / DeviceState::g_aspectRatio;
		}

		ImVec2 imageSize = ImVec2(imageWidth, imageHeight);

		// 이미지가 중앙에 위치하도록 오프셋 계산 (가로, 세로 모두 중앙 정렬)
		ImVec2 offset = ImVec2((availRegion.x - imageSize.x) * 0.5f, (availRegion.y - imageSize.y) * 0.5f);

		ImVec2 currentPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(currentPos.x + offset.x, currentPos.y + offset.y));

		//TODO : 카메라를 컨트롤러에서 찾아서 해당 뷰포트를 보여주도록 변경하고, 
		// 위에 카메라 컨트롤러에 등록된 카메라 번호를 보이게 해야함.
		ImGui::Image((ImTextureID)m_renderScene->m_MainCamera.m_renderTarget->m_pSRV, imageSize);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void SceneRenderer::ShowLogWindow()
{
	static int levelFilter = spdlog::level::trace;
	bool isClear = Debug->IsClear();

	ImGui::Begin("Log", &m_bShowLogWindow, ImGuiWindowFlags_NoDocking);
	if (ImGui::Button("Clear"))
	{
		Debug->Clear();
	}
	ImGui::SameLine();
	ImGui::Combo("Log Filter", &levelFilter,
		"Trace\0Debug\0Info\0Warning\0Error\0Critical\0\0");

	float sizeX = ImGui::GetContentRegionAvail().x;
	float sizeY = ImGui::CalcTextSize(Debug->GetBackLogMessage().c_str()).y;

	if (isClear)
	{
		Debug->toggleClear();
		ImGui::End();
		return;
	}

	auto entries = Debug->get_entries();
	for (size_t i = 0; i < entries.size(); i++)
	{
		const auto& entry = entries[i];
		bool is_selected = (i == selected_log_index);

		if (entry.level != spdlog::level::trace && entry.level < levelFilter)
			continue;

		ImVec4 color;
		switch (entry.level)
		{
		case spdlog::level::info: color = ImVec4(1, 1, 1, 1); break;
		case spdlog::level::warn: color = ImVec4(1, 1, 0, 1); break;
		case spdlog::level::err:  color = ImVec4(1, 0.4f, 0.4f, 1); break;
		default: color = ImVec4(0.7f, 0.7f, 0.7f, 1); break;
		}

		// 선택 시 배경색
		if (is_selected)
			ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(100, 100, 255, 100));

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(color));

		if (ImGui::Selectable(std::string(ICON_FA_INFO + entry.message).c_str(), is_selected, ImGuiSelectableFlags_None, { sizeX , 50 }))
		{
			selected_log_index = i;
		}
		ImGui::PopStyleColor(); // Text color

		if (is_selected)
			ImGui::PopStyleColor(); // Header color
	}

	ImGui::End();


}

void SceneRenderer::ShowGridSettings()
{
	ImGui::Begin("Grid Settings", &m_bShowGridSettings, ImGuiWindowFlags_AlwaysAutoResize);
	m_pGridPass->GridSetting();
	ImGui::End();
}
