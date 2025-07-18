#pragma once
#include "Core.Minimal.h"
#include "AssetEntry.generated.h"

enum class ManagedAssetType
{
	Model,
	Material,
	Texture,
	SpriteFont
};
AUTO_REGISTER_ENUM(ManagedAssetType);

struct AssetEntry
{
   ReflectAssetEntry
	[[Serializable]]
	AssetEntry() = default;
	AssetEntry(ManagedAssetType assetTypeID, const file::path& assetName)
		: assetTypeID((int)assetTypeID), assetName(assetName) {
	}
	~AssetEntry() = default;
	AssetEntry(const AssetEntry&) = default;
	AssetEntry(AssetEntry&&) noexcept = default;

	AssetEntry& operator=(const AssetEntry&) = default;
	AssetEntry& operator=(AssetEntry&&) noexcept = default;

	[[Property]]
	int assetTypeID;
	[[Property]]
	file::path assetName;

	friend auto operator<=>(const AssetEntry& lhs, const AssetEntry& rhs)
	{
		return std::tie(lhs.assetTypeID, lhs.assetName)
			<=> std::tie(rhs.assetTypeID, rhs.assetName);
	}

	friend bool operator==(const AssetEntry& lhs, const AssetEntry& rhs)
	{
		return lhs.assetTypeID == rhs.assetTypeID && lhs.assetName == rhs.assetName;
	}
};
