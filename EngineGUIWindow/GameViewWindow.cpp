#ifndef DYNAMICCPP_EXPORTS
#include "GameViewWindow.h"
#include "SceneRenderer.h"
#include "CameraComponent.h"
#include "IconsFontAwesome6.h"
#include "fa.h"

void GameViewWindow::RenderGameViewWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(ICON_FA_GAMEPAD "  Game        ", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());
	{
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 availRegion = ImGui::GetContentRegionAvail();

		float imageHeight = availRegion.y;
		float imageWidth = imageHeight * DeviceState::g_aspectRatio;

		if (imageWidth > availRegion.x) {
			imageWidth = availRegion.x;
			imageHeight = imageWidth / DeviceState::g_aspectRatio;
		}

		ImVec2 imageSize = ImVec2(imageWidth, imageHeight);
		ImVec2 offset = ImVec2((availRegion.x - imageSize.x) * 0.5f, (availRegion.y - imageSize.y) * 0.5f);
		ImVec2 currentPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(currentPos.x + offset.x, currentPos.y + offset.y));

		auto scene = SceneManagers->GetRenderScene();
		auto camera = CameraManagement->GetLastCamera();
		auto renderData = RenderPassData::GetData(camera);

		ImGui::Image((ImTextureID)renderData->m_renderTarget->m_pSRV, imageSize);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
#endif // !DYNAMICCPP_EXPORTS