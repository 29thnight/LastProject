#include "RenderScene.h"
#include "ImGuiRegister.h"
#include "../ScriptBinder/Scene.h"
#include "LightProperty.h"
#include "../ScriptBinder/RenderableComponents.h"
#include "Skeleton.h"
#include "LightController.h"
#include "Benchmark.hpp"
#include "MeshRenderer.h"
#include "TimeSystem.h"
#include "DataSystem.h"
#include "SceneManager.h"
#include "MeshRendererProxy.h"
#include "ImageComponent.h"
#include "UIManager.h"

constexpr size_t TRANSFORM_SIZE = sizeof(Mathf::xMatrix) * MAX_BONES;
concurrent_queue<HashedGuid> RenderScene::RegisteredDestroyProxyGUIDs;

ShadowMapRenderDesc RenderScene::g_shadowMapDesc{};

RenderScene::~RenderScene()
{
	Memory::SafeDelete(m_LightController);
}

void RenderScene::Initialize()
{
	m_LightController = new LightController();
}

void RenderScene::SetBuffers(ID3D11Buffer* modelBuffer)
{
	m_ModelBuffer = modelBuffer;
}

void RenderScene::Update(float deltaSecond)
{
	m_currentScene = SceneManagers->GetActiveScene();
	if (m_currentScene == nullptr) return;

    m_LightController->m_lightCount = m_currentScene->UpdateLight(m_LightController->m_lightProperties);
}

void RenderScene::ShadowStage(Camera& camera)
{
	m_LightController->SetEyePosition(camera.m_eyePosition);
	m_LightController->Update();
	m_LightController->RenderAnyShadowMap(*this, camera);
}

void RenderScene::CreateShadowCommandList(ID3D11DeviceContext* deferredContext, Camera& camera)
{
	m_LightController->CreateShadowCommandList(deferredContext, *this, camera);
}

void RenderScene::UseModel()
{
	DirectX11::VSSetConstantBuffer(0, 1, &m_ModelBuffer);
}

void RenderScene::UseModel(ID3D11DeviceContext* deferredContext)
{
	deferredContext->VSSetConstantBuffers(0, 1, &m_ModelBuffer);
}

void RenderScene::UpdateModel(const Mathf::xMatrix& model)
{
	DirectX11::UpdateBuffer(m_ModelBuffer, &model);
}

void RenderScene::UpdateModel(const Mathf::xMatrix& model, ID3D11DeviceContext* deferredContext)
{
	deferredContext->UpdateSubresource(m_ModelBuffer, 0, nullptr, &model, 0, 0);
}

RenderPassData* RenderScene::AddRenderPassData(size_t cameraIndex)
{
	auto it = m_renderDataMap.find(cameraIndex);
	if (it != m_renderDataMap.end())
	{
		return it->second.get();
	}

	auto newRenderData = std::make_shared<RenderPassData>();
	newRenderData->Initalize(cameraIndex);
	//m_renderDataMap[cameraIndex] = newRenderData;
	m_renderDataMap.insert({ cameraIndex, newRenderData });

	return newRenderData.get();
}

RenderPassData* RenderScene::GetRenderPassData(size_t cameraIndex)
{
	auto it = m_renderDataMap.find(cameraIndex);
	if (it == m_renderDataMap.end())
	{
		return nullptr;
	}

	return it->second.get();
}

void RenderScene::RemoveRenderPassData(size_t cameraIndex)
{
	auto it = m_renderDataMap.find(cameraIndex);
	if (it != m_renderDataMap.end())
	{
		it->second->m_isDestroy = true;
	}
}

void RenderScene::EraseRenderPassData()
{
	auto begin = m_renderDataMap.unsafe_begin(0);
	auto end = m_renderDataMap.unsafe_end(0);

	for(auto it = begin; it != end;)
	{
		if (it->second->m_isDestroy)
		{
			it->second.reset();
			m_renderDataMap.unsafe_erase(it->first);
			it = m_renderDataMap.unsafe_begin(0);
		}
		else
		{
			++it;
		}
	}
}

