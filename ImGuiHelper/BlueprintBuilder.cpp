#include "BlueprintBuilder.h"
#include "HorizontalLayout.h"
#include <imgui.h>
#include <string>

namespace ed = ax::NodeEditor;

BlueprintNodeBuilder::BlueprintNodeBuilder(ImTextureID texture, int textureWidth, int textureHeight) :
    HeaderTextureId(texture),
    HeaderTextureWidth(textureWidth),
    HeaderTextureHeight(textureHeight),
    CurrentNodeId(0),
    CurrentStage(Stage::Invalid),
    HasHeader(false)
{
}

void BlueprintNodeBuilder::Begin(NodeId id)
{
    HasHeader = false;
    HeaderMin = HeaderMax = ImVec2();

    ed::PushStyleVar(StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

    ed::BeginNode(id);

    ImGui::PushID(id.AsPointer());
    CurrentNodeId = id;

    SetStage(Stage::Begin);
}

void BlueprintNodeBuilder::End()
{
    SetStage(Stage::End);

    ed::EndNode();

    if (ImGui::IsItemVisible())
    {
        auto alpha = static_cast<int>(255 * ImGui::GetStyle().Alpha);

        auto drawList = ed::GetNodeBackgroundDrawList(CurrentNodeId);

        const auto halfBorderWidth = ed::GetStyle().NodeBorderWidth * 0.5f;

        auto headerColor = IM_COL32(0, 0, 0, alpha) | (HeaderColor & IM_COL32(255, 255, 255, 0));
        if ((HeaderMax.x > HeaderMin.x) && (HeaderMax.y > HeaderMin.y) && HeaderTextureId)
        {
            const auto uv = ImVec2(
                (HeaderMax.x - HeaderMin.x) / (float)(4.0f * HeaderTextureWidth),
                (HeaderMax.y - HeaderMin.y) / (float)(4.0f * HeaderTextureHeight));

            drawList->AddImageRounded(HeaderTextureId,
                HeaderMin - ImVec2(8 - halfBorderWidth, 4 - halfBorderWidth),
                HeaderMax + ImVec2(8 - halfBorderWidth, 0),
                ImVec2(0.0f, 0.0f), uv,
#if IMGUI_VERSION_NUM > 18101
                headerColor, GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
#else
                headerColor, GetStyle().NodeRounding, 1 | 2);
#endif

            if (ContentMin.y > HeaderMax.y)
            {
                drawList->AddLine(
                    ImVec2(HeaderMin.x - (8 - halfBorderWidth), HeaderMax.y - 0.5f),
                    ImVec2(HeaderMax.x + (8 - halfBorderWidth), HeaderMax.y - 0.5f),
                    ImColor(255, 255, 255, 96 * alpha / (3 * 255)), 1.0f);
            }
        }
    }

    CurrentNodeId = 0;

    ImGui::PopID();

    ed::PopStyleVar();

    SetStage(Stage::Invalid);
}

void BlueprintNodeBuilder::Header(const ImVec4& color)
{
    HeaderColor = ImColor(color);
    SetStage(Stage::Header);
}

void BlueprintNodeBuilder::EndHeader()
{
    SetStage(Stage::Content);
}

void BlueprintNodeBuilder::Input(ed::PinId id)
{
    if (CurrentStage == Stage::Begin)
        SetStage(Stage::Content);

    const auto applyPadding = (CurrentStage == Stage::Input);

    SetStage(Stage::Input);

    if (applyPadding)
        ImGui::Spring(0);

    Pin(id, PinKind::Input);

	uint16_t idValue = id.Get();
    ImGui::BeginHorizontal(std::to_string(idValue).c_str());
}

void BlueprintNodeBuilder::EndInput()
{
    ImGui::EndHorizontal();

    EndPin();
}

void BlueprintNodeBuilder::Middle()
{
    if (CurrentStage == Stage::Begin)
        SetStage(Stage::Content);

    SetStage(Stage::Middle);
}

void BlueprintNodeBuilder::Output(ed::PinId id)
{
    if (CurrentStage == Stage::Begin)
        SetStage(Stage::Content);

    const auto applyPadding = (CurrentStage == Stage::Output);

    SetStage(Stage::Output);

    if (applyPadding)
        ImGui::Spring(0);

    Pin(id, PinKind::Output);

    uint16_t idValue = id.Get();
    ImGui::BeginHorizontal(std::to_string(idValue).c_str());
}

void BlueprintNodeBuilder::EndOutput()
{
    ImGui::EndHorizontal();

    EndPin();
}

bool BlueprintNodeBuilder::SetStage(Stage stage)
{
    if (stage == CurrentStage)
        return false;

    auto oldStage = CurrentStage;
    CurrentStage = stage;

    ImVec2 cursor;
    switch (oldStage)
    {
    case Stage::Begin:
        break;

    case Stage::Header:
        ImGui::EndHorizontal();
        HeaderMin = ImGui::GetItemRectMin();
        HeaderMax = ImGui::GetItemRectMax();

        // spacing between header and content
        ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.y * 2.0f);

        break;

    case Stage::Content:
        break;

    case Stage::Input:
        ed::PopStyleVar(2);

        ImGui::Spring(1, 0);
        ImGui::EndVertical();

        // #debug
        // ImGui::GetWindowDrawList()->AddRect(
        //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

        break;

    case Stage::Middle:
        ImGui::EndVertical();

        // #debug
        // ImGui::GetWindowDrawList()->AddRect(
        //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

        break;

    case Stage::Output:
        ed::PopStyleVar(2);

        ImGui::Spring(1, 0);
        ImGui::EndVertical();

        // #debug
        // ImGui::GetWindowDrawList()->AddRect(
        //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

        break;

    case Stage::End:
        break;

    case Stage::Invalid:
        break;
    }

    switch (stage)
    {
    case Stage::Begin:
        ImGui::BeginVertical("node");
        break;

    case Stage::Header:
        HasHeader = true;

        ImGui::BeginHorizontal("header");
        break;

    case Stage::Content:
        if (oldStage == Stage::Begin)
            ImGui::Spring(0);

        ImGui::BeginHorizontal("content");
        ImGui::Spring(0, 0);
        break;

    case Stage::Input:
        ImGui::BeginVertical("inputs");

        ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
        ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

        if (!HasHeader)
            ImGui::Spring(1, 0);
        break;

    case Stage::Middle:
        ImGui::Spring(1);
        ImGui::BeginVertical("middle");
        break;

    case Stage::Output:
        if (oldStage == Stage::Middle || oldStage == Stage::Input)
            ImGui::Spring(1);
        else
            ImGui::Spring(1, 0);
        ImGui::BeginVertical("outputs");

        ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
        ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

        if (!HasHeader)
            ImGui::Spring(1, 0);
        break;

    case Stage::End:
        if (oldStage == Stage::Input)
            ImGui::Spring(1, 0);
        if (oldStage != Stage::Begin)
            ImGui::EndHorizontal();
        ContentMin = ImGui::GetItemRectMin();
        ContentMax = ImGui::GetItemRectMax();

        //ImGui::Spring(0);
        ImGui::EndVertical();
        NodeMin = ImGui::GetItemRectMin();
        NodeMax = ImGui::GetItemRectMax();
        break;

    case Stage::Invalid:
        break;
    }

    return true;
}

void BlueprintNodeBuilder::Pin(ed::PinId id, ed::PinKind kind)
{
    ed::BeginPin(id, kind);
}

void BlueprintNodeBuilder::EndPin()
{
    ed::EndPin();

    // #debug
    // ImGui::GetWindowDrawList()->AddRectFilled(
    //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 64));
}