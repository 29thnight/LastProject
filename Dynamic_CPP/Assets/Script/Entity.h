#pragma once
#include "Core.Minimal.h"
#include "ModuleBehavior.h"

class Entity : public ModuleBehavior
{
public:
	MODULE_BEHAVIOR_BODY(Entity)
	virtual void Awake() override {}
	virtual void Start() override {}
	virtual void FixedUpdate(float fixedTick) override {}
	virtual void OnTriggerEnter(const Collision& collision) override {}
	virtual void OnTriggerStay(const Collision& collision) override {}
	virtual void OnTriggerExit(const Collision& collision) override {}
	virtual void OnCollisionEnter(const Collision& collision) {}
	virtual void OnCollisionStay(const Collision& collision) override {}
	virtual void OnCollisionExit(const Collision& collision) {}
	virtual void Update(float tick) override {}
	virtual void LateUpdate(float tick) override {}
	virtual void OnDisable() override  {}
	virtual void OnDestroy() override  {}
public:
	virtual void Interact() {}
};
