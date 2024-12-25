#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <memory>

#ifdef STV_UNIX
#include <sdbus-c++/sdbus-c++.h>
#endif

class SleepService : public std::enable_shared_from_this<SleepService>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    SleepService(Key, const boost::asio::any_io_executor& executor);
    static std::shared_ptr<SleepService>
    Create(const boost::asio::any_io_executor& executor);
    void disableComputerSleep();
    void enableComputerSleep();

private:
    void setComputerSleep(bool flag);

private:
    boost::asio::any_io_executor executor;
    uint32_t screenSaverDBusCookie = 0;
#ifdef STV_UNIX
    std::unique_ptr<sdbus::IConnection> sessionConnection;
#endif
};