#include "PhysicsManager.h"
#include "SceneManager.h"
#include "Scene.h"
#include "GameObject.h"
#include "Transform.h"
#include "RigidBodyComponent.h"
#include "BoxColliderComponent.h"
#include "SphereColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "MeshCollider.h"
#include "MeshRenderer.h"
#include "Terrain.h"
#include "TerrainCollider.h"
#include "CharacterControllerComponent.h"

class Scene;
void PhysicsManager::Initialize()
{
	//물리엔진 초기화
	m_bIsInitialized = Physics->Initialize();
	
	//델리게이트 등록
	m_OnSceneLoadHandle		= sceneLoadedEvent.AddRaw(this, &PhysicsManager::OnLoadScene);
	m_OnSceneUnloadHandle	= sceneUnloadedEvent.AddRaw(this, &PhysicsManager::OnUnloadScene);
	m_OnChangeSceneHandle	= SceneManagers->activeSceneChangedEvent.AddRaw(this, &PhysicsManager::ChangeScene);

	//콜리전 콜백 등록
	Physics->SetCallBackCollisionFunction([this](CollisionData data, ECollisionEventType type) {
		this->CallbackEvent(data, type);
		});
}
void PhysicsManager::Update(float fixedDeltaTime)
{
	if (!m_bIsInitialized) return;
	
	//물리씬에 데이터 셋
	SetPhysicData();

	//물리씬 업데이트
	Physics->Update(fixedDeltaTime);

	//물리씬에 데이터 가져오기
	GetPhysicData();
	//콜백 이벤트 처리
	ProcessCallback();
	
}
void PhysicsManager::Shutdown()
{
	//모든 객체 제거
	Physics->ChangeScene();
	//컨테이너 제거
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	Container.clear();
	//물리 씬 해제
	Physics->UnInitialize();
}
void PhysicsManager::ChangeScene()
{
	Physics->ChangeScene();
}
void PhysicsManager::OnLoadScene()
{
	//todo : 물리씬 초기화
	//CleanUp();

	//todo : 현제의 게임씬을 찾아오기
#pragma region NotUse
	//auto scene = SceneManagers->GetActiveScene();

	//for (auto& obj : scene->m_SceneObjects)
	//{

	//	AddTerrainCollider(obj.get());


	//	auto rigid = obj->GetComponent<RigidBodyComponent>();
	//	if (!rigid)
	//	{
	//		continue;
	//	}

	//	auto& transform = obj->m_transform;
	//	auto bodyType = rigid->GetBodyType();

	//	auto box = obj->GetComponent<BoxColliderComponent>();

	//	//박스 콜라이더라면
	//	if (box)
	//	{
	//		unsigned int colliderID = ++m_lastColliderID;
	//		auto boxInfo = box->GetBoxInfo();
	//		auto tranformOffset = box->GetPositionOffset();
	//		auto rotationOffset = box->GetRotationOffset();

	//		boxInfo.colliderInfo.id = colliderID;
	//		boxInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	//		boxInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	//		boxInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	//		boxInfo.colliderInfo.collsionTransform.localMatrix.Decompose(boxInfo.colliderInfo.collsionTransform.localScale, boxInfo.colliderInfo.collsionTransform.localRotation, boxInfo.colliderInfo.collsionTransform.localPosition);
	//		boxInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(boxInfo.colliderInfo.collsionTransform.worldScale, boxInfo.colliderInfo.collsionTransform.worldRotation, boxInfo.colliderInfo.collsionTransform.worldPosition);

	//		//offset 계산
	//		if (tranformOffset != DirectX::SimpleMath::Vector3::Zero)
	//		{
	//			boxInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
	//			boxInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
	//			boxInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;

	//			tranformOffset = DirectX::SimpleMath::Vector3::Transform(tranformOffset, boxInfo.colliderInfo.collsionTransform.worldMatrix);

	//			boxInfo.colliderInfo.collsionTransform.worldPosition += tranformOffset;

	//			boxInfo.colliderInfo.collsionTransform.worldMatrix._41 = boxInfo.colliderInfo.collsionTransform.worldPosition.x;
	//			boxInfo.colliderInfo.collsionTransform.worldMatrix._42 = boxInfo.colliderInfo.collsionTransform.worldPosition.y;
	//			boxInfo.colliderInfo.collsionTransform.worldMatrix._43 = boxInfo.colliderInfo.collsionTransform.worldPosition.z;

	//		}

	//		box->SetBoxInfoMation(boxInfo);

	//		if (bodyType == EBodyType::STATIC) {
	//			//pxScene에 엑터 추가
	//			Physics->CreateStaticBody(boxInfo, EColliderType::COLLISION);
	//			//콜라이더 정보 저장
	//			m_colliderContainer.insert({ colliderID, {
	//				m_boxTypeId,
	//				box,
	//				box->GetOwner(),
	//				box,
	//				false
	//				}
	//				});
	//		}
	//		else {
	//			bool isKinematic = bodyType == EBodyType::KINEMATIC;
	//			Physics->CreateDynamicBody(boxInfo, EColliderType::COLLISION, isKinematic);
	//			//콜라이더 정보 저장
	//			m_colliderContainer.insert({ colliderID,
	//				{m_boxTypeId,box,box->GetOwner(),box,false
	//				}
	//				});
	//		}

	//	}
	//	//todo :sphere 콜라이더라면
	//	auto sphere = obj->GetComponent<SphereColliderComponent>();
	//	if (sphere) {

	//		auto type = sphere->GetColliderType();
	//		auto sphereInfo = sphere->GetSphereInfo();
	//		auto posOffset = sphere->GetPositionOffset();
	//		auto rotOffset = sphere->GetRotationOffset();

	//		unsigned int colliderID = ++m_lastColliderID;

	//		sphereInfo.colliderInfo.id = colliderID;
	//		sphereInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	//		sphereInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	//		sphereInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	//		sphereInfo.colliderInfo.collsionTransform.localMatrix.Decompose(sphereInfo.colliderInfo.collsionTransform.localScale, sphereInfo.colliderInfo.collsionTransform.localRotation, sphereInfo.colliderInfo.collsionTransform.localPosition);
	//		sphereInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(sphereInfo.colliderInfo.collsionTransform.worldScale, sphereInfo.colliderInfo.collsionTransform.worldRotation, sphereInfo.colliderInfo.collsionTransform.worldPosition);

	//		//offset 계산
	//		if (posOffset != DirectX::SimpleMath::Vector3::Zero)
	//		{
	//			sphereInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
	//			sphereInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
	//			sphereInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;

	//			posOffset = DirectX::SimpleMath::Vector3::Transform(posOffset, sphereInfo.colliderInfo.collsionTransform.worldMatrix);

	//			sphereInfo.colliderInfo.collsionTransform.worldPosition += posOffset;

	//			sphereInfo.colliderInfo.collsionTransform.worldMatrix._41 = sphereInfo.colliderInfo.collsionTransform.worldPosition.x;
	//			sphereInfo.colliderInfo.collsionTransform.worldMatrix._42 = sphereInfo.colliderInfo.collsionTransform.worldPosition.y;
	//			sphereInfo.colliderInfo.collsionTransform.worldMatrix._43 = sphereInfo.colliderInfo.collsionTransform.worldPosition.z;
	//		}
	//		sphere->SetSphereInfoMation(sphereInfo);

	//		if (bodyType == EBodyType::STATIC) {
	//			//pxScene에 엑터 추가
	//			Physics->CreateStaticBody(sphereInfo, EColliderType::COLLISION);
	//			//콜라이더 정보 저장
	//			m_colliderContainer.insert({ colliderID, {
	//				m_sphereTypeId,
	//				sphere,
	//				sphere->GetOwner(),
	//				sphere,
	//				false
	//				}
	//				});
	//		}
	//		else {
	//			bool isKinematic = bodyType == EBodyType::KINEMATIC;
	//			Physics->CreateDynamicBody(sphereInfo, EColliderType::COLLISION, isKinematic);
	//			//콜라이더 정보 저장
	//			m_colliderContainer.insert({ colliderID, {
	//				m_sphereTypeId,
	//				sphere,
	//				sphere->GetOwner(),
	//				sphere,
	//				false
	//				}
	//				});
	//		}
	//	}
	//	//todo : capsule 콜라이더라면
	//	auto capsule = obj->GetComponent<CapsuleColliderComponent>();
	//	if (capsule) {

	//		auto capsuleInfo = capsule->GetCapsuleInfo();
	//		auto posOffset = capsule->GetPositionOffset();
	//		auto rotOffset = capsule->GetRotationOffset();
	//		unsigned int colliderID = ++m_lastColliderID;
	//		capsuleInfo.colliderInfo.id = colliderID;
	//		capsuleInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	//		capsuleInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	//		capsuleInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	//		capsuleInfo.colliderInfo.collsionTransform.localMatrix.Decompose(capsuleInfo.colliderInfo.collsionTransform.localScale, capsuleInfo.colliderInfo.collsionTransform.localRotation, capsuleInfo.colliderInfo.collsionTransform.localPosition);
	//		capsuleInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(capsuleInfo.colliderInfo.collsionTransform.worldScale, capsuleInfo.colliderInfo.collsionTransform.worldRotation, capsuleInfo.colliderInfo.collsionTransform.worldPosition);
	//		//offset 계산
	//		if (posOffset != DirectX::SimpleMath::Vector3::Zero)
	//		{
	//			capsuleInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
	//			capsuleInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
	//			capsuleInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;
	//			posOffset = DirectX::SimpleMath::Vector3::Transform(posOffset, capsuleInfo.colliderInfo.collsionTransform.worldMatrix);
	//			capsuleInfo.colliderInfo.collsionTransform.worldPosition += posOffset;
	//			capsuleInfo.colliderInfo.collsionTransform.worldMatrix._41 = capsuleInfo.colliderInfo.collsionTransform.worldPosition.x;
	//			capsuleInfo.colliderInfo.collsionTransform.worldMatrix._42 = capsuleInfo.colliderInfo.collsionTransform.worldPosition.y;
	//			capsuleInfo.colliderInfo.collsionTransform.worldMatrix._43 = capsuleInfo.colliderInfo.collsionTransform.worldPosition.z;
	//		}
	//		capsule->SetCapsuleInfoMation(capsuleInfo);
	//		if (bodyType == EBodyType::STATIC) {
	//			//pxScene에 엑터 추가
	//			Physics->CreateStaticBody(capsule->GetCapsuleInfo(), EColliderType::COLLISION);
	//			m_colliderContainer.insert({ colliderID, {
	//				m_capsuleTypeId,
	//				capsule,
	//				capsule->GetOwner(),
	//				capsule,
	//				false
	//				}
	//				});
	//		}
	//		else {
	//			bool isKinematic = bodyType == EBodyType::KINEMATIC;
	//			Physics->CreateDynamicBody(capsuleInfo, EColliderType::COLLISION, isKinematic);
	//			m_colliderContainer.insert({ colliderID, {
	//				m_capsuleTypeId,
	//				capsule,
	//				capsule->GetOwner(),
	//				capsule,
	//				false
	//				}
	//				});
	//		}
	//	}
	//	//todo : convex 콜라이더라면

	//	auto convex = obj->GetComponent<MeshColliderComponent>();
	//	bool hasMesh = obj->HasComponent<MeshRenderer>();
	//	if (convex && hasMesh) {

	//		auto type = convex->GetColliderType();
	//		auto convexMeshInfo = convex->GetMeshInfo();
	//		auto posOffset = convex->GetPositionOffset();
	//		auto rotOffset = convex->GetRotationOffset();

	//		unsigned int colliderID = ++m_lastColliderID;
	//		convexMeshInfo.colliderInfo.id = colliderID;
	//		convexMeshInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	//		//convexMeshInfo.colliderInfo.layerNumber = 0xffffff;

	//		convexMeshInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	//		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	//		convexMeshInfo.colliderInfo.collsionTransform.localMatrix.Decompose(convexMeshInfo.colliderInfo.collsionTransform.localScale, convexMeshInfo.colliderInfo.collsionTransform.localRotation, convexMeshInfo.colliderInfo.collsionTransform.localPosition);
	//		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(convexMeshInfo.colliderInfo.collsionTransform.worldScale, convexMeshInfo.colliderInfo.collsionTransform.worldRotation, convexMeshInfo.colliderInfo.collsionTransform.worldPosition);

	//		//offset 계산
	//		if (posOffset != DirectX::SimpleMath::Vector3::Zero)
	//		{
	//			convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
	//			convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
	//			convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;
	//			posOffset = DirectX::SimpleMath::Vector3::Transform(posOffset, convexMeshInfo.colliderInfo.collsionTransform.worldMatrix);
	//			convexMeshInfo.colliderInfo.collsionTransform.worldPosition += posOffset;
	//			convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._41 = convexMeshInfo.colliderInfo.collsionTransform.worldPosition.x;
	//			convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._42 = convexMeshInfo.colliderInfo.collsionTransform.worldPosition.y;
	//			convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._43 = convexMeshInfo.colliderInfo.collsionTransform.worldPosition.z;
	//		}

	//		auto model = obj->GetComponent<MeshRenderer>();
	//		auto modelVertices = model->m_Mesh->GetVertices();
	//		convexMeshInfo.vertices = new DirectX::SimpleMath::Vector3[modelVertices.size()];
	//		convexMeshInfo.vertexSize = modelVertices.size();
	//		for (int i = 0; i < modelVertices.size(); i++)
	//		{
	//			convexMeshInfo.vertices[i] = modelVertices[i].position;
	//		}

	//		convex->SetMeshInfoMation(convexMeshInfo);

	//		if (bodyType == EBodyType::STATIC) {
	//			//pxScene에 엑터 추가
	//			Physics->CreateStaticBody(convexMeshInfo, EColliderType::COLLISION);
	//			m_colliderContainer.insert({ colliderID, {
	//				m_convexTypeId,
	//				convex,
	//				convex->GetOwner(),
	//				convex,
	//				false
	//				}
	//				});
	//		}
	//		else {
	//			bool isKinematic = bodyType == EBodyType::KINEMATIC;
	//			Physics->CreateDynamicBody(convexMeshInfo, EColliderType::COLLISION, isKinematic);
	//			m_colliderContainer.insert({ colliderID, {
	//				m_convexTypeId,
	//				convex,
	//				convex->GetOwner(),
	//				convex,
	//				false
	//				}
	//				});
	//		}
	//	}

	//	//todo : controller Component라면
	//	auto controller = obj->GetComponent<CharacterControllerComponent>();
	//	if (controller) {
	//		auto controllerInfo = controller->GetControllerInfo();
	//		auto movementInfo = controller->GetMovementInfo();
	//		auto posOffset = controller->GetPositionOffset();
	//		auto rotOffset = controller->GetRotationOffset();
	//		auto transform = obj->m_transform;

	//		ColliderID colliderID = ++m_lastColliderID;
	//		controllerInfo.id = colliderID;
	//		controllerInfo.layerNumber = obj->GetCollisionType();
	//		DirectX::SimpleMath::Vector3 position = transform.GetWorldPosition();
	//		controllerInfo.position = position + controller->GetPositionOffset();

	//		Physics->CreateCCT(controllerInfo, movementInfo);

	//		m_colliderContainer.insert({ colliderID, {
	//			m_controllerTypeId,
	//			controller,
	//			controller->GetOwner(),
	//			controller,
	//			false
	//			} });

	//		controller->SetControllerInfo(controllerInfo);
	//	}
	//	//관절 정보가 있는 ragdoll이라면
	//	//auto ragdoll = obj->GetComponent<RagdollComponent>();
	//	//if (ragdoll) {
	//		//todo : ragdoll 콜라이더 생성
	//	//}
	//	//todo : heightfield 콜라이더라면
	//	//auto heightfield = obj->GetComponent<HeightFieldColliderComponent>();
	//	//if (heightfield) {
	//		//todo : heightfield 콜라이더 생성
	//	// }


	//};
#pragma endregion
	
}

