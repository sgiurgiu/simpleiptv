#include "dbus_mpris_service.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

MprisService::MprisService(Key)
: sessionConnection{ sdbus::createSessionBusConnection() }
, serviceName{ fmt::format("org.mpris.MediaPlayer2.simpleiptv.instance{}",
                           getpid()) }
{
    sessionConnection->requestName(serviceName);
    sessionConnection->enterEventLoopAsync();
    managerAdaptor = std::make_unique<ManagerAdaptor>(
        *sessionConnection, sdbus::ObjectPath{ "/org/mpris/MediaPlayer2" });
    mediaPlayerAdaptor = std::make_unique<MediaPlayer2Adaptor>(
        *sessionConnection, sdbus::ObjectPath{ "/org/mpris/MediaPlayer2" }, this);
}
MprisService::~MprisService()
{
    sessionConnection->releaseName(serviceName);
    Stop();
}
void MprisService::Stop()
{
    // leaveEventLoop() is what stops Next()/Previous()/... from being dispatched.
    // Guard so it runs once whether called at shutdown or from the destructor.
    if (eventLoopRunning.exchange(false))
    {
        sessionConnection->leaveEventLoop();
    }
}
std::shared_ptr<MprisService> MprisService::Create()
{
    return std::make_shared<MprisService>(Key{});
}
void MprisService::SetCurrentPlayerState(PlayerState state)
{
    this->currentPlayerState = state;
    try
    {
        mediaPlayerAdaptor->emitPropertiesChangedSignal(
            org::mpris::MediaPlayer2::Player_adaptor::INTERFACE_NAME,
            std::vector<sdbus::PropertyName>{
                sdbus::PropertyName{ "PlaybackStatus" },
                sdbus::PropertyName{ "CanPlay" },
                sdbus::PropertyName{ "CanPause" } });
    }
    catch (const sdbus::Error& error)
    {
        spdlog::error(error.getMessage());
    }
}
void MprisService::SetCurrentFullscreen(bool fullscreen)
{
    if (currentFullscreen == fullscreen)
    {
        return;
    }
    currentFullscreen = fullscreen;
    try
    {
        mediaPlayerAdaptor->emitPropertiesChangedSignal(
            org::mpris::MediaPlayer2_adaptor::INTERFACE_NAME,
            std::vector<sdbus::PropertyName>{
                sdbus::PropertyName{ "Fullscreen" } });
    }
    catch (const sdbus::Error& error)
    {
        spdlog::error(error.getMessage());
    }
}
void MprisService::SetCurrentChannel(ChannelPtr channel)
{
    this->currentChannel = channel;
    try
    {
        mediaPlayerAdaptor->emitPropertiesChangedSignal(
            org::mpris::MediaPlayer2::Player_adaptor::INTERFACE_NAME,
            std::vector<sdbus::PropertyName>{
                sdbus::PropertyName{ "Metadata" } });
    }
    catch (const sdbus::Error& error)
    {
        spdlog::error(error.getMessage());
    }
}
void MprisService::SetCurrentChannelGroup(ChannelsGroupPtr group)
{
    this->currentGroup = group;
    /*mediaPlayerAdaptor->emitTrackListReplaced(
        org::mpris::MediaPlayer2::TrackList_adaptor::INTERFACE_NAME,
        std::vector<sdbus::PropertyName>{ sdbus::PropertyName{ "Metadata" } });*/
}
void MprisService::SetVolume(double vol)
{
    this->volume = vol;
    try
    {
        mediaPlayerAdaptor->emitPropertiesChangedSignal(
            org::mpris::MediaPlayer2::Player_adaptor::INTERFACE_NAME,
            std::vector<sdbus::PropertyName>{
                sdbus::PropertyName{ "Volume" } });
    }
    catch (const sdbus::Error& error)
    {
        spdlog::error(error.getMessage());
    }
}
MprisService::MediaPlayer2Adaptor::MediaPlayer2Adaptor(
    sdbus::IConnection& connection, sdbus::ObjectPath path, MprisService* service)
