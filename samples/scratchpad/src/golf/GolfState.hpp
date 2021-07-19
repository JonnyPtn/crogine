/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#pragma once

#include "../StateIDs.hpp"
#include "HoleData.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/core/State.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Image.hpp>

#include <array>

namespace cro
{
    struct NetEvent;
}

class GolfState final : public cro::State, public cro::GuiClient
{
public:
    GolfState(cro::StateStack&, cro::State::Context, struct SharedStateData&);

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return States::Golf::Game; }

private:
    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    bool m_wantsGameState;
    cro::Clock m_readyClock; //pings ready state until ack'd

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_renderTexture;

    std::vector<HoleData> m_holeData;
    std::uint32_t m_currentHole;
    ActivePlayer m_currentPlayer;

    struct SpriteID final
    {
        enum
        {
            Flag01,
            Flag02,
            Flag03,
            Flag04,
            PowerBar,
            WindIndicator,
            Player01,
            Player02,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    struct FontID final
    {
        enum
        {
            UI,

            Count
        };
    };

    struct BallResource final
    {
        std::int32_t materialID = -1;
        std::size_t ballMeshID = 0;
        std::size_t shadowMeshID = 0;
    }m_ballResources;

    void loadAssets();
    void addSystems();
    void buildScene();
    void buildUI();

    void spawnBall(const struct ActorInfo&);

    void handleNetEvent(const cro::NetEvent&);
    void removeClient(std::uint8_t);

    void setCurrentHole(std::uint32_t);
    void setCameraPosition(glm::vec3);
    void setCurrentPlayer(const ActivePlayer&);
    void hitBall();

#ifdef CRO_DEBUG_
    cro::Entity m_debugCam;
    cro::RenderTexture m_debugTexture;
#endif
};