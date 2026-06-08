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

// A <channel> entry from an XMLTV guide: its id and human-readable name. Stored
// so EPG search can name a match even when the channel isn't in CHANNELS.
struct EpgChannelInfo
{
    std::string channelId;   // channel@id -> EPG_CHANNEL_ID
    std::string displayName; // first <display-name>
};
