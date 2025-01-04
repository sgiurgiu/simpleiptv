#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

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
    std::chrono::local_time<std::chrono::system_clock::duration> GetStartTime() const;
    std::chrono::local_time<std::chrono::system_clock::duration> GetEndTime() const;

private:
    std::string decode64(const std::string& val);
    std::chrono::system_clock::time_point getTimePoint(const std::string& timestamp);

private:
    std::string id;
    std::string epgId;
    std::string title;
    std::string description;
    std::string channelId;
    std::string streamId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::local_time<std::chrono::system_clock::duration> localStartTime;
    std::chrono::local_time<std::chrono::system_clock::duration> localEndTime;
    std::string startLocalHour;
    std::string endLocalHour;
    std::string startLocalTimeString;
    std::string endLocalTimeString;
    std::string timeAndProgram;
    std::string time;
};