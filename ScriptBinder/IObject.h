#pragma once
#ifndef interface
#define interface struct
#endif

#include <string>

interface IObject
{
	virtual unsigned int GetInstanceID() const = 0;
	virtual std::string ToString() const = 0;
};