void PhysicsManager::OnUnloadScene()
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	for (auto& [id, info] : Container) 
	{
		info.bIsDestroyed = true;
	}

	Physics->ChangeScene();
	Physics->Update(1.0f);
	Physics->FinalUpdate();
	m_callbacks.clear();
}

void PhysicsManager::ProcessCallback()
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	for (auto& [data, type] : m_callbacks) {

		auto lhs = Container.find(data.thisId);
		auto rhs = Container.find(data.otherId);
		bool isSameID = data.thisId == data.otherId;
		auto iterEnd = Container.end();

		if (isSameID || lhs == iterEnd || rhs == iterEnd)
		{
			//자신의 콜라이더와 충돌 이거나 충돌체가 등록이 되어 있지 않은 경우 -> error
			Debug->LogError("Collision Callback Error lfs :" + std::to_string(data.thisId) + " ,rhs : " + std::to_string(data.otherId));
			continue;
		}

		auto lhsObj = lhs->second.gameObject;
		auto rhsObj = rhs->second.gameObject;

		Collision collision{ lhsObj,rhsObj,data.contactPoints };

		switch (type)
		{
		case ECollisionEventType::ENTER_OVERLAP:
			SceneManagers->GetActiveScene()->OnCollisionEnter(collision);
			break;
		case ECollisionEventType::ON_OVERLAP:
			SceneManagers->GetActiveScene()->OnCollisionStay(collision);
			break;
		case ECollisionEventType::END_OVERLAP:
			SceneManagers->GetActiveScene()->OnCollisionExit(collision);
			break;
		case ECollisionEventType::ENTER_COLLISION:
			SceneManagers->GetActiveScene()->OnTriggerEnter(collision);
			break;
		case ECollisionEventType::ON_COLLISION:
			SceneManagers->GetActiveScene()->OnTriggerStay(collision);
			break;
		case ECollisionEventType::END_COLLISION:
			SceneManagers->GetActiveScene()->OnTriggerExit(collision);
			break;
		default:
			break;
		}

	}
}


