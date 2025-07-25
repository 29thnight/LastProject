#include "Physx.h"
#include "PhysicsCommon.h"
#include "PhysicsHelper.h"
#include "PhysicsEventCallback.h"
#include "ConvexMeshResource.h"
#include "TriangleMeshResource.h"
#include "HeightFieldResource.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <tuple>
#include <mutex>
#include <iostream>

#include "ICollider.h"
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

PxFilterFlags CustomFilterShader(
	PxFilterObjectAttributes at0,
	PxFilterData fd0,
	PxFilterObjectAttributes at1,
	PxFilterData fd1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	
	if (PxFilterObjectIsTrigger(at0) || PxFilterObjectIsTrigger(at1)) {
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_LOST;
		return PxFilterFlag::eDEFAULT;
	}
	else {
		if ((fd0.word1 & (1 << fd1.word0)) && (fd1.word1 & (1 << fd0.word0))) {
			pairFlags = PxPairFlag::eCONTACT_DEFAULT
				| PxPairFlag::eNOTIFY_CONTACT_POINTS
				| PxPairFlag::eNOTIFY_TOUCH_FOUND
				| PxPairFlag::eNOTIFY_TOUCH_LOST
				| PxPairFlag::eNOTIFY_TOUCH_PERSISTS;;
			return PxFilterFlag::eDEFAULT;
		}
		return PxFilterFlag::eSUPPRESS;
	}
}

class BlockRaycastQueryFilter : public physx::PxQueryFilterCallback {
public:
	virtual ~BlockRaycastQueryFilter() {}
	physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData,
		const physx::PxShape* shape,
		const physx::PxRigidActor* actor,
		physx::PxHitFlags& queryFlags) override {

		auto data = shape->getSimulationFilterData();

		if (filterData.word1 & (1 << data.word0)) {
			return physx::PxQueryHitType::eBLOCK;
		}

		return physx::PxQueryHitType::eNONE;

	}

	physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData,
		const physx::PxQueryHit& hit,
		const physx::PxShape* shape,
		const physx::PxRigidActor* actor) override {

		auto data = shape->getSimulationFilterData();

		if (filterData.word1 & (1 << data.word0)) {
			return physx::PxQueryHitType::eBLOCK;
		}
		return physx::PxQueryHitType::eNONE;
	}
};

class TouchRaycastQueryFilter : public physx::PxQueryFilterCallback
{
public:
	virtual ~TouchRaycastQueryFilter(){}
	physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData,
		const physx::PxShape* shape,
		const physx::PxRigidActor* actor,
		physx::PxHitFlags& queryFlags) override {

		auto data = shape->getSimulationFilterData();

		if (filterData.word1 & (1 << data.word0)) {
			return physx::PxQueryHitType::eTOUCH;
		}

		return physx::PxQueryHitType::eNONE;

	}
	
	physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData,
		const physx::PxQueryHit& hit,
		const physx::PxShape* shape,
		const physx::PxRigidActor* actor) override {

		auto data = shape->getSimulationFilterData();

		if (filterData.word1 & (1 << data.word0)) {
			return physx::PxQueryHitType::eTOUCH;
		}
		return physx::PxQueryHitType::eNONE;
	}
};

bool PhysicX::Initialize()
{
	m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback);     

#if _DEBUG
	// PVD 연결
	// PVD는 _DEBUG가 아니면 nullptr
	pvd = PxCreatePvd(*m_foundation);
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	auto isconnected = pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
#endif

	//m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale(1.f, 40.f), recordMemoryAllocations, pvd);  
	m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale(1.f, 10.f), true, pvd);  // 물리 초기화
	if (m_physics == nullptr)
	{
		return false;
	}

	// physics 코어 개수
	UINT MaxThread = 8;
	UINT core = std::thread::hardware_concurrency();
	// 현재 코어 개수가 최대 코어 개수보다 많으면 최대 코어 개수 사용
	if (core < 4) core = 2;
	else if (core > MaxThread + 4) core = MaxThread;
	else core -= 4;
	// 디스패처 생성
	gDispatcher = PxDefaultCpuDispatcherCreate(core);

	m_defaultMaterial = m_physics->createMaterial(1.f, 1.f, 0.f);

	//gDispatcher = PxDefaultCpuDispatcherCreate(2);  
	// CUDA 초기화
	PxCudaContextManagerDesc cudaDesc;
	m_cudaContextManager = PxCreateCudaContextManager(*m_foundation, cudaDesc);
	if (!m_cudaContextManager || !m_cudaContextManager->contextIsValid())
	{
		m_cudaContextManager->release();
		m_cudaContextManager = nullptr;
	}

	if (m_cudaContextManager) {
		m_cudaContext = m_cudaContextManager->getCudaContext();
		if (m_cudaContext) {
			std::cout << "CUDA context created successfully." << std::endl;
		}
		else {
			std::cout << "Failed to create CUDA context." << std::endl;
		}
	}
	else {
		std::cout << "CUDA context manager creation failed." << std::endl;
	}


	//충돌 처리를 위한 콜백 등록
	m_eventCallback = new PhysicsEventCallback();


	// Scene 생성
	physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
	sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);

	
	sceneDesc.cpuDispatcher = gDispatcher;
	//sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	sceneDesc.filterShader = CustomFilterShader;

	sceneDesc.simulationEventCallback = m_eventCallback;

	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS; // 활성 액터를 업데이트
	//sceneDesc.kineKineFilteringMode = PxPairFilteringMode::eKEEP;
	// GPU 시뮬레이션 사용 여부 GPU에서 시뮬레이션이 가능하도록 설정
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_GPU_DYNAMICS; // GPU 사용
	sceneDesc.broadPhaseType = PxBroadPhaseType::eGPU; // GPU 사용
	sceneDesc.cudaContextManager = m_cudaContextManager;

	m_scene = m_physics->createScene(sceneDesc); // 씬 생성

	m_characterControllerManager = PxCreateControllerManager(*m_scene);


	//======================================================================
	//debug용 plane 생성 --> triangle mesh로 객체 생성
	
	physx::PxRigidStatic* plane = m_physics->createRigidStatic(PxTransform(PxQuat(PxPi / 2, PxVec3(0, 0, 1))));
	auto material = m_defaultMaterial;
	material->setRestitution(0.0f);
	material->setDynamicFriction(1.0f);
	material->setStaticFriction(1.0f);
	physx::PxShape* planeShape = m_physics->createShape(physx::PxPlaneGeometry(), *material);
	planeShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
	planeShape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
	physx::PxFilterData filterData;
	filterData.word0 = 0;
	filterData.word1 = 0xFFFFFFFF;
	planeShape->setSimulationFilterData(filterData);
	planeShape->setQueryFilterData(filterData);
	plane->attachShape(*planeShape);
	plane->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
	plane->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);
	m_scene->addActor(*plane);
	planeShape->release();

	//======================================================================


	return true;
}
//void PhysicX::HasConvexMeshResource(const unsigned int& hash)
//{
//	
//}
void PhysicX::RemoveActors()
{
	for (auto removeActor : m_removeActorList)
	{
		m_scene->removeActor(*removeActor);
	}
	m_removeActorList.clear();
}
void PhysicX::UnInitialize() {
	if (m_foundation) m_foundation->release();
	
}


