#include "dbconnection_pool.h"
#include <soci/connection-pool.h>
#include <soci/sqlite3/soci-sqlite3.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "stv_utils.h"

namespace
{
static constexpr size_t poolSize = 3;
static soci::connection_pool pool{ poolSize };
} // namespace

void DatabaseConnections::Initialize()
{
    auto confFolder = Utils::GetAppConfigFolder();
#ifdef STV_DEBUG
    auto dbFile = confFolder / "simpleiptv_debug.db";
#else
    auto dbFile = confFolder / "simpleiptv.db";
#endif

    try
    {
        for (size_t i = 0; i < poolSize; i++)
        {
            pool.at(i).open(soci::sqlite3, dbFile.string());
        }
        initTables();
    }
    catch (const soci::soci_error& ex)
    {
        spdlog::critical("Database initialization failed: {}", ex.what());
        throw std::runtime_error("Database initialization failed");
    }
}
soci::session DatabaseConnections::GetConnection()
{
    return soci::session(pool);
}
void DatabaseConnections::initTables()
{
    auto con = GetConnection();
    con << "PRAGMA foreign_keys = ON";
    con << "PRAGMA synchronous = NORMAL";
    con << "CREATE TABLE IF NOT EXISTS SCHEMA_VERSION(VERSION INT)";
    con << "CREATE TABLE IF NOT EXISTS CHANNEL_GROUPS(GROUP_ID "
           "INTEGER NOT NULL PRIMARY KEY, "
           "PARENT_GROUP_ID INTEGER, NAME TEXT, FOREIGN KEY "
           "(PARENT_GROUP_ID) REFERENCES CHANNEL_GROUPS(GROUP_ID))";
    con << "CREATE TABLE IF NOT EXISTS XSTREAM_SERVERS(SERVER_ID "
           "INTEGER NOT NULL PRIMARY KEY,"
           "HOST TEXT, PORT TEXT, SERVER_URL_SCHEMA TEXT, USERNAME "
           "TEXT, PASSWORD TEXT, TIMEZONE TEXT)";
    con << "CREATE TABLE IF NOT EXISTS HTTP_PROXY(HOST TEXT, PORT "
           "INT, USE BOOLEAN)";

    con << "CREATE TABLE IF NOT EXISTS CHANNELS(CHANNEL_ID INTEGER NOT NULL "
           "PRIMARY KEY, "
           "GROUP_ID INTEGER, NAME TEXT, URI TEXT, LOGO_URI TEXT, LOGO BLOB, "
           "FAVOURITE BOOLEAN DEFAULT 0 NOT NULL CHECK (FAVOURITE IN (0, 1)),"
           "EPG_CHANNEL_URI TEXT, EPG_CHANNEL_ID TEXT,XSTREAM_SERVER_ID INT,"
           "FOREIGN KEY (GROUP_ID) REFERENCES CHANNEL_GROUPS(GROUP_ID),"
           "FOREIGN KEY (XSTREAM_SERVER_ID) REFERENCES "
           "XSTREAM_SERVERS(SERVER_ID))";

    con << "CREATE TABLE IF NOT EXISTS EPG_PROGRAMMES(XSTREAM_SERVER_ID INT,"
           "EPG_CHANNEL_ID TEXT, START_TIME INT, STOP_TIME INT, TITLE TEXT,"
           "DESCRIPTION TEXT, FOREIGN KEY (XSTREAM_SERVER_ID) REFERENCES "
           "XSTREAM_SERVERS(SERVER_ID))";
    con << "CREATE INDEX IF NOT EXISTS IDX_EPG_LOOKUP ON "
           "EPG_PROGRAMMES(XSTREAM_SERVER_ID, EPG_CHANNEL_ID, START_TIME)";

    // Display names for every channel the XMLTV guide describes, including ones
    // we don't have in CHANNELS. Lets EPG search name a match by its channel
    // even when that channel isn't in the user's list.
    con << "CREATE TABLE IF NOT EXISTS EPG_CHANNELS(XSTREAM_SERVER_ID INT,"
           "EPG_CHANNEL_ID TEXT, DISPLAY_NAME TEXT, FOREIGN KEY "
           "(XSTREAM_SERVER_ID) REFERENCES XSTREAM_SERVERS(SERVER_ID))";
    con << "CREATE INDEX IF NOT EXISTS IDX_EPG_CHANNELS ON "
           "EPG_CHANNELS(XSTREAM_SERVER_ID, EPG_CHANNEL_ID)";

    con << "CREATE TABLE IF NOT EXISTS SETTINGS(KEY TEXT PRIMARY KEY, VALUE "
           "TEXT)";
    int version = getSchemaVersion(con);
    if (version < 1)
    {
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN STATUS TEXT";
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN EXPIRY_DATE INT";
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN MAX_CONNECTIONS INT";
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN CREATED_AT INT";
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN RTMP_PORT TEXT";
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN HTTPS_PORT TEXT";
        con << "CREATE TABLE IF NOT EXISTS SERVER_OUTPUT_FORMATS "
               "(XSTREAM_SERVER_ID INT, FORMAT TEXT, FOREIGN KEY "
               "(XSTREAM_SERVER_ID) REFERENCES "
               "XSTREAM_SERVERS(SERVER_ID))";
        incrementSchemaVersion(con, version);
    }
    if (version < 2)
    {
        con << "ALTER TABLE XSTREAM_SERVERS ADD COLUMN XMLTV_UPDATED_AT TEXT";
        incrementSchemaVersion(con, version);
    }
}
int DatabaseConnections::getSchemaVersion(soci::session& con)
{
    int version = 0;
    soci::indicator ind;
    con << "SELECT VERSION FROM SCHEMA_VERSION LIMIT 1", soci::into(version, ind);
    if (ind != soci::i_ok)
    {
        return 0;
    }
    return version;
}
void DatabaseConnections::incrementSchemaVersion(soci::session& con, int version)
{
    if (version == 0)
    {
        con << "INSERT INTO SCHEMA_VERSION (VERSION) VALUES(1)";
    }
    else
    {
        con << "UPDATE SCHEMA_VERSION SET VERSION=:ver",
            soci::use(version + 1, "ver");
    }
}
