#include "SceneManager.h"
#include "Scene.h"
#include "Object.h"
#include "FileIO.h"
#include "DataSystem.h"
#include "ComponentFactory.h"
#include "RegisterReflect.def"
#include "CullingManager.h"
#include "Profiler.h"
#include "InputActionManager.h"
#include "NodeFactory.h"
#include "TagManager.h"
#include "GameObjectPool.h"

void SceneManager::ManagerInitialize()
{
    REFLECTION_REGISTER_EXECUTE();
	ComponentFactorys->Initialize();
	m_threadPool = new ThreadPool;
    m_inputActionManager = new InputActionManager();
    InputActionManagers = m_inputActionManager;
	TagManager::GetInstance()->Initialize();
}

void SceneManager::Editor()
{
    PROFILE_CPU_BEGIN("Editor");
    if(m_isGameStart && !m_isEditorSceneLoaded)
    {
        CreateEditorOnlyPlayScene();
        m_activeScene.load()->Reset();
		m_isEditorSceneLoaded = true;
    }
    else if (!m_isGameStart && m_isEditorSceneLoaded)
    {
        DeleteEditorOnlyPlayScene();
    }

    if (!m_isGameStart)
    {
		m_inputActionManager->ClearActionMaps();  //&&&&&TODO:게임스타트떄 한번만 초기화하고 다시들어가게
        ScriptManager->ReloadDynamicLibrary();
        m_isInitialized = false; // Reset initialization state for editor scene
		m_activeScene.load()->Awake();
	}
    PROFILE_CPU_END();
}

void SceneManager::Initialization()
{
    if(!m_isInitialized)
    {
        ScriptManager->UpdateSceneManager(SceneManager::GetInstance);
        ScriptManager->UpdateBTNodeFactory(BT::NodeFactory::GetInstance);
        ScriptManager->UpdatePhysicsManager(PhysicsManager::GetInstance);
        ScriptManager->UpdatePhysx(PhysicX::GetInstance);
		ScriptManager->UpdateObjectAllocFunc(GameObjectPool::GetInstance);
		m_isInitialized = true;
    }

    if (m_loadingSceneFuture.valid() &&
        m_loadingSceneFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            // .get() retrieves the result. It will re-throw any exception caught in the async task.
            Scene* loadedScene = m_loadingSceneFuture.get();
            if (loadedScene)
            {
                // The scene is loaded, now activate it on the main thread.
                ActivateScene(loadedScene);
            }
        }
        catch (const std::exception& e)
        {
            Debug->LogError("Failed to activate loaded scene.");
            // Handle loading failure
        }
        // The future is now invalid after .get(), so this block won't run again until a new scene is loaded.
    }

    PROFILE_CPU_BEGIN("Awake");
	m_activeScene.load()->Awake();
    PROFILE_CPU_END();
    PROFILE_CPU_BEGIN("OnEnable");
    m_activeScene.load()->OnEnable();
    PROFILE_CPU_END();
    PROFILE_CPU_BEGIN("Start");
    m_activeScene.load()->Start();
    PROFILE_CPU_END();
}

void SceneManager::Physics(float deltaSecond)
{
    PROFILE_CPU_BEGIN("FixedUpdate");
    m_activeScene.load()->FixedUpdate(deltaSecond);
    PROFILE_CPU_END();
}

void SceneManager::InputEvents(float deltaSecond)
{
    PROFILE_CPU_BEGIN("InputEvents");
    InputEvent.Broadcast(deltaSecond);
    PROFILE_CPU_END();
}

void SceneManager::GameLogic(float deltaSecond)
{
    PROFILE_CPU_BEGIN("Update");
    m_activeScene.load()->Update(deltaSecond);
    PROFILE_CPU_END();

    PROFILE_CPU_BEGIN("YieldNull");
    m_activeScene.load()->YieldNull();
    PROFILE_CPU_END();

    PROFILE_CPU_BEGIN("InternalAnimationUpdateEvent");
    InternalAnimationUpdateEvent.Broadcast(deltaSecond);
    PROFILE_CPU_END();

    PROFILE_CPU_BEGIN("LateUpdate");
    m_activeScene.load()->LateUpdate(deltaSecond);
    PROFILE_CPU_END();
}

void SceneManager::SceneRendering(float deltaSecond)
{
    SceneRenderingEvent.Broadcast(deltaSecond);
}

void SceneManager::OnDrawGizmos()
{
	OnDrawGizmosEvent.Broadcast();
}

void SceneManager::GUIRendering()
{
    GUIRenderingEvent.Broadcast();
}

