#pragma once

#include <boost/url/url.hpp>
#include <string>

boost::urls::url sanitize_uri(const std::string& s);