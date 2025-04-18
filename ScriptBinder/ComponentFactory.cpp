#include "ComponentFactory.h"
#include "GameObject.h"
#include "RenderableComponents.h"
#include "LightComponent.h"
#include "CameraComponent.h"
#include "DataSystem.h"

void ComponentFactory::LoadComponent(GameObject* obj, const MetaYml::detail::iterator_value& itNode)
{
    const Meta::Type* componentType = Meta::ExtractTypeFromYAML(itNode);
    if (nullptr == componentType)
    {
        return;
    }

    auto component = obj->AddComponent((*componentType)).get();
    if (component)
    {
        using namespace TypeTrait;
        if (componentType->typeID == GUIDCreator::GetTypeID<MeshRenderer>())
        {
            auto meshRenderer = static_cast<MeshRenderer*>(component);
            Model* model = nullptr;
            if (itNode["m_Material"])
            {
                auto materialNode = itNode["m_Material"];
                FileGuid guid = materialNode["m_fileGuid"].as<std::string>();
                model = DataSystems->LoadModelGUID(guid);
            }
            MetaYml::Node getMeshNode = itNode["m_Mesh"];
            if (model && getMeshNode)
            {
                meshRenderer->m_Material = model->GetMaterial(getMeshNode["m_materialIndex"].as<int>());
                meshRenderer->m_Mesh = model->GetMesh(getMeshNode["m_name"].as<std::string>());
            }
            Deserialize(meshRenderer, itNode);
            meshRenderer->SetEnabled(true);
        }
		else if (componentType->typeID == GUIDCreator::GetTypeID<LightComponent>())
		{
			auto lightComponent = static_cast<LightComponent*>(component);
			Deserialize(lightComponent, itNode);
			lightComponent->SetEnabled(true);
		}
		//else if (componentType->typeID == GUIDCreator::GetTypeID<CameraComponent>())
		//{
		//	auto cameraComponent = static_cast<CameraComponent*>(component);
		//	Deserialize(cameraComponent, itNode);
		//	cameraComponent->SetEnabled(true);
		//}
		else if (componentType->typeID == GUIDCreator::GetTypeID<SpriteRenderer>())
		{
			auto spriteRenderer = static_cast<SpriteRenderer*>(component);
			Texture* texture = nullptr;
			if (itNode["m_Sprite"])
			{
				auto spriteNode = itNode["m_Sprite"];
				FileGuid guid = spriteNode["m_fileGuid"].as<std::string>();
				
			}
			Deserialize(spriteRenderer, itNode);
			spriteRenderer->SetEnabled(true);
		}
		else
		{
			Deserialize(component, itNode);
		}
    }
}
