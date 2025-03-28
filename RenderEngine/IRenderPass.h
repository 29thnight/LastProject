#pragma once
#include "Core.Minimal.h"
#include "DeviceResources.h"
#include "PSO.h"

enum RTV_Type
{
	BaseColor,
	OccRoughMetal,
	Normal,
	Emissive,
	RTV_TypeMax
};

class Camera;
class Scene;
class IRenderPass abstract
{
public:
	IRenderPass() = default;
	virtual ~IRenderPass() = default;

	virtual void Execute(Scene& scene, Camera& camera) abstract;
	virtual void ControlPanel() {};

protected:
	std::unique_ptr<PipelineStateObject> m_pso{ nullptr };
	bool m_abled{ true };
};