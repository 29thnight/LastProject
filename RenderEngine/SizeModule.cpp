#include "SizeModule.h"

void SizeModule::Update(float delta, std::vector<ParticleData>& particles)
{
	for (auto& particle : particles)
	{
		if (particle.isActive)
		{
			float normalizedAge = particle.age / particle.lifeTime;
			if (IsEasingEnabled())
			{
				normalizedAge = ApplyEasing(normalizedAge);
			}
			particle.size = m_sizeOverLife(normalizedAge);
		}
	}
}