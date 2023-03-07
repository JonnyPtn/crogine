/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#include "CameraFollowSystem.hpp"
#include "InputParser.hpp"
#include "HoleData.hpp"
#include "Billboard.hpp"
#include "GameConsts.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/UniformBuffer.hpp>

#include <crogine/detail/glm/vec2.hpp>

//callback data for anim/self destruction
//of messages / options window
struct MessageAnim final
{
    enum
    {
        Delay, Open, Hold, Close
    }state = Delay;
    float currentTime = 0.5f;
};

struct SharedStateData;
class DrivingState final : public cro::State, public cro::GuiClient
{
public:
    DrivingState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::DrivingRange; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

    struct MenuID final
    {
        enum
        {
            Dummy,
            Options,
            Summary,
            Leaderboard,

            Count
        };
    };


private:
    SharedStateData& m_sharedData;
    InputParser m_inputParser;

    cro::Scene m_gameScene;
    cro::Scene m_skyScene;
    cro::Scene m_uiScene;
    glm::vec2 m_viewScale;

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_backgroundTexture;
    cro::CubemapTexture m_reflectionMap;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;

    cro::Clock m_idleTimer;

    struct WindUpdate final
    {
        float currentWindSpeed = 0.f;
        glm::vec3 currentWindVector = glm::vec3(0.f);
    }m_windUpdate;


    struct MaterialID final
    {
        enum
        {
            Billboard,
            Cel,
            CelTextured,
            CelTexturedSkinned,
            Course,
            Hair,
            Wireframe,
            WireframeCulled,
            Beacon,
            Horizon,
            Trophy,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    std::vector<HoleData> m_holeData;
    std::int32_t m_targetIndex;
    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    struct SpriteID final
    {
        enum
        {
            PowerBar,
            PowerBarInner,
            HookBar,
            WindIndicator,
            WindSpeed,
            MessageBoard,
            Stars,
            SpinBg,
            SpinFg,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    Avatar m_avatar;

    struct ClubModel final
    {
        enum {Wood, Iron, Count};
    };
    std::array<cro::Entity, ClubModel::Count> m_clubModels = {};

    std::array<std::int32_t, 3u> m_strokeCounts = { 5,9,18 };
    std::size_t m_strokeCountIndex;

    std::array<cro::Entity, CameraID::Count> m_cameras = {};
    std::int32_t m_currentCamera;

    cro::Entity m_defaultCam;
    cro::Entity m_freeCam;
    void toggleFreeCam();

    void addSystems();
    void loadAssets();
    void initAudio();
    void createScene();
    void createFoliage(cro::Entity);
    void createClouds();
    void createPlayer(cro::Entity);
    void createBall();
    void createFlag();
    void startTransition();

    void hitBall();
    void setHole(std::int32_t);
    void setActiveCamera(std::int32_t);
    
    //DrivingStateUI.cpp
#ifdef USE_GNS
    cro::Entity m_leaderboardEntity;
#endif
    cro::RenderTexture m_mapTexture;
    cro::Entity m_mapCam;
    cro::SimpleQuad m_flagQuad;
    void createUI();
    void createSwingMeter(cro::Entity);
    void createGameOptions();
    void createSummary();
    void updateMinimap();
    void updateWindDisplay(glm::vec3);    
    void showMessage(float);
    void floatingMessage(const std::string&);

    //create the summary screen with its own
    //encapsulation just to update the text more easily
    struct SummaryScreen final
    {
        cro::Entity root;
        cro::Entity fadeEnt;
        cro::Entity text01;
        cro::Entity text02;
        cro::Entity summary;
        cro::Entity bestMessage;
        std::array<cro::Entity, 3u> stars = {};
        cro::Entity audioEnt;
    }m_summaryScreen;
    static constexpr float FadeDepth = 1.f;

    std::array<float, 3u> m_topScores = {};
    void loadScores();
    void saveScores();

    cro::Clock m_statClock;

#ifdef CRO_DEBUG_
    cro::Texture m_debugHeightmap;
#endif
};