#include "screenshot_dialog.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
constexpr std::string_view LABEL_TEMPLATE = "XXXXXXXXXXXXXXX";
constexpr std::string_view FIELD_TEMPLATE = "XXXXXXXXXXXXXXXXXXXXXX";

} // namespace

ScreenshotDialog::ScreenshotDialog(Key, WorkersProvider* workersProvider)
: workersProvider{ workersProvider }
{
    screenshotPath = workersProvider->GetSettingsRepository()
                         ->GetScreenshotPath(std::filesystem::path("."))
                         .string();
    auto screenshotFormatString =
        workersProvider->GetSettingsRepository()->GetScreenshotFormat("jpg");
    for (size_t i = 0; i < screenshotFormats.size(); i++)
    {
        if (screenshotFormats[i] == screenshotFormatString)
        {
            screenshotFormat = static_cast<int>(i);
            break;
        }
    }
    screenshotFileTemplate =
        workersProvider->GetSettingsRepository()->GetScreenshotFileTemplate(
            "screenshot_%04n");
}

std::shared_ptr<ScreenshotDialog>
ScreenshotDialog::Create(WorkersProvider* workersProvider)
{
    return std::make_shared<ScreenshotDialog>(Key{}, workersProvider);
}

void ScreenshotDialog::ShowDialog()
{
    if (showingDialog)
    {
        ImGui::OpenPopup("Screenshot");
        showingDialog = false;
    }
    if (ImGui::BeginPopupModal("Screenshot", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto fieldsPosition = ImGui::CalcTextSize(LABEL_TEMPLATE.data()).x;
        ImGui::Text("Path:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        ImGui::InputText("##screenshot_path", &screenshotPath);
        ImGui::PopItemWidth();
        ImGui::Text("Format:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        ImGui::Combo("##screenshot_format", &screenshotFormat,
                     screenshotFormats.data(), (int)screenshotFormats.size());
        ImGui::PopItemWidth();
        ImGui::Text("File Template:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        ImGui::InputText("##screenshot_file_template", &screenshotFileTemplate);
        ImGui::PopItemWidth();
        if (ImGui::Button("OK", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            workersProvider->GetSettingsRepository()->SetScreenshotPath(
                screenshotPath);
            workersProvider->GetSettingsRepository()->SetScreenshotFormat(
                screenshotFormats[screenshotFormat]);
            workersProvider->GetSettingsRepository()->SetScreenshotFileTemplate(
                screenshotFileTemplate);
            screenshotSettingsChangedSignal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