void PhysicsManager::DrawDebugInfo()
{
	//collider 정보를 수집하여 render에서 wireframe로 그려줄 수 있도록 한다.
	//DebugRender debug;

	//물리씬에 있는 객체를 순회하며 콜리전 정보를 가져온다.

	//DebugData dd;
	//// 컴포넌트별 데이터 수집
	//for (auto* rb : m_rigidBodies)
	//	rb->FillDebugData(dd);

	//// 매니저가 처리하는 Raycast 디버그 추가
	//for (auto& rq : m_raycastSystem->GetRequests())
	//{
	//	if (rq.hit)
	//	{
	//		dd.lines.push_back({ rq.origin, rq.origin + rq.direction * rq.distance, {1,1,0,1} });
	//		dd.points.push_back({ rq.hitPosition, {1,0,0,1} });
	//	}
	//}

	//// IDebugRenderer에 제출
	//for (auto& l : dd.lines)   debug->DrawLine(l);
	//for (auto& s : dd.spheres) debug->DrawSphere(s);
	//for (auto& p : dd.points)  debug->DrawPoint(p);
	//for (auto& b : dd.boxes)   debug->DrawBox(b);
	//for (auto& c : dd.capsules) debug->DrawCapsule(c);
	//for (auto& m : dd.convexes) debug->DrawConvexMesh(c);
	//debug->Flush();
	
}

