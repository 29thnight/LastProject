#pragma once
#include "HotLoadSystem.h"
#include "Object.h"
#include "Component.h"
#include "Transform.h"
#include "GameObjectType.h"
#include "GameObject.generated.h"

class Scene;
class Bone;
class RenderScene;
class ModelLoader;
class ModuleBehavior;
class GameObject : public Object
{
public:
	using Index = int;
	static constexpr GameObject::Index INVALID_INDEX = std::numeric_limits<uint32_t>::max();
	enum class Type
	{
		Empty,
		Camera,
		Light,
		Mesh,
		Bone,
		TypeMax
	};
    ReflectGameObject
    [[Serializable(Inheritance:Object)]]
	GameObject();
	GameObject(Scene* scene, const std::string_view& name, GameObjectType type, GameObject::Index index, GameObject::Index parentIndex);
	GameObject(Scene* scene, size_t instanceID, const std::string_view& name, GameObjectType type, GameObject::Index index, GameObject::Index parentIndex);
	GameObject(GameObject&) = delete;
	GameObject(GameObject&&) noexcept = default;
	GameObject& operator=(GameObject&) = delete;
	~GameObject() override = default;

	HashingString GetHashedName() const { return m_name; }
    void SetName(const std::string_view& name) { m_name = name.data(); }
    void SetTag(const std::string_view& tag);
    void SetLayer(const std::string_view& layer);

	virtual void Destroy() override final;

	std::shared_ptr<Component> AddComponent(const Meta::Type& type);
	ModuleBehavior* AddScriptComponent(const std::string_view& scriptName);
    std::shared_ptr<Component> GetComponent(const Meta::Type& type);
	std::shared_ptr<Component> GetComponentByTypeID(uint32 id);
	void RefreshComponentIdIndices();

	template<typename T>
	T* AddComponent();

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);

	template<typename T>
	T* GetComponent(uint32 id);

	template<typename T>
	T* GetComponent();

	template<typename T>
	bool HasComponent();

	template<typename T>
	std::vector<T*> GetComponents();

	template<typename T>
	void RemoveComponent(T* component);

	void RemoveComponentIndex(uint32 id);
	void RemoveComponentTypeID(uint32 typeID);
	void RemoveScriptComponent(const std::string_view& scriptName);
	void RemoveScriptComponent(ModuleBehavior* ptr);
	void RemoveComponent(Meta::Type& type);

	bool IsStatic() const { return m_isStatic; }
	void SetStatic(bool isStatic) { m_isStatic = isStatic; }

	GameObjectType GetType() const { return m_gameObjectType; }

    static GameObject* Find(const std::string_view& name);
	static GameObject* FindIndex(GameObject::Index index);
	static GameObject* FindInstanceID(const HashedGuid& guid);
	static GameObject* FindAttachedID(const HashedGuid& guid);

	static inline bool IsValidIndex(Index index)
	{
		return index != INVALID_INDEX;
	}

	static inline bool IsInvalidIndex(Index index)
	{
		return index == INVALID_INDEX;
	}

	void SetEnabled(bool able) override final;
	void SetCollisionType(uint32 type) { m_collisionType = type; }
	uint32 GetCollisionType() const { return m_collisionType; }
	Scene* GetScene() { return m_ownerScene; }

    [[Property]]
	Transform m_transform{};
    [[Property]]
	GameObject::Index m_index{ INVALID_INDEX };
    [[Property]]
	GameObject::Index m_parentIndex{ INVALID_INDEX };
	//for bone update
    [[Property]]
	GameObject::Index m_rootIndex{ 0 };
    [[Property]]
	std::vector<GameObject::Index> m_childrenIndices;
	[[Property]]
	HashedGuid m_attachedSoketID{};
	[[Property]]
	uint32 m_collisionType = 1;
	[[Property]]
	bool m_isStatic{ false };

public:
	[[Property]]
	GameObjectType m_gameObjectType{ GameObjectType::Empty };
    [[Property]]
    HashingString m_tag{ "Untagged" };
    [[Property]]
    HashingString m_layer{ "Default" };
	
	std::unordered_map<HashedGuid, size_t> m_componentIds{};
    [[Property]]
	std::vector<std::shared_ptr<Component>> m_components{};

	Scene* m_ownerScene{ nullptr };
};

#include "GameObject.inl"