void PhysicX::Update(float fixedDeltaTime)
{
	// PxScene 업데이트
	RemoveActors();
	//첫 틱 마다 시뮬레이션 업데이트 -> 물리 업데이트
	//rigid body 업데이트
	{
		//콜라이더 업데이트
		//업데이트 할 액터들을 씬에 추가
		for (RigidBody* body : m_updateActors) {
			DynamicRigidBody* dynamicBody = dynamic_cast<DynamicRigidBody*>(body);
			if (dynamicBody) {
				m_scene->addActor(*dynamicBody->GetRigidDynamic());
			}
			StaticRigidBody* staticBody = dynamic_cast<StaticRigidBody*>(body);
			if (staticBody) {
				m_scene->addActor(*staticBody->GetRigidStatic());
			}
		}
		m_updateActors.clear();
	}
	{
		//캐릭터 업데이트
		//캐릭터 컨트롤러 업데이트
		for (const auto& controller : m_characterControllerContainer) {
			controller.second->Update(fixedDeltaTime);
			//컨트롤러의 물리 상태가 변하면
			//if(todo : condition){
			//controller.second->GetController()->release();
			// }

		}
		//새로 생성된 캐릭터 컨트롤러를 업데이트 리스트에 추가
		for (auto& [contrllerInfo, movementInfo] : m_updateCCTList)
		{
			CollisionData* collisionData = new CollisionData();

			physx::PxCapsuleControllerDesc desc;

			CharacterController* controller = new CharacterController();
			controller->Initialize(contrllerInfo, movementInfo, m_characterControllerManager, m_defaultMaterial, collisionData, m_collisionMatrix);

			desc.height = contrllerInfo.height;
			desc.radius = contrllerInfo.radius;
			desc.contactOffset = contrllerInfo.contactOffset;
			desc.stepOffset = contrllerInfo.stepOffset;
			desc.slopeLimit = contrllerInfo.slopeLimit;
			desc.position.x = contrllerInfo.position.x;
			desc.position.y = contrllerInfo.position.y;
			desc.position.z = contrllerInfo.position.z; 
			desc.scaleCoeff = 1.0f;
			desc.maxJumpHeight = 100.0f;
			desc.nonWalkableMode = physx::PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
			desc.material = m_defaultMaterial;
		
			physx::PxController* pxController = m_characterControllerManager->createController(desc);
			
			physx::PxRigidDynamic* body = pxController->getActor();
			physx::PxShape* shape;
			int shapeSize = body->getNbShapes();
			body->getShapes(&shape, shapeSize);
			body->setSolverIterationCounts(8, 4);
			shape->setContactOffset(0.02f);
			shape->setRestOffset(0.01f);
			physx::PxFilterData filterData;
			filterData.word0 = contrllerInfo.layerNumber;
			filterData.word1= 0xFFFFFFFF;
			
			//filterData.word1 = m_collisionMatrix[contrllerInfo.layerNumber];
			shape->setSimulationFilterData(filterData);
			//shape->setQueryFilterData(filterData);
			//shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			//shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true); //&&&&&sehwan

			collisionData->thisId = contrllerInfo.id;
			collisionData->thisLayerNumber = contrllerInfo.layerNumber;
			shape->userData = collisionData;
			body->userData = collisionData;

			controller->SetController(pxController);

			CreateCollisionData(contrllerInfo.id, collisionData);
			m_characterControllerContainer.insert({ controller->GetID(), controller });
		}
		m_updateCCTList.clear();

		//대기중인 캐릭터 컨트롤러를 업데이트 리스트에 추가
		for (auto& controllerInfo : m_waittingCCTList) {
			m_updateCCTList.push_back(controllerInfo);
		}
		m_waittingCCTList.clear();
	}
	{
		//충돌 업데이트
		//새로 생성된 충돌 데이터를 제거
		for (unsigned int& removeID : m_removeCollisionIds) {
			auto iter = m_collisionDataContainer.find(removeID);
			if (iter != m_collisionDataContainer.end()) {
				m_collisionDataContainer.erase(iter);
			}
		}
		m_removeCollisionIds.clear();

		for (auto& collisionData : m_collisionDataContainer) {
			if (collisionData.second->isDead == true)
			{
				m_removeCollisionIds.push_back(collisionData.first);
			}
		}
	}
	{
	//ragdoll 업데이트
		for (auto& [id,ragdoll] : m_ragdollContainer) {
		
			ragdoll->Update(fixedDeltaTime);
		}
	}
	//Scene 시뮬레이션
	if (!m_scene->simulate(fixedDeltaTime))
	{
		Debug->LogCritical("physic m_scene simulate failed");
		return;
	}
	//시뮬레이션 결과 가져오기
	if (!m_scene->fetchResults(true))
	{
		Debug->LogCritical("physic m_scene fetchResults failed");
		return;
	}
	//콜백 이벤트 처리
	m_eventCallback->StartTrigger();
	//오류 처리
	//if (cudaGetLastError() != cudaError::cudaSuccess)
	//{
	//	Debug->LogError("cudaGetLastError : "+ std::string(cudaGetErrorString(cudaGetLastError())));
	//}
}

void PhysicX::FinalUpdate()
{
	//딱 한번만 불린다. 아마도 씬이 끝날때
}

void PhysicX::SetCallBackCollisionFunction(std::function<void(CollisionData, ECollisionEventType)> func)
{
	m_eventCallback->SetCallbackFunction(func);
}

void PhysicX::SetPhysicsInfo()
{
		
	// m_scene->setGravity(PxVec3(0.0f, -9.81f, 0.0f));
	//collisionMatrix[0][0] = true;기본 모든 레이어가 충돌하는 상태로 설정
}

void PhysicX::ChangeScene()
{
	// 모든 객체를 씬에서 제거하고 물리 엔진 객체 제거
	RemoveAllRigidBody(m_scene,m_removeActorList);
	RemoveAllCCT();
	RemoveAllCharacterInfo();
	//새로운 시뮬레이션에 추가될 객체
}

void PhysicX::DestroyActor(unsigned int id)
{
	//컨테이너에 있는지 확인
	auto iter = m_rigidBodyContainer.find(id);
	if (iter == m_rigidBodyContainer.end())
	{
		//std::cout << "PhysicX::DestroyActor : Actor not found with id" << id << std::endl;
		return;
	}

	RigidBody* body = iter->second;

	if (auto* dynamicBody = dynamic_cast<DynamicRigidBody*>(body))
	{
		//다이나믹 바디인 경우
		m_removeActorList.push_back(dynamicBody->GetRigidDynamic());
	}
	else if (auto* staticBody = dynamic_cast<StaticRigidBody*>(body))
	{
		//스테틱 바디인 경우
		m_removeActorList.push_back(staticBody->GetRigidStatic());
	}
	else
	{
		Debug->LogError("PhysicX::DestroyActor : Actor is not a valid RigidBody type");
		return;
	}

	delete body; // 메모리 해제
	m_rigidBodyContainer.erase(iter); // 컨테이너에서 제거

	// 충돌 데이터 제거
	RemoveCollisionData(id);
}

RayCastOutput PhysicX::RayCast(const RayCastInput& in, bool isStatic)
{
	physx::PxVec3 pxOrgin;
	physx::PxVec3 pxDirection;
	CopyVectorDxToPx(in.origin, pxOrgin);
	CopyVectorDxToPx(in.direction, pxDirection);

	// RaycastHit 
	const physx::PxU32 maxHits = 20;
	physx::PxRaycastHit hitBuffer[maxHits];
	physx::PxRaycastBuffer hitBufferStruct(hitBuffer, maxHits);

	//충돌 필터 데이터
	physx::PxQueryFilterData filterData;
	filterData.data.word0 = in.layerNumber;
	filterData.data.word1 = m_collisionMatrix[in.layerNumber];

	if (isStatic)
	{
		filterData.flags = physx::PxQueryFlag::eSTATIC
			| physx::PxQueryFlag::ePREFILTER
			| physx::PxQueryFlag::eNO_BLOCK
			| physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;
	}
	else
	{
		filterData.flags = physx::PxQueryFlag::eDYNAMIC
			| physx::PxQueryFlag::ePREFILTER
			| physx::PxQueryFlag::eNO_BLOCK
			| physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;
	}

	TouchRaycastQueryFilter queryFilter;
	
	bool isAnyHit;
		
	isAnyHit = m_scene->raycast(pxOrgin, pxDirection, in.distance, hitBufferStruct, physx::PxHitFlag::eDEFAULT, filterData,&queryFilter);


	RayCastOutput out;

	//hit가 있는 경우
	if (isAnyHit)
	{
		out.hasBlock = hitBufferStruct.hasBlock;
		if (out.hasBlock)
		{
			const physx::PxRaycastHit& blockHit = hitBufferStruct.block;
			if (blockHit.shape && blockHit.shape->userData != nullptr) {
				out.id = static_cast<CollisionData*>(hitBufferStruct.block.shape->userData)->thisId;
				CopyVectorPxToDx(hitBufferStruct.block.position, out.blockPosition);
				CopyVectorPxToDx(hitBufferStruct.block.normal, out.blockNormal);
			}
			else {
				out.hasBlock = false; // userData가 유효하지 않으면 블록 처리 안함
			}
		}

		unsigned int hitSize = hitBufferStruct.nbTouches;
		out.hitSize = hitSize;

		for (unsigned int hitNum = 0; hitNum < hitSize; hitNum++)
		{
			const physx::PxRaycastHit& hit = hitBufferStruct.touches[hitNum];
			physx::PxShape* shape = hit.shape;

			if (shape && shape->userData != nullptr) // 추가: shape 및 userData 유효성 확인
			{
				DirectX::SimpleMath::Vector3 position;
				DirectX::SimpleMath::Vector3 normal;
				CopyVectorPxToDx(hit.position, position);
				CopyVectorPxToDx(hit.normal, normal);
				unsigned int id = static_cast<CollisionData*>(shape->userData)->thisId;
				unsigned int layerNumber = static_cast<CollisionData*>(shape->userData)->thisLayerNumber;

				out.contectPoints.push_back(position);
				out.contectNormals.push_back(normal);
				out.hitLayerNumber.push_back(layerNumber);
				out.hitId.push_back(id);
			}
			else {
				out.hitSize--; // 유효하지 않은 hit는 카운트에서 제외
			}
		}
	}
	
	return out;
}

