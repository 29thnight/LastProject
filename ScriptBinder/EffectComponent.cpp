#include "EffectComponent.h"

EffectComponent::EffectComponent()
{
	m_orderID = Component::Order2Uint(ComponentOrder::MeshRenderer);
	m_typeID = TypeTrait::GUIDCreator::GetTypeID<EffectComponent>();
	
}

void EffectComponent::Update(float tick)
{
}
