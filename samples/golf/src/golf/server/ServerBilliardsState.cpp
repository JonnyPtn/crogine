/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "../PacketIDs.hpp"
#include "ServerBilliardsState.hpp"
#include "EightballDirector.hpp"
#include "ServerMessages.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Network.hpp>

using namespace sv;

namespace
{
    const cro::Time TurnTime = cro::seconds(20.f);
}

BilliardsState::BilliardsState(SharedData& sd)
    : m_returnValue     (StateID::Billiards),
    m_sharedData        (sd),
    m_scene             (sd.messageBus, 512),
    m_tableDataValid    (false),
    m_gameStarted       (false),
    m_allMapsLoaded     (false),
    m_activeDirector    (nullptr)
{
    if (m_tableDataValid = validateData(); m_tableDataValid)
    {
        initScene();
        buildWorld();
    }
    LOG("Entered Billiards state", cro::Logger::Type::Info);
}

//public
void BilliardsState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::SceneMessage)
    {
        const auto& data = msg.getData<cro::Message::SceneEvent>();
        if (data.event == cro::Message::SceneEvent::EntityDestroyed)
        {
            m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, data.entityID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
    else if (msg.id == MessageID::BilliardsMessage)
    {
        const auto& data = msg.getData<BilliardsEvent>();

        switch (data.type)
        {
        default: break;
        case BilliardsEvent::PlayerSwitched:
            setNextPlayer(true);
            break;
        case BilliardsEvent::GameEnded:
            auto winner = m_playerInfo[m_activeDirector->getCurrentPlayer()];
            endGame(winner);
            break;
        }        
    }
    else if (msg.id == sv::MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            m_playerInfo.erase(std::remove_if(m_playerInfo.begin(), m_playerInfo.end(),
                [data](const BilliardsPlayer& p)
                {
                    return p.client == data.clientID;
                }),
                m_playerInfo.end());

            //default win the game to remaining player
            for (const auto& player : m_playerInfo)
            {
                if (player.client != data.clientID)
                {
                    endGame(player);
                }
            }

            checkReadyQuit(10); //doesn't matter which client number, just update status...
        }
    }

    m_scene.forwardMessage(msg);
}

void BilliardsState::netEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::CueUpdate:
            //just forward this to other client
            m_sharedData.host.broadcastPacket(PacketID::CueUpdate, evt.packet.as<BilliardsUpdate>(), cro::NetFlag::Unreliable);
            break;
        case PacketID::BallPlaced:
        if (!m_scene.getSystem<BilliardsSystem>()->hasCueball())
        {
            auto data = evt.packet.as<BilliardBallInput>();
            auto currentPlayer = m_activeDirector->getCurrentPlayer();

            if (data.client == m_playerInfo[currentPlayer].client
                && data.player == m_playerInfo[currentPlayer].player)
            {
                spawnBall(addBall(data.offset, CueBall));
            }
        }
            break;
        case PacketID::InputUpdate:
        {
            auto data = evt.packet.as<BilliardBallInput>();
            auto currentPlayer = m_activeDirector->getCurrentPlayer();

            if (data.client == m_playerInfo[currentPlayer].client
                && data.player == m_playerInfo[currentPlayer].player)
            {
                m_scene.getSystem<BilliardsSystem>()->applyImpulse(data.impulse, data.offset);
            }
        }
            break;
        case PacketID::ClientReady:
            if (!m_sharedData.clients[evt.packet.as<std::uint8_t>()].ready)
            {
                sendInitialGameState(evt.packet.as<std::uint8_t>());
            }
            break;
        case PacketID::ServerCommand:
            if (evt.peer.getID() == m_sharedData.hostID)
            {
                doServerCommand(evt);
            }
            break;
        case PacketID::TransitionComplete:
        {
            auto clientID = evt.packet.as<std::uint8_t>();
            m_sharedData.clients[clientID].mapLoaded = true;
        }
            break;
        case PacketID::ReadyQuit:
            checkReadyQuit(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::TurnReady:
            if (evt.packet.as<std::uint8_t>() == m_playerInfo[m_activeDirector->getCurrentPlayer()].client)
            {
                setNextPlayer(false);
            }
            //rebroadcast to tell clients to clear their UI
            m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::TurnReady, std::uint8_t(255), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            break;
        }
    }
}

