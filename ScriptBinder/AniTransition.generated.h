#pragma once

#define ReflectAniTransition \
ReflectionField(AniTransition) \
{ \
	PropertyField \
	({ \
		meta_property(conditions) \
		meta_property(m_name) \
		meta_property(curStateName) \
		meta_property(nextStateName) \
		meta_property(hasExitTime) \
		meta_property(exitTime) \
		meta_property(blendTime) \
	}); \
	FieldEnd(AniTransition, PropertyOnly) \
};
