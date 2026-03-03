#include "simpleiptv_coordinator.h"

SimpleIPTVCoordinator::SimpleIPTVCoordinator(boost::asio::io_context& uiContext,
                                             WorkersProvider* workersProvider,
                                             SimpleIPTVVulkan* vulkanInstance)
: workersProvider{ workersProvider }
, simpleiptv{ uiContext.get_executor(), workersProvider, vulkanInstance }
, mpvPlayer{ uiContext.get_executor(), workersProvider, vulkanInstance }
{
    mpvPlayer.InitializeMpv([this]() { return simpleiptv.RenderDesktop(); });

    simpleiptv.AddChannelActivatedListener([this](ChannelPtr channel) {
        mpvPlayer.Play(channel);
#ifdef STV_UNIX
        auto mprisService = this->workersProvider->GetMprisService();
        mprisService->SetCurrentChannel(channel);
#endif
    });
#ifdef STV_UNIX
    auto mprisService = workersProvider->GetMprisService();
    mprisService->AddNextListener([this]()
                                  { simpleiptv.ActivateNextChannel(); });
    mprisService->AddPreviousListener(
        [this]() { simpleiptv.ActivatePreviousChannel(); });
    mprisService->AddPlayListener([this]() { mpvPlayer.Play(); });
    mprisService->AddPauseListener([this]() { mpvPlayer.Pause(); });
    mprisService->AddStopListener([this]() { mpvPlayer.Stop(); });
    mprisService->AddPlayPauseListener([]() { /*player.PlayPause();*/ });
    mprisService->AddQuitListener([this]() { simpleiptv.SetQuit(true); });
    mpvPlayer.AddPlayerStateListener(
        [mprisService](PlayerState state)
        { mprisService->SetCurrentPlayerState(state); });
    mprisService->AddVolumeListener([this](double vol)
                                    { mpvPlayer.SetVolume(vol); });
    mprisService->SetVolume(mpvPlayer.GetVolume());
#endif
    mpvPlayer.AddFileLoadingErrorListener(
        [this](const std::string& error)
        { simpleiptv.SetFileLoadingError(error); });
    mpvPlayer.AddVolumeListener(
        [this](double vol)
        {
    #ifdef STV_UNIX
            this->workersProvider->GetMprisService()->SetVolume(vol);
    #endif
            simpleiptv.SetVolume(vol);
        });
    mpvPlayer.AddSubsAvailableListener(
        [this](std::vector<std::string> subsIds)
        { simpleiptv.SetAvailableSubIds(std::move(subsIds)); });

    mpvPlayer.AddPlayerStateListener(
            [this](PlayerState state)
            { simpleiptv.SetPlayerState(state); });

    simpleiptv.AddPlayChannelListener([this]() { mpvPlayer.Play(); });
    simpleiptv.AddStopChannelListener([this]() { mpvPlayer.Stop(); });
    simpleiptv.AddVolumeListener([this](double vol)
                                        { mpvPlayer.SetVolume(vol); });
    simpleiptv.AddCCButtonChangedListener([this](const std::string& id)
    { mpvPlayer.ClosedCaptions(id); });
    simpleiptv.SetVolume(mpvPlayer.GetVolume());
    simpleiptv.AddVolumeIncreaseListener([this]() { mpvPlayer.VolumeIncrease(); });
    simpleiptv.AddVolumeDecreaseListener([this]() { mpvPlayer.VolumeDecrease(); });
    simpleiptv.AddVolumeToggleMuteListener([this]() { mpvPlayer.VolumeToggleMute(); });
    simpleiptv.AddScreenshotListener([this]() { mpvPlayer.Screenshot(); });
    simpleiptv.AddScreenshotSettingsChangedListener(
        [this]()
        {
            mpvPlayer.SetScreenshotPath(
                this->workersProvider->GetSettingsRepository()->GetScreenshotPath(
                    std::filesystem::path(".")));
            mpvPlayer.SetScreenshotFormat(
                this->workersProvider->GetSettingsRepository()->GetScreenshotFormat(
                    "jpg"));
            mpvPlayer.SetScreenshotFileTemplate(
                this->workersProvider->GetSettingsRepository()
                    ->GetScreenshotFileTemplate("screenshot_%04n"));
        });
}

void SimpleIPTVCoordinator::Render()
{
    mpvPlayer.Render();
}

void SimpleIPTVCoordinator::SetSize(int width, int height)
{
    mpvPlayer.SetSize(width, height);
}

bool SimpleIPTVCoordinator::ShouldQuit() const
{
    return simpleiptv.ShouldQuit();
}