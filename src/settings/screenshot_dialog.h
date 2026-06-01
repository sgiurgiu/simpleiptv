#pragma once

#include <boost/signals2.hpp>
#include <memory>
#include <string>

#include "../workers_provider.h"

class ScreenshotDialog : public std::enable_shared_from_this<ScreenshotDialog>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ScreenshotDialog(Key, WorkersProvider* workersProvider);
    static std::shared_ptr<ScreenshotDialog>
    Create(WorkersProvider* workersProvider);
    void ShowDialog();
    void SetShowScreenshotDialog(bool flag)
    {
        showingDialog = flag;
    }
    template <typename S>
    void AddScreenshotSettingsChangedListener(S slot)
    {
        screenshotSettingsChangedSignal.connect(slot);
    }

private:
    WorkersProvider* workersProvider;
    bool showingDialog = false;
    std::string screenshotPath;
    std::vector<const char*> screenshotFormats = { "jpg", "png", "webp" };
    int screenshotFormat = 0;
    std::string screenshotFileTemplate;
    boost::signals2::signal<void()> screenshotSettingsChangedSignal;
};
