#include "Camera.h"
#include "../InputManager.h"
#include "DeviceState.h"
#include "ImGuiRegister.h"

const static float pi = XM_PIDIV2 - 0.01f;
const static float pi2 = XM_PI * 2.f;

Mathf::Vector4 Camera::ConvertScreenToWorld(Mathf::Vector2 screenPosition, float depth)
{
	// 1. 스크린 좌표를 NDC 좌표로 변환
	float x_ndc = (2.0f * screenPosition.x / GetScreenSize().width) - 1.0f;
	float y_ndc = 1.0f - (2.0f * screenPosition.y / GetScreenSize().height);
	Mathf::Vector4 screenPositionNDC = { x_ndc, y_ndc, depth, 1.0f };

	// 2. 역행렬 투영 변환 (Projection^-1)
	Mathf::xMatrix invProj = CalculateInverseProjection();
	Mathf::Vector4 viewPosition = XMVector3TransformCoord(screenPositionNDC, invProj);

	// 3. 뷰 역행렬 변환 (View^-1)
	Mathf::xMatrix invView = CalculateInverseView();
	Mathf::Vector4 worldPosition = XMVector3TransformCoord(viewPosition, invView);

	return worldPosition;
}

Mathf::Vector4 Camera::RayCast(Mathf::Vector2 screenPosition)
{
	Mathf::Vector4 _near = ConvertScreenToWorld(screenPosition, 0.f);
	std::cout << "Near" << _near.x << _near.y << _near.z << _near.w << std::endl;
	Mathf::Vector4 _far = ConvertScreenToWorld(screenPosition, 1.f);
	std::cout << "Far" << _far.x << _far.y << _far.z << _far.w << std::endl;
	Mathf::Vector4 direction = _far - _near;
	direction = XMVector3Normalize(direction);
	return direction;
}

Mathf::xMatrix Camera::CalculateView() const
{
	return XMMatrixLookAtLH(m_eyePosition, m_lookAt, m_up);
}

Mathf::xMatrix Camera::CalculateInverseView() const
{
	return XMMatrixInverse(nullptr, CalculateView());
}

Mathf::xMatrix Camera::CalculateInverseProjection()
{
	return XMMatrixInverse(nullptr, CalculateProjection());
}

DirectX11::Sizef Camera::GetScreenSize() const
{
	return DeviceState::g_ClientRect;
}

DirectX::BoundingFrustum Camera::GetFrustum()
{
	DirectX::BoundingFrustum frustum;
	frustum.CreateFromMatrix(frustum, CalculateProjection());
	frustum.Transform(frustum, CalculateView());
	return frustum;
}

