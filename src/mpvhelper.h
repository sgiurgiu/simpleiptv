#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

#include <mpv/client.h>

using NodeVariant = std::variant<std::string, bool, int64_t, int, double>;