void RenderScene::RegisterAnimator(Animator* animatorPtr)
{
	if (nullptr == animatorPtr) return;

	HashedGuid animatorGuid = animatorPtr->GetInstanceID();

	if (m_animatorMap.find(animatorGuid) != m_animatorMap.end()) return;

	m_animatorMap[animatorGuid] = animatorPtr;

	void* voidPtr = std::malloc(TRANSFORM_SIZE);
	if(voidPtr)
	{
		m_palleteMap[animatorGuid].second = (Mathf::xMatrix*)voidPtr;
	}
}

void RenderScene::UnregisterAnimator(Animator* animatorPtr)
{
	if (nullptr == animatorPtr) return;

	HashedGuid animatorGuid = animatorPtr->GetInstanceID();

	if (m_animatorMap.find(animatorGuid) != m_animatorMap.end()) return;
	if (m_palleteMap.find(animatorGuid) != m_palleteMap.end()) return;

	m_animatorMap.erase(animatorGuid);

	if (m_palleteMap[animatorGuid].second)
	{
		free(m_palleteMap[animatorGuid].second);
		m_palleteMap.erase(animatorGuid);
	}
}

void RenderScene::RegisterCommand(MeshRenderer* meshRendererPtr)
{
	if (nullptr == meshRendererPtr) return;

	HashedGuid meshRendererGuid = meshRendererPtr->GetInstanceID();

	SpinLock lock(m_proxyMapFlag);

	if (m_proxyMap.find(meshRendererGuid) != m_proxyMap.end()) return;

	// Create a new proxy for the mesh renderer and insert it into the map
	auto managedCommand = std::make_shared<PrimitiveRenderProxy>(meshRendererPtr);
	m_proxyMap[meshRendererGuid] = managedCommand;
}

bool RenderScene::InvaildCheckMeshRenderer(MeshRenderer* meshRendererPtr)
{
	if (nullptr == meshRendererPtr || meshRendererPtr->IsDestroyMark()) return false;

	auto owner = meshRendererPtr->GetOwner();
	if (nullptr == owner || owner->IsDestroyMark()) return false;

	HashedGuid meshRendererGuid = meshRendererPtr->GetInstanceID();

	SpinLock lock(m_proxyMapFlag);

	if (m_proxyMap.find(meshRendererGuid) == m_proxyMap.end()) return false;

	auto& proxyObject = m_proxyMap[meshRendererGuid];

	if (nullptr == proxyObject) return false;

	return true;
}

PrimitiveRenderProxy* RenderScene::FindProxy(size_t guid)
{
	SpinLock lock(m_proxyMapFlag);

	if (m_proxyMap.find(guid) == m_proxyMap.end()) return nullptr;

	return m_proxyMap[guid].get();
}

void RenderScene::OnProxyDestroy()
{
	while (!RenderScene::RegisteredDestroyProxyGUIDs.empty())
	{
		HashedGuid ID;
		if (RenderScene::RegisteredDestroyProxyGUIDs.try_pop(ID))
		{
			{
				SpinLock lock(m_proxyMapFlag);
				m_proxyMap.erase(ID);
			}
		}
	}

	for (auto& [guid, pair] : m_palleteMap)
	{
		auto& isUpdated = pair.first;

		isUpdated = false;
	}
}

void RenderScene::UpdateCommand(MeshRenderer* meshRendererPtr)
{
	ProxyCommand moveCommand = MakeProxyCommand(meshRendererPtr);
	ProxyCommandQueue->PushProxyCommand(std::move(moveCommand));
}

ProxyCommand RenderScene::MakeProxyCommand(MeshRenderer* meshRendererPtr)
{
	if (!InvaildCheckMeshRenderer(meshRendererPtr)) 
	{
		throw std::runtime_error("InvaildCheckMeshRenderer");
	}

	ProxyCommand command(meshRendererPtr);
	return command;
}

void RenderScene::UnregisterCommand(MeshRenderer* meshRendererPtr)
{
	if (nullptr == meshRendererPtr) return;

	HashedGuid meshRendererGuid = meshRendererPtr->GetInstanceID();

	SpinLock lock(m_proxyMapFlag);
	
	if (m_proxyMap.find(meshRendererGuid) == m_proxyMap.end()) return;

	m_proxyMap[meshRendererGuid]->DestroyProxy();
}