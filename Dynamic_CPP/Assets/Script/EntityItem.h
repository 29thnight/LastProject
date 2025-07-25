#pragma once
#include "Core.Minimal.h"
#include "Entity.h"

class Player;

enum EItemState
{
	NONE = 0,
	ACQUIERED,  // ������ ȹ��
	THROWN,     // ������ ����
	DESTROYED   // ������ �ı�
	// Add more item types as needed
};

class EntityItem : public Entity
{
public:
	MODULE_BEHAVIOR_BODY(EntityItem)
	virtual void Awake() override {}
	virtual void Start() override;
	virtual void FixedUpdate(float fixedTick) override {}
	virtual void OnTriggerEnter(const Collision& collision) override;
	virtual void OnTriggerStay(const Collision& collision) override {}
	virtual void OnTriggerExit(const Collision& collision) override;
	virtual void OnCollisionEnter(const Collision& collision) override;
	virtual void OnCollisionStay(const Collision& collision) override {}
	virtual void OnCollisionExit(const Collision& collision) override;
	virtual void Update(float tick) override;
	virtual void LateUpdate(float tick) override {}
	virtual void OnDisable() override  {}
	virtual void OnDestroy() override  {}

	void SetThrowOwner(Player* player);
	Player* GetThrowOwner();
	void ClearThrowOwner();
public:
	GameObject* asisTail{ nullptr };
	Mathf::Vector3 startPos{ 0.f, 0.f, 0.f };
	float timer = 0.f;
	float speed = 2.f;

private:
	Player* throwOwner{ nullptr }; // �� �������� ���� ��ü.
};