void PhysicsManager::RayCast(RayEvent& rayEvent)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	RayCastInput inputInfo;

	inputInfo.origin = rayEvent.origin;
	inputInfo.direction = rayEvent.direction;
	inputInfo.distance = rayEvent.distance;
	inputInfo.layerNumber = rayEvent.layerMask;

	auto result = Physics->RayCast(inputInfo,rayEvent.isStatic);

	if (result.hasBlock)
	{
		rayEvent.resultData->hasBlock = result.hasBlock;
		rayEvent.resultData->blockObject = Container[result.id].gameObject;
		rayEvent.resultData->blockPoint = result.blockPosition;
	}

	rayEvent.resultData->hitCount = result.hitSize;
	rayEvent.resultData->hitObjects.reserve(result.hitSize);
	rayEvent.resultData->hitPoints.reserve(result.hitSize);
	rayEvent.resultData->hitObjectLayer.reserve(result.hitSize);

	for (int i = 0; i < result.hitSize; i++)
	{
		rayEvent.resultData->hitObjects.push_back(Container[result.hitId[i]].gameObject);
		rayEvent.resultData->hitPoints.push_back(result.contectPoints[i]);
		rayEvent.resultData->hitObjectLayer.push_back(result.hitLayerNumber[i]);
	}

	if (rayEvent.bUseDebugDraw)
	{
		//todo : debug draw
		//origin , direction , distance
	}

}

