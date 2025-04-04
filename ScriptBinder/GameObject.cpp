#include "GameObject.h"
#include "Scene.h"

Scene* GameObject::m_pScene = nullptr;

GameObject::GameObject(const std::string_view& name, GameObject::Type type, GameObject::Index index, GameObject::Index parentIndex) :
    m_name(name.data()), 
    m_gameObjectType(type),
    m_index(index), 
    m_parentIndex(parentIndex)
{
}

std::string GameObject::ToString() const
{
    return m_name.ToString();
}
