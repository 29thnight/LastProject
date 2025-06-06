#include "RenderCommand.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Material.h"
#include "Core.OctreeNode.h"
#include "CullingManager.h"

constexpr size_t TRANSFORM_SIZE = sizeof(Mathf::xMatrix) * MAX_BONES;

MeshRendererProxy::MeshRendererProxy(MeshRenderer* component) :
    m_Material(component->m_Material),
    m_Mesh(component->m_Mesh),
    m_LightMapping(component->m_LightMapping),
    m_isSkinnedMesh(component->m_isSkinnedMesh),
    m_worldMatrix(component->GetOwner()->m_transform.GetWorldMatrix())
{
    GameObject* owner = GameObject::FindIndex(component->GetOwner()->m_parentIndex);
    if(owner)
    {
        Animator* animator = owner->GetComponent<Animator>();
        if (nullptr != animator && animator->IsEnabled())
        {
            m_isAnimationEnabled = true;
            m_animatorGuid = animator->GetInstanceID();
        }
    }

    if (!m_isSkinnedMesh)
    {
        //TODO : Change CullingManager Collect Class : MeshRenderer -> MeshRendererProxy
        //CullingManagers->Insert(this);

        m_isNeedUptateCulling = true;
    }
}

MeshRendererProxy::~MeshRendererProxy()
{
}

MeshRendererProxy::MeshRendererProxy(const MeshRendererProxy& other) :
    m_Material(other.m_Material),
    m_Mesh(other.m_Mesh),
    m_LightMapping(other.m_LightMapping),
    m_isSkinnedMesh(other.m_isSkinnedMesh),
    m_worldMatrix(other.m_worldMatrix),
    m_animatorGuid(other.m_animatorGuid)
{
    memcpy(m_finalTransforms, other.m_finalTransforms, TRANSFORM_SIZE);
}

MeshRendererProxy::MeshRendererProxy(MeshRendererProxy&& other) noexcept :
    m_Material(std::exchange(other.m_Material, nullptr)),
    m_Mesh(std::exchange(other.m_Mesh, nullptr)),
    m_LightMapping(other.m_LightMapping),
    m_isSkinnedMesh(other.m_isSkinnedMesh),
    m_worldMatrix(std::exchange(other.m_worldMatrix, {})),
    m_animatorGuid(std::exchange(other.m_animatorGuid, {}))
{
    memcpy(m_finalTransforms, other.m_finalTransforms, TRANSFORM_SIZE);
}

void MeshRendererProxy::Draw()
{
    if (nullptr == m_Mesh) return;

    m_Mesh->Draw();
}

ID3D11CommandList* MeshRendererProxy::Draw(ID3D11DeviceContext* _defferedContext)
{
    if (nullptr == m_Mesh || nullptr == _defferedContext) return nullptr;

    m_Mesh->Draw(_defferedContext);
}