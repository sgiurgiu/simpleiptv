#include "http_proxy_dialog.h"

namespace
{
constexpr std::string_view LABEL_TEMPLATE = "XXXXXXXXXX";
constexpr std::string_view FIELD_TEMPLATE = "XXXXXXXXXXXXXXXXXXXXXX";

} // namespace

HTTPProxyDialog::HTTPProxyDialog(Key,
                                 boost::asio::any_io_executor executor,
                                 WorkersProvider* workersProvider)
: ui_executor{ executor }, workersProvider{ workersProvider }
{
}

std::shared_ptr<HTTPProxyDialog> HTTPProxyDialog::Create(
    boost::asio::any_io_executor executor, WorkersProvider* workersProvider)
{
    return std::make_shared<HTTPProxyDialog>(Key{}, executor, workersProvider);
}

void HTTPProxyDialog::ShowDialog()
{
    if (showingDialog)
    {
        ImGui::OpenPopup("HTTP Proxy");
        showingDialog = false;
        workersProvider->GetProxyRepository()->LoadConfiguredProxy(
            [weak = weak_from_this()](HttpProxy proxy)
            {
                auto self = weak.lock();
                if (!self)
                    return;
                self->proxy = std::move(proxy);
                strncpy(self->proxyHostname, self->proxy.host.c_str(),
                        sizeof(self->proxyHostname));
                self->proxyHostname[sizeof(self->proxyHostname) - 1] = '\0';
                self->proxyPort = self->proxy.port;
                self->useProxy = self->proxy.use;
            },
            ui_executor);
    }
    if (ImGui::BeginPopupModal("HTTP Proxy", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto fieldsPosition = ImGui::CalcTextSize(LABEL_TEMPLATE.data()).x;
        ImGui::Text("Host:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        ImGui::InputText("##proxy_host", proxyHostname,
                         IM_ARRAYSIZE(proxyHostname), ImGuiInputTextFlags_None);
        ImGui::PopItemWidth();

        ImGui::Text("Port:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x);
        ImGui::InputInt("##proxy_port", &proxyPort);
        ImGui::PopItemWidth();

        ImGui::Text("Use Proxy:");
        ImGui::SameLine(fieldsPosition);
        ImGui::Checkbox("##use_proxy", &useProxy);

        ImGui::Separator();
        ImGui::Text("Any changes made here require the\n application to be "
                    "restarted to take effect.");

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            proxy.host = proxyHostname;
            proxy.port = proxyPort;
            proxy.use = useProxy;
            workersProvider->GetProxyRepository()->SaveConfiguredProxy(proxy);
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