void SceneManager::EndOfFrame()
{
	CoroutineManagers->yield_WaitForEndOfFrame();
    endOfFrameEvent.Broadcast();
}

void SceneManager::Pausing()
{
}

void SceneManager::DisableOrEnable()
{
    m_activeScene.load()->OnDisable();
    m_activeScene.load()->OnDestroy();
}

void SceneManager::Decommissioning()
{
    m_activeScene.load()->OnDisable();
    m_activeScene.load()->AllDestroyMark();
    m_activeScene.load()->OnDestroy();

    Memory::SafeDelete(m_inputActionManager);
    Memory::SafeDelete(m_threadPool);
}

Scene* SceneManager::CreateScene(const std::string_view& name)
{
    resourceTrimEvent.Broadcast();
    Scene* allocScene = Scene::CreateNewScene(name);
	Scene* swapScene = nullptr;

	if (!allocScene) return nullptr;

    if (m_activeScene)
    {
		swapScene = m_activeScene.load();
        
        sceneUnloadedEvent.Broadcast();

        swapScene->AllDestroyMark();
        swapScene->OnDisable();
        swapScene->OnDestroy();

        std::erase_if(m_scenes,
            [&](const auto& scene) { return scene == swapScene; });

		swapScene = nullptr;
        m_activeScene = allocScene;
    }
    else
    {
        m_activeScene = allocScene;
    }

    m_scenes.push_back(allocScene);
    m_activeSceneIndex = m_scenes.size() - 1;
    allocScene->m_buildIndex = m_activeSceneIndex.load();
    activeSceneChangedEvent.Broadcast();
    newSceneCreatedEvent.Broadcast();

    return allocScene;
}

Scene* SceneManager::SaveScene(const std::string_view& name)
{
	std::string fileStem = name.data();
	//std::string fileExtension = ".creator";
    file::path saveSceneFileName = fileStem /*+ fileExtension*/;

	std::ofstream sceneFileOut(saveSceneFileName);
    MetaYml::Node sceneNode{};
	MetaYml::Node assetsBundleNode{};

    m_activeScene.load()->m_SceneObjects[0]->m_name = saveSceneFileName.stem().string();

    try
    {
        sceneNode = Meta::Serialize(m_activeScene.load());
    }
	catch (const std::exception& e)
	{
		Debug->LogError(e.what());
		return nullptr;
	}

	sceneFileOut << sceneNode;

    sceneFileOut.close();
}

Scene* SceneManager::LoadSceneImmediate(const std::string_view& name)
{
	std::string loadSceneName = name.data();

	try
	{
        MetaYml::Node sceneNode = MetaYml::LoadFile(loadSceneName);
        Scene* swapScene{};
        if (m_activeScene)
        {
			swapScene = m_activeScene.load();
            sceneUnloadedEvent.Broadcast();
            m_activeScene.load()->AllDestroyMark();
            m_activeScene.load()->OnDisable();
            m_activeScene.load()->OnDestroy();
			
            m_activeScene = nullptr;
            
            std::erase_if(m_scenes,
                [&](const auto& scene) { return scene == swapScene; });

            delete swapScene;
        }
		file::path sceneName = name.data();
        resourceTrimEvent.Broadcast();
		m_activeScene = Scene::LoadScene(sceneName.stem().string());

        //if(sceneNode["AssetsBundle"])
        //{
        //    auto assetsBundleNode = sceneNode["AssetsBundle"];
        //    if (assetsBundleNode.IsNull())
        //    {
        //        Debug->LogError("AssetsBundle node is null.");
        //    }
        //    else
        //    {
        //        auto* AssetBundle = &m_activeScene.load()->m_requiredLoadAssetsBundle;
        //        Meta::Deserialize(AssetBundle, assetsBundleNode);
        //    }
        //}

        for (const auto& objNode : sceneNode["m_SceneObjects"])
        {
			const Meta::Type* type = Meta::ExtractTypeFromYAML(objNode);
			if (!type)
			{
				Debug->LogError("Failed to extract type from YAML node.");
				continue;
			}

            DesirealizeGameObject(type, objNode);
        }
        m_activeScene.load()->AllUpdateWorldMatrix();

		m_scenes.push_back(m_activeScene);
		m_activeSceneIndex = m_scenes.size() - 1;
		activeSceneChangedEvent.Broadcast();
		sceneLoadedEvent.Broadcast();
	}
	catch (const std::exception& e)
	{
		Debug->LogError(e.what());
		return nullptr;
	}
	return m_activeScene;
}