void Camera::HandleMovement(float deltaTime)
{
	float x = 0.f, y = 0.f, z = 0.f;

	if (InputManagement->IsKeyPressed('W'))
	{
		z += 1.f;
	}
	if (InputManagement->IsKeyPressed('S'))
	{
		z -= 1.f;
	}
	if (InputManagement->IsKeyPressed('A'))
	{
		x -= 1.f;
	}
	if (InputManagement->IsKeyPressed('D'))
	{
		x += 1.f;
	}
	if (InputManagement->IsKeyPressed('Q'))
	{
		y -= 1.f;
	}
	if (InputManagement->IsKeyPressed('E'))
	{
		y += 1.f;
	}

	//if (InputManagement->IsMouseButtonDown(MouseKey::MIDDLE))
	//{
	//	m_pitch += InputManagement->GetMouseDelta().y * 0.01f;
	//	m_pitch = fmax(-pi, fmin(m_pitch, pi));
	//	m_yaw += InputManagement->GetMouseDelta().x * 0.01f;
	//	if (m_yaw > XM_PI) m_yaw -= pi2;
	//	if (m_yaw < -XM_PI) m_yaw += pi2;

	//	Mathf::xVector viewRotation = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0.f);
	//	m_forward = XMVector3Normalize(XMVector3Rotate(FORWARD, viewRotation));
	//	m_right = XMVector3Normalize(XMVector3Cross(UP, m_forward));
	//}
	XMVECTOR m_rotationQuat = XMQuaternionIdentity();

	//Change the Camera Rotaition Quaternion Not Use XMQuaternionRotationRollPitchYaw
	if (InputManagement->IsMouseButtonDown(MouseKey::MIDDLE))
	{
		// 마우스 이동량 가져오기
		m_pitch += InputManagement->GetMouseDelta().y * 0.01f;
		m_yaw += InputManagement->GetMouseDelta().x * 0.01f;

		// 현재 회전 기준 축을 얻음
		XMVECTOR rightAxis = XMVector3Normalize(XMVector3Cross(UP, m_forward));

		// 쿼터니언 회전 생성
		XMVECTOR pitchQuat = XMQuaternionRotationAxis(rightAxis, m_pitch);
		XMVECTOR yawQuat = XMQuaternionRotationAxis(UP, m_yaw);

		// 누적 회전 적용 (기존 회전에 새로운 회전 적용)
		XMVECTOR newRotation = XMQuaternionMultiply(m_rotationQuat, yawQuat);
		newRotation = XMQuaternionMultiply(newRotation, pitchQuat);

		// 슬러핑(SLERP) 적용하여 부드러운 회전
		m_rotationQuat = XMQuaternionSlerp(m_rotationQuat, newRotation, 0.9f);
		m_rotationQuat = XMQuaternionNormalize(m_rotationQuat);

		// 새로운 방향 벡터 계산
		m_forward = XMVector3Normalize(XMVector3Rotate(FORWARD, m_rotationQuat));

		// Up 벡터와 교차하여 Right 벡터 보정
		m_right = XMVector3Normalize(XMVector3Cross(UP, m_forward));
	}

	if (InputManagement->IsMouseButtonDown(MouseKey::LEFT))
	{
		m_rayDirection = RayCast(InputManagement->GetMousePos());
		std::cout << "MousePos" << InputManagement->GetMousePos().x << InputManagement->GetMousePos().y << std::endl;
		std::cout << "RayCast" << m_rayDirection.x << m_rayDirection.y << m_rayDirection.z << m_rayDirection.w << std::endl;
	}

	m_eyePosition += ((z * m_forward) + (y * UP) + (x * m_right)) * m_speed * deltaTime;
	m_lookAt = m_eyePosition + m_forward;
}

PerspacetiveCamera::PerspacetiveCamera()
{
	ImGui::ContextRegister("Perspacetive Camera", [&]()
	{
		ImGui::DragFloat("FOV", &m_fov, 1.f, 1.f, 179.f);
		ImGui::DragFloat("Aspect Ratio", &m_aspectRatio, 0.1f, 0.1f, 10.f);
		ImGui::DragFloat("Near Plane", &m_nearPlane, 0.1f, 0.1f, 100.f);
		ImGui::DragFloat("Far Plane", &m_farPlane, 1.f, 1.f, 1000.f);
		ImGui::DragFloat("Speed", &m_speed, 1.f, 1.f, 100.f);
		ImGui::DragFloat("Pitch", &m_pitch, 0.01f, -pi, pi);
		ImGui::DragFloat("Yaw", &m_yaw, 0.01f, -pi, pi);
		ImGui::DragFloat("Roll", &m_roll, 0.01f, -pi, pi);

		ImGui::Text("Eye Position");
		ImGui::DragFloat3("##Eye Position", &m_eyePosition.m128_f32[0], -1000, 1000);
		ImGui::Text("Forward");
		ImGui::DragFloat3("##Forward", &m_forward.m128_f32[0], -1000, 1000);
		ImGui::Text("Right");
		ImGui::DragFloat3("##Right", &m_right.m128_f32[0], -1000, 1000);
	});
}

Mathf::xMatrix PerspacetiveCamera::CalculateProjection()
{
	return XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fov), m_aspectRatio, 0.1f, 200.f);
}

OrthographicCamera::OrthographicCamera()
{
}

Mathf::xMatrix OrthographicCamera::CalculateProjection()
{
	return XMMatrixOrthographicLH(m_viewWidth, m_viewHeight, m_nearPlane, m_farPlane);
}