// 단일 레이캐스트, static dynamic 모두 검사, 
RayCastOutput PhysicX::Raycast(const RayCastInput& in)
{
	physx::PxVec3 pxOrgin;
	physx::PxVec3 pxDirection;
	CopyVectorDxToPx(in.origin, pxOrgin);
	CopyVectorDxToPx(in.direction, pxDirection);

	// RaycastHit 
	physx::PxRaycastBuffer hitBufferStruct;

	//충돌 필터 데이터
	physx::PxQueryFilterData filterData;
	filterData.data.word0 = in.layerNumber;
	filterData.data.word1 = m_collisionMatrix[in.layerNumber];

	filterData.flags = physx::PxQueryFlag::eSTATIC 
		| physx::PxQueryFlag::eDYNAMIC
		| physx::PxQueryFlag::ePREFILTER
		| physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;

	BlockRaycastQueryFilter queryFilter;

	bool isAnyHit;

	isAnyHit = m_scene->raycast(pxOrgin, pxDirection, in.distance, hitBufferStruct, physx::PxHitFlag::eDEFAULT, filterData, &queryFilter);

	RayCastOutput out;

	//hit가 있는 경우
	if (isAnyHit)
	{
		out.hasBlock = hitBufferStruct.hasBlock;
		if (out.hasBlock)
		{
			const physx::PxRaycastHit& blockHit = hitBufferStruct.block;
			if (blockHit.shape && blockHit.shape->userData != nullptr) {
				out.id = static_cast<CollisionData*>(hitBufferStruct.block.shape->userData)->thisId;
				out.blockLayerNumber = static_cast<CollisionData*>(hitBufferStruct.block.shape->userData)->thisLayerNumber;
				CopyVectorPxToDx(hitBufferStruct.block.position, out.blockPosition);
				CopyVectorPxToDx(hitBufferStruct.block.normal, out.blockNormal);
			}
			else {
				out.hasBlock = false;
				std::cout << "Not physx block userdata" << std::endl;
			}
		}
	}

	return out;
}

//PxQueryFlag::eNO_BLOCK 활성화시 block 처리x
RayCastOutput PhysicX::RaycastAll(const RayCastInput& in)
{
	physx::PxVec3 pxOrgin;
	physx::PxVec3 pxDirection;
	CopyVectorDxToPx(in.origin, pxOrgin);
	CopyVectorDxToPx(in.direction, pxDirection);

	// RaycastHit 
	const physx::PxU32 maxHits = 20;
	physx::PxRaycastHit hitBuffer[maxHits];
	physx::PxRaycastBuffer hitBufferStruct(hitBuffer, maxHits);

	//충돌 필터 데이터
	physx::PxQueryFilterData filterData;
	filterData.data.word0 = in.layerNumber;
	filterData.data.word1 = m_collisionMatrix[in.layerNumber];

	filterData.flags = physx::PxQueryFlag::eSTATIC
		| physx::PxQueryFlag::eDYNAMIC
		| physx::PxQueryFlag::ePREFILTER
		| physx::PxQueryFlag::eNO_BLOCK
		| physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;
	

	TouchRaycastQueryFilter queryFilter;

	bool isAnyHit;

	isAnyHit = m_scene->raycast(pxOrgin, pxDirection, in.distance, hitBufferStruct, physx::PxHitFlag::eDEFAULT, filterData, &queryFilter);


	RayCastOutput out;

	//hit가 있는 경우
	if (isAnyHit)
	{
		unsigned int hitSize = hitBufferStruct.nbTouches;
		out.hitSize = hitSize;

		for (unsigned int hitNum = 0; hitNum < hitSize; hitNum++)
		{
			const physx::PxRaycastHit& hit = hitBufferStruct.touches[hitNum];
			physx::PxShape* shape = hit.shape;

			if (shape && shape->userData != nullptr) { // 추가: shape 및 userData 유효성 확인
			DirectX::SimpleMath::Vector3 position;
			DirectX::SimpleMath::Vector3 normal;
			CopyVectorPxToDx(hit.position, position);
			CopyVectorPxToDx(hit.normal, normal);
			unsigned int id = static_cast<CollisionData*>(shape->userData)->thisId;
			unsigned int layerNumber = static_cast<CollisionData*>(shape->userData)->thisLayerNumber;

			out.contectPoints.push_back(position);
			out.contectNormals.push_back(normal);
			out.hitLayerNumber.push_back(layerNumber);
			out.hitId.push_back(id);
			}
			else {
				out.hitSize--; // 유효하지 않은 hit는 카운트에서 제외
			}
		}
	}

	return out;
}
//==========================================================================================
//rigid body

void PhysicX::CreateStaticBody(const BoxColliderInfo & info, const EColliderType & colliderType)
{
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxBoxGeometry(info.boxExtent.x, info.boxExtent.y, info.boxExtent.z), *material, true);

	StaticRigidBody* staticBody = SettingStaticBody(shape, info.colliderInfo, colliderType, m_collisionMatrix);

	shape->release();

	if (staticBody != nullptr)
	{
		staticBody->SetExtent(info.boxExtent.x, info.boxExtent.y, info.boxExtent.z);
	}
	
}

void PhysicX::CreateStaticBody(const SphereColliderInfo & info, const EColliderType & colliderType)
{
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxSphereGeometry(info.radius), *material, true);

	StaticRigidBody* staticBody = SettingStaticBody(shape, info.colliderInfo, colliderType, m_collisionMatrix);

	shape->release();

	if (staticBody != nullptr)
	{
		staticBody->SetRadius(info.radius);
	}
}

void PhysicX::CreateStaticBody(const CapsuleColliderInfo & info, const EColliderType & colliderType)
{
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxCapsuleGeometry(info.radius, info.height), *material, true);

	StaticRigidBody* staticBody = SettingStaticBody(shape, info.colliderInfo, colliderType, m_collisionMatrix);

	shape->release();

	if (staticBody != nullptr)
	{
		staticBody->SetRadius(info.radius);
		staticBody->SetHalfHeight(info.height);
	}
}

void PhysicX::CreateStaticBody(const ConvexMeshColliderInfo & info, const EColliderType & colliderType)
{
	ConvexMeshResource* convexMesh = new ConvexMeshResource(m_physics, info.vertices, info.vertexSize, info.convexPolygonLimit);
	physx::PxConvexMesh* pxConvexMesh = convexMesh->GetConvexMesh();

	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxConvexMeshGeometry(pxConvexMesh), *material, true);

	StaticRigidBody* staticBody = SettingStaticBody(shape, info.colliderInfo, colliderType, m_collisionMatrix);
	shape->release();
}

void PhysicX::CreateStaticBody(const TriangleMeshColliderInfo & info, const EColliderType & colliderType)
{
	TriangleMeshResource* triangleMesh = new TriangleMeshResource(m_physics, info.vertices, info.vertexSize, info.indices,info.indexSize);
	physx::PxTriangleMesh* pxTriangleMesh = triangleMesh->GetTriangleMesh();

	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxTriangleMeshGeometry(pxTriangleMesh), *material, true);

	StaticRigidBody* staticBody = SettingStaticBody(shape, info.colliderInfo, colliderType, m_collisionMatrix);

	shape->release();	
}