Scene* SceneManager::LoadScene(const std::string_view& name)
{
    std::string loadSceneName = name.data();
    Scene* scene{ nullptr };

    try
    {
        MetaYml::Node sceneNode = MetaYml::LoadFile(loadSceneName);
        file::path sceneName = name.data();
        scene = Scene::LoadScene(sceneName.stem().string());

        //if(sceneNode["AssetsBundle"])
        //{
        //    auto assetsBundleNode = sceneNode["AssetsBundle"];
        //    if (assetsBundleNode.IsNull())
        //    {
        //        Debug->LogError("AssetsBundle node is null.");
        //    }
        //    else
        //    {
        //        auto* AssetBundle = &m_activeScene.load()->m_requiredLoadAssetsBundle;
        //        Meta::Deserialize(AssetBundle, assetsBundleNode);
        //    }
        //}

        for (const auto& objNode : sceneNode["m_SceneObjects"])
        {
            const Meta::Type* type = Meta::ExtractTypeFromYAML(objNode);
            if (!type)
            {
                Debug->LogError("Failed to extract type from YAML node.");
                continue;
            }

            DesirealizeGameObject(scene, type, objNode);
        }
        scene->AllUpdateWorldMatrix();

        m_scenes.push_back(scene);
        sceneLoadedEvent.Broadcast();
    }
    catch (const std::exception& e)
    {
        Debug->LogError(e.what());
        return nullptr;
    }

	return scene;
}

void SceneManager::SaveSceneAsync(const std::string_view& name)
{
}

std::future<Scene*> SceneManager::LoadSceneAsync(const std::string_view& name)
{
    return std::async(std::launch::async, [this, scenePath = std::string(name)]() -> Scene* {
        try
        {
            // This code runs in a background thread.
            MetaYml::Node sceneNode = MetaYml::LoadFile(scenePath);
            Scene* newScene = Scene::LoadScene(std::filesystem::path(scenePath).stem().string());

            for (const auto& objNode : sceneNode["m_SceneObjects"])
            {
                const Meta::Type* type = Meta::ExtractTypeFromYAML(objNode);
                if (!type) {
                    Debug->LogError("Failed to extract type from YAML node.");
                    continue;
                }
                DesirealizeGameObject(newScene, type, objNode);
            }
            newScene->AllUpdateWorldMatrix();
            return newScene;
        }
        catch (const std::exception& e)
        {
            Debug->LogError(e.what());
            // Returning nullptr indicates failure. The exception is also stored in the future.
            return nullptr;
        }
    });
}

void SceneManager::LoadSceneAsyncAndWaitCallback(const std::string_view& name)
{
    // std::launch::async ensures the task runs on a new thread immediately.
    m_loadingSceneFuture = std::async(std::launch::async, [this, scenePath = std::string(name)]() -> Scene* {
        try
        {
            // This code runs in a background thread.
            MetaYml::Node sceneNode = MetaYml::LoadFile(scenePath);
            Scene* newScene = Scene::LoadScene(std::filesystem::path(scenePath).stem().string());

            for (const auto& objNode : sceneNode["m_SceneObjects"])
            {
                const Meta::Type* type = Meta::ExtractTypeFromYAML(objNode);
                if (!type) {
                    Debug->LogError("Failed to extract type from YAML node.");
                    continue;
                }
                DesirealizeGameObject(newScene, type, objNode);
            }
            newScene->AllUpdateWorldMatrix();
            return newScene;
        }
        catch (const std::exception& e)
        {
            Debug->LogError(e.what());
            // Returning nullptr indicates failure. The exception is also stored in the future.
            return nullptr;
        }
    });
}

void SceneManager::ActivateScene(Scene* sceneToActivate)
{
    if (!sceneToActivate) return;

    Scene* oldScene = m_activeScene.load();
    if (oldScene)
    {
        sceneUnloadedEvent.Broadcast();
        oldScene->AllDestroyMark();
        oldScene->OnDisable();
        oldScene->OnDestroy();
        std::erase_if(m_scenes, [&](const auto& scene) { return scene == oldScene; });
        delete oldScene;
    }

    resourceTrimEvent.Broadcast();
    m_activeScene = sceneToActivate;
    m_scenes.push_back(m_activeScene);
    m_activeSceneIndex = m_scenes.size() - 1;

    activeSceneChangedEvent.Broadcast();
    sceneLoadedEvent.Broadcast();
}

void SceneManager::AddDontDestroyOnLoad(Object* objPtr)
{
    if (objPtr)
    {
        m_dontDestroyOnLoadObjects.push_back(objPtr);
    }
}

std::vector<MeshRenderer*> SceneManager::GetAllMeshRenderers() const
{
	return m_activeScene.load()->m_allMeshRenderers;
}

