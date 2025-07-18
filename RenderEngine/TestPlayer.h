#pragma once
#include "GameObject.h"
#include "Transform.h"
#include "AnimationState.h"
#include "AnimationController.h"
#include "AniBehaviour.h"
#include "Animator.h"
class TestPlayer
{

public:
	void GetPlayer(GameObject* _player);
	void Update(float deltaTime);

	float deta = 0;
	Mathf::Vector2 dir;
	bool OnMove = false;
	void Punch();
	void Move(Mathf::Vector2 dir);
	void Jump();
	GameObject* player;
	float speed = 0.1f;
	float maxSpeed = 10.0f;
};

class IdleAni : public AniBehaviour
{
public:
	IdleAni() { name = "Idle"; };
	virtual void Enter() override
	{

	};
	virtual void Update(float DeltaTime) override
	{

	};
	virtual void Exit() override
	{

	};
};
class WalkAni : public AniBehaviour
{
public:
	WalkAni() { name = "Walk"; }
	virtual void Enter() override
	{
	};
	virtual void Update(float DeltaTime) override
	{

	}; 
	virtual void Exit() override
	{

	};
};

class RunAni : public AniBehaviour
{
public:
	RunAni()  {name = "Run";}
	virtual void Enter() override
	{
	};
	virtual void Update(float DeltaTime)override
	{

	};
	virtual void Exit()override
	{

	};
};