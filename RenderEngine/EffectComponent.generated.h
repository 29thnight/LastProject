#pragma once

#define ReflectEffectComponent \
ReflectionFieldInheritance(EffectComponent, Component) \
{ \
	PropertyField \
	({ \
		meta_property(num) \
	}); \
	MethodField \
	({ \
		meta_method(RemoveEmitter, "index") \
		meta_method(PlayAll) \
		meta_method(StopAll) \
		meta_method(PlayEmitter, "index") \
		meta_method(StopEmitter, "index") \
	}); \
	FieldEnd(EffectComponent, PropertyAndMethodInheritance) \
};
