#pragma once
#include "ImGui.h"

namespace ImGui
{
    inline bool DrawCollapsingHeaderWithButton(const char* label, ImGuiTreeNodeFlags flags, const char* buttonLabel, bool* outButtonClicked)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGui::PushID(label);
        ImGuiID id = ImGui::GetID("##Header");

        const float buttonWidth = 24.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImVec2 fullSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImRect headerRect(cursorPos, cursorPos + fullSize);

        // 1. 배경 렌더링 (시각용)
        bool open = ImGui::GetStateStorage()->GetBool(id, true);
        bool hovered = false, held = false;
        ImU32 bgColor = open
            ? ImGui::GetColorU32(ImGuiCol_HeaderActive)
            : (hovered ? ImGui::GetColorU32(ImGuiCol_HeaderHovered) : ImGui::GetColorU32(ImGuiCol_Header));
        ImGui::RenderFrame(headerRect.Min, headerRect.Max, bgColor, true, ImGui::GetStyle().FrameRounding);

        // 2. 오른쪽 버튼 먼저 배치 (그래야 아래에서 클릭 충돌 안 남)
        ImVec2 buttonPos = ImVec2(cursorPos.x + fullSize.x - buttonWidth, cursorPos.y);
        ImGui::SetCursorScreenPos(buttonPos);
        bool buttonPressed = ImGui::Button(buttonLabel, ImVec2(buttonWidth, 0));
        if (buttonPressed && outButtonClicked)
            *outButtonClicked = true;

        // 3. 왼쪽 영역에 클릭 영역 설정
        ImVec2 clickZoneSize = ImVec2(fullSize.x - buttonWidth - spacing, fullSize.y);
        ImRect clickZone(cursorPos, cursorPos + clickZoneSize);
        ImGui::SetCursorScreenPos(cursorPos); // 클릭 영역 위치 복원

        ImGui::ItemSize(clickZone);
        if (ImGui::ItemAdd(clickZone, id))
        {
            hovered = ImGui::IsItemHovered();
            held = ImGui::IsItemActive();
            if (ImGui::IsItemClicked())
            {
                open = !open;
                ImGui::GetStateStorage()->SetBool(id, open);
            }
        }

        // 4. 텍스트 수동 출력
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        ImVec2 textPos = ImVec2(cursorPos.x + ImGui::GetStyle().FramePadding.x,
            cursorPos.y + (clickZoneSize.y - labelSize.y) * 0.5f);
        ImGui::RenderText(textPos, label);

        // 5. 다음 줄로
        ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + fullSize.y));
        ImGui::PopID();
		ImGui::Dummy(ImVec2(0, 3));

        return open;
    }

    inline bool DrawCollapsingHeaderWithButton(const char* label, ImGuiTreeNodeFlags flags, const char* buttonLabel, bool* outButtonClicked, bool* pChecked)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGui::PushID(label);
        ImGuiID id = ImGui::GetID("##Header");

        const float checkboxWidth = ImGui::GetFrameHeight(); // 정사각형 체크박스
        const float buttonWidth = 24.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        ImVec2 fullSize = ImVec2(ImGui::GetContentRegionAvail().x, checkboxWidth);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImRect headerRect(cursorPos, cursorPos + fullSize);

        // 1. 배경 렌더링
        bool open = ImGui::GetStateStorage()->GetBool(id, true);
        bool hovered = false, held = false;
        ImU32 bgColor = open
            ? ImGui::GetColorU32(ImGuiCol_HeaderActive)
            : (hovered ? ImGui::GetColorU32(ImGuiCol_HeaderHovered)
                : ImGui::GetColorU32(ImGuiCol_Header));
        ImGui::RenderFrame(headerRect.Min, headerRect.Max, bgColor, true, ImGui::GetStyle().FrameRounding);

        // 2. 체크박스 위치
        ImVec2 checkboxPos = cursorPos;
        ImGui::SetCursorScreenPos(checkboxPos);
        ImGui::PushID("CheckBox");
        ImGui::Checkbox("##Enable", pChecked);
        ImGui::PopID();

        // 3. 우측 버튼
        ImVec2 buttonPos = ImVec2(cursorPos.x + fullSize.x - buttonWidth, cursorPos.y);
        ImGui::SetCursorScreenPos(buttonPos);
        if (ImGui::Button(buttonLabel, ImVec2(buttonWidth, 0)) && outButtonClicked)
            *outButtonClicked = true;

        // 4. 클릭 영역 (텍스트 및 토글)
        float leftOffset = checkboxWidth + spacing;
        ImVec2 clickZoneStart = ImVec2(cursorPos.x + leftOffset, cursorPos.y);
        ImVec2 clickZoneEnd = ImVec2(cursorPos.x + fullSize.x - buttonWidth - spacing, cursorPos.y + fullSize.y);
        ImRect clickZone(clickZoneStart, clickZoneEnd);
        ImGui::SetCursorScreenPos(clickZoneStart);

        ImGui::ItemSize(clickZone);
        if (ImGui::ItemAdd(clickZone, id))
        {
            hovered = ImGui::IsItemHovered();
            held = ImGui::IsItemActive();
            if (ImGui::IsItemClicked())
            {
                open = !open;
                ImGui::GetStateStorage()->SetBool(id, open);
            }
        }

        // 5. 텍스트
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        ImVec2 textPos = ImVec2(clickZoneStart.x + ImGui::GetStyle().FramePadding.x,
            cursorPos.y + (fullSize.y - labelSize.y) * 0.5f);
        ImGui::RenderText(textPos, label);

        // 6. 다음 줄로
        ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + fullSize.y));
        ImGui::Dummy(ImVec2(0, 3));
        ImGui::PopID();

        return open;
    }
}