void BilliardsState::netBroadcast()
{
    if (m_tableDataValid)
    {
        auto timestamp = m_serverTime.elapsed().asMilliseconds();

        auto& balls = m_scene.getSystem<BilliardsSystem>()->getEntities();
        for (auto entity : balls)
        {
            auto& ball = entity.getComponent<BilliardBall>();
            if (ball.hadUpdate) //sending all updates, even non-moving ones, provides smoother client side interpolation (??)
            {
                BilliardsUpdate info;
                info.position = cro::Util::Net::compressVec3(entity.getComponent<cro::Transform>().getPosition(), ConstVal::PositionCompressionRange);
                info.rotation = cro::Util::Net::compressQuat(entity.getComponent<cro::Transform>().getRotation());
                info.serverID = entity.getIndex();
                info.timestamp = timestamp;

                m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, cro::NetFlag::Unreliable);

                ball.hadUpdate = false;
            }
        }
    }
}

std::int32_t BilliardsState::process(float dt)
{
    if (m_gameStarted)
    {
        if (m_turnTimer.elapsed() < TurnTime)
        {
            //AFK'd
        }

        //we have to keep checking this as a client might
        //disconnect mid-transition and the final 'complete'
        //packet will never arrive.
        if (!m_allMapsLoaded)
        {
            bool allReady = true;
            for (const auto& client : m_sharedData.clients)
            {
                if (client.connected && !client.mapLoaded)
                {
                    allReady = false;
                }
            }
            m_allMapsLoaded = allReady;
        }
    }

    m_scene.simulate(dt);
    return m_returnValue;
}

//private
bool BilliardsState::validateData()
{
    auto path = "assets/golf/tables/" + m_sharedData.mapDir + ".table";
    if (m_tableData.loadFromFile(path))
    {
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + m_tableData.collisionModel))
        {
            return false;
        }
        return true;
    }
    return false;
}

void BilliardsState::initScene()
{
    auto& mb = m_sharedData.messageBus;
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<BilliardsSystem>(mb);

    //add director based on rule set
    switch (m_tableData.rules)
    {
    default: break;
    case TableData::Eightball:
        m_activeDirector = m_scene.addDirector<EightballDirector>();
        break;
    }

    //place active players into a more convenient container..
    for (auto i = 0u; i < m_sharedData.clients.size(); ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            for (auto j = 0u; j < m_sharedData.clients[i].playerCount && m_playerInfo.size() < 2u; ++j)
            {
                auto& player = m_playerInfo.emplace_back();
                player.client = i;
                player.player = j;
            }
        }
    }
}

void BilliardsState::buildWorld()
{
    m_scene.getSystem<BilliardsSystem>()->initTable(m_tableData);

    if (m_activeDirector)
    {
        for (const auto& ball : m_activeDirector->getBallLayout())
        {
            addBall(ball.position, ball.id);
        }
    }
}

void BilliardsState::sendInitialGameState(std::uint8_t clientID)
{
    //only send data the first time
    if (!m_sharedData.clients[clientID].ready)
    {
        if (!m_tableDataValid)
        {
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), cro::NetFlag::Reliable);
            return;
        }

        //send all the ball positions
        auto timestamp = m_serverTime.elapsed().asMilliseconds();

        auto& balls = m_scene.getSystem<BilliardsSystem>()->getEntities();
        for (auto entity : balls)
        {
            auto& ball = entity.getComponent<BilliardBall>();

            ActorInfo info;
            info.position = entity.getComponent<cro::Transform>().getPosition();
            info.rotation = cro::Util::Net::compressQuat(entity.getComponent<cro::Transform>().getRotation());
            info.serverID = entity.getIndex();
            info.timestamp = timestamp;
            info.state = ball.id;

            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ActorSpawn, info, cro::NetFlag::Reliable);
        }

        //and the table info such as the cueball spawn
        TableInfo info;
        info.cueballPosition = m_activeDirector->getCueballPosition();
        m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::TableInfo, info, cro::NetFlag::Reliable);
    }

    m_sharedData.clients[clientID].ready = true;

    bool allReady = true;
    for (const auto& client : m_sharedData.clients)
    {
        if (client.connected && !client.ready)
        {
            allReady = false;
        }
    }

    if (allReady && !m_gameStarted)
    {
        //a guard to make sure this is sent just once
        m_gameStarted = true;

        //send command to set clients to first player
        //this also tells the client to stop requesting state

        //create an ent which waits for the clients to
        //finish loading
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            if (m_allMapsLoaded)
            {
                setNextPlayer(true);
                e.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(e);
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };
    }
}

