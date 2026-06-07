#pragma once

#include <cstddef>
#include <expat.h>
#include <functional>

#include "epg_programme.h"

// Incremental, push-driven XMLTV parser built on libexpat. Feed() may be called
// repeatedly with successive chunks of the document; each completed <programme>
// element is delivered to the sink. At most one in-flight programme is held, so
// memory stays bounded regardless of document size.
//
// Not thread-safe: callers must serialize Feed() calls (e.g. via a strand).
class XmlTvParser
{
public:
    using ProgrammeSink = std::function<void(EpgProgramme&&)>;

    explicit XmlTvParser(ProgrammeSink sink);
    ~XmlTvParser();

    XmlTvParser(const XmlTvParser&) = delete;
    XmlTvParser& operator=(const XmlTvParser&) = delete;

    // Feeds the next chunk; pass isFinal=true for the final chunk (which may be
    // empty). Returns false on a fatal XML error; stop feeding after that.
    bool Feed(const char* data, std::size_t len, bool isFinal);

private:
    enum class Capture
    {
        None,
        Title,
        Description
    };

    static void XMLCALL onStartElement(void* userData, const XML_Char* name,
                                       const XML_Char** atts);
    static void XMLCALL onEndElement(void* userData, const XML_Char* name);
    static void XMLCALL onCharacterData(void* userData, const XML_Char* s,
                                        int len);

    void startElement(const XML_Char* name, const XML_Char** atts);
    void endElement(const XML_Char* name);
    void characterData(const XML_Char* s, int len);

private:
    XML_Parser parser = nullptr;
    ProgrammeSink sink;
    EpgProgramme current;
    Capture capture = Capture::None;
    bool inProgramme = false;
    // Total bytes fed to the parser before the current Feed() call. Used to map
    // expat's document-wide byte index back into the current chunk for logging.
    std::size_t bytesConsumed = 0;
};
