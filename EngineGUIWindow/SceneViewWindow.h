#pragma once
#ifndef DYNAMICCPP_EXPORTS
#include "Core.Mathf.h"
#include "ImGuiRegister.h"

class SceneRenderer;
class GizmoRenderer;
class GameObject;
class Camera;
struct Ray { XMFLOAT3 origin, direction; };
struct RayHitResult
{
	GameObject* object;
	float distance;
};

class SceneViewWindow
{
public:
	SceneViewWindow(SceneRenderer* ptr, GizmoRenderer* gizmo_ptr);
	~SceneViewWindow() = default;

	void RenderSceneViewWindow();
private:
	void RenderSceneView(float* cameraView, float* cameraProjection, float* matrix, bool editTransformDecomposition, GameObject* obj, Camera* cam);
	Mathf::Vector3 ConvertMouseToWorldPosition(Camera* cam, const ImVec2& mouseScreenPos, const ImVec2& imagePos, const ImVec2& imageSize, float depth = 0.0f);
	Ray CreateRayFromCamera(Camera* cam, const ImVec2& mousePos, const ImVec2& imagePos, const ImVec2& imageSize);
	//[[deprecated("Soon Deleted")]]
	GameObject* PickObjectFromRay(const Ray& ray, const std::vector<std::shared_ptr<GameObject>>& sceneObjects);
	
	std::vector<RayHitResult> PickObjectsFromRay(const Ray& ray, const std::vector<std::shared_ptr<GameObject>>& sceneObjects);
	
private:
	SceneRenderer* m_sceneRenderer{ nullptr };
	GizmoRenderer* m_gizmoRenderer{ nullptr };

	std::vector<RayHitResult> m_hitResults;
	size_t m_currentHitIndex = 0;
};
#endif // !DYNAMICCPP_EXPORTS