void PhysicX::CreateStaticBody(const HeightFieldColliderInfo & info, const EColliderType & colliderType)
{
	HeightFieldResource* heightField = new HeightFieldResource(m_physics, info.heightMep, info.numCols,info.numRows);
	physx::PxHeightField* pxHeightField = heightField->GetHeightField();

	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxHeightFieldGeometry(pxHeightField,physx::PxMeshGeometryFlag::eDOUBLE_SIDED), *material, true);

	StaticRigidBody* staticBody = SettingStaticBody(shape, info.colliderInfo, colliderType, m_collisionMatrix);
	staticBody->SetOffsetRotation(DirectX::SimpleMath::Matrix::CreateRotationZ(180.0f / 180.0f * 3.14f));
	staticBody->SetOffsetTranslation(DirectX::SimpleMath::Matrix::CreateTranslation(DirectX::SimpleMath::Vector3(info.rowScale * info.numRows * 0.5f, 0.0f, -info.colScale * info.numCols * 0.5f)));

	shape->release();

}

void PhysicX::CreateDynamicBody(const BoxColliderInfo & info, const EColliderType & colliderType,  bool isKinematic)
{
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxBoxGeometry(info.boxExtent.x, info.boxExtent.y, info.boxExtent.z), *material, true);

	DynamicRigidBody* dynamicBody = SettingDynamicBody(shape, info.colliderInfo, colliderType, m_collisionMatrix, isKinematic);

	shape->release();

	if (dynamicBody != nullptr)
	{
		dynamicBody->SetExtent(info.boxExtent.x, info.boxExtent.y, info.boxExtent.z);
	}
}

void PhysicX::CreateDynamicBody(const SphereColliderInfo & info, const EColliderType & colliderType,  bool isKinematic)
{
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxSphereGeometry(info.radius), *material, true);
	DynamicRigidBody* dynamicBody = SettingDynamicBody(shape, info.colliderInfo, colliderType, m_collisionMatrix, isKinematic);
	shape->release();
	if (dynamicBody != nullptr)
	{
		dynamicBody->SetRadius(info.radius);
	}
}

void PhysicX::CreateDynamicBody(const CapsuleColliderInfo & info, const EColliderType & colliderType,  bool isKinematic)
{
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxCapsuleGeometry(info.radius, info.height), *material, true);
	DynamicRigidBody* dynamicBody = SettingDynamicBody(shape, info.colliderInfo, colliderType, m_collisionMatrix, isKinematic);
	shape->release();
	if (dynamicBody != nullptr)
	{
		dynamicBody->SetRadius(info.radius);
		dynamicBody->SetHalfHeight(info.height);
	}
}

void PhysicX::CreateDynamicBody(const ConvexMeshColliderInfo & info, const EColliderType & colliderType,  bool isKinematic)
{
	/*static int number = 0;
	number++;*/

	ConvexMeshResource* convexMesh = new ConvexMeshResource(m_physics, info.vertices, info.vertexSize, info.convexPolygonLimit);
	physx::PxConvexMesh* pxConvexMesh = convexMesh->GetConvexMesh();

	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxConvexMeshGeometry(pxConvexMesh), *material, true);

	DynamicRigidBody* dynamicBody = SettingDynamicBody(shape, info.colliderInfo, colliderType, m_collisionMatrix, isKinematic);
	shape->release();
}

void PhysicX::CreateDynamicBody(const TriangleMeshColliderInfo & info, const EColliderType & colliderType, bool isKinematic)
{
	TriangleMeshResource* triangleMesh = new TriangleMeshResource(m_physics, info.vertices, info.vertexSize, info.indices, info.indexSize);
	physx::PxTriangleMesh* pxTriangleMesh = triangleMesh->GetTriangleMesh();

	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxTriangleMeshGeometry(pxTriangleMesh), *material, true);

	shape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE,true);
	DynamicRigidBody* dynamicBody = SettingDynamicBody(shape, info.colliderInfo, colliderType, m_collisionMatrix, isKinematic);
	shape->release();
}

void PhysicX::CreateDynamicBody(const HeightFieldColliderInfo & info, const EColliderType & colliderType,  bool isKinematic)
{
	HeightFieldResource* heightField = new HeightFieldResource(m_physics, info.heightMep, info.numCols, info.numRows);
	physx::PxHeightField* pxHeightField = heightField->GetHeightField();
	physx::PxMaterial* material = m_physics->createMaterial(info.colliderInfo.staticFriction, info.colliderInfo.dynamicFriction, info.colliderInfo.restitution);
	physx::PxShape* shape = m_physics->createShape(PxHeightFieldGeometry(pxHeightField), *material, true);
	DynamicRigidBody* dynamicBody = SettingDynamicBody(shape, info.colliderInfo, colliderType, m_collisionMatrix, isKinematic);
	shape->release();
}

StaticRigidBody* PhysicX::SettingStaticBody(physx::PxShape* shape, const ColliderInfo& colInfo, const EColliderType& collideType, unsigned int* collisionMatrix)
{
	//filterData
	physx::PxFilterData filterData;
	filterData.word0 = colInfo.layerNumber;
	filterData.word1 = collisionMatrix[colInfo.layerNumber];
	//filterData.word1 = 0xFFFFFFFF;
	shape->setSimulationFilterData(filterData);
	shape->setQueryFilterData(filterData);
	//collisionData
	StaticRigidBody* staticBody = new StaticRigidBody(collideType,colInfo.id,colInfo.layerNumber);
	CollisionData* collisionData = new CollisionData();
	if (collisionData) // 추가: collisionData 유효성 확인
	{
		//스테틱 바디 초기화-->rigidbody 생성 및 shape attach , collider 정보 등록, collisionData 등록
		if (!staticBody->Initialize(colInfo,shape,m_physics,collisionData))
		{
			Debug->LogError("PhysicX::SettingStaticBody() : staticBody Initialize failed id :" + std::to_string(colInfo.id));
			delete collisionData; // 실패 시 메모리 해제
			return nullptr;
		}

		//충돌데이터 등록, 물리 바디 등록
		m_collisionDataContainer.insert(std::make_pair(colInfo.id, collisionData));
		m_rigidBodyContainer.insert(std::make_pair(staticBody->GetID(), staticBody));

		m_updateActors.push_back(staticBody);
	}
	else
	{
		Debug->LogError("PhysicX::SettingStaticBody() : Failed to allocate CollisionData for id " + std::to_string(colInfo.id));
		delete staticBody; // staticBody도 해제
		return nullptr;
	}
	return staticBody;
}

DynamicRigidBody* PhysicX::SettingDynamicBody(physx::PxShape* shape, const ColliderInfo& colInfo, const EColliderType& collideType, unsigned int* collisionMatrix, bool isKinematic)
{
	//필터데이터
	physx::PxFilterData filterData;
	filterData.word0 = colInfo.layerNumber;
	filterData.word1 = collisionMatrix[colInfo.layerNumber];
	//filterData.word1 = 0xFFFFFFFF;
	shape->setSimulationFilterData(filterData);
	shape->setQueryFilterData(filterData);
	//collisionData
	DynamicRigidBody* dynamicBody = new DynamicRigidBody(collideType, colInfo.id, colInfo.layerNumber);
	CollisionData* collisionData = new CollisionData();
	if (collisionData) // 추가: collisionData 유효성 확인
	{
		//다이나믹 바디 초기화-->rigidbody 생성 및 shape attach , collider 정보 등록, collisionData 등록
		if (!dynamicBody->Initialize(colInfo, shape, m_physics, collisionData,isKinematic))
		{
			Debug->LogError("PhysicX::SettingDynamicBody() : dynamicBody Initialize failed id :" + std::to_string(colInfo.id));
			delete collisionData; // 실패 시 메모리 해제
			return nullptr;
		}

		//충돌데이터 등록, 물리 바디 등록
		m_collisionDataContainer.insert(std::make_pair(colInfo.id, collisionData));
		m_rigidBodyContainer.insert(std::make_pair(dynamicBody->GetID(), dynamicBody));

		m_updateActors.push_back(dynamicBody);
	}
	else
	{
		Debug->LogError("PhysicX::SettingDynamicBody() : Failed to allocate CollisionData for id " + std::to_string(colInfo.id));
		delete dynamicBody; // dynamicBody도 해제
		return nullptr;
	}
	return dynamicBody;
}

