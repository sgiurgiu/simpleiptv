#pragma once

#include <atomic>
#include <memory>

#ifndef STV_UNIX
#error "Building the mpris service is only supported on Unix/Linux"
#endif

#include <boost/signals2.hpp>
#include <dbus/org.mpris.MediaPlayer2.Player.xml.h>
#include <dbus/org.mpris.MediaPlayer2.TrackList.xml.h>
#include <dbus/org.mpris.MediaPlayer2.xml.h>
#include <sdbus-c++/sdbus-c++.h>

#include "channels/channel.h"
#include "channels/channels_group.h"
#include "mpvplayer_state.h"

class MprisService : public std::enable_shared_from_this<MprisService>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    MprisService(Key);
    ~MprisService();
    static std::shared_ptr<MprisService> Create();
    // Stops dispatching D-Bus method calls (idempotent). Call during shutdown
    // before the objects wired to its signals are destroyed.
    void Stop();
    void SetCurrentPlayerState(PlayerState state);
    void SetCurrentChannel(ChannelPtr channel);
    void SetCurrentChannelGroup(ChannelsGroupPtr group);
    void SetVolume(double vol);
    void SetCurrentFullscreen(bool fullscreen);
    template <typename S>
    void AddNextListener(S slot)
    {
        nextSignal.connect(slot);
    }
    template <typename S>
    void AddPreviousListener(S slot)
    {
        previousSignal.connect(slot);
    }
    template <typename S>
    void AddPauseListener(S slot)
    {
        pauseSignal.connect(slot);
    }
    template <typename S>
    void AddPlayPauseListener(S slot)
    {
        playPauseSignal.connect(slot);
    }
    template <typename S>
    void AddStopListener(S slot)
    {
        stopSignal.connect(slot);
    }
    template <typename S>
    void AddPlayListener(S slot)
    {
        playSignal.connect(slot);
    }
    template <typename S>
    void AddQuitListener(S slot)
    {
        quitSignal.connect(slot);
    }
    template <typename S>
    void AddVolumeListener(S slot)
    {
        volumeSignal.connect(slot);
    }
    template <typename S>
    void AddFullscreenListener(S slot)
    {
        fullscreenSignal.connect(slot);
    }

private:
    class ManagerAdaptor
    : public sdbus::AdaptorInterfaces<sdbus::ObjectManager_adaptor>
    {
    public:
        ManagerAdaptor(sdbus::IConnection& connection, sdbus::ObjectPath path)
        : AdaptorInterfaces(connection, std::move(path))
        {
            registerAdaptor();
        }

        ~ManagerAdaptor()
        {
            unregisterAdaptor();
        }
    };
    class MediaPlayer2Adaptor final
    : public sdbus::AdaptorInterfaces<org::mpris::MediaPlayer2_adaptor,
                                      org::mpris::MediaPlayer2::Player_adaptor,
                                      org::mpris::MediaPlayer2::TrackList_adaptor,
                                      sdbus::ManagedObject_adaptor,
                                      sdbus::Properties_adaptor>
    {
    public:
        MediaPlayer2Adaptor(sdbus::IConnection& connection,
                            sdbus::ObjectPath path,
                            MprisService* service);
        ~MediaPlayer2Adaptor();
        virtual void Raise();
        virtual void Quit();

        virtual bool CanQuit();
        virtual bool Fullscreen();
        virtual void Fullscreen(const bool& value);
        virtual bool CanSetFullscreen();
        virtual bool CanRaise();
        virtual bool HasTrackList();
        virtual std::string Identity();
        virtual std::string DesktopEntry();
        virtual std::vector<std::string> SupportedUriSchemes();
        virtual std::vector<std::string> SupportedMimeTypes();
        virtual void Next();
        virtual void Previous();
        virtual void Pause();
        virtual void PlayPause();
        virtual void Stop();
        virtual void Play();
        virtual void Seek(const int64_t& Offset);
        virtual void SetPosition(const sdbus::ObjectPath& TrackId,
                                 const int64_t& Position);
        virtual void OpenUri(const std::string& Uri);
        virtual std::string PlaybackStatus();
        virtual std::string LoopStatus();
        virtual void LoopStatus(const std::string& value);
        virtual double Rate();
        virtual void Rate(const double& value);
        virtual bool Shuffle();
        virtual void Shuffle(const bool& value);
        virtual std::map<std::string, sdbus::Variant> Metadata();
        virtual double Volume();
        virtual void Volume(const double& value);
        virtual int64_t Position();
        virtual double MinimumRate();
        virtual double MaximumRate();
        virtual bool CanGoNext();
        virtual bool CanGoPrevious();
        virtual bool CanPlay();
        virtual bool CanPause();
        virtual bool CanSeek();
        virtual bool CanControl();
        virtual std::vector<std::map<std::string, sdbus::Variant>>
        GetTracksMetadata(const std::vector<sdbus::ObjectPath>& TrackIds);
        virtual void AddTrack(const std::string& Uri,
                              const sdbus::ObjectPath& AfterTrack,
                              const bool& SetAsCurrent);
        virtual void RemoveTrack(const sdbus::ObjectPath& TrackId);
        virtual void GoTo(const sdbus::ObjectPath& TrackId);
        virtual std::vector<sdbus::ObjectPath> Tracks();
        virtual bool CanEditTracks();

    private:
        MprisService* service = nullptr;
    };

    using PlayerControlSignal = boost::signals2::signal<void()>;
    using VolumeSignal = boost::signals2::signal<void(double)>;
    using FullscreenSignal = boost::signals2::signal<void(bool)>;

    std::unique_ptr<sdbus::IConnection> sessionConnection;
    std::atomic_bool eventLoopRunning{ true };
    sdbus::ServiceName serviceName;
    std::unique_ptr<ManagerAdaptor> managerAdaptor;
    std::unique_ptr<MediaPlayer2Adaptor> mediaPlayerAdaptor;
    PlayerState currentPlayerState = PlayerState::STOPPED;
    bool currentFullscreen = false;
    PlayerControlSignal nextSignal;
    PlayerControlSignal previousSignal;
    PlayerControlSignal pauseSignal;
    PlayerControlSignal playPauseSignal;
    PlayerControlSignal stopSignal;
    PlayerControlSignal playSignal;
    PlayerControlSignal quitSignal;
    VolumeSignal volumeSignal;
    FullscreenSignal fullscreenSignal;
    ChannelPtr currentChannel;
    ChannelsGroupPtr currentGroup;
    double volume = 0.0;
};