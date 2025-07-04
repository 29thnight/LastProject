#pragma once
#include "Core.Minimal.h"
#include "ConditionParameter.generated.h"


enum class ConditionType
{
	Greater,
	Less,
	Equal,
	NotEqual,
	True,
	False,
	None,
};
AUTO_REGISTER_ENUM(ConditionType)


enum class ValueType
{
	Float,
	Int,
	Bool,
	Trigger,
};
AUTO_REGISTER_ENUM(ValueType)

class ConditionParameter
{
public:
   ReflectConditionParameter
	[[Serializable]]
	ConditionParameter() = default;
	template<typename T>
	ConditionParameter(T value, ValueType _vType, std::string _name = "None")
		: vType(_vType), name(_name)
	{
		switch (vType)
		{
		case ValueType::Float:
			fValue = static_cast<float>(value);
			break;
		case ValueType::Int:
			iValue = static_cast<int>(value);
			break;
		case ValueType::Bool:
			bValue = static_cast<bool>(value);
			break;
		case ValueType::Trigger:
			tValue = static_cast<bool>(value);
			break;
		}
	}

	template<typename T>
	void UpdateParameter(T value)
	{
		switch (vType)
		{
		case ValueType::Float:
			fValue = static_cast<float>(value);
			break;
		case ValueType::Int:
			iValue = static_cast<int>(value);
			break;
		case ValueType::Bool:
			bValue = static_cast<bool>(value);
			break;
		case ValueType::Trigger:
			tValue = static_cast<bool>(value);
			break;
		}
	}

	void ResetTrigger() 
	{
		tValue = false; 
	}

	template<typename T>
	T GetValue()
	{
		switch (vType)
		{
		case ValueType::Float:
			return fValue;
			break;
		case ValueType::Int:
			return iValue;
			break;
		case ValueType::Bool:
			return bValue;
			break;
		case ValueType::Trigger:
			return bValue;
			break;
		}
	}

	template<typename T>
	void SetParameter(T value, ValueType _vType, std::string _name = "None")
	{
		vType = _vType;
		name = _name;
		switch (vType)
		{
		case ValueType::Float:
			fValue = static_cast<float>(value);
			break;
		case ValueType::Int:
			iValue = static_cast<int>(value);
			break;
		case ValueType::Bool:
			bValue = static_cast<bool>(value);
			break;
		case ValueType::Trigger:
			tValue = static_cast<bool>(value);
			break;
		}
	}
	[[Property]]
	ValueType vType =ValueType::Float;
	[[Property]]
	std::string name = "None";
	[[Property]]
	float fValue{};
	[[Property]]
	int iValue{};
	[[Property]]
	bool bValue{};
	
	//Trigger   basic = false,
	[[Property]]
	bool tValue{false};
};
