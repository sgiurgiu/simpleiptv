#pragma once

#include <imgui.h>
#include <libplacebo/colorspace.h>

class MpvPlayer;

class ColorspaceDialog
{
public:
    ColorspaceDialog();
    void ShowColorspaceMenus(MpvPlayer* player);
    void ShowColorspaceDialog();
    void SetShowColorspaceDialog(bool flag, MpvPlayer* player)
    {
        showingDialog = flag;
        this->player = player;
    }

private:
    void updateColorspace();

private:
    bool showingDialog = false;
    int selectedPrimaries = 0;
    int selectedTransfer = 0;
    bool hdr = false;
    MpvPlayer* player = nullptr;
};
