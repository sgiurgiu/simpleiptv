#include "channel.h"

Channel::Channel(int id, std::string name) : id{ id }, name{ std::move(name) }
{
}