void PhysicsManager::AddCollider(BoxColliderComponent* box)
{
	if (!box) return;

	auto obj = box->GetOwner();
	if (obj == nullptr)
	{
		Debug->LogError("BoxColliderComponent has no owner GameObject.");
		return;
	}

	auto& transform = obj->m_transform;
	unsigned int colliderID = ++m_lastColliderID;
	auto boxInfo = box->GetBoxInfo();
	auto tranformOffset = box->GetPositionOffset();
	auto rotationOffset = box->GetRotationOffset();

	boxInfo.colliderInfo.id = colliderID;
	boxInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	boxInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	boxInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	boxInfo.colliderInfo.collsionTransform.localMatrix.Decompose(boxInfo.colliderInfo.collsionTransform.localScale, boxInfo.colliderInfo.collsionTransform.localRotation, boxInfo.colliderInfo.collsionTransform.localPosition);
	boxInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(boxInfo.colliderInfo.collsionTransform.worldScale, boxInfo.colliderInfo.collsionTransform.worldRotation, boxInfo.colliderInfo.collsionTransform.worldPosition);

	//offset 계산
	if (tranformOffset != DirectX::SimpleMath::Vector3::Zero)
	{
		boxInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
		boxInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
		boxInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;

		tranformOffset = DirectX::SimpleMath::Vector3::Transform(tranformOffset, boxInfo.colliderInfo.collsionTransform.worldMatrix);

		boxInfo.colliderInfo.collsionTransform.worldPosition += tranformOffset;

		boxInfo.colliderInfo.collsionTransform.worldMatrix._41 = boxInfo.colliderInfo.collsionTransform.worldPosition.x;
		boxInfo.colliderInfo.collsionTransform.worldMatrix._42 = boxInfo.colliderInfo.collsionTransform.worldPosition.y;
		boxInfo.colliderInfo.collsionTransform.worldMatrix._43 = boxInfo.colliderInfo.collsionTransform.worldPosition.z;

	}

	box->SetBoxInfoMation(boxInfo);
}

void PhysicsManager::AddCollider(SphereColliderComponent* sphere)
{
	if (!sphere) return;

	auto obj = sphere->GetOwner();
	auto& transform = obj->m_transform;
	auto type = sphere->GetColliderType();
	auto sphereInfo = sphere->GetSphereInfo();
	auto posOffset = sphere->GetPositionOffset();
	auto rotOffset = sphere->GetRotationOffset();

	unsigned int colliderID = ++m_lastColliderID;

	sphereInfo.colliderInfo.id = colliderID;
	sphereInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	sphereInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	sphereInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	sphereInfo.colliderInfo.collsionTransform.localMatrix.Decompose(sphereInfo.colliderInfo.collsionTransform.localScale, sphereInfo.colliderInfo.collsionTransform.localRotation, sphereInfo.colliderInfo.collsionTransform.localPosition);
	sphereInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(sphereInfo.colliderInfo.collsionTransform.worldScale, sphereInfo.colliderInfo.collsionTransform.worldRotation, sphereInfo.colliderInfo.collsionTransform.worldPosition);

	//offset 계산
	if (posOffset != DirectX::SimpleMath::Vector3::Zero)
	{
		sphereInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
		sphereInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
		sphereInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;

		posOffset = DirectX::SimpleMath::Vector3::Transform(posOffset, sphereInfo.colliderInfo.collsionTransform.worldMatrix);

		sphereInfo.colliderInfo.collsionTransform.worldPosition += posOffset;

		sphereInfo.colliderInfo.collsionTransform.worldMatrix._41 = sphereInfo.colliderInfo.collsionTransform.worldPosition.x;
		sphereInfo.colliderInfo.collsionTransform.worldMatrix._42 = sphereInfo.colliderInfo.collsionTransform.worldPosition.y;
		sphereInfo.colliderInfo.collsionTransform.worldMatrix._43 = sphereInfo.colliderInfo.collsionTransform.worldPosition.z;
	}

	sphere->SetSphereInfoMation(sphereInfo);
}

