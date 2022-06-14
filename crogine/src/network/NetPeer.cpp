/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include "../detail/enet/enet/enet.h"

#include <crogine/network/NetData.hpp>

using namespace cro;

std::string NetPeer::getAddress() const
{
    CRO_ASSERT(m_peer, "Not a valid peer");

    auto bytes = m_peer->address.host;

    std::string ret = std::to_string(bytes & 0x000000FF);
    ret += "." + std::to_string((bytes & 0x0000FF00) >> 8);
    ret += "." + std::to_string((bytes & 0x00FF0000) >> 16);
    ret += "." + std::to_string((bytes & 0xFF000000) >> 24);
    return ret;
}

std::uint16_t NetPeer::getPort() const
{
    CRO_ASSERT(m_peer, "Not a valid peer");

    return m_peer->address.port;
}

std::uint64_t NetPeer::getID() const
{
    CRO_ASSERT(m_peer, "Not a valid peer");

    return m_peer->connectID;
}

std::uint32_t NetPeer::getRoundTripTime()const
{
    CRO_ASSERT(m_peer, "Not a valid peer");

    return m_peer->roundTripTime;
}

NetPeer::State NetPeer::getState() const
{
    if (!m_peer)
    {
        return State::Disconnected;
    }

    switch (m_peer->state)
    {
    case ENET_PEER_STATE_ACKNOWLEDGING_CONNECT:
        return State::AcknowledingConnect;
    case ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT:
        return State::AcknowledingDisconnect;
    case ENET_PEER_STATE_CONNECTED:
        return State::Connected;
    case ENET_PEER_STATE_CONNECTING:
        return State::Connecting;
    case ENET_PEER_STATE_CONNECTION_PENDING:
        return State::PendingConnect;
    case ENET_PEER_STATE_CONNECTION_SUCCEEDED:
        return State::Succeeded;
    case ENET_PEER_STATE_DISCONNECTED:
        return State::Disconnected;
    case ENET_PEER_STATE_DISCONNECTING:
        return State::Disconnecting;
    case ENET_PEER_STATE_DISCONNECT_LATER:
        return State::DisconnectLater;
    case ENET_PEER_STATE_ZOMBIE:
        return State::Zombie;
    }
    return State::Zombie;
}