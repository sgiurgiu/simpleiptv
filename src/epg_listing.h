#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

#if __cpp_lib_chrono < 201907L
#include <date/date.h>
#include <date/tz.h>
#endif

class EpgListing
{
public:
    EpgListing(const nlohmann::json& json);
    std::string GetTimeAndProgram() const;
    std::string GetTime() const;
    bool isListingCurrent() const;
    std::string GetTitle() const;
    std::string GetDescription() const;
    std::chrono::system_clock::duration GetDuration() const;
#if __cpp_lib_chrono >= 201907L
    using LocalTime =
        std::chrono::local_time<std::chrono::system_clock::duration>;
#else
    using LocalTime = date::local_time<std::chrono::system_clock::duration>;
#endif

    LocalTime GetStartTime() const;
    LocalTime GetEndTime() const;

    // Build a synthetic "No Data" listing spanning [start, end), used to pad
    // gaps in a channel's guide so the whole window is always covered.
    static EpgListing MakeNoData(LocalTime start, LocalTime end);
    bool IsNoData() const;

private:
    EpgListing() = default;
    std::string decode64(const std::string& val);
    std::chrono::system_clock::time_point getTimePoint(const std::string& timestamp);

private:
    std::string id;
    std::string epgId;
    std::string title;
    std::string description;
    std::string channelId;
    std::string streamId;
    std::string startLocalHour;
    std::string endLocalHour;
    std::string startLocalTimeString;
    std::string endLocalTimeString;
    std::string timeAndProgram;
    std::string time;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    LocalTime localStartTime;
    LocalTime localEndTime;
    bool noData = false;
};