void PhysicsManager::AddCollider(CapsuleColliderComponent* capsule)
{
	if (!capsule) return;

	auto obj = capsule->GetOwner();
	auto& transform = obj->m_transform;
	auto capsuleInfo = capsule->GetCapsuleInfo();
	auto posOffset = capsule->GetPositionOffset();
	auto rotOffset = capsule->GetRotationOffset();
	unsigned int colliderID = ++m_lastColliderID;
	capsuleInfo.colliderInfo.id = colliderID;
	capsuleInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	capsuleInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	capsuleInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	capsuleInfo.colliderInfo.collsionTransform.localMatrix.Decompose(capsuleInfo.colliderInfo.collsionTransform.localScale, capsuleInfo.colliderInfo.collsionTransform.localRotation, capsuleInfo.colliderInfo.collsionTransform.localPosition);
	capsuleInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(capsuleInfo.colliderInfo.collsionTransform.worldScale, capsuleInfo.colliderInfo.collsionTransform.worldRotation, capsuleInfo.colliderInfo.collsionTransform.worldPosition);
	//offset 계산
	if (posOffset != DirectX::SimpleMath::Vector3::Zero)
	{
		capsuleInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
		capsuleInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
		capsuleInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;
		posOffset = DirectX::SimpleMath::Vector3::Transform(posOffset, capsuleInfo.colliderInfo.collsionTransform.worldMatrix);
		capsuleInfo.colliderInfo.collsionTransform.worldPosition += posOffset;
		capsuleInfo.colliderInfo.collsionTransform.worldMatrix._41 = capsuleInfo.colliderInfo.collsionTransform.worldPosition.x;
		capsuleInfo.colliderInfo.collsionTransform.worldMatrix._42 = capsuleInfo.colliderInfo.collsionTransform.worldPosition.y;
		capsuleInfo.colliderInfo.collsionTransform.worldMatrix._43 = capsuleInfo.colliderInfo.collsionTransform.worldPosition.z;
	}
	capsule->SetCapsuleInfoMation(capsuleInfo);
}

void PhysicsManager::AddCollider(MeshColliderComponent* mesh)
{
	if (!mesh) return;

	auto obj = mesh->GetOwner();
	bool hasMesh = obj->HasComponent<MeshRenderer>();

	if (!hasMesh) return;

	auto& transform = obj->m_transform;
	auto type = mesh->GetColliderType();
	auto convexMeshInfo = mesh->GetMeshInfo();
	auto posOffset = mesh->GetPositionOffset();
	auto rotOffset = mesh->GetRotationOffset();
	unsigned int colliderID = ++m_lastColliderID;
	convexMeshInfo.colliderInfo.id = colliderID;
	convexMeshInfo.colliderInfo.layerNumber = obj->GetCollisionType();
	convexMeshInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	convexMeshInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();
	convexMeshInfo.colliderInfo.collsionTransform.localMatrix.Decompose(convexMeshInfo.colliderInfo.collsionTransform.localScale, convexMeshInfo.colliderInfo.collsionTransform.localRotation, convexMeshInfo.colliderInfo.collsionTransform.localPosition);
	convexMeshInfo.colliderInfo.collsionTransform.worldMatrix.Decompose(convexMeshInfo.colliderInfo.collsionTransform.worldScale, convexMeshInfo.colliderInfo.collsionTransform.worldRotation, convexMeshInfo.colliderInfo.collsionTransform.worldPosition);
	//offset 계산
	if (posOffset != DirectX::SimpleMath::Vector3::Zero)
	{
		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._41 = 0.0f;
		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._42 = 0.0f;
		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._43 = 0.0f;
		posOffset = DirectX::SimpleMath::Vector3::Transform(posOffset, convexMeshInfo.colliderInfo.collsionTransform.worldMatrix);
		convexMeshInfo.colliderInfo.collsionTransform.worldPosition += posOffset;
		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._41 = convexMeshInfo.colliderInfo.collsionTransform.worldPosition.x;
		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._42 = convexMeshInfo.colliderInfo.collsionTransform.worldPosition.y;
		convexMeshInfo.colliderInfo.collsionTransform.worldMatrix._43 = convexMeshInfo.colliderInfo.collsionTransform.worldPosition.z;
	}
	auto model = obj->GetComponent<MeshRenderer>();
	auto modelVertices = model->m_Mesh->GetVertices();

	mesh->SetMeshInfoMation(convexMeshInfo);
}

