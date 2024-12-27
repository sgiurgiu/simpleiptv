#include "boost_error_code_converter.h"

#include <boost/system/error_code.hpp>

#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>

namespace
{
// This class passes the std::error_category functions through to the
// boost::system::error_category object.
class BoostErrorCategoryShim : public std::error_category
{
public:
    BoostErrorCategoryShim(const boost::system::error_category& in_boostErrorCategory)
    : m_boostErrorCategory(in_boostErrorCategory)
    , m_name(std::string("boost.") + in_boostErrorCategory.name())
    {
    }

    virtual const char* name() const noexcept override;
    virtual std::string message(int in_errorValue) const override;
    virtual std::error_condition
    default_error_condition(int in_errorValue) const noexcept override;

private:
    // The target boost error category.
    const boost::system::error_category& m_boostErrorCategory;

    // The modified name of the error category.
    const std::string m_name;
};

// A converter class that maintains a mapping between a
// boost::system::error_category and a std::error_category.
class BoostErrorCodeConverter
{
public:
    const std::error_category&
    GetErrorCategory(const boost::system::error_category& in_boostErrorCategory)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if we already have an entry for this error category, if so we
        // return it directly.
        ConversionMapType::iterator stdErrorCategoryIt =
            m_conversionMap.find(&in_boostErrorCategory);
        if (stdErrorCategoryIt != m_conversionMap.end())
            return *stdErrorCategoryIt->second;

        // We don't have an entry for this error category, create one and add it
        // to the map.
        const std::pair<ConversionMapType::iterator, bool> insertResult =
            m_conversionMap.insert(ConversionMapType::value_type(
                &in_boostErrorCategory,
                std::unique_ptr<const BoostErrorCategoryShim>(
                    new BoostErrorCategoryShim(in_boostErrorCategory))));

        // Return the newly created category.
        return *insertResult.first->second;
    }

private:
    // We keep a mapping of boost::system::error_category to our error category
    // shims.  The error categories are implemented as singletons so there
    // should be relatively few of these.
    typedef std::unordered_map<const boost::system::error_category*,
                               std::unique_ptr<const BoostErrorCategoryShim>>
        ConversionMapType;
    ConversionMapType m_conversionMap;

    // This is accessed globally so we must manage access.
    std::mutex m_mutex;
};

namespace Private
{
// The init flag.
std::once_flag g_onceFlag;

// The pointer to the converter, set in CreateOnce.
BoostErrorCodeConverter* g_converter = nullptr;

// Create the log target manager.
void CreateBoostErrorCodeConverterOnce()
{
    static BoostErrorCodeConverter converter;
    g_converter = &converter;
}
} // namespace Private

// Get the log target manager.
BoostErrorCodeConverter& GetBoostErrorCodeConverter()
{
    std::call_once(Private::g_onceFlag,
                   &Private::CreateBoostErrorCodeConverterOnce);

    return *Private::g_converter;
}

const std::error_category&
GetConvertedErrorCategory(const boost::system::error_category& in_errorCategory)
{
    // If we're accessing boost::system::generic_category() or
    // boost::system::system_category() then just convert to the std::error_code
    // versions.
    if (in_errorCategory == boost::system::generic_category())
        return std::generic_category();

    // I thought this should work, but at least in VC++10 std::error_category
    // interprets the errors as generic instead of system errors.  This means an
    // error returned by GetLastError() like 5 (access denied) gets interpreted
    // incorrectly as IO error.
    // if( in_errorCategory == boost::system::system_category() )
    //  return std::system_category();

    // The error_category was not one of the standard boost error categories,
    // use a converter.
    return GetBoostErrorCodeConverter().GetErrorCategory(in_errorCategory);
}

// BoostErrorCategoryShim implementation.
const char* BoostErrorCategoryShim::name() const noexcept
{
    return m_name.c_str();
}

std::string BoostErrorCategoryShim::message(int in_errorValue) const
{
    return m_boostErrorCategory.message(in_errorValue);
}

std::error_condition
BoostErrorCategoryShim::default_error_condition(int in_errorValue) const noexcept
{
    const boost::system::error_condition boostErrorCondition =
        m_boostErrorCategory.default_error_condition(in_errorValue);

    // We have to convert the error category here since it may not have the same
    // category as in_errorValue.
    return std::error_condition(
        boostErrorCondition.value(),
        GetConvertedErrorCategory(boostErrorCondition.category()));
}
} // namespace

std::error_code BoostToErrorCode(boost::system::error_code in_errorCode)
{
    return std::error_code(in_errorCode.value(),
                           GetConvertedErrorCategory(in_errorCode.category()));
}
