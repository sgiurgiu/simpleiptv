#pragma once

#include <boost/system/error_code.hpp>
#include <system_error>

std::error_code BoostToErrorCode(boost::system::error_code in_errorCode);