bool TransformDifferent(const physx::PxTransform& first, physx::PxTransform& second, float positionTolerance, float rotationTolerance) {

	//compare position
	if (!(first.p).isFinite() || !(second.p).isFinite() || (first.p - second.p).magnitude() > positionTolerance) {
		if (!(first.q).isFinite() || !(second.q).isFinite() || (physx::PxAbs(first.q.w - second.q.w) > rotationTolerance) || 
			(physx::PxAbs(first.q.x - second.q.x)>rotationTolerance)||
			(physx::PxAbs(first.q.y - second.q.y) > rotationTolerance) || 
			(physx::PxAbs(first.q.z - second.q.z) > rotationTolerance))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}


RigidBodyGetSetData PhysicX::GetRigidBodyData(unsigned int id)
{
	RigidBodyGetSetData rigidBodyData;

	auto it = m_rigidBodyContainer.find(id);
	if (it == m_rigidBodyContainer.end())
	{
		//Debug->LogError("PhysicX::GetRigidBodyData: RigidBody with ID " + std::to_string(id) + " not found in container. This might indicate a lifecycle management issue.");
		return rigidBodyData;
	}
	auto body = it->second;
	
	//dynamicBody 인 경우
	DynamicRigidBody* dynamicBody = dynamic_cast<DynamicRigidBody*>(body);
	if (dynamicBody)
	{
		physx::PxRigidDynamic* pxBody = dynamicBody->GetRigidDynamic();
		DirectX::SimpleMath::Matrix dxMatrix;
		CopyMatrixPxToDx(pxBody->getGlobalPose(), dxMatrix);
		rigidBodyData.transform = DirectX::SimpleMath::Matrix::CreateScale(dynamicBody->GetScale()) * dxMatrix * dynamicBody->GetOffsetTranslation();
		CopyVectorPxToDx(pxBody->getLinearVelocity(), rigidBodyData.linearVelocity);
		CopyVectorPxToDx(pxBody->getAngularVelocity(), rigidBodyData.angularVelocity);

		physx::PxRigidDynamicLockFlags flags = pxBody->getRigidDynamicLockFlags();

		rigidBodyData.isLockLinearX = (flags & physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X);
		rigidBodyData.isLockLinearY = (flags & physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y);
		rigidBodyData.isLockLinearZ = (flags & physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z);
		rigidBodyData.isLockAngularX = (flags & physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X);
		rigidBodyData.isLockAngularY = (flags & physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y);
		rigidBodyData.isLockAngularZ = (flags & physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z);

		rigidBodyData.isKinematic = pxBody->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC;
		rigidBodyData.isDisabled = pxBody->getActorFlags() & physx::PxActorFlag::eDISABLE_SIMULATION;
		rigidBodyData.useGravity = !(pxBody->getActorFlags() & physx::PxActorFlag::eDISABLE_GRAVITY);

		const physx::PxU32 numShapes = pxBody->getNbShapes();
		if (numShapes > 0)
		{
			std::vector<physx::PxShape*> shapes(numShapes);
			pxBody->getShapes(shapes.data(), numShapes);
			if (shapes[0])
			{
				rigidBodyData.m_EColliderType = (shapes[0]->getFlags() & physx::PxShapeFlag::eTRIGGER_SHAPE) ? EColliderType::TRIGGER : EColliderType::COLLISION;
				rigidBodyData.isColliderEnabled = (shapes[0]->getFlags() & physx::PxShapeFlag::eSIMULATION_SHAPE) || (shapes[0]->getFlags() & physx::PxShapeFlag::eTRIGGER_SHAPE);
			}
		}
	}
	
	//staticBody 인 경우
	StaticRigidBody* staticBody = dynamic_cast<StaticRigidBody*>(body);
	if (staticBody)
	{
		physx::PxRigidStatic* pxBody = staticBody->GetRigidStatic();
		DirectX::SimpleMath::Matrix dxMatrix;
		CopyMatrixPxToDx(pxBody->getGlobalPose(), dxMatrix);
		rigidBodyData.transform = DirectX::SimpleMath::Matrix::CreateScale(staticBody->GetScale()) * staticBody->GetOffsetRotation() *dxMatrix * staticBody->GetOffsetTranslation();
	}

	return rigidBodyData;
}

RigidBody* PhysicX::GetRigidBody(const unsigned int& id)
{
	//등록되어 있는지 검색
	if (m_rigidBodyContainer.find(id) == m_rigidBodyContainer.end())
	{
		Debug->LogWarning("GetRigidBody id :" + std::to_string(id) + " Get Failed");
		return nullptr;
	}
	RigidBody* body = m_rigidBodyContainer[id];
	if (body)
	{
		return body;
	}
	else
	{
		Debug->LogWarning("GetRigidBody id :" + std::to_string(id) + " Get Failed");
		return nullptr;
	}
}

void PhysicX::SetRigidBodyData(const unsigned int& id, RigidBodyGetSetData& rigidBodyData)
{
	//데이터를 설정할 물리 바디 등록되어 있는지 검색
	if (m_rigidBodyContainer.find(id) == m_rigidBodyContainer.end())
	{
		std::cout << "PhysicX::SetRigidBodyData: RigidBody with ID " << id << " not found in container." << std::endl;
		return;
	}
	auto body = m_rigidBodyContainer.find(id)->second;

	//dynamicBody 인 경우
	DynamicRigidBody* dynamicBody = dynamic_cast<DynamicRigidBody*>(body);
	if (dynamicBody)
	{
		physx::PxRigidDynamic* pxBody = dynamicBody->GetRigidDynamic();
		if (!pxBody) return;

		// Actor 및 Body 플래그 설정
		pxBody->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !rigidBodyData.useGravity);
		pxBody->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, rigidBodyData.isDisabled);
		pxBody->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, rigidBodyData.isKinematic);

		// Shape 플래그 설정
		const PxU32 numShapes = pxBody->getNbShapes();
		std::vector<physx::PxShape*> shapes(numShapes);
		pxBody->getShapes(shapes.data(), numShapes);
		for (physx::PxShape* shape : shapes)
		{
			if (rigidBodyData.isColliderEnabled)
			{
				bool isTrigger = (rigidBodyData.m_EColliderType == EColliderType::TRIGGER);
				shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
				shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
			}
			else
			{
				shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
				shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
			}
		}

		// 속도 및 기타 데이터 설정
		if (!rigidBodyData.isKinematic)
		{
		physx::PxVec3 pxLinearVelocity;
		physx::PxVec3 pxAngularVelocity;
		CopyVectorDxToPx(rigidBodyData.linearVelocity, pxLinearVelocity);
		CopyVectorDxToPx(rigidBodyData.angularVelocity, pxAngularVelocity);
			pxBody->setLinearVelocity(pxLinearVelocity);
			pxBody->setAngularVelocity(pxAngularVelocity);
		}

		if (rigidBodyData.forceMode != 4) {
			PxVec3 velocity;
			CopyVectorDxToPx(rigidBodyData.velocity, velocity);
			pxBody->addForce(velocity, static_cast<physx::PxForceMode::Enum>(rigidBodyData.forceMode));
			rigidBodyData.forceMode = 4;
		}

		pxBody->setMaxLinearVelocity(rigidBodyData.maxLinearVelocity);
		pxBody->setMaxAngularVelocity(rigidBodyData.maxAngularVelocity);
		pxBody->setMaxContactImpulse(rigidBodyData.maxContactImpulse);
		pxBody->setMaxDepenetrationVelocity(rigidBodyData.maxDepenetrationVelocity);

		pxBody->setAngularDamping(rigidBodyData.AngularDamping);
		pxBody->setLinearDamping(rigidBodyData.LinearDamping);
		pxBody->setMass(rigidBodyData.mass);
		pxBody->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, rigidBodyData.isLockAngularX);
		pxBody->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, rigidBodyData.isLockAngularY);
		pxBody->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, rigidBodyData.isLockAngularZ);
		pxBody->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X, rigidBodyData.isLockLinearX);
		pxBody->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y, rigidBodyData.isLockLinearY);
		pxBody->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z, rigidBodyData.isLockLinearZ);

		DirectX::SimpleMath::Matrix dxMatrix = rigidBodyData.transform;
		physx::PxTransform pxTransform;
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector3 scale = { 1.0f, 1.0f, 1.0f };
		DirectX::SimpleMath::Quaternion rotation;
		dxMatrix.Decompose(scale, rotation, position);
		dxMatrix = DirectX::SimpleMath::Matrix::CreateScale(1.0f) * DirectX::SimpleMath::Matrix::CreateFromQuaternion(rotation) * DirectX::SimpleMath::Matrix::CreateTranslation(position);

		CopyMatrixDxToPx(dxMatrix, pxTransform);
		pxBody->setGlobalPose(pxTransform);
		dynamicBody->ChangeLayerNumber(rigidBodyData.LayerNumber, m_collisionMatrix);

		if (scale.x>0.0f&&scale.y>0.0f&&scale.z>0.0f)
		{
			dynamicBody->SetConvertScale(scale, m_physics, m_collisionMatrix);
		}
		else {
			Debug->LogError("PhysicX::SetRigidBodyData() : scale is 0.0f id :" + std::to_string(id));
		}
	}
	//staticBody 인 경우
	StaticRigidBody* staticBody = dynamic_cast<StaticRigidBody*>(body);
	if (staticBody)
	{
		physx::PxRigidStatic* pxBody = staticBody->GetRigidStatic();
		DirectX::SimpleMath::Matrix dxMatrix = rigidBodyData.transform;
		physx::PxTransform pxPrevTransform = pxBody->getGlobalPose();
		physx::PxTransform pxCurrTransform;
		
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector3 scale = { 1.0f, 1.0f, 1.0f };
		DirectX::SimpleMath::Quaternion rotation;

		dxMatrix.Decompose(scale, rotation, position);
		dxMatrix = DirectX::SimpleMath::Matrix::CreateScale(1.0f) * 
			DirectX::SimpleMath::Matrix::CreateFromQuaternion(rotation) * staticBody->GetOffsetRotation().Invert() *
			DirectX::SimpleMath::Matrix::CreateTranslation(position) * staticBody->GetOffsetTranslation().Invert();

		CopyMatrixDxToPx(dxMatrix, pxCurrTransform);
		pxBody->setGlobalPose(pxCurrTransform);
	}
}

