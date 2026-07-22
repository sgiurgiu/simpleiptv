#include "channel_dialog.h"

#include <imgui_stdlib.h>
#include <optional>

#include "../channels/channel.h"
#include "../channels/channels_group.h"
#include "../channels/channels_repository.h"

namespace
{
constexpr std::string_view LABEL_TEMPLATE = "Favourite:X";
constexpr std::string_view FIELD_TEMPLATE = "XXXXXXXXXXXXXXXXXXXXXX";
constexpr const char* POPUP_ID = "Add New Channel";
} // namespace

ChannelDialog::ChannelDialog(Key,
                             boost::asio::any_io_executor executor,
                             WorkersProvider* workersProvider)
: ui_executor{ executor }, workersProvider{ workersProvider }
{
}

std::shared_ptr<ChannelDialog> ChannelDialog::Create(
    boost::asio::any_io_executor executor, WorkersProvider* workersProvider)
{
    return std::make_shared<ChannelDialog>(Key{}, executor, workersProvider);
}

void ChannelDialog::SetShowAddChannelDialog()
{
    name.clear();
    uri.clear();
    groupName.clear();
    logoUri.clear();
    favourite = false;
    existingGroups.clear();
    showingDialog = true;
}

void ChannelDialog::saveChannel()
{
    auto channel = std::make_shared<Channel>(0, name, uri, logoUri, "", "", "", 0,
                                             favourite, std::nullopt);
    workersProvider->GetChannelsRepository()->AddChannel(
        channel,
        groupName.empty() ? std::optional<std::string>{}
                          : std::optional<std::string>{ groupName },
        [weak = weak_from_this()](ChannelPtr)
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->channelsChangedSignal();
        },
        ui_executor);
}

void ChannelDialog::ShowDialog()
{
    if (showingDialog)
    {
        ImGui::OpenPopup(POPUP_ID);
        showingDialog = false;
        // load existing group names to populate the dropdown
        workersProvider->GetChannelsRepository()->GetGroups(
            [weak = weak_from_this()](std::vector<ChannelsGroupPtr> groups)
            {
                auto self = weak.lock();
                if (!self)
                    return;
                self->existingGroups.clear();
                for (const auto& g : groups)
                {
                    self->existingGroups.push_back(g->GetName());
                }
            },
            ui_executor);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(POPUP_ID, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto fieldsPosition = ImGui::CalcTextSize(LABEL_TEMPLATE.data()).x;
        auto fieldWidth = ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x;

        ImGui::Text("Name:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##channel_name", &name);
        ImGui::PopItemWidth();

        ImGui::Text("URL:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##channel_url", &uri);
        ImGui::PopItemWidth();

        ImGui::Text("Group:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##channel_group", &groupName);
        ImGui::PopItemWidth();
        if (!existingGroups.empty())
        {
            ImGui::SameLine(0.f, 0.f);
            if (ImGui::BeginCombo("##channel_group_pick", "",
                                  ImGuiComboFlags_NoPreview))
            {
                for (const auto& g : existingGroups)
                {
                    if (ImGui::Selectable(g.c_str()))
                    {
                        groupName = g;
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Text("Logo URL:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##channel_logo", &logoUri);
        ImGui::PopItemWidth();

        ImGui::Text("Favourite:");
        ImGui::SameLine(fieldsPosition);
        ImGui::Checkbox("##channel_favourite", &favourite);

        ImGui::Separator();
        bool canSave = !name.empty() && !uri.empty();
        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button("OK", ImVec2(120, 0)) ||
            (canSave && ImGui::IsKeyPressed(ImGuiKey_Enter)))
        {
            saveChannel();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
