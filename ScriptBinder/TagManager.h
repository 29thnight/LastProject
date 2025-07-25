#pragma once
#include "Core.Minimal.h"
#include "GameObject.h"

class TagManager : public Singleton<TagManager>
{
private:
	friend Singleton;
	TagManager() = default;
	~TagManager() = default;

public:
	void Initialize();
	void Finalize();
	void Load();
    void Save();
    void AddTag(const std::string_view& tag);
    void RemoveTag(const std::string_view& tag);
    bool HasTag(const std::string_view& tag) const;

    void AddLayer(const std::string_view& layer);
    void RemoveLayer(const std::string_view& layer);
    bool HasLayer(const std::string_view& layer) const;

    std::vector<std::string>& GetTags()
    {
        return m_tags;
    }

    std::vector<std::string>& GetLayers()
    {
        return m_layers;
    }

    size_t GetTagIndex(const std::string_view& tag) const
    {
        if (tag.empty() || tag == "Untagged")
        {
            return 0;
        }
        auto it = m_tagMap.find(tag.data());
        if (it != m_tagMap.end())
        {
            return it->second;
        }
        return 0; // "Untagged" index
	}

    size_t GetLayerIndex(const std::string_view& layer) const
    {
        if (layer.empty())
        {
            return 0;
        }
        auto it = m_layerMap.find(layer.data());
        if (it != m_layerMap.end())
        {
            return it->second;
        }
        return 0; // Default layer index
	}

    void AddTagToObject(const std::string_view& tag, GameObject* object);
    void RemoveTagFromObject(const std::string_view& tag, GameObject* object);

    void AddObjectToLayer(const std::string_view& layer, GameObject* object);
    void RemoveObjectFromLayer(const std::string_view& layer, GameObject* object);

    std::vector<GameObject*> GetObjectsInLayer(const std::string_view& layer) const
    {
        if (layer.empty())
        {
                return {};
        }

        auto it = m_layeredObjects.find(layer.data());
        if (it != m_layeredObjects.end())
        {
                return it->second;
        }
        return {};
    }

    GameObject* GetObjectInLayer(const std::string_view& layer) const
    {
        if (layer.empty())
        {
                return nullptr;
        }

        auto it = m_layeredObjects.find(layer.data());
        if (it != m_layeredObjects.end() && !it->second.empty())
        {
                return it->second[0];
        }
        return nullptr;
    }

	std::vector<GameObject*> GetObjectsWithTag(const std::string_view& tag) const
	{
		if (tag.empty() || tag == "Untagged")
		{
			return {};
		}

		auto it = m_taggedObjects.find(tag.data());
		if (it != m_taggedObjects.end())
		{
			return it->second;
		}
		return {};
	}

	GameObject* GetObjectWithTag(const std::string_view& tag) const
	{
		if (tag.empty() || tag == "Untagged")
		{
			return nullptr;
		}

		auto it = m_taggedObjects.find(tag.data());
		if (it != m_taggedObjects.end() && !it->second.empty())
		{
			return it->second[0];
		}
		return nullptr;
	}

    void ClearTags()
    {
       m_tags.clear();
       m_tagMap.clear();
       m_taggedObjects.clear();
    }

    void ClearLayers()
    {
       m_layers.clear();
       m_layerMap.clear();
       m_layeredObjects.clear();
    }

private:
    std::unordered_map<std::string, size_t> m_tagMap{};
    std::vector<std::string> m_tags{};
    std::unordered_map<std::string, std::vector<GameObject*>> m_taggedObjects{};

    std::unordered_map<std::string, size_t> m_layerMap{};
    std::vector<std::string> m_layers{};
    std::unordered_map<std::string, std::vector<GameObject*>> m_layeredObjects{};
	
};