void PhysicX::RemoveRigidBody(const unsigned int& id, physx::PxScene* scene, std::vector<physx::PxActor*>& removeActorList)
{
	//등록되어 있는지 검색
	if (m_rigidBodyContainer.find(id)==m_rigidBodyContainer.end())
	{
		Debug->LogWarning("RemoveRigidBody id :" + std::to_string(id) + " Remove Failed");
		return;
	}

	RigidBody* body = m_rigidBodyContainer[id];

	//씬에서 제거할 바디인 경우 (PxActor를 직접 씬에서 제거)
	if (body)
	{
		DynamicRigidBody* dynamicBody = dynamic_cast<DynamicRigidBody*>(body);
		if (dynamicBody) {
			if (dynamicBody->GetRigidDynamic()->getScene() == scene)
			{
				removeActorList.push_back(dynamicBody->GetRigidDynamic());
				m_rigidBodyContainer.erase(id);
			}
		}
		StaticRigidBody* staticBody = dynamic_cast<StaticRigidBody*>(body);
		if (staticBody) {
			if (staticBody->GetRigidStatic()->getScene() == scene)
			{
				removeActorList.push_back(staticBody->GetRigidStatic());
				m_rigidBodyContainer.erase(id);
			}
		}
	}

	//업데이트 리스트에서 해당 바디를 제거
	auto bodyIter = m_updateActors.begin();
	for (bodyIter; bodyIter!= m_updateActors.end(); bodyIter++)
	{
		if ((*bodyIter)->GetID()==id)
		{
			m_updateActors.erase(bodyIter);
			break;
		}
	}
	
}


void PhysicX::RemoveAllRigidBody(physx::PxScene* scene, std::vector<physx::PxActor*>& removeActorList)
{
	//씬에서 제거할 바디인 경우 (컨테이너에 등록된 모든 바디를 씬에서 제거)
	for (const auto& body : m_rigidBodyContainer) {
		DynamicRigidBody* dynamicBody = dynamic_cast<DynamicRigidBody*>(body.second);
		if (dynamicBody) {
			
			if (dynamicBody->GetRigidDynamic()->getScene() == scene)
			{
				removeActorList.push_back(dynamicBody->GetRigidDynamic());
				continue;
			}
		}
		StaticRigidBody* staticBody = dynamic_cast<StaticRigidBody*>(body.second);
		if (staticBody) {
			if (staticBody->GetRigidStatic()->getScene() == scene)
			{
				removeActorList.push_back(staticBody->GetRigidStatic());
				continue;
			}
		}
	}
	//컨테이너의 모든 바디 제거 
	m_rigidBodyContainer.clear();

	//업데이트 리스트의 모든 바디 제거
	for (const auto& body : m_updateActors) {
		DynamicRigidBody* dynamicBody = dynamic_cast<DynamicRigidBody*>(body);
		if (dynamicBody) {
			if (dynamicBody->GetRigidDynamic()->getScene() == scene)
			{
				removeActorList.push_back(dynamicBody->GetRigidDynamic());
				continue;
			}
		}
		StaticRigidBody* staticBody = dynamic_cast<StaticRigidBody*>(body);
		if (staticBody) {
			if (staticBody->GetRigidStatic()->getScene() == scene)
			{
				removeActorList.push_back(staticBody->GetRigidStatic());
				continue;
			}
		}
	}
	//등록된 모든 바디 제거
	m_updateActors.clear();
}

//==============================================
//캐릭터 컨트롤러

void PhysicX::CreateCCT(const CharacterControllerInfo& controllerInfo, const CharacterMovementInfo& movementInfo)
{
	m_waittingCCTList.push_back(std::make_pair(controllerInfo, movementInfo));
}

void PhysicX::RemoveCCT(const unsigned int& id)
{
	auto controllerIter = m_characterControllerContainer.find(id);
	if (controllerIter!= m_characterControllerContainer.end()) {
		m_characterControllerContainer.erase(controllerIter);
	}
	
}

void PhysicX::RemoveAllCCT()
{
	m_characterControllerManager->purgeControllers();
	m_characterControllerContainer.clear();
	m_updateCCTList.clear();
	m_waittingCCTList.clear();
}

void PhysicX::AddInputMove(const CharactorControllerInputInfo& info)
{
	if (m_characterControllerContainer.find(info.id) == m_characterControllerContainer.end())
	{
		return;
	}

	CharacterController* controller = m_characterControllerContainer[info.id];
	controller->AddMovementInput(info.input, info.isDynamic);
}

CharacterControllerGetSetData PhysicX::GetCCTData(const unsigned int& id)
{
	CharacterControllerGetSetData data;
	if (m_characterControllerContainer.find(id) != m_characterControllerContainer.end())
	{
		auto& controller = m_characterControllerContainer[id];
		physx::PxController* pxController = controller->GetController();

		controller->GetPosition(data.position);
	}
	else {
		for (auto& [controller, movement] : m_updateCCTList) {
			if (controller.id == id)
			{
				data.position = controller.position;
			}
		}

		for (auto& [controller, movement]:m_waittingCCTList)
		{
			if (controller.id == id)
			{
				data.position = controller.position;
			}
		}

	}	
	return data;
}

CharacterMovementGetSetData PhysicX::GetMovementData(const unsigned int& id)
{
	CharacterMovementGetSetData data;
	if (m_characterControllerContainer.find(id) != m_characterControllerContainer.end())
	{
		auto& controller = m_characterControllerContainer[id];
		CharacterMovement* movement = controller->GetCharacterMovement();
		
		data.velocity = movement->GetOutVector();
		data.isFall = movement->GetIsFall();
		data.maxSpeed = movement->GetMaxSpeed();
		
	}

	return data;
}