: AdaptorInterfaces(connection, std::move(path)), service{ service }
{
    registerAdaptor();
    emitInterfacesAddedSignal({ sdbus::InterfaceName{
        org::mpris::MediaPlayer2_adaptor::INTERFACE_NAME } });
    emitInterfacesAddedSignal({ sdbus::InterfaceName{
        org::mpris::MediaPlayer2::Player_adaptor::INTERFACE_NAME } });
    emitInterfacesAddedSignal({ sdbus::InterfaceName{
        org::mpris::MediaPlayer2::TrackList_adaptor::INTERFACE_NAME } });
}
MprisService::MediaPlayer2Adaptor::~MediaPlayer2Adaptor()
{
    emitInterfacesRemovedSignal({ sdbus::InterfaceName{
        org::mpris::MediaPlayer2_adaptor::INTERFACE_NAME } });
    emitInterfacesRemovedSignal({ sdbus::InterfaceName{
        org::mpris::MediaPlayer2::Player_adaptor::INTERFACE_NAME } });
    emitInterfacesRemovedSignal({ sdbus::InterfaceName{
        org::mpris::MediaPlayer2::TrackList_adaptor::INTERFACE_NAME } });
    unregisterAdaptor();
}
void MprisService::MediaPlayer2Adaptor::Raise()
{
}
void MprisService::MediaPlayer2Adaptor::Quit()
{
    service->quitSignal();
}
bool MprisService::MediaPlayer2Adaptor::CanQuit()
{
    return true;
}
bool MprisService::MediaPlayer2Adaptor::Fullscreen()
{
    return service->currentFullscreen;
}
void MprisService::MediaPlayer2Adaptor::Fullscreen(const bool& fullscreen)
{
    service->fullscreenSignal(fullscreen);
}
bool MprisService::MediaPlayer2Adaptor::CanSetFullscreen()
{
    return true;
}
bool MprisService::MediaPlayer2Adaptor::CanRaise()
{
    return false;
}
bool MprisService::MediaPlayer2Adaptor::HasTrackList()
{
    return true;
}
std::string MprisService::MediaPlayer2Adaptor::Identity()
{
    return "SimpleIPTV"
#ifdef STV_DEBUG
           " (Debug)"
#endif
        ;
}
std::string MprisService::MediaPlayer2Adaptor::DesktopEntry()
{
    return "simpleiptv";
}
std::vector<std::string> MprisService::MediaPlayer2Adaptor::SupportedUriSchemes()
{
    return { "http" };
}
std::vector<std::string> MprisService::MediaPlayer2Adaptor::SupportedMimeTypes()
{
    return { "video/mpeg" };
}
void MprisService::MediaPlayer2Adaptor::Next()
{
    service->nextSignal();
}
void MprisService::MediaPlayer2Adaptor::Previous()
{
    service->previousSignal();
}
void MprisService::MediaPlayer2Adaptor::Pause()
{
    service->pauseSignal();
}
void MprisService::MediaPlayer2Adaptor::PlayPause()
{
    service->playPauseSignal();
}
void MprisService::MediaPlayer2Adaptor::Stop()
{
    service->stopSignal();
}
void MprisService::MediaPlayer2Adaptor::Play()
{
    service->playSignal();
}
void MprisService::MediaPlayer2Adaptor::Seek(const int64_t&)
{
}
void MprisService::MediaPlayer2Adaptor::SetPosition(const sdbus::ObjectPath&,
                                                    const int64_t&)
{
}
void MprisService::MediaPlayer2Adaptor::OpenUri(const std::string& Uri)
{
    spdlog::debug("OpenURI: {}", Uri);
}
std::string MprisService::MediaPlayer2Adaptor::PlaybackStatus()
{
    switch (service->currentPlayerState)
    {
    case PlayerState::PLAYING:
        return "Playing";
    case PlayerState::STOPPED:
        return "Stopped";
    case PlayerState::PAUSED:
        return "Paused";
    default:
        return "N/A";
    }
}
std::string MprisService::MediaPlayer2Adaptor::LoopStatus()
{
    return "Track";
}
void MprisService::MediaPlayer2Adaptor::LoopStatus(const std::string&)
{
}
double MprisService::MediaPlayer2Adaptor::Rate()
{
    return 1.0;
}
void MprisService::MediaPlayer2Adaptor::Rate(const double&)
{
}
bool MprisService::MediaPlayer2Adaptor::Shuffle()
{
    return false;
}
void MprisService::MediaPlayer2Adaptor::Shuffle(const bool&)
{
}
std::map<std::string, sdbus::Variant> MprisService::MediaPlayer2Adaptor::Metadata()
{
    spdlog::debug("MprisService::MediaPlayer2Adaptor::Metadata()");
    std::map<std::string, sdbus::Variant> metadata;
    if (service->currentChannel)
    {
        metadata["mpris:trackid"] = sdbus::Variant{ fmt::format(
            "/simpleiptv/track/{}", service->currentChannel->GetId()) };
        metadata["mpris:artUrl"] =
            sdbus::Variant{ service->currentChannel->GetLogoUri() };
        metadata["mpris:length"] = sdbus::Variant{ 0 };
        metadata["xesam:url"] =
            sdbus::Variant{ service->currentChannel->GetUri() };
        metadata["xesam:title"] =
            sdbus::Variant{ service->currentChannel->GetName() };
    }

    /*for (const auto& t : metadata)
    {
        if (t.second.containsValueOfType<std::string>())
        {
            spdlog::debug("Metadata: key: {}, value: {}", t.first,
                          t.second.get<std::string>());
        }
        else if (t.second.containsValueOfType<int>())
        {
            spdlog::debug("Metadata: key: {}, value: {}", t.first,
                          t.second.get<int>());
        }
    }*/

    return metadata;
}
double MprisService::MediaPlayer2Adaptor::Volume()
{
    return service->volume / 100.0;
}
void MprisService::MediaPlayer2Adaptor::Volume(const double& value)
{
    service->volume = value * 100.0;
    service->volumeSignal(service->volume);
}
int64_t MprisService::MediaPlayer2Adaptor::Position()
{
    return 0;
}
double MprisService::MediaPlayer2Adaptor::MinimumRate()
{
    return 1.0;
}
double MprisService::MediaPlayer2Adaptor::MaximumRate()
{
    return 1.0;
}
bool MprisService::MediaPlayer2Adaptor::CanGoNext()
{
    return true;
}
bool MprisService::MediaPlayer2Adaptor::CanGoPrevious()
{
    return true;
}
bool MprisService::MediaPlayer2Adaptor::CanPlay()
{
    return service->currentPlayerState != PlayerState::PLAYING;
}
bool MprisService::MediaPlayer2Adaptor::CanPause()
{
    return service->currentPlayerState == PlayerState::PLAYING;
}
bool MprisService::MediaPlayer2Adaptor::CanSeek()
{
    return false;
}
bool MprisService::MediaPlayer2Adaptor::CanControl()
{
    return true;
}
std::vector<std::map<std::string, sdbus::Variant>>
MprisService::MediaPlayer2Adaptor::GetTracksMetadata(
    const std::vector<sdbus::ObjectPath>& TrackIds)
{
    for (const auto& t : TrackIds)
    {
        spdlog::debug("GetTracksMetadata: TrackId: {}", t);
    }
    return {};
}
void MprisService::MediaPlayer2Adaptor::AddTrack(const std::string& Uri,
                                                 const sdbus::ObjectPath& AfterTrack,
                                                 const bool& SetAsCurrent)
{
    spdlog::debug("AddTrack: uri: {}, AfterTrack: {}, SetAsCurrent: {}", Uri,
                  AfterTrack, SetAsCurrent);
}
void MprisService::MediaPlayer2Adaptor::RemoveTrack(const sdbus::ObjectPath& TrackId)
{
    spdlog::debug("RemoveTrack: TrackId: {}", TrackId);
}
void MprisService::MediaPlayer2Adaptor::GoTo(const sdbus::ObjectPath& TrackId)
{
    spdlog::debug("GoTo: TrackId: {}", TrackId);
}
std::vector<sdbus::ObjectPath> MprisService::MediaPlayer2Adaptor::Tracks()
{
    return {};
}
bool MprisService::MediaPlayer2Adaptor::CanEditTracks()
{
    return false;
}
