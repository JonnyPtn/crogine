/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "Server.hpp"
#include "PacketIDs.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/core/Clock.hpp>

#include <functional>

Server::Server()
    : m_running(false)
{

}

Server::~Server()
{
    if (m_running)
    {
        stop();
    }
}

//public
void Server::launch()
{
    //stop any existing instance first
    stop();

    m_running = true;
    m_thread = std::make_unique<std::thread>(&Server::run, this);
}

void Server::stop()
{
    if (m_thread)
    {
        m_running = false;
        m_thread->join();

        m_thread.reset();
    }
}

//private
void Server::run()
{
    if (!m_host.start("", ConstVal::GamePort, ConstVal::MaxClients, 4))
    {
        m_running = false;
        cro::Logger::log("Failed to start host service", cro::Logger::Type::Error);
        return;
    }    
    
    LOG("Server launched", cro::Logger::Type::Info);

    m_currentState = std::make_unique<Sv::LobbyState>();
    std::int32_t nextState = m_currentState->stateID();

    const cro::Time frameTime = cro::milliseconds(50);
    cro::Clock frameClock;
    cro::Time accumulatedTime;

    while (m_running)
    {
        cro::NetEvent evt;
        while(m_host.pollEvent(evt))
        {
            m_currentState->netUpdate(evt);
        
            //handle connects / disconnects
            if (evt.type == cro::NetEvent::ClientConnect)
            {
                //refuse if not in lobby state
                //else add to client list
                if (m_currentState->stateID() == Sv::StateID::Lobby)
                {
                    if (!addClient(evt))
                    {
                        //tell client server is full
                        m_host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::ServerFull), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                }
                else
                {
                    //send rejection packet
                    m_host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::NotInLobby), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            }
            else if (evt.type == cro::NetEvent::ClientDisconnect)
            {
                //remove from client list
                removeClient(evt);
            }
            else if (evt.type == cro::NetEvent::PacketReceived)
            {
                switch (evt.packet.getID())
                {
                default: break;
                case PacketID::RequestGameStart:
                    if (m_currentState->stateID() == Sv::StateID::Lobby)
                    {
                        //TODO assert sender is host
                        m_currentState = std::make_unique<Sv::GameState>();
                        nextState = Sv::StateID::Game;

                        m_host.broadcastPacket(PacketID::StateChange, std::uint8_t(nextState), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                    break;
                }
            }
        }

        accumulatedTime += frameClock.restart();
        while (accumulatedTime > frameTime)
        {
            accumulatedTime -= frameTime;
            nextState = m_currentState->process(frameTime.asSeconds());
        }

        if (nextState != m_currentState->stateID())
        {
            switch (nextState)
            {
            default: m_running = false; break;
            case Sv::StateID::Game:
                m_currentState = std::make_unique<Sv::GameState>();
                break;
            case Sv::StateID::Lobby:
                m_currentState = std::make_unique<Sv::LobbyState>();
                break;
            }

            //mitigate large DT which may have built up while new state was loading.
            frameClock.restart();
        }
    }

    m_currentState.reset();
    //TODO clear all client data
    //TODO force disconnect clients
    m_host.stop();

    LOG("Server quit", cro::Logger::Type::Info);
}

bool Server::addClient(const cro::NetEvent& evt)
{
    auto i = 0u;
    for (i; i < m_clients.size(); ++i)
    {
        if (!m_clients[i].connected)
        {
            LOG("Added client to server with id " + std::to_string(evt.peer.getID()), cro::Logger::Type::Info);

            m_clients[i].connected = true;
            //m_clients[i].id;
            m_clients[i].peer = evt.peer;

            //broadcast to all connected clients
            //so they can update lobby view.
            m_host.broadcastPacket(PacketID::ClientConnected, evt.peer.getID(), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

            break;
        }
    }

    return (i != m_clients.size());
}

void Server::removeClient(const cro::NetEvent& evt)
{
    LOG("Check this event has valid client ID", cro::Logger::Type::Info);

    auto result = std::find_if(m_clients.begin(), m_clients.end(), 
        [&evt](const Sv::ClientConnection& c) 
        {
            return c.peer == evt.peer;
        });

    if (result != m_clients.end())
    {
        result->connected = false;
        result->peer = {};

        //broadcast to all connected clients
        m_host.broadcastPacket(PacketID::ClientDisconnected, evt.peer.getID(), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}