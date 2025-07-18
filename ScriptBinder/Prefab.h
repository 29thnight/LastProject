#pragma once
#include "Object.h"
#include "GameObject.h"
#include "ReflectionYml.h"
#include "SceneManager.h"
#include "ComponentFactory.h"
#include "Prefab.generated.h"

class Prefab : public Object
{
public:
   ReflectPrefab
    [[Serializable(Inheritance:Object)]]
    Prefab() = default;
    Prefab(const std::string_view& name, const GameObject* source);
    ~Prefab() override = default;

    static Prefab* CreateFromGameObject(const GameObject* source, const std::string_view& name = "");

    GameObject* Instantiate(const std::string_view& newName = "") const;

private:
    MetaYml::Node m_prefabData{};

    [[Property]]
	FileGuid m_fileGuid{};
};