void BilliardsState::doServerCommand(const cro::NetEvent& evt)
{
#ifdef CRO_DEBUG_
    switch (evt.packet.as<std::uint8_t>())
    {
    default: break;
    case ServerCommand::SpawnBall:
        spawnBall(addBall({ cro::Util::Random::value(-0.1f, 0.1f), 0.5f, cro::Util::Random::value(-0.1f, 0.1f) }, cro::Util::Random::value(0, 15)));
        break;
    case ServerCommand::StrikeBall:
        m_scene.getSystem<BilliardsSystem>()->applyImpulse({0.f, 0.f, -0.7f}, {0.f, 0.f, 0.f});
        break;
    }
#endif
}

void BilliardsState::setNextPlayer(bool waitForAck)
{
    auto packetID = waitForAck ? PacketID::NotifyPlayer : PacketID::SetPlayer;

    auto info = m_playerInfo[m_activeDirector->getCurrentPlayer()];
    info.targetID = m_activeDirector->getTargetID();
    m_sharedData.host.broadcastPacket(packetID, info, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_turnTimer.restart();
}

cro::Entity BilliardsState::addBall(glm::vec3 position, std::uint8_t id)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<BilliardBall>().id = id;
    
    if (id == CueBall)
    {
        m_activeDirector->setCueball(entity);
    }

    return entity;
}

void BilliardsState::spawnBall(cro::Entity entity)
{
    /*auto position = entity.getComponent<cro::Transform>().getPosition();
    auto id = entity.getComponent<BilliardBall>().id;

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, position, id](cro::Entity e, float)
    {
        if (e.getComponent<cro::Transform>().getPosition().y < -1.f)
        {
            m_scene.destroyEntity(e);

            spawnBall(addBall(position, id));
        }
    };*/

    //notify client
    ActorInfo info;
    info.serverID = entity.getIndex();
    info.position = entity.getComponent<cro::Transform>().getPosition();
    info.rotation = cro::Util::Net::compressQuat(entity.getComponent<cro::Transform>().getRotation());
    info.state = entity.getComponent<BilliardBall>().id;
    info.timestamp = m_serverTime.elapsed().asMilliseconds();

    m_sharedData.host.broadcastPacket(PacketID::ActorSpawn, info, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void BilliardsState::endGame(const BilliardsPlayer& winner)
{
    //create a timer ent which returns to lobby on time out
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(ConstVal::SummaryTimeout);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& remain = e.getComponent<cro::Callback>().getUserData<float>();
        remain -= dt;
        if (remain < 0)
        {
            m_returnValue = StateID::Lobby;
            e.getComponent<cro::Callback>().active = false;
        }
    };

    m_gameStarted = false;

    m_sharedData.host.broadcastPacket(PacketID::GameEnd, winner, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void BilliardsState::checkReadyQuit(std::uint8_t clientID)
{
    if (m_gameStarted)
    {
        return;
    }

    std::uint8_t broadcastFlags = 0;

    for (auto& p : m_playerInfo)
    {
        if (p.client == clientID)
        {
            p.readyQuit = !p.readyQuit;
        }

        if (p.readyQuit)
        {
            broadcastFlags |= (1 << p.client);
        }
        else
        {
            broadcastFlags &= ~(1 << p.client);
        }
    }
    //let clients know to update their display
    m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::ReadyQuitStatus, broadcastFlags, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    for (const auto& p : m_playerInfo)
    {
        if (!p.readyQuit)
        {
            //not everyone is ready
            return;
        }
    }
    //if we made it here it's time to quit!
    m_returnValue = StateID::Lobby;
}