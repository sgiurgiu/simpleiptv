#pragma once

#include <soci/session.h>

class DatabaseConnections
{
public:
    static void Initialize();
    static soci::session GetConnection();

private:
    static void initTables();
};