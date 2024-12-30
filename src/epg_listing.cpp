#include "epg_listing.h"
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <format>

EpgListing::EpgListing(const nlohmann::json& json)
{
    id = json["id"].get<std::string>();
    epgId = json["epg_id"].get<std::string>();
    channelId = json["channel_id"].get<std::string>();
    streamId = json["stream_id"].get<std::string>();
    title = decode64(json["title"].get<std::string>());
    description = decode64(json["description"].get<std::string>());
    startTime = getTimePoint(json["start_timestamp"].get<std::string>());
    endTime = getTimePoint(json["stop_timestamp"].get<std::string>());

    auto tz = std::chrono::current_zone();
    localStartTime = tz->to_local(startTime);
    localEndTime = tz->to_local(endTime);

    startLocalHour = std::format("{:%H:%M}", localStartTime);
    endLocalHour = std::format("{:%H:%M}", localEndTime);
    timeAndProgram = std::format("{}-{} {}", startLocalHour, endLocalHour, title);
    time = std::format("{}-{}", startLocalHour, endLocalHour);
    startLocalTimeString = std::format("{:%c}", localStartTime);
    endLocalTimeString = std::format("{:%c}", localEndTime);
}

std::string EpgListing::decode64(const std::string& val)
{
    using namespace boost::archive::iterators;
    using It =
        transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
    return boost::algorithm::trim_right_copy_if(
        std::string(It(std::begin(val)), It(std::end(val))),
        [](char c) { return c == '\0'; });
}
std::chrono::system_clock::time_point
EpgListing::getTimePoint(const std::string& timestamp)
{
    std::chrono::system_clock::time_point chronoTime;
    time_t time = 0;
    auto [ptr, ec] = std::from_chars(timestamp.data(),
                                     timestamp.data() + timestamp.size(), time);
    if (ec == std::errc())
    {
        chronoTime = std::chrono::system_clock::from_time_t(time);
    }
    return chronoTime;
}
std::string EpgListing::GetTimeAndProgram() const
{
    return timeAndProgram;
}
bool EpgListing::isListingCurrent() const
{
    auto now = std::chrono::system_clock::now();
    return startTime < now && endTime > now;
}
std::string EpgListing::GetTitle() const
{
    return title;
}
std::string EpgListing::GetDescription() const
{
    return description;
}
std::chrono::system_clock::duration EpgListing::GetDuration() const
{
    return endTime - startTime;
}
std::chrono::local_time<std::chrono::system_clock::duration>
EpgListing::GetStartTime() const
{
    return localStartTime;
}
std::string EpgListing::GetTime() const
{
    return time;
}
std::chrono::system_clock::duration EpgListing::GetDurationLeft() const
{
    if (!isListingCurrent())
        return GetDuration();
    auto now = std::chrono::system_clock::now();
    return endTime - now;
}
std::chrono::system_clock::duration EpgListing::GetDurationLeftFromHourStart() const
{
    if (!isListingCurrent())
        return GetDuration();
    auto top_of_the_hour_tp =
        std::chrono::floor<std::chrono::hours>(std::chrono::system_clock::now());
    return endTime - top_of_the_hour_tp;
}