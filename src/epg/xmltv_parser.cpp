#include "xmltv_parser.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <expat.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <utility>

namespace
{
int toInt(std::string_view sv)
{
    int v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}

// Parse an XMLTV timestamp "YYYYMMDDHHMMSS [+/-HHMM]" into unix seconds (UTC).
// When the offset is absent the time is interpreted as UTC.
int64_t parseXmltvTime(std::string_view ts)
{
    std::size_t i = 0;
    while (i < ts.size() && ts[i] == ' ')
        ++i;
    if (ts.size() - i < 14)
        return 0;

    auto d = ts.substr(i, 14);
    int y = toInt(d.substr(0, 4));
    unsigned mo = static_cast<unsigned>(toInt(d.substr(4, 2)));
    unsigned dy = static_cast<unsigned>(toInt(d.substr(6, 2)));
    int hh = toInt(d.substr(8, 2));
    int mi = toInt(d.substr(10, 2));
    int ss = toInt(d.substr(12, 2));

    int offsetSeconds = 0;
    std::size_t j = i + 14;
    while (j < ts.size() && ts[j] == ' ')
        ++j;
    if (j + 5 <= ts.size() && (ts[j] == '+' || ts[j] == '-'))
    {
        int sign = ts[j] == '-' ? -1 : 1;
        offsetSeconds = sign * (toInt(ts.substr(j + 1, 2)) * 3600 +
                                toInt(ts.substr(j + 3, 2)) * 60);
    }

    namespace ch = std::chrono;
    ch::year_month_day ymd{ ch::year{ y }, ch::month{ mo }, ch::day{ dy } };
    if (!ymd.ok())
        return 0;
    auto tp = ch::sys_days{ ymd } + ch::hours{ hh } + ch::minutes{ mi } +
              ch::seconds{ ss } - ch::seconds{ offsetSeconds };
    return ch::duration_cast<ch::seconds>(tp.time_since_epoch()).count();
}
} // namespace

XmlTvParser::XmlTvParser(ProgrammeSink sink)
: parser(XML_ParserCreate(nullptr))
, sink(std::move(sink))
{
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, &XmlTvParser::onStartElement,
                          &XmlTvParser::onEndElement);
    XML_SetCharacterDataHandler(parser, &XmlTvParser::onCharacterData);
}

XmlTvParser::~XmlTvParser()
{
    if (parser)
        XML_ParserFree(parser);
}

bool XmlTvParser::Feed(const char* data, std::size_t len, bool isFinal)
{
    if (XML_Parse(parser, data, static_cast<int>(len), isFinal ? 1 : 0) ==
        XML_STATUS_ERROR)
    {
        spdlog::error("XMLTV parse error at line {}, column {}: {}",
                      XML_GetCurrentLineNumber(parser),
                      XML_GetCurrentColumnNumber(parser),
                      XML_ErrorString(XML_GetErrorCode(parser)));
        // XML_GetCurrentByteIndex is an offset into the whole document, not into
        // this chunk; map it back to a local position to dump real context.
        auto byteIndex =
            static_cast<std::size_t>(XML_GetCurrentByteIndex(parser));
        if (byteIndex >= bytesConsumed && byteIndex - bytesConsumed <= len)
        {
            std::size_t local = byteIndex - bytesConsumed;
            std::size_t from = local > 40 ? local - 40 : 0;
            std::size_t to = std::min(len, local + 40);
            spdlog::error("Context: [{}>>>{}]",
                          std::string_view(data + from, local - from),
                          std::string_view(data + local, to - local));
        }
        else
        {
            spdlog::error("Error byte index {} outside current chunk "
                          "[{}, {}) -- bytes reaching the parser are not XML",
                          byteIndex, bytesConsumed, bytesConsumed + len);
        }
        return false;
    }
    bytesConsumed += len;
    return true;
}

void XMLCALL XmlTvParser::onStartElement(void* userData, const XML_Char* name,
                                         const XML_Char** atts)
{
    static_cast<XmlTvParser*>(userData)->startElement(name, atts);
}

void XMLCALL XmlTvParser::onEndElement(void* userData, const XML_Char* name)
{
    static_cast<XmlTvParser*>(userData)->endElement(name);
}

void XMLCALL XmlTvParser::onCharacterData(void* userData, const XML_Char* s,
                                          int len)
{
    static_cast<XmlTvParser*>(userData)->characterData(s, len);
}

void XmlTvParser::startElement(const XML_Char* name, const XML_Char** atts)
{
    std::string_view el{ name };
    if (el == "programme")
    {
        current = EpgProgramme{};
        inProgramme = true;
        capture = Capture::None;
        for (std::size_t k = 0; atts[k] != nullptr; k += 2)
        {
            std::string_view key{ atts[k] };
            const XML_Char* val = atts[k + 1];
            if (val == nullptr)
                break;
            if (key == "channel")
                current.channelId = val;
            else if (key == "start")
                current.startTime = parseXmltvTime(val);
            else if (key == "stop")
                current.stopTime = parseXmltvTime(val);
        }
    }
    else if (inProgramme && el == "title")
        capture = Capture::Title;
    else if (inProgramme && el == "desc")
        capture = Capture::Description;
}

void XmlTvParser::endElement(const XML_Char* name)
{
    std::string_view el{ name };
    if (el == "programme")
    {
        if (inProgramme)
        {
            inProgramme = false;
            capture = Capture::None;
            if (sink)
                sink(std::move(current));
            current = EpgProgramme{};
        }
    }
    else if (el == "title" || el == "desc")
        capture = Capture::None;
}

void XmlTvParser::characterData(const XML_Char* s, int len)
{
    if (!inProgramme || capture == Capture::None)
        return;
    std::string_view chunk{ s, static_cast<std::size_t>(len) };
    if (capture == Capture::Title)
        current.title.append(chunk);
    else
        current.description.append(chunk);
}
