#pragma once
#include "Core.Minimal.h"

enum class BoneRegion
{
	Root,
	Spine,
	Neck,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,
};
AUTO_REGISTER_ENUM(BoneRegion)


class Bone;
class Animation;
class Skeleton
{
public:
	Bone* m_rootBone{};
	std::vector<Animation> m_animations;
	std::vector<Bone*> m_bones;
	Mathf::xMatrix m_rootTransform;
	Mathf::xMatrix m_globalInverseTransform;

	Skeleton() = default;
	~Skeleton();
	static constexpr uint32 MAX_BONES{ 512 };

	Bone* FindBone(const std::string_view& _name);

	void MarkRegionSkeleton();
	void MarkRegion(Bone* bone, BoneRegion region);
};

std::string ToLower(std::string boneName);

class Bone
{
public:
	std::string m_name{};
	int m_index{};
	int m_parentIndex{};
	Mathf::xMatrix m_globalTransform;
	Mathf::xMatrix m_localTransform;
	Mathf::xMatrix m_offset;
	std::vector<Bone*> m_children;
	BoneRegion m_region = BoneRegion::Root;
    Bone() = default;
	Bone(const std::string& _name, int _index, const Mathf::xMatrix& _offset) :
		m_name(_name),
		m_index(_index),
		m_offset(_offset),
		m_globalTransform(XMMatrixIdentity())
	{

	}
};

struct NodeAnimation
{
	std::string m_name{};

	struct PositionKey
	{
		Mathf::xVector m_position;
		double m_time;
	};
	std::vector<PositionKey> m_positionKeys;

	struct RotationKey
	{
		Mathf::xVector m_rotation;
		double m_time;
	};
	std::vector<RotationKey> m_rotationKeys;

	struct ScaleKey
	{
		Mathf::Vector3 m_scale;
		double m_time;
	};
	std::vector<ScaleKey> m_scaleKeys;
};

class Animation
{
public:
	std::string m_name{};
	std::map<std::string, NodeAnimation> m_nodeAnimations;
	double m_duration{};
	double m_ticksPerSecond{};
};