void PhysicsManager::AddCollider(CharacterControllerComponent* controller)
{
	if (!controller) return;

	auto obj = controller->GetOwner();
	auto& transform = obj->m_transform;
	auto controllerInfo = controller->GetControllerInfo();
	auto movementInfo = controller->GetMovementInfo();
	auto posOffset = controller->GetPositionOffset();
	auto rotOffset = controller->GetRotationOffset();
	ColliderID colliderID = ++m_lastColliderID;
	controllerInfo.id = colliderID;
	controllerInfo.layerNumber = obj->GetCollisionType();
	DirectX::SimpleMath::Vector3 position = transform.GetWorldPosition();
	controllerInfo.position = position + controller->GetPositionOffset();
	Physics->CreateCCT(controllerInfo, movementInfo);

	controller->SetControllerInfo(controllerInfo);
}

void PhysicsManager::AddCollider(TerrainColliderComponent* terrain)
{
	if (!terrain) return;

	auto object = terrain->GetOwner();
	TerrainColliderComponent* collider = object->GetComponent<TerrainColliderComponent>();
	Transform& transform = object->m_transform;
	auto terrainComponent = object->GetComponent<TerrainComponent>();

	HeightFieldColliderInfo heightFieldInfo;

	ColliderID colliderID = ++m_lastColliderID;

	collider->SetColliderID(colliderID);
	heightFieldInfo.colliderInfo.id = colliderID;
	heightFieldInfo.colliderInfo.layerNumber = object->GetCollisionType();
	heightFieldInfo.colliderInfo.collsionTransform.localMatrix = transform.GetLocalMatrix();
	heightFieldInfo.colliderInfo.collsionTransform.worldMatrix = transform.GetWorldMatrix();

	heightFieldInfo.numCols = terrainComponent->GetWidth();
	heightFieldInfo.numRows = terrainComponent->GetHeight();
	heightFieldInfo.heightMep = terrainComponent->GetHeightMap();

	heightFieldInfo.colliderInfo.staticFriction = 0.5f;
	heightFieldInfo.colliderInfo.dynamicFriction = 0.5f;
	heightFieldInfo.colliderInfo.restitution = 0.1f;

	Physics->CreateStaticBody(heightFieldInfo, EColliderType::COLLISION);

	collider->SetHeightFieldColliderInfo(heightFieldInfo);
}

void PhysicsManager::RemoveCollider(BoxColliderComponent* box)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	if (box && box->IsDestroyMark())
	{
		auto ID = box->GetBoxInfo().colliderInfo.id;
		Container[ID].bIsDestroyed = true;
	}
}

void PhysicsManager::RemoveCollider(SphereColliderComponent* sphere)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	if (sphere && sphere->IsDestroyMark())
	{
		auto ID = sphere->GetSphereInfo().colliderInfo.id;
		Container[ID].bIsDestroyed = true;
	}
}

void PhysicsManager::RemoveCollider(CapsuleColliderComponent* capsule)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	if (capsule && capsule->IsDestroyMark())
	{
		auto ID = capsule->GetCapsuleInfo().colliderInfo.id;
		Container[ID].bIsDestroyed = true;
	}
}

void PhysicsManager::RemoveCollider(MeshColliderComponent* mesh)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	if (mesh && mesh->IsDestroyMark())
	{
		auto ID = mesh->GetMeshInfo().colliderInfo.id;
		Container[ID].bIsDestroyed = true;
	}
}

void PhysicsManager::RemoveCollider(CharacterControllerComponent* controller)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	if (controller && controller->IsDestroyMark())
	{
		auto ID = controller->GetControllerInfo().id;
		Container[ID].bIsDestroyed = true;
	}
}

void PhysicsManager::RemoveCollider(TerrainColliderComponent* terrain)
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	if (terrain && terrain->IsDestroyMark())
	{
		auto ID = terrain->GetColliderID();
		Container[ID].bIsDestroyed = true;
	}
}

void PhysicsManager::CallbackEvent(CollisionData data, ECollisionEventType type)
{
	m_callbacks.push_back({ data,type });
}