void SceneManager::CreateEditorOnlyPlayScene()
{
    MetaYml::Node sceneNode{};

    try
    {
		resourceTrimEvent.Broadcast();
        //resetSelectedObjectEvent.Broadcast();
        sceneNode = Meta::Serialize(m_activeScene.load());
		resourceTrimEvent.Broadcast();
		Scene* playScene = Scene::LoadScene("PlayScene");
        m_scenes.push_back(playScene);
        m_EditorSceneIndex = m_activeSceneIndex;
        m_activeSceneIndex = m_scenes.size() - 1;
        m_activeScene = playScene;

        for (const auto& objNode : sceneNode["m_SceneObjects"])
        {
            const Meta::Type* type = Meta::ExtractTypeFromYAML(objNode);
            if (!type)
            {
                Debug->LogError("Failed to extract type from YAML node.");
                continue;
            }

            DesirealizeGameObject(type, objNode);
        }
        m_activeScene.load()->AllUpdateWorldMatrix();

		activeSceneChangedEvent.Broadcast();
		sceneLoadedEvent.Broadcast();
    }
    catch (const std::exception& e)
    {
        Debug->LogError(e.what());
        return;
    }
}

void SceneManager::DeleteEditorOnlyPlayScene()
{
	if (m_activeScene)
	{
        resetSelectedObjectEvent.Broadcast();
        sceneUnloadedEvent.Broadcast();
		m_activeScene.load()->AllDestroyMark();
		m_activeScene.load()->OnDisable();
		m_activeScene.load()->OnDestroy();
		m_activeScene = nullptr;
	}

	Scene* swapScene = m_scenes[m_activeSceneIndex];

    std::erase_if(m_scenes,
        [&](const auto& scene) { return scene == swapScene; });

    delete swapScene;
	swapScene = nullptr;

	m_activeSceneIndex = m_EditorSceneIndex;
	m_activeScene = m_scenes[m_EditorSceneIndex];
	activeSceneChangedEvent.Broadcast();

	m_isEditorSceneLoaded = false;
}

void SceneManager::DesirealizeGameObject(const Meta::Type* type, const MetaYml::detail::iterator_value& itNode)
{
    if (type->typeID == TypeTrait::GUIDCreator::GetTypeID<GameObject>())
    {
        auto obj = m_activeScene.load()->LoadGameObject(
            itNode["m_instanceID"].as<size_t>(),
            itNode["m_name"].as<std::string>(),
            GameObjectType::Empty,
            itNode["m_parentIndex"].as<GameObject::Index>()
        ).get();

        if (obj)
        {
            Meta::Deserialize(obj, itNode);
            if (!obj->m_tag.ToString().empty())
            {
                TagManager::GetInstance()->AddObjectToLayer(obj->m_tag.ToString(), obj);
            }

            if (!obj->m_layer.ToString().empty())
            {
                TagManager::GetInstance()->AddObjectToLayer(obj->m_layer.ToString(), obj);
            }
        }

        if (itNode["m_components"])
        {
            for (const auto& componentNode : itNode["m_components"])
            {
                try
                {
                    ComponentFactorys->LoadComponent(obj, componentNode, m_isGameStart);
                }
                catch (const std::exception& e)
                {
                    Debug->LogError(e.what());
                    continue;
                }
            }
        }
    }
}

void SceneManager::DesirealizeGameObject(Scene* targetScene, const Meta::Type* type, const MetaYml::detail::iterator_value& itNode)
{
    if (type->typeID == TypeTrait::GUIDCreator::GetTypeID<GameObject>())
    {
        auto obj = targetScene->LoadGameObject(
            itNode["m_instanceID"].as<size_t>(),
            itNode["m_name"].as<std::string>(),
            GameObjectType::Empty,
            itNode["m_parentIndex"].as<GameObject::Index>()
        ).get();

        if (obj)
        {
            Meta::Deserialize(obj, itNode);
            if (!obj->m_tag.ToString().empty())
            {
                TagManager::GetInstance()->AddObjectToLayer(obj->m_tag.ToString(), obj);
            }

            if (!obj->m_layer.ToString().empty())
            {
                TagManager::GetInstance()->AddObjectToLayer(obj->m_layer.ToString(), obj);
            }
        }

        if (itNode["m_components"])
        {
            for (const auto& componentNode : itNode["m_components"])
            {
                try
                {
                    //이게 문제가 안될까???
                    m_loadSceneReturn = true;
                    ComponentFactorys->LoadComponent(obj, componentNode, m_isGameStart);
                    m_loadSceneReturn = false;
                }
                catch (const std::exception& e)
                {
                    Debug->LogError(e.what());
                    continue;
                }
            }
        }
    }
}
