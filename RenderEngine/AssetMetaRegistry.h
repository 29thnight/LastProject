#pragma once
#include "Core.Minimal.h"

class AssetMetaRegistry
{
public:
    void Register(const FileGuid& guid, const file::path& path)
    {
        m_guidToPath[guid] = path;
        m_pathToGuid[path] = guid;
    }

    void Unregister(const FileGuid& guid)
    {
        auto it = m_guidToPath.find(guid);
        if (it != m_guidToPath.end())
        {
            file::path path = it->second;
            m_guidToPath.erase(it);
            m_pathToGuid.erase(path);
        }
    }

    void Unregister(const file::path& path)
    {
        auto it = m_pathToGuid.find(path);
        if (it != m_pathToGuid.end())
        {
            FileGuid guid = it->second;
            m_pathToGuid.erase(it);
            m_guidToPath.erase(guid);
        }
    }

    file::path GetPath(const FileGuid& guid) const
    {
        auto it = m_guidToPath.find(guid);
        return it != m_guidToPath.end() ? it->second : file::path{};
    }

    FileGuid GetGuid(const file::path& path) const
    {
        auto it = m_pathToGuid.find(path);
        return it != m_pathToGuid.end() ? it->second : FileGuid{};
    }

    bool Contains(const FileGuid& guid) const
    {
        return m_guidToPath.contains(guid);
    }

    bool Contains(const file::path& path) const
    {
        return m_pathToGuid.contains(path);
    }

private:
    std::unordered_map<FileGuid, file::path> m_guidToPath;
    std::unordered_map<file::path, FileGuid> m_pathToGuid;
};