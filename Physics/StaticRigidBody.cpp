
#include "StaticRigidBody.h"

StaticRigidBody::StaticRigidBody(EColliderType collidreType, unsigned int id, unsigned int layerNumber)
	: RigidBody(collidreType, id, layerNumber)
	, m_rigidStatic(nullptr)
{
}

StaticRigidBody::~StaticRigidBody()
{
	if (m_rigidStatic) // 추가: nullptr 체크
	{
		// Physics->RemoveCollisionData(m_id); // 이 줄 제거

		physx::PxShape* shape;
		m_rigidStatic->getShapes(&shape, 1);
		if (shape) // 추가: shape 유효성 확인
		{
			m_rigidStatic->detachShape(*shape);
		}
	}
}

bool StaticRigidBody::Initialize(ColliderInfo colliderInfo, physx::PxShape* shape, physx::PxPhysics* physics, CollisionData* data)
{
	if (m_colliderType == EColliderType::COLLISION)
	{
		shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
		shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
	}
	else {
		shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
		shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
	}

	shape->setContactOffset(0.02f);
	shape->setRestOffset(0.01f);

	DirectX::SimpleMath::Matrix transform = colliderInfo.collsionTransform.worldMatrix;

	physx::PxTransform transformPx;

	CopyMatrixDxToPx(transform, transformPx);

	m_rigidStatic = physics->createRigidStatic(transformPx);
	m_rigidStatic->userData = data;

	if (m_rigidStatic == nullptr) {
		Debug->LogError("StaticRigidBody::Initialize() : m_rigidStatic is nullptr id :" + std::to_string(m_id));
		return false;
	}
	if (!m_rigidStatic->attachShape(*shape))
	{
		Debug->LogError("StaticRigidBody::Initialize() : attachShape failed id :" + std::to_string(m_id));
		return false;
	}
	data->thisId = m_id;
	data->thisLayerNumber = m_layerNumber;
	shape->userData = data; // 이 위치로 이동

	return true;
}

void StaticRigidBody::ChangeLayerNumber(const unsigned int& layerNumber, int* collisionMatrix)
{
	if (layerNumber == UINT_MAX)
	{
		return;
	}

	m_layerNumber = layerNumber;

	physx::PxShape* shape;
	if (!m_rigidStatic) return; // 추가: m_rigidStatic 유효성 확인
	m_rigidStatic->getShapes(&shape, 1);

	if (shape && shape->userData != nullptr) // 추가: shape 및 userData 유효성 확인
	{
		physx::PxFilterData filterData;
		filterData.word0 = layerNumber;
		filterData.word1 = collisionMatrix[layerNumber];
		shape->setSimulationFilterData(filterData);

		auto userData = static_cast<CollisionData*>(shape->userData);
		userData->thisLayerNumber = layerNumber;
	}
}

void StaticRigidBody::SetConvertScale(const DirectX::SimpleMath::Vector3& scale, physx::PxPhysics* physics, unsigned int* collisionMatrix)
{
	if (std::isnan(m_scale.x) || std::isnan(m_scale.y) || std::isnan(m_scale.z))
	{
		return;
	}

	if (fabs(m_scale.x - scale.x) < 0.001f && fabs(m_scale.y - scale.y) < 0.001f && fabs(m_scale.z - scale.z) < 0.001f)
	{
		return;
	}

	m_scale = scale;

	physx::PxShape* shape;
	physx::PxMaterial* material;

	if (!m_rigidStatic) return; // 추가: m_rigidStatic 유효성 확인
	m_rigidStatic->getShapes(&shape, 1);
	if (!shape) return; // 추가: shape 유효성 확인
	
	shape->getMaterials(&material, 1);
	void* userData = shape->userData;

	if (shape->getGeometry().getType() == physx::PxGeometryType::eBOX)
	{
		physx::PxBoxGeometry boxGeometry = static_cast<const physx::PxBoxGeometry&>(shape->getGeometry());
		boxGeometry.halfExtents.x = m_Extent.x * m_scale.x;
		boxGeometry.halfExtents.y = m_Extent.y * m_scale.y;
		boxGeometry.halfExtents.z = m_Extent.z * m_scale.z;
		m_rigidStatic->detachShape(*shape);
		if (userData) // 추가: userData 유효성 확인
		UpdateShapeGeometry(m_rigidStatic, boxGeometry, physics, material, collisionMatrix, userData);
	}
	else if (shape->getGeometry().getType() == physx::PxGeometryType::eSPHERE)
	{
		physx::PxSphereGeometry sphereGeometry = static_cast<const physx::PxSphereGeometry&>(shape->getGeometry());
		float maxScale = std::max<float>(m_scale.x, std::max<float>(m_scale.y, m_scale.z));
		sphereGeometry.radius = m_radius * maxScale;
		m_rigidStatic->detachShape(*shape);
		if (userData) // 추가: userData 유효성 확인
		UpdateShapeGeometry(m_rigidStatic, sphereGeometry, physics, material, collisionMatrix, userData);
	}
	else if (shape->getGeometry().getType() == physx::PxGeometryType::eCAPSULE)
	{
		physx::PxCapsuleGeometry capsuleGeometry = static_cast<const physx::PxCapsuleGeometry&>(shape->getGeometry());
		capsuleGeometry.halfHeight = m_halfHeight * m_scale.y;
		capsuleGeometry.radius = m_radius * m_scale.x;
		m_rigidStatic->detachShape(*shape);
		if (userData) // 추가: userData 유효성 확인
		UpdateShapeGeometry(m_rigidStatic, capsuleGeometry, physics, material, collisionMatrix, userData);
	}
	else if (shape->getGeometry().getType() == physx::PxGeometryType::eCONVEXMESH)
	{
		physx::PxConvexMeshGeometry convexMeshGeometry = static_cast<const physx::PxConvexMeshGeometry&>(shape->getGeometry());
		convexMeshGeometry.scale.scale.x = m_scale.x;
		convexMeshGeometry.scale.scale.y = m_scale.y;
		convexMeshGeometry.scale.scale.z = m_scale.z;
		m_rigidStatic->detachShape(*shape);
		if (userData) // 추가: userData 유효성 확인
		UpdateShapeGeometry(m_rigidStatic, convexMeshGeometry, physics, material, collisionMatrix, userData);
	}
	else if (shape->getGeometry().getType() == physx::PxGeometryType::eTRIANGLEMESH)
	{
		Debug->LogError("StaticRigidBody::SetConvertScale() : TriangleMesh is not supported id :" + std::to_string(m_id));
	}
	else if (shape->getGeometry().getType() == physx::PxGeometryType::eHEIGHTFIELD)
	{
		Debug->LogError("StaticRigidBody::SetConvertScale() : HeightField is not supported id :" + std::to_string(m_id));
	}
}