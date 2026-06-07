#pragma once

#include <cstdint>
#include <string>

// A single completed <programme> row extracted from an XMLTV guide.
struct EpgProgramme
{
    std::string channelId;   // programme@channel -> CHANNELS.EPG_CHANNEL_ID
    std::string title;
    std::string description;
    int64_t startTime = 0;   // unix seconds (UTC)
    int64_t stopTime = 0;    // unix seconds (UTC)
};
