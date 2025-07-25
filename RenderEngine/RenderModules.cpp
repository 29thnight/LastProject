﻿#include "RenderModules.h"
#include "../ShaderSystem.h"

void RenderModules::CleanupRenderState()
{
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DeviceState::g_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

	DeviceState::g_pDeviceContext->GSSetShader(nullptr, nullptr, 0);
}

void RenderModules::SaveRenderState()
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	if (m_prevDepthState) m_prevDepthState->Release();
	if (m_prevBlendState) m_prevBlendState->Release();
	if (m_prevRasterizerState) m_prevRasterizerState->Release();

	deviceContext->OMGetDepthStencilState(&m_prevDepthState, &m_prevStencilRef);
	deviceContext->OMGetBlendState(&m_prevBlendState, m_prevBlendFactor, &m_prevSampleMask);
	deviceContext->RSGetState(&m_prevRasterizerState);
}

void RenderModules::RestoreRenderState()
{
	auto& deviceContext = DeviceState::g_pDeviceContext;

	deviceContext->OMSetDepthStencilState(m_prevDepthState, m_prevStencilRef);
	deviceContext->OMSetBlendState(m_prevBlendState, m_prevBlendFactor, m_prevSampleMask);
	deviceContext->RSSetState(m_prevRasterizerState);

	DirectX11::UnbindRenderTargets();
}

void RenderModules::EnableClipping(bool enable)
{
	if (!SupportsClipping()) return;

	m_clippingEnabled = enable;
	m_clippingParams.polarClippingEnabled = enable ? 1.0f : 0.0f;

	OnClippingStateChanged();
	UpdateClippingBuffer();
}

void RenderModules::SetClippingProgress(float progress)
{
	if (!SupportsClipping()) return;
	// -1.0 ~ 1.0 범위로 확장 (음수는 역방향)
	m_clippingParams.polarClippingEnabled = std::clamp(progress, -1.0f, 1.0f);
	UpdateClippingBuffer();
}

void RenderModules::SetClippingAxis(const Mathf::Vector3& axis)
{
	if (!SupportsClipping()) return;

	// 벡터 정규화
	Mathf::Vector3 normalizedAxis = axis;
	float length = sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);

	if (length > 0.0001f) {
		normalizedAxis.x /= length;
		normalizedAxis.y /= length;
		normalizedAxis.z /= length;
	}
	else {
		normalizedAxis = Mathf::Vector3(1.0f, 0.0f, 0.0f);
	}

	m_clippingParams.polarReferenceDir = normalizedAxis;

	UpdateClippingBuffer();
}
