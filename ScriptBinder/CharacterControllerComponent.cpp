#include "CharacterControllerComponent.h"

void CharacterControllerComponent::OnStart()
{
	m_transform = &(GetOwner()->m_transform);
	
	m_fBaseSpeed = m_movementInfo.maxSpeed;
	m_fBaseAcceleration = m_movementInfo.acceleration;
}

void CharacterControllerComponent::OnFixedUpdate(float fixedDeltaTime)
{
	//등록되지 않은 컨트롤러
	if (m_controllerInfo.id == 0)
	{
		return;
	}


	DirectX::SimpleMath::Vector3 input = DirectX::SimpleMath::Vector3{ 0.f, 0.f, 0.f };
	input.x = m_moveInput.x;
	input.z = m_moveInput.y;
	if(m_isKnockBack)
	{ 
		input.y = JumpPower;
	}

	//케릭터 컨트롤러
	//todo : 이동 불가한 스턴 상태 체크 필요 --> 필요시 추가

	m_bOnMove = input != DirectX::SimpleMath::Vector3{ 0.f, 0.f, 0.f };

	input.Normalize();

	CharactorControllerInputInfo inputInfo;
	inputInfo.id = m_controllerInfo.id;
	inputInfo.input = input;

	auto component = GetOwner()->GetComponent<RigidBodyComponent>();
	if (!component) return;

	inputInfo.isDynamic = component->GetBodyType() == EBodyType::DYNAMIC;
	Physics->AddInputMove(inputInfo);


	constexpr float rotationOffsetSquare = 0.5f * 0.5f;

	float inputSquare = input.LengthSquared();

	if (!m_isKnockBack) //&&&&& 넉백당하면어케할지 상의필요
	{
		if (inputSquare >= rotationOffsetSquare) {
			DirectX::SimpleMath::Vector3 flatInput = input;
			flatInput.y = 0.f; 
			flatInput.Normalize();
			//input.Normalize();
			DirectX::SimpleMath::Quaternion currentRotation = m_transform->GetWorldQuaternion();
			DirectX::SimpleMath::Quaternion targetRotation;
			if (flatInput == DirectX::SimpleMath::Vector3{ 0.f, 0.f, 1.f }) {
				//m_transform->SetRotation(DirectX::SimpleMath::Quaternion::LookRotation(flatInput, { 0.0f,-1.0f,0.0f }));
				targetRotation = DirectX::SimpleMath::Quaternion::LookRotation(flatInput, { 0.0f,-1.0f,0.0f });
			}
			else if (flatInput != DirectX::SimpleMath::Vector3{ 0.f, 0.f, 0.f }) {
				//m_transform->SetRotation(DirectX::SimpleMath::Quaternion::LookRotation(flatInput, { 0.0f,1.0f,0.0f }));
				targetRotation = DirectX::SimpleMath::Quaternion::LookRotation(flatInput, { 0.0f,1.0f,0.0f });
			}
			DirectX::SimpleMath::Quaternion newRotation = DirectX::SimpleMath::Quaternion::Slerp(currentRotation, targetRotation, m_rotationSpeed* fixedDeltaTime);

			m_transform->SetRotation(newRotation);
		}
	}

}

void CharacterControllerComponent::OnLateUpdate(float fixedDeltaTime)
{
	m_movementInfo.maxSpeed = m_fBaseSpeed * m_fFinalMultiplierSpeed;
	m_movementInfo.acceleration = m_fBaseAcceleration * m_fFinalMultiplierSpeed;

	//m_fFinalMultiplierSpeed = 1.0f; 
}

void CharacterControllerComponent::Stun(float stunTime)
{
	m_isStun = true;
	m_stunTime = stunTime;
	stunElapsedTime = 0.f;
}

void CharacterControllerComponent::SetKnockBack(float KnockBackPower, float yKnockBackPower)
{
	preRotation = m_transform->GetForward();
	PreSpeed = m_fBaseSpeed;
	m_fFinalMultiplierSpeed = KnockBackPower;
	//m_fBaseSpeed = KnockBackPower;
	JumpPower = yKnockBackPower;
	m_isKnockBack = true;
	

}



void CharacterControllerComponent::EndKnockBack()
{
	m_fBaseSpeed = PreSpeed;
	m_moveInput.y = 0;
	m_isKnockBack = false;
	if (preRotation == DirectX::SimpleMath::Vector3{ 0.f, 0.f, 1.f })
	{
		m_transform->SetRotation(DirectX::SimpleMath::Quaternion::LookRotation(preRotation, { 0.0f,-1.0f,0.0f }));
	}
	else if (preRotation != DirectX::SimpleMath::Vector3{ 0.f, 0.f, 0.f }) {
		m_transform->SetRotation(DirectX::SimpleMath::Quaternion::LookRotation(preRotation, { 0.0f,1.0f,0.0f }));
	}
	m_fFinalMultiplierSpeed = 1.0f;

}

void CharacterControllerComponent::OnTriggerEnter(ICollider* other)
{
	++m_collsionCount;
}

void CharacterControllerComponent::OnTriggerStay(ICollider* other)
{
}

void CharacterControllerComponent::OnTriggerExit(ICollider* other)
{
	if (m_collsionCount != 0) {
		--m_collsionCount;
	}
}

void CharacterControllerComponent::OnCollisionEnter(ICollider* other)
{
	++m_collsionCount;
}

void CharacterControllerComponent::OnCollisionStay(ICollider* other)
{
}

void CharacterControllerComponent::OnCollisionExit(ICollider* other)
{
	if (m_collsionCount != 0) {
		--m_collsionCount;
	}
}