void PhysicX::SetCCTData(const unsigned int& id,const CharacterControllerGetSetData& controllerData)
{
	if (m_characterControllerContainer.find(id) != m_characterControllerContainer.end())
	{
		auto& controller = m_characterControllerContainer[id];
		physx::PxController* pxController = controller->GetController();

		m_characterControllerContainer[id]->ChangeLayerNumber(controllerData.LayerNumber, m_collisionMatrix);
		controller->SetPosition(controllerData.position);
	}
	else {
		for (auto& [controller, movement] : m_updateCCTList) {
			if (controller.id == id)
			{
				controller.position = controllerData.position;
			}
		}
		for (auto& [controller, movement] : m_waittingCCTList)
		{
			if (controller.id == id)
			{
				controller.position = controllerData.position;
			}
		}
	}

}

void PhysicX::SetMovementData(const unsigned int& id,const CharacterMovementGetSetData& movementData)
{
	if (m_characterControllerContainer.find(id) != m_characterControllerContainer.end()) {
		auto& controller = m_characterControllerContainer[id];
		CharacterMovement* movement = controller->GetCharacterMovement();

		controller->SetMoveRestrct(movementData.restrictDirection);
		movement->SetIsFall(movementData.isFall);
		movement->SetVelocity(movementData.velocity);
		movement->SetMaxSpeed(movementData.maxSpeed);
		movement->SetAcceleration(movementData.acceleration);
	}
}

//===================================================
//캐릭터 물리 제어 -> 래그돌

void PhysicX::CreateCharacterInfo(const ArticulationInfo& info)
{
	if (m_ragdollContainer.find(info.id) != m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::CreateCharacterInfo() : id is already exist id :" + std::to_string(info.id));
		return;
	}

	RagdollPhysics* ragdoll = new RagdollPhysics();
	CollisionData* collisionData = new CollisionData();

	collisionData->thisId = info.id;
	collisionData->thisLayerNumber = info.layerNumber;

	m_collisionDataContainer.insert(std::make_pair(info.id, collisionData));

	ragdoll->Initialize(info, m_physics, collisionData);
	m_ragdollContainer.insert(std::make_pair(info.id, ragdoll));

}

void PhysicX::RemoveCharacterInfo(const unsigned int& id)
{
	if (m_ragdollContainer.find(id) == m_ragdollContainer.end())
	{
		Debug->LogWarning("remove ragdoll data fail container have not data id:"+std::to_string(id));
		return;
	}

	auto articulationIter = m_ragdollContainer.find(id);
	auto pxArticulation = articulationIter->second->GetPxArticulation();

	if (articulationIter->second->GetIsRagdoll() == true)
	{
		m_scene->removeArticulation(*pxArticulation);
		if (pxArticulation)
		{
			pxArticulation->release();
			pxArticulation = nullptr;
		}
	}

	m_ragdollContainer.erase(articulationIter);
}

void PhysicX::RemoveAllCharacterInfo()
{
	for (auto& [id, ragdoll] : m_ragdollContainer)
	{
		auto pxArticulation = ragdoll->GetPxArticulation();
		if (ragdoll->GetIsRagdoll() == true)
		{
			m_scene->removeArticulation(*pxArticulation);
			if (pxArticulation) {
				pxArticulation->release();
				pxArticulation = nullptr;
			}

			Debug->Log("PhysicX::RemoveAllCharacterInfo() : remove articulation id :" + std::to_string(id));
		}
	}
	m_ragdollContainer.clear();
}

void PhysicX::AddArticulationLink(unsigned int id, LinkInfo& info, const DirectX::SimpleMath::Vector3& extent)
{
	if (m_ragdollContainer.find(id) == m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::AddArticulationLink() : id is not exist id :" + std::to_string(id));
		return;
	}

	RagdollPhysics* ragdoll = m_ragdollContainer[id];
	ragdoll->AddArticulationLink(info,m_collisionMatrix, extent);
}

void PhysicX::AddArticulationLink(unsigned int id, LinkInfo& info, const float& radius)
{
	if (m_ragdollContainer.find(id) == m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::AddArticulationLink() : id is not exist id :" + std::to_string(id));
		return;
	}
	RagdollPhysics* ragdoll = m_ragdollContainer[id];
	ragdoll->AddArticulationLink(info, m_collisionMatrix, radius);
}

void PhysicX::AddArticulationLink(unsigned int id, LinkInfo& info, const float& halfHeight, const float& radius)
{
	if (m_ragdollContainer.find(id) == m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::AddArticulationLink() : id is not exist id :" + std::to_string(id));
		return;
	}
	RagdollPhysics* ragdoll = m_ragdollContainer[id];
	ragdoll->AddArticulationLink(info, m_collisionMatrix, halfHeight, radius);
}

void PhysicX::AddArticulationLink(unsigned int id, LinkInfo& info)
{
	if (m_ragdollContainer.find(id) == m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::AddArticulationLink() : id is not exist id :" + std::to_string(id));
		return;
	}
	RagdollPhysics* ragdoll = m_ragdollContainer[id];
	ragdoll->AddArticulationLink(info, m_collisionMatrix);
}

ArticulationGetData PhysicX::GetArticulationData(const unsigned int& id)
{
	ArticulationGetData data;
	auto articulationIter = m_ragdollContainer.find(id);
	if (articulationIter == m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::GetArticulationData() : id is not exist id :" + std::to_string(id));
		return data;
	}

	auto ragdoll = articulationIter->second;

	physx::PxTransform pxTransform = ragdoll->GetPxArticulation()->getRootGlobalPose();
	DirectX::SimpleMath::Matrix dxMatrix;
	CopyMatrixPxToDx(pxTransform, dxMatrix);

	data.WorldTransform = dxMatrix;
	data.bIsRagdollSimulation = ragdoll->GetIsRagdoll();

	for (auto& [name, link] : ragdoll->GetLinkContainer()) {
		ArticulationLinkGetData linkData;
		linkData.jointLocalTransform = link->GetRagdollJoint()->GetSimulLocalTransform();
		linkData.name = link->GetName();

		data.linkData.push_back(linkData);
	}

	return data;
}

void PhysicX::SetArticulationData(const unsigned int& id, const ArticulationSetData& articulationData)
{
	auto articulationIter = m_ragdollContainer.find(id);
	if (articulationIter == m_ragdollContainer.end())
	{
		Debug->LogError("PhysicX::SetArticulationData() : id is not exist id :" + std::to_string(id));
		return;
	}

	auto ragdoll = articulationIter->second;
	
	ragdoll->ChangeLayerNumber(articulationData.LayerNumber, m_collisionMatrix);

	if (articulationData.bIsRagdollSimulation != ragdoll->GetIsRagdoll()) {
		ragdoll->SetIsRagdoll(articulationData.bIsRagdollSimulation);
		ragdoll->SetWorldTransform(articulationData.WorldTransform);

		//래그돌 시뮬레이션을 해야하는 경우
		if (articulationData.bIsRagdollSimulation)
		{
			for (const auto& linkData : articulationData.linkData)
			{
				ragdoll->SetLinkTransformUpdate(linkData.name, linkData.boneWorldTransform);
			}
			auto pxArticulation = ragdoll->GetPxArticulation();
			bool isCheck = m_scene->addArticulation(*pxArticulation);

			if (isCheck == false)
			{
				Debug->LogError("PhysicX::SetArticulationData() : add articulation failed id :" + std::to_string(id));
			}
		}
	}

}

unsigned int PhysicX::GetArticulationCount()
{
	return m_scene->getNbArticulations();
}

void PhysicX::ConnectPVD()
{
    if (pvd != nullptr) {
        if (pvd->isConnected())
            pvd->disconnect();
    }
    pvd = PxCreatePvd(*m_foundation);
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    auto isconnected = pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
    std::cout << "pvd connected : " << isconnected << std::endl;
}

void PhysicX::CollisionDataUpdate()
{
	//컨테이너에서 제거
	for (auto& removeId : m_removeCollisionIds) {
		auto iter = m_collisionDataContainer.find(removeId);
		if (iter != m_collisionDataContainer.end())
		{
			m_collisionDataContainer.erase(iter);
		}
	}
	m_removeCollisionIds.clear();

	//충돌 데이터 리스트에서 제거
	for (auto& collisionIter : m_collisionDataContainer) {
		if (collisionIter.second->isDead == true) {
			m_removeCollisionIds.push_back(collisionIter.first);
		}
	}
}

