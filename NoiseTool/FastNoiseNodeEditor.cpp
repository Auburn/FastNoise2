#include "FastNoiseNodeEditor.h"
#include <imgui.h>
#include "imnodes.h"

void FastNoiseNodeEditor::Update()
{
    ImGui::Begin("FastNoise Node Editor");
    ImGui::TextUnformatted("A -- add node");

    imnodes::BeginNodeEditor();

    for (Node& node : mNodes)
    {
        imnodes::BeginNode(node.id);

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("node");
        imnodes::EndNodeTitleBar();
        
        imnodes::BeginInputAttribute(node.id << 8);
        ImGui::TextUnformatted("input");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::DragFloat("value", &node.value, 0.01f);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(node.id << 16);
        const float text_width = ImGui::CalcTextSize("output").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
        ImGui::TextUnformatted("output");
        imnodes::EndAttribute();

        imnodes::EndNode();
    }

    for (const Link& link : mLinks)
    {
        imnodes::Link(link.id, link.startAttr, link.endAttr);
    }

    imnodes::EndNodeEditor();

    {
        Link link;
        if (imnodes::IsLinkCreated(&link.startAttr, &link.endAttr))
        {
            link.id = ++mCurrentNodeId;
            mLinks.push_back(link);
        }
    }

    if (ImGui::IsKeyReleased(ImGuiKey_A) &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        const int node_id = ++mCurrentNodeId;
        imnodes::SetNodeScreenSpacePos(node_id, ImGui::GetMousePos());
        mNodes.push_back(Node{ node_id, 0.f });
    }

    ImGui::End();
}
