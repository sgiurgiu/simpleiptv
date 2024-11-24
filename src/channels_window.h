#pragma once

#include <boost/asio/any_io_executor.hpp>

class ChannelsWindow
{
public:
    ChannelsWindow(const boost::asio::any_io_executor& ui_executor);
    void showWindow();

private:
    void loadChannels();

private:
    const boost::asio::any_io_executor& ui_executor;
};