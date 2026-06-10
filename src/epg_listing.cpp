#include "epg_listing.h"
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <charconv>
#include <fmt/chrono.h>
#include <fmt/format.h>

EpgListing::EpgListing(const nlohmann::json& json)
{
    if (json.contains("id"))
    {
        id = json["id"].get<std::string>();
    }
    if (json.contains("epg_id"))
    {
        epgId = json["epg_id"].get<std::string>();
    }
    if (json.contains("channel_id"))
    {
        channelId = json["channel_id"].get<std::string>();
    }
    if (json.contains("stream_id"))
    {
        streamId = json["stream_id"].get<std::string>();
    }
    if (json.contains("title"))
    {
        title = decode64(json["title"].get<std::string>());
    }
    if (json.contains("description"))
    {
        description = decode64(json["description"].get<std::string>());
    }
    if (json.contains("start_timestamp"))
    {
        startTime = getTimePoint(json["start_timestamp"].get<std::string>());
    }
    if (json.contains("stop_timestamp"))
    {
        endTime = getTimePoint(json["stop_timestamp"].get<std::string>());
    }
    finalize();
}

EpgListing EpgListing::FromProgramme(std::int64_t startUnix,
                                     std::int64_t stopUnix,
                                     std::string title,
                                     std::string description)
{
    EpgListing listing;
    listing.title = std::move(title);
    listing.description = std::move(description);
    listing.startTime =
        std::chrono::system_clock::from_time_t(static_cast<time_t>(startUnix));
    listing.endTime =
        std::chrono::system_clock::from_time_t(static_cast<time_t>(stopUnix));
    listing.finalize();
    return listing;
}

void EpgListing::finalize()
{
    std::string day;
#if __cpp_lib_chrono >= 201907L
    auto tz = std::chrono::current_zone();
    localStartTime = tz->to_local(startTime);
    localEndTime = tz->to_local(endTime);
    startLocalHour = fmt::format("{:%H:%M}", localStartTime);
    endLocalHour = fmt::format("{:%H:%M}", localEndTime);
    startLocalTimeString = fmt::format("{:%c}", localStartTime);
    endLocalTimeString = fmt::format("{:%c}", localEndTime);
    day = fmt::format("{:%F}", localStartTime);
#else
    localStartTime =
        date::make_zoned(date::current_zone(), startTime).get_local_time();
    localEndTime =
        date::make_zoned(date::current_zone(), endTime).get_local_time();

    startLocalHour = date::format("%H:%M", localStartTime);
    endLocalHour = date::format("%H:%M", localEndTime);
    startLocalTimeString = date::format("%Y-%m-%d %H:%M", localStartTime);
    endLocalTimeString = date::format("%Y-%m-%d %H:%M", localEndTime);
    day = date::format("{:%F}", localStartTime);
#endif

    timeAndProgram =
        fmt::format("{} {}-{} {}", day, startLocalHour, endLocalHour, title);
    time = fmt::format("{}-{}", startLocalHour, endLocalHour);
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

EpgListing::LocalTime EpgListing::GetStartTime() const
{
    return localStartTime;
}

EpgListing::LocalTime EpgListing::GetEndTime() const
{
    return localEndTime;
}

std::string EpgListing::GetTime() const
{
    return time;
}

EpgListing EpgListing::MakeNoData(LocalTime start, LocalTime end)
{
    EpgListing listing;
    listing.noData = true;
    listing.title = "No Data";
    listing.description = "No Data";
    listing.localStartTime = start;
    listing.localEndTime = end;
    // Synthetic fillers only need their local times for layout; the absolute
    // system time is irrelevant, so derive it directly instead of converting
    // local->system (which would risk throwing around DST boundaries).
    listing.startTime =
        std::chrono::system_clock::time_point{ start.time_since_epoch() };
    listing.endTime =
        std::chrono::system_clock::time_point{ end.time_since_epoch() };
#if __cpp_lib_chrono >= 201907L
    listing.startLocalHour = fmt::format("{:%H:%M}", start);
    listing.endLocalHour = fmt::format("{:%H:%M}", end);
#else
    listing.startLocalHour = date::format("%H:%M", start);
    listing.endLocalHour = date::format("%H:%M", end);
#endif
    listing.timeAndProgram = fmt::format("{}-{} {}", listing.startLocalHour,
                                         listing.endLocalHour, listing.title);
    listing.time =
        fmt::format("{}-{}", listing.startLocalHour, listing.endLocalHour);
    return listing;
}

bool EpgListing::IsNoData() const
{
    return noData;
}
