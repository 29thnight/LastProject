#include "EffectComponent.h"

void EffectComponent::Awake()
{
	ImGui::ContextRegister("EffectEdit", true, [&]() {
        
		ImGui::Text("asd");
		});

	ImGui::GetContext("EffectEdit").Close();
}

void EffectComponent::Update(float tick)
{
}

void EffectComponent::OnDistroy()
{
}

void EffectComponent::Update()
{
}

ParticleSystem* EffectComponent::AddEmiiter(ParticleDataType type, UINT maxParticles)
{
	return nullptr;
}

void EffectComponent::RemoveEmitter(int index)
{
}

void EffectComponent::PlayAll()
{
}

void EffectComponent::StopAll()
{
}

void EffectComponent::PlayEmitter(int index)
{
}

void EffectComponent::StopEmitter(int index)
{
}
