#include "aboutwindow.h"
#include "fonts/IconsFontAwesome4.h"
#include "images/logo.h"
#include "imgui.h"

#include <stb_image.h>
#include <string_view>

namespace
{
constexpr auto ABOUT_WINDOW_POPUP_TITLE = "About Simple IPTV";
constexpr auto LICENSE_TEXT =
    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, either version 3 of the License, or\n"
    "(at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n"
    "\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program.  If not, see <https://www.gnu.org/licenses/>.";
} // namespace

AboutWindow::AboutWindow(SimpleIPTVVulkan* vulkanInstance)
: vulkanInstance(vulkanInstance)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    auto imageData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(______icons_simpleiptv_icon_256_png),
        ______icons_simpleiptv_icon_256_png_len, &width, &height, &channels,
        STBI_rgb_alpha);

    logo = vulkanInstance->CreateImageData(width, height, channels, imageData);
    stbi_image_free(imageData);
}

AboutWindow::~AboutWindow()
{
    vulkanInstance->DestroyImageData(logo);
}

void AboutWindow::SetWindowShowing(bool flag)
{
    windowShowing = flag;
}
void AboutWindow::ShowAboutWindow()
{
    if (windowShowing)
    {
        ImGui::OpenPopup(ABOUT_WINDOW_POPUP_TITLE);
        windowShowing = false;
    }
    ImVec2 size(560.f, 245.f + ImGui::GetTextLineHeightWithSpacing() * 3.f);
    ImGui::SetNextWindowSize(size, ImGuiCond_Once);
    if (ImGui::BeginPopupModal(ABOUT_WINDOW_POPUP_TITLE, nullptr,
                               ImGuiWindowFlags_None))
    {
        if (logo.tex)
        {
            ImTextureID texture = reinterpret_cast<ImTextureID>(logo.tex);
            ImGui::Image(texture, ImVec2(150, 150));
            ImGui::SameLine();
        }

        ImGui::BeginGroup();
        ImGui::Text("%s", SIMPLEIPTV_STRING);
        ImGui::Text("%s",
                    reinterpret_cast<const char*>("Copyright " ICON_FA_COPYRIGHT
                                                  " 2026, Sergiu Giurgiu"));
        ImGui::Text("%s", LICENSE_TEXT);
        ImGui::EndGroup();
        if (ImGui::Button("OK", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
