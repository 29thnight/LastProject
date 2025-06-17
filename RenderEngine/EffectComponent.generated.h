#pragma once

#define ReflectEffectComponent \
ReflectionField(EffectComponent) \
{ \
	PropertyField \
	({ \
		meta_property(m_name) \
	}); \
	MethodField \
	({ \
		meta_method(Update) \
	}); \
	FieldEnd(EffectComponent, PropertyAndMethod) \
};
