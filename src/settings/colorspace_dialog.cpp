#include "colorspace_dialog.h"
#include "../mpvplayer.h"

#include <string>
#include <vector>

namespace
{
constexpr std::string_view LABEL_TEMPLATE = "XXXXXXXXXX";
constexpr std::string_view FIELD_TEMPLATE = "XXXXXXXXXXXXXXXXXXXXXX";

struct ComboItem
{
    std::string label;
    int value;
};

static std::vector<ComboItem> primariesItems;
static std::vector<ComboItem> transferItems;

int findComboIndex(const std::vector<ComboItem>& items, int value)
{
    auto it = std::find_if(items.begin(), items.end(), [value](const ComboItem& item) { return item.value == value; });
    if (it != items.end())
        return static_cast<int>(std::distance(items.begin(), it));
    return -1;
}

} // namespace

ColorspaceDialog::ColorspaceDialog()
{
    for(int i = PL_COLOR_PRIM_UNKNOWN; i < PL_COLOR_PRIM_COUNT; ++i)
    {
        primariesItems.push_back({ pl_color_primaries_name(static_cast<pl_color_primaries>(i)), i });
    }
    for(int i = PL_COLOR_TRC_UNKNOWN; i < PL_COLOR_TRC_COUNT; ++i)
    {
        transferItems.push_back({ pl_color_transfer_name(static_cast<pl_color_transfer>(i)), i });
    }
}


void ColorspaceDialog::ShowColorspaceDialog()
{
    if (showingDialog && player)
    {
        ImGui::OpenPopup("Colorspace");
        showingDialog = false;
        auto cs = player->GetColorspace();
        selectedPrimaries = findComboIndex(
            primariesItems, cs.primaries);
        selectedTransfer = findComboIndex(
            transferItems, cs.transfer);
        hdr = pl_color_space_is_hdr(&cs);
    }
    if (ImGui::BeginPopup("Colorspace", ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto fieldsPosition = ImGui::CalcTextSize(LABEL_TEMPLATE.data()).x;

        ImGui::Text("Primaries:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        if (ImGui::BeginCombo("##primaries",
                              primariesItems[selectedPrimaries].label.c_str()))
        {
            for (const auto& item : primariesItems)
            {
                bool isSelected = (selectedPrimaries == item.value);
                if (ImGui::Selectable(item.label.c_str(), isSelected))
                {
                    selectedPrimaries = item.value;
                    updateColorspace();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::Text("Transfer:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        if (ImGui::BeginCombo("##transfer",
                              transferItems[selectedTransfer].label.c_str()))
        {
            for (const auto& item : transferItems)
            {
                bool isSelected = (selectedTransfer == item.value);
                if (ImGui::Selectable(item.label.c_str(), isSelected))
                {
                    selectedTransfer = item.value;
                    updateColorspace();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::Text("HDR:");
        ImGui::SameLine(fieldsPosition);
        if (ImGui::Checkbox("##hdr", &hdr))
        {
            updateColorspace();
        }

        if (ImGui::Button("Reset", ImVec2(120, 0)))
        {
            auto cs = player->GetDefaultColorspace();
            selectedPrimaries = findComboIndex(primariesItems, cs.primaries);
            selectedTransfer = findComboIndex(transferItems, cs.transfer);
            hdr = pl_color_space_is_hdr(&cs);

            updateColorspace();
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            showingDialog = false;
            player = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ColorspaceDialog::updateColorspace()
{
    pl_color_space cs = {};
    cs.primaries =
        static_cast<pl_color_primaries>(primariesItems[selectedPrimaries].value);
    cs.transfer =
        static_cast<pl_color_transfer>(transferItems[selectedTransfer].value);
    if (hdr)
        cs.hdr = pl_hdr_metadata_hdr10;
    else
        cs.hdr = pl_hdr_metadata_empty;
    player->SetColorspace(cs);
}