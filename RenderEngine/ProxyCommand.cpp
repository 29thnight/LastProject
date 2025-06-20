#include "ProxyCommand.h"
#include "MeshRenderer.h"
#include "RenderScene.h"
#include "SceneManager.h"

constexpr size_t TRANSFORM_SIZE = sizeof(Mathf::xMatrix) * MAX_BONES;

ProxyCommand::ProxyCommand(MeshRenderer* pComponent) :
	m_proxyGUID(pComponent->GetInstanceID())
{
	auto renderScene = SceneManagers->m_ActiveRenderScene;
	auto componentPtr = pComponent;
	auto owner = componentPtr->GetOwner();
	Mathf::xMatrix worldMatrix = owner->m_transform.GetWorldMatrix();

	auto& proxyObject = renderScene->m_proxyMap[m_proxyGUID];
	HashedGuid aniGuid = proxyObject->m_animatorGuid;

	Mathf::xMatrix* palletePtr{ nullptr };
	bool isAnimationUpdate{ false };

	if (renderScene->m_animatorMap.find(aniGuid) != renderScene->m_animatorMap.end()
		&& proxyObject->IsSkinnedMesh())
	{
		palletePtr = renderScene->m_palleteMap[aniGuid].second;
		if (!proxyObject->m_finalTransforms)
		{
			proxyObject->m_finalTransforms = palletePtr;
		}

		if (false == renderScene->m_palleteMap[aniGuid].first)
		{
			auto* srcPalete = &renderScene->m_animatorMap[aniGuid]->m_FinalTransforms;

			memcpy(palletePtr, srcPalete, TRANSFORM_SIZE);
		}

		renderScene->m_palleteMap[aniGuid].first = true;
		isAnimationUpdate = true;
	}

	constexpr int INVAILD_INDEX = -1;

	bool isLightMappingUpdatable{ false };
	LightMapping copyLightMapping{};
	int& lightMapIndex = pComponent->m_LightMapping.lightmapIndex;
	int& proxyLightMapIndex = proxyObject->m_LightMapping.lightmapIndex;

	if (INVAILD_INDEX != lightMapIndex && proxyLightMapIndex != lightMapIndex)
	{
		copyLightMapping = pComponent->m_LightMapping;
		isLightMappingUpdatable = true;
	}

	m_updateFunction = [=]
	{
		if(isAnimationUpdate && palletePtr)
		{
			proxyObject->m_finalTransforms = palletePtr;
		}

		proxyObject->m_worldMatrix = worldMatrix;

		if(isLightMappingUpdatable)
		{
			proxyObject->m_LightMapping = copyLightMapping;
		}
	};
}

ProxyCommand::ProxyCommand(const ProxyCommand& other) :
	m_proxyGUID(other.m_proxyGUID),
	m_updateFunction(other.m_updateFunction)
{
}

ProxyCommand::ProxyCommand(ProxyCommand&& other) noexcept :
	m_proxyGUID(other.m_proxyGUID),
	m_updateFunction(std::move(other.m_updateFunction))
{
}

ProxyCommand& ProxyCommand::operator=(const ProxyCommand& other)
{
	m_proxyGUID = other.m_proxyGUID;
	m_updateFunction = other.m_updateFunction;

	return *this;
}

ProxyCommand& ProxyCommand::operator=(ProxyCommand&& other) noexcept
{
	m_proxyGUID = other.m_proxyGUID;
	m_updateFunction = std::move(other.m_updateFunction);

	return *this;
}

void ProxyCommand::ProxyCommandExecute()
{
	if (!m_updateFunction) throw std::runtime_error("proxy invokable empty");

	m_updateFunction();
}