CollisionData* PhysicX::FindCollisionData(const unsigned int& id)
{
	auto iter = m_collisionDataContainer.find(id);
	if (iter != m_collisionDataContainer.end())
	{
		return iter->second;
	}
	else
	{
		Debug->LogError("PhysicX::FindCollisionData() : id is not exist id :" + std::to_string(id));
		return nullptr;
	}
}

void PhysicX::CreateCollisionData(const unsigned int& id, CollisionData* data)
{
	m_collisionDataContainer.insert(std::make_pair(id, data));
}

void PhysicX::RemoveCollisionData(const unsigned int& id)
{
	//충돌 데이터가 등록되어 있는지 검색
	for (unsigned int& removeId : m_removeCollisionIds) {
		if (removeId == id) {
			return;
		}
	}

	m_removeCollisionIds.push_back(id);
}

void PhysicX::extractDebugConvexMesh(physx::PxRigidActor* body, physx::PxShape* shape, std::vector<std::vector<DirectX::SimpleMath::Vector3>>& debuPolygon)
{
	const physx::PxConvexMeshGeometry& convexMeshGeom = static_cast<const physx::PxConvexMeshGeometry&>(shape->getGeometry());

	DirectX::SimpleMath::Matrix dxMatrix;
	DirectX::SimpleMath::Matrix matrix;

	std::vector<std::vector<DirectX::SimpleMath::Vector3>> polygon;

	//convexMesh의 모든 인덱스 데이터 가져오기
	const physx::PxVec3* convexMeshVertices = convexMeshGeom.convexMesh->getVertices();
	const physx::PxU32 vertexCount = convexMeshGeom.convexMesh->getNbVertices();

	const physx::PxU8* convexMeshIndices = convexMeshGeom.convexMesh->getIndexBuffer();
	const physx::PxU32 polygonCount = convexMeshGeom.convexMesh->getNbPolygons();

	//폴리곤 개수만큼 polygon 벡터 생성
	polygon.reserve(polygonCount);

	for (PxU32 i = 0; i < polygonCount-1; i++)
	{
		physx::PxHullPolygon pxPolygon;
		convexMeshGeom.convexMesh->getPolygonData(i, pxPolygon);
		int totalVertexCount = pxPolygon.mNbVerts;
		int startIndex = pxPolygon.mIndexBase;

		std::vector<DirectX::SimpleMath::Vector3> vertices;
		vertices.reserve(totalVertexCount);

		for (int j = 0; j < totalVertexCount; j++)
		{
			DirectX::SimpleMath::Vector3 vertex;
			vertex.x = convexMeshVertices[convexMeshIndices[startIndex + j]].x;
			vertex.y = convexMeshVertices[convexMeshIndices[startIndex + j]].y;
			vertex.z = convexMeshVertices[convexMeshIndices[startIndex + j]].z;

			vertex = DirectX::SimpleMath::Vector3::Transform(vertex, dxMatrix);

			vertices.push_back(vertex);
		}

		debuPolygon.push_back(vertices);
	}

}

void PhysicX::DrawPVDLine(DirectX::SimpleMath::Vector3 ori, DirectX::SimpleMath::Vector3 end)
{
	PxPvdSceneClient* pvdClient = m_scene->getScenePvdClient();
	if (pvdClient && pvd->isConnected())
	{
		// 색상 설정
		const PxU32 green = PxDebugColor::eARGB_GREEN;
		const PxU32 red = PxDebugColor::eARGB_RED;

		// 충돌 여부에 따라 색상 변경
		PxU32 color = green;

		PxVec3 origin;
		PxVec3 endpoint;

		CopyVectorDxToPx(ori, origin);
		CopyVectorDxToPx(end, endpoint);

		// 선 하나를 그리는 것이므로 lineCount = 1
		PxDebugLine line(origin, endpoint, color);

		pvdClient->drawLines(&line, 1);
	}
}

void PhysicX::ShowNotRelease()
{
    std::cout << "physx count " << std::endl;
   std::cout << m_physics->getNbBVHs()<< std::endl;
   std::cout << m_physics->getNbBVHs()<< std::endl;
   std::cout << m_physics->getNbConvexMeshes()<< std::endl;
   std::cout << m_physics->getNbDeformableSurfaceMaterials()<< std::endl;
   std::cout << m_physics->getNbDeformableVolumeMaterials()<< std::endl;
   std::cout << m_physics->getNbFEMSoftBodyMaterials()<< std::endl;
   std::cout << m_physics->getNbHeightFields()<< std::endl;
   std::cout << m_physics->getNbMaterials()<< std::endl; // 1
   std::cout << m_physics->getNbPBDMaterials()<< std::endl;
   std::cout << m_physics->getNbScenes()<< std::endl;  // 1
   std::cout << m_physics->getNbShapes()<< std::endl;  // 3
   std::cout << m_physics->getNbTetrahedronMeshes()<< std::endl;
   std::cout << m_physics->getNbTriangleMeshes() << std::endl;
}

bool PhysicX::IsKinematic(unsigned int id) const
{
	auto it = m_rigidBodyContainer.find(id);
	if (it != m_rigidBodyContainer.end())
	{
		if (auto* dynamicBody = dynamic_cast<DynamicRigidBody*>(it->second))
		{
			if (dynamicBody->GetRigidDynamic()) // 추가: GetRigidDynamic() 유효성 확인
			{
				return dynamicBody->GetRigidDynamic()->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC;
			}
		}
	}
	return false;
}

bool PhysicX::IsTrigger(unsigned int id) const
{
	auto it = m_rigidBodyContainer.find(id);
	if (it != m_rigidBodyContainer.end())
	{
		physx::PxRigidActor* actor = nullptr;
		if (auto* dynamicBody = dynamic_cast<DynamicRigidBody*>(it->second)) actor = dynamicBody->GetRigidDynamic();
		else if (auto* staticBody = dynamic_cast<StaticRigidBody*>(it->second)) actor = staticBody->GetRigidStatic();

		if (actor)
		{
			const physx::PxU32 numShapes = actor->getNbShapes();
			if (numShapes > 0)
			{
				std::vector<physx::PxShape*> shapes(numShapes);
				actor->getShapes(shapes.data(), numShapes);
				if (shapes[0]) // 추가: shapes[0] 유효성 확인
				{
					return shapes[0]->getFlags() & physx::PxShapeFlag::eTRIGGER_SHAPE;
				}
			}
		}
	}
	return false;
}

bool PhysicX::IsColliderEnabled(unsigned int id) const
{
	auto it = m_rigidBodyContainer.find(id);
	if (it != m_rigidBodyContainer.end())
	{
		physx::PxRigidActor* actor = nullptr;
		if (auto* dynamicBody = dynamic_cast<DynamicRigidBody*>(it->second)) actor = dynamicBody->GetRigidDynamic();
		else if (auto* staticBody = dynamic_cast<StaticRigidBody*>(it->second)) actor = staticBody->GetRigidStatic();

		if (actor)
		{
			const physx::PxU32 numShapes = actor->getNbShapes();
			if (numShapes > 0)
			{
				std::vector<physx::PxShape*> shapes(numShapes);
				actor->getShapes(shapes.data(), numShapes);
				if (shapes[0]) // 추가: shapes[0] 유효성 확인
				{
					// eSIMULATION_SHAPE 또는 eTRIGGER_SHAPE가 설정되어 있으면 활성화된 상태로 간주
					return (shapes[0]->getFlags() & physx::PxShapeFlag::eSIMULATION_SHAPE) || (shapes[0]->getFlags() & physx::PxShapeFlag::eTRIGGER_SHAPE);
				}
			}
		}
	}
	return false;
}

bool PhysicX::IsUseGravity(unsigned int id) const
{
	auto it = m_rigidBodyContainer.find(id);
	if (it != m_rigidBodyContainer.end())
	{
		if (auto* dynamicBody = dynamic_cast<DynamicRigidBody*>(it->second))
		{
			if (dynamicBody->GetRigidDynamic())
			{
				return !(dynamicBody->GetRigidDynamic()->getActorFlags() & physx::PxActorFlag::eDISABLE_GRAVITY);
			}
		}
	}
	return false; // 혹은 기본값
}
