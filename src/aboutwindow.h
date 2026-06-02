#ifndef ABOUTWINDOW_H
#define ABOUTWINDOW_H

#include "simpleiptv_vulkan.h"

class AboutWindow
{
public:
    AboutWindow(SimpleIPTVVulkan* vulkanInstance);
    ~AboutWindow();
    void SetWindowShowing(bool flag);
    void ShowAboutWindow();

private:
    bool windowShowing = false;
    SimpleIPTVVulkan* vulkanInstance = nullptr;
    ImageData logo;
};

#endif // ABOUTWINDOW_H