void PhysicsManager::SetPhysicData()
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	for (auto& [id, colliderInfo] : Container) 
	{
		if (colliderInfo.bIsDestroyed)
		{
			continue;
		}

		auto& transform = colliderInfo.gameObject->m_transform;
		auto rigidbody = colliderInfo.gameObject->GetComponent<RigidBodyComponent>();
		auto offset = colliderInfo.collider->GetPositionOffset();

		//todo : CCT,Controller,ragdoll,capsule,차후 deformeSuface
		if (colliderInfo.id == m_controllerTypeId)
		{
			//케릭터 컨트롤러
			auto controller = colliderInfo.gameObject->GetComponent<CharacterControllerComponent>();
			CharacterControllerGetSetData data;
			DirectX::SimpleMath::Vector3 position = transform.GetWorldPosition();
			data.position = position+controller->GetPositionOffset();
			data.rotation = transform.GetWorldQuaternion();
			data.Scale = transform.GetWorldScale();

			auto controllerInfo = controller->GetControllerInfo();
			auto prevlayer = controllerInfo.layerNumber;
			auto currentLayer = static_cast<unsigned int>(colliderInfo.gameObject->GetCollisionType());
			
			if (prevlayer != currentLayer) {
				data.LayerNumber = currentLayer;
				controller->SetControllerInfo(controllerInfo);
			}

			Physics->SetCCTData(id, data);

			CharacterMovementGetSetData movementData;

			auto movementInfo = controller->GetMovementInfo();
			movementData.acceleration = movementInfo.acceleration;
			movementData.maxSpeed = movementInfo.maxSpeed;
			movementData.velocity = rigidbody->GetLinearVelocity();
			movementData.isFall = controller->IsFalling();
			movementData.restrictDirection = controller->GetMoveRestrict();

			Physics->SetMovementData(id, movementData);

		}
		else
		{
			//기본도형
			RigidBodyGetSetData data;
			data.transform = transform.GetWorldMatrix();
			data.angularVelocity = rigidbody->GetAngularVelocity();
			data.linearVelocity = rigidbody->GetLinearVelocity();
			data.isLockLinearX = rigidbody->IsLockLinearX();
			data.isLockLinearY = rigidbody->IsLockLinearY();
			data.isLockLinearZ = rigidbody->IsLockLinearZ();
			data.isLockAngularX = rigidbody->IsLockAngularX();
			data.isLockAngularY = rigidbody->IsLockAngularY();
			data.isLockAngularZ = rigidbody->IsLockAngularZ();

			if (offset != DirectX::SimpleMath::Vector3::Zero) 
			{
				data.transform._41 = 0.0f;
				data.transform._42 = 0.0f;
				data.transform._43 = 0.0f;

				auto pos = transform.GetWorldPosition();
				auto vecPos = DirectX::SimpleMath::Vector3(pos);
				offset = DirectX::SimpleMath::Vector3::Transform(offset, data.transform);
				data.transform._41 = vecPos.x + offset.x;
				data.transform._42 = vecPos.y + offset.y;
				data.transform._43 = vecPos.z + offset.z;

			}
			
			Physics->SetRigidBodyData(id, data);
		}
	}
}

//PxScene --> GameScene
void PhysicsManager::GetPhysicData()
{
	auto Container = SceneManagers->GetActiveScene()->m_colliderContainer;
	for (auto& [id, ColliderInfo] : Container) {
		
		if(nullptr == ColliderInfo.gameObject)
			continue;
		
		if (ColliderInfo.gameObject->IsDestroyMark())
		{
			ColliderInfo.bIsDestroyed = true;
			continue;
		}

		//삭제 예정된 콜라이더는 건너뛴다
		if (ColliderInfo.bIsDestroyed)
		{
			continue;
		}

		auto rigidbody = ColliderInfo.gameObject->GetComponent<RigidBodyComponent>();
		auto& transform = ColliderInfo.gameObject->m_transform;
		auto offset = ColliderInfo.collider->GetPositionOffset();

		if (rigidbody->GetBodyType() != EBodyType::DYNAMIC) {
			continue;
		}

		//todo : CCT,Controller,ragdoll,capsule,차후 deformeSuface
		if (ColliderInfo.id == m_controllerTypeId) {
			//케릭터 컨트롤러
			auto controller = ColliderInfo.gameObject->GetComponent<CharacterControllerComponent>();
			auto controll = Physics->GetCCTData(id);
			auto movement = Physics->GetMovementData(id);
			auto position = controll.position - controller->GetPositionOffset();

			controller->SetFalling(movement.isFall);
			rigidbody->SetLinearVelocity(movement.velocity);
			transform.SetPosition(position);
		}
		else
		{
			//기본 도형
			auto data = Physics->GetRigidBodyData(id);
			rigidbody->SetLinearVelocity(data.linearVelocity);
			rigidbody->SetAngularVelocity(data.angularVelocity);

			rigidbody->SetLockLinearX(data.isLockLinearX);
			rigidbody->SetLockLinearY(data.isLockLinearY);
			rigidbody->SetLockLinearZ(data.isLockLinearZ);
			rigidbody->SetLockAngularX(data.isLockAngularX);
			rigidbody->SetLockAngularY(data.isLockAngularY);
			rigidbody->SetLockAngularZ(data.isLockAngularZ);

			auto matrix = data.transform;

			if (offset != DirectX::SimpleMath::Vector3::Zero) {

				DirectX::SimpleMath::Vector3 pos, scale;
				DirectX::SimpleMath::Quaternion rotation;
				matrix.Decompose(scale, rotation, pos);
				matrix._41 = 0.0f;
				matrix._42 = 0.0f;
				matrix._43 = 0.0f;

				offset = DirectX::SimpleMath::Vector3::Transform(offset, matrix);
				pos -= offset;

				matrix._41 = pos.x;
				matrix._42 = pos.y;
				matrix._43 = pos.z;

			}

			transform.SetAndDecomposeMatrix(matrix);
		}
	}
}

//void PhysicsManager::SetCollisionMatrix(unsigned int layer, unsigned int other, bool isCollision)
//{
//	Physics->SetCollisionMatrix(layer, other, isCollision);
//}
