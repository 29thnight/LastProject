#include "Transform.h"
#include "GameObject.h"

Transform::Transform(const Transform& other) :
	position(other.position),
	rotation(other.rotation),
	scale(other.scale),
	m_parentID(other.m_parentID),
	m_worldMatrix(other.m_worldMatrix),
	m_localMatrix(other.m_localMatrix),
	m_inverseMatrix(other.m_inverseMatrix),
	m_worldScale(other.m_worldScale),
	m_worldQuaternion(other.m_worldQuaternion),
	m_worldPosition(other.m_worldPosition)
{
}

Transform::Transform(Transform&& other) noexcept :
	position(std::exchange(other.position, {})),
	rotation(std::exchange(other.rotation, {})),
	scale(std::exchange(other.scale, {})),
	m_parentID(std::exchange(other.m_parentID, {})),
	m_worldMatrix(std::exchange(other.m_worldMatrix, {})),
	m_localMatrix(std::exchange(other.m_localMatrix, {})),
	m_inverseMatrix(std::exchange(other.m_inverseMatrix, {})),
	m_worldScale(std::exchange(other.m_worldScale, {})),
	m_worldQuaternion(std::exchange(other.m_worldQuaternion, {})),
	m_worldPosition(std::exchange(other.m_worldPosition, {}))
{
}

Transform& Transform::operator=(const Transform& rhs)
{
	position = rhs.position;
	rotation = rhs.rotation;
	scale = rhs.scale;
	m_parentID = rhs.m_parentID;
	m_worldMatrix = rhs.m_worldMatrix;
	m_localMatrix = rhs.m_localMatrix;
	m_inverseMatrix = rhs.m_inverseMatrix;
	m_worldScale = rhs.m_worldScale;
	m_worldQuaternion = rhs.m_worldQuaternion;
	m_worldPosition = rhs.m_worldPosition;

	return *this;
}

Transform& Transform::operator=(Transform&& rhs) noexcept
{
	position			= std::exchange(rhs.position,{});
	rotation			= std::exchange(rhs.rotation, {});
	scale				= std::exchange(rhs.scale, {});
	m_parentID			= std::exchange(rhs.m_parentID, {});
	m_worldMatrix		= std::exchange(rhs.m_worldMatrix, {});
	m_localMatrix		= std::exchange(rhs.m_localMatrix, {});
	m_inverseMatrix		= std::exchange(rhs.m_inverseMatrix, {});
	m_worldScale		= std::exchange(rhs.m_worldScale, {});
	m_worldQuaternion	= std::exchange(rhs.m_worldQuaternion, {});
	m_worldPosition		= std::exchange(rhs.m_worldPosition, {});

	return *this;
}

Transform& Transform::SetScale(Mathf::Vector3 scale)
{
	m_dirty = true;
	this->scale = Mathf::Vector4(scale);

	return *this;
}

Transform& Transform::SetPosition(Mathf::Vector3 pos)
{
	m_dirty = true;
	position = Mathf::Vector4(pos);

	return *this;
}

Transform& Transform::AddPosition(Mathf::Vector3 pos)
{
	m_dirty = true;
	position = DirectX::XMVectorAdd(position, pos);

	return *this;
}

Transform& Transform::SetRotation(Mathf::Quaternion quaternion)
{
	m_dirty = true;
	rotation = quaternion;

	return *this;
}

Transform& Transform::AddRotation(Mathf::Quaternion quaternion)
{
	m_dirty = true;
	rotation = DirectX::XMQuaternionMultiply(quaternion, rotation);

	return *this;
}

Mathf::xMatrix Transform::GetLocalMatrix()
{
	if (m_dirty)
	{
		m_localMatrix = DirectX::XMMatrixScalingFromVector(scale);
		m_localMatrix *= DirectX::XMMatrixRotationQuaternion(rotation);
		m_localMatrix *= DirectX::XMMatrixTranslationFromVector(position);
		m_dirty = false;
	}

	return m_localMatrix;
}

Mathf::xMatrix Transform::GetWorldMatrix() const
{
	return m_worldMatrix;
}

Mathf::xMatrix Transform::GetInverseMatrix() const
{
	return m_inverseMatrix;
}

void Transform::UpdateLocalMatrix()
{
	if (m_dirty)
	{
		m_localMatrix = DirectX::XMMatrixScalingFromVector(scale);
		m_localMatrix *= DirectX::XMMatrixRotationQuaternion(rotation);
		m_localMatrix *= DirectX::XMMatrixTranslationFromVector(position);
		m_dirty = false;
	}
}

void Transform::SetLocalMatrix(const Mathf::xMatrix& matrix)
{
	Mathf::xVector _scale{}, _rotation{}, _position{};

	m_localMatrix = matrix;
	DirectX::XMMatrixDecompose(&_scale, &_rotation, &_position, m_localMatrix);

	XMStoreFloat4(&position, _position);
	XMStoreFloat4(&scale, _scale);
	XMStoreFloat4(&rotation, _rotation);

	m_dirty = false;
}

void Transform::SetAndDecomposeMatrix(const Mathf::xMatrix& matrix)
{
	Mathf::Matrix compareMat = matrix;
	if (compareMat == m_worldMatrix) return;

	m_worldMatrix = matrix;
	XMMatrixDecompose(&m_worldScale, &m_worldQuaternion, &m_worldPosition, m_worldMatrix);
	
	XMMATRIX parentMat = GameObject::FindIndex(m_parentID)->m_transform.GetWorldMatrix();
	XMMATRIX parentWorldInverse = XMMatrixInverse(nullptr, parentMat);
	XMMATRIX newLocalMatrix = XMMatrixMultiply(XMMATRIX(matrix), parentWorldInverse);
	m_localMatrix = newLocalMatrix;

	SetLocalMatrix(m_localMatrix);
}

Mathf::xVector Transform::GetWorldPosition() const
{
	return m_worldPosition;
}

Mathf::xVector Transform::GetWorldScale() const
{
	return m_worldScale;
}

Mathf::xVector Transform::GetWorldQuaternion() const
{
	return m_worldQuaternion;
}

void Transform::SetDirty()
{
	m_dirty = true;
}

bool Transform::IsDirty() const
{
	return m_dirty;
}

void Transform::SetParentID(uint32 id)
{
	m_parentID = id;
}

void Transform::TransformReset()
{
	position = { Mathf::xVectorZero };
	rotation = { Mathf::xVectorZero };
	scale = { Mathf::xVectorOne };
	m_dirty = true;
}
