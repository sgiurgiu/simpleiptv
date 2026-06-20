#pragma once

#include <soci/session.h>

class DatabaseConnections
{
public:
    static void Initialize();
    static soci::session GetConnection();

private:
    static void initTables();
    static int getSchemaVersion(soci::session& con);
    static void incrementSchemaVersion(soci::session& con, int& version);
};