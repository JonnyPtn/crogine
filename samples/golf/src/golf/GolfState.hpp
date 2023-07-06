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
#include "../sqlite/ProfileDB.hpp"
#include "HoleData.hpp"
#include "GameConsts.hpp"
#include "InputParser.hpp"
#include "TerrainBuilder.hpp"
#include "CameraFollowSystem.hpp"
#include "CollisionMesh.hpp"
#include "LeaderboardTexture.hpp"
#include "CPUGolfer.hpp"
#include "TerrainDepthmap.hpp"
#include "TerrainChunks.hpp"
#include "MinimapZoom.hpp"
#include "BallTrail.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/core/HiResTimer.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/CubemapTexture.hpp>

#include <array>
#include <future>
#include <unordered_map>

#ifdef CRO_DEBUG_
//#define PATH_TRACING
#endif


namespace cro
{
    struct NetEvent;
}

//sprite which carries green overhead view
struct GreenCallbackData final
{
    float currTime = 0.f;
    float state = 0;
    float targetScale = 1.f;
};

//callback data for green overhead view
struct MiniCamData final
{
    static constexpr float MaxSize = 2.25f;
    static constexpr float MinSize = 0.75f;
    float targetSize = MaxSize;
    float currentSize = MinSize;
};

//used in player sprite animation
struct PlayerCallbackData final
{
    std::int32_t direction = 0; //grow or shrink
    float scale = 0.f;
};

class GolfState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    GolfState(cro::StateStack&, cro::State::Context, struct SharedStateData&);

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Golf; }

private:
    cro::ResourceCollection m_resources;    
    
    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_skyScene;
    cro::Scene m_uiScene;
    cro::Scene m_trophyScene;
    TerrainDepthmap m_depthMap;

    InputParser m_inputParser;
    CPUGolfer m_cpuGolfer;
    cro::Clock m_turnTimer;

    cro::Clock m_idleTimer;

    bool m_wantsGameState;
    bool m_allowAchievements;
    cro::Clock m_readyClock; //pings ready state until ack'd

    cro::RenderTexture m_gameSceneTexture;
    cro::RenderTexture m_trophySceneTexture;
    cro::CubemapTexture m_reflectionMap;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;

    struct WindUpdate final
    {
        float currentWindSpeed = 0.f;
        glm::vec3 currentWindVector = glm::vec3(0.f);
        glm::vec3 windVector = glm::vec3(0.f);
    }m_windUpdate;

    struct ResolutionUpdate final
    {
        ResolutionData resolutionData;
        float targetFade = 2.f;
    }m_resolutionUpdate;

    struct PostID final
    {
        enum
        {
            Noise, Fog,
            Count
        };
    };
    std::array<PostProcess, PostID::Count> m_postProcesses = {};

    cro::Image m_currentMap; 
    float m_holeToModelRatio;
    std::vector<HoleData> m_holeData;
    std::uint32_t m_currentHole;
    float m_distanceToHole;
    ActivePlayer m_currentPlayer;
    CollisionMesh m_collisionMesh;
    TerrainChunker m_terrainChunker;

    TerrainBuilder m_terrainBuilder;

    struct MaterialID final
    {
        enum
        {
            WireFrame,
            WireFrameCulled,
            Water,
            Horizon,
            Cel,
            CelSkinned,
            CelTextured,
            CelTexturedSkinned,
            ShadowMap,
            ShadowMapInstanced,
            ShadowMapSkinned,
            Leaderboard,
            Player,
            Hair,
            Course,
            Ball,
            Billboard,
            Trophy,
            Beacon,
            BallTrail,
            Flag,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    struct ModelID final
    {
        enum
        {
            BallShadow,
            PlayerShadow,

            Count
        };
    };
    std::array<std::unique_ptr<cro::ModelDefinition>, ModelID::Count> m_modelDefs = {};
    std::unordered_map<std::int32_t, std::unique_ptr<cro::ModelDefinition>> m_ballModels;

    struct BallResource final
    {
        std::int32_t materialID = -1;
        std::size_t ballMeshID = 0;
        std::size_t shadowMeshID = 0;
    }m_ballResources;
    BallTrail m_ballTrail;

    std::string m_audioPath;
    cro::String m_courseTitle;

    std::vector<cro::Entity> m_spectatorModels;

    void loadAssets();
    void loadSpectators();
    void addSystems();
    void buildScene();
    void initAudio(bool loadTrees);

    struct WeatherType final
    {
        enum
        {
            Snow, Rain
        };
    };
    void createWeather(std::int32_t); //weather.cpp
    void createClouds();
    void buildBow();
    void createDrone();
    void spawnBall(const struct ActorInfo&);

    void handleNetEvent(const net::NetEvent&);
    void removeClient(std::uint8_t);

    void setCurrentHole(std::uint16_t); //(number << 8) | par
    void setCameraPosition(glm::vec3, float, float);
    void requestNextPlayer(const ActivePlayer&);
    void setCurrentPlayer(const ActivePlayer&);
    void predictBall(float);
    void hitBall();
    void updateActor(const ActorInfo&);

    void createTransition(const ActivePlayer&);
    void startFlyBy();
    std::int32_t getClub() const;

    //allows switching camera, TV style
    std::array<cro::Entity, CameraID::Count> m_cameras = {};
    std::int32_t m_currentCamera;
    void setActiveCamera(std::int32_t);
    void updateCameraHeight(float);
    void setGreenCamPosition();

    cro::Entity m_drone;
    cro::Entity m_defaultCam;
    cro::Entity m_freeCam;
    bool m_photoMode;
    bool m_restoreInput;
    void toggleFreeCam();
    void applyShadowQuality();

    //UI stuffs - found in GolfStateUI.cpp
    struct SpriteID final
    {
        enum
        {
            PowerBar,
            PowerBarInner,
            HookBar,
            SlopeStrength,
            BallSpeed,
            MiniFlag,
            MapFlag,
            WindIndicator,
            WindSpeed,
            WindSpeedBg,
            Thinking,
            MessageBoard,
            Bunker,
            Foul,
            QuitReady,
            QuitNotReady,
            EmoteHappy,
            EmoteGrumpy,
            EmoteLaugh,
            EmoteSad,
            EmotePulse,
            SpinBg,
            SpinFg,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    std::array<std::array<Avatar, ConstVal::MaxPlayers>, ConstVal::MaxClients> m_avatars;
    Avatar* m_activeAvatar;

    struct ClubModel final
    {
        enum { Wood, Iron, Count };
    };
    std::array<cro::Entity, ClubModel::Count> m_clubModels = {};

    float m_camRotation; //used to offset the rotation of the wind indicator
    bool m_roundEnded;
    bool m_newHole; //prevents closing scoreboard until everyone is ready
    glm::vec2 m_viewScale;
    std::size_t m_scoreColumnCount;
    LeaderboardTexture m_leaderboardTexture;

    cro::Entity m_courseEnt;
    cro::Entity m_waterEnt;
    cro::Entity m_minimapEnt;
    cro::Entity m_miniGreenEnt;
    cro::Entity m_miniGreenIndicatorEnt;
    cro::Entity m_scoreboardEnt;
    std::uint8_t m_readyQuitFlags;

    void buildUI();
    void createSwingMeter(cro::Entity);
    void showCountdown(std::uint8_t);
    void createScoreboard();
    void updateScoreboard(bool updatePardiff = true);
    void logCSV() const;
    void showScoreboard(bool visible);
    void updateWindDisplay(glm::vec3);
    float estimatePuttPower();

    enum class MessageBoardID
    {
        Bunker, Scrub, Water,
        PlayerName, HoleScore,
        Gimme
    };
    void showMessageBoard(MessageBoardID, bool special = false);
    void floatingMessage(const std::string&);
    void createTransition();
    void notifyAchievement(const std::array<std::uint8_t, 2u>&);
    void showNotification(const cro::String&);
    void showLevelUp(std::uint64_t);
    void toggleQuitReady();
    
    SkipState m_skipState;
    void updateSkipMessage(float);
    void refreshUI();

    //hack to allow the profile update to be const.
    std::int32_t m_courseIndex; //-1 if not an official course
    mutable std::array<std::array<PersonalBestRecord, 18>, ConstVal::MaxPlayers> m_personalBests = {};
    std::future<void> m_statResult; //stat updates are async - this makes sure to wait when quitting
    void updateProfileDB() const;

    void buildTrophyScene();
    struct Trophy final
    {
        cro::Entity trophy;
        cro::Entity badge;
        cro::Entity label;
        cro::Entity avatar;
    };
    std::array<Trophy, 3u> m_trophies = {};

    struct GridShader final
    {
        std::uint32_t shaderID = 0;
        std::int32_t transparency = -1;
        std::int32_t holeHeight = -1;
    };
    std::array<GridShader, 2u> m_gridShaders = {};

    struct EmoteWheel final
    {
        EmoteWheel(const SharedStateData& ib, const ActivePlayer& cp)
            : sharedData(ib), currentPlayer(cp) {}

        const SharedStateData& sharedData;
        const ActivePlayer& currentPlayer;
        cro::Entity rootNode;
        std::array<cro::Entity, 4u> buttonNodes = {};
        std::array<cro::Entity, 4u> labelNodes = {};

        float targetScale = 0.f;
        float currentScale = 0.f;
        float cooldown = 0.f;

        void build(cro::Entity, cro::Scene&, cro::TextureResource&);
        bool handleEvent(const cro::Event&);
        void update(float);

        void refreshLabels();
    }m_emoteWheel;
    void showEmote(std::uint32_t);

    //-----------

    cro::Entity m_mapCam;
    cro::RenderTexture m_mapTexture;
    cro::MultiRenderTexture m_mapTextureMRT; //hack to create images for map explorer

    void updateMiniMap();

    MinimapZoom m_minimapZoom;
    void retargetMinimap(bool reset);

    cro::Entity m_greenCam;
    cro::RenderTexture m_greenBuffer;

    std::vector<cro::Entity> m_netStrengthIcons;

    //------------

    struct AchievementTracker final
    {
        bool hadFoul = false; //tracks 'boomerang' stat
        bool hadBackspin = false; //tracks 'spin class' and xp award
        bool hadTopspin = false; //tracks XP award

        bool noHolesOverPar = true; //no mistake
        bool noGimmeUsed = true; //never give you up
        bool twoShotsSpare = true; //greens in regulation
        bool alwaysOnTheCourse = true; //consistency

        bool underTwoPutts = true;
        std::int32_t puttCount = 0; //no more than two putts on every hole

        std::int32_t eagles = 0;
        std::int32_t birdies = 0; //nested achievement
        std::int32_t gimmes = 0; //gimme gimme gimme
    }m_achievementTracker;
    cro::Clock m_playTimer; //track avg play time stat
    cro::Time m_playTime;

    //for tracking scoreboard based stats
    struct StatBoardEntry final
    {
        std::uint8_t client = 0;
        std::uint8_t player = 0;
        std::int32_t score = 0;
    };
    std::vector<StatBoardEntry> m_statBoardScores;

    struct GamepadNotify final
    {
        enum
        {
            NewPlayer, Hole, HoleInOne
        };
    };
    void gamepadNotify(std::int32_t);

#ifdef PATH_TRACING
    //------------
    struct BallDebugPoint final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(1.f);
        BallDebugPoint(glm::vec3 p, glm::vec4 c) : position(p), colour(c) {}
    };
    std::vector<BallDebugPoint> m_ballDebugPoints;
    std::vector<std::uint32_t> m_ballDebugIndices;
    bool m_ballDebugActive = false;
    void initBallDebug();
    void beginBallDebug();
    void updateBallDebug(glm::vec3);
    void endBallDebug();
#endif
    struct DebugTx final
    {
        glm::quat q = glm::quat(1.f,0.f,0.f,0.f);
        glm::vec3 pos = glm::vec3(0.f);
        bool hadUpdate = false;
        DebugTx(glm::quat r, glm::vec3 p, bool b) :q(r), pos(p), hadUpdate(b) {}
    };
    std::vector<std::vector<DebugTx>> m_cameraDebugPoints;
    void addCameraDebugging();
    void registerDebugCommands();
    void registerDebugWindows();

    struct AchievementDebugContext final
    {
        bool wasActivated = false; //true after the first time and window was registered
        bool visible = false;

        std::string achievementEnableReason;
        std::string awardStatus;
    }m_achievementDebug;

    struct Benchmark final
    {
        float minRate = std::numeric_limits<float>::max();
        float maxRate = 0.f;

        float getAverage() const
        {
            return m_averageAccumulator / m_sampleCount;
        }

        void update()
        {
            float f = 1.f / m_timer.restart();

            if (f < minRate)
            {
                minRate = f;
            }
            else if (f > maxRate)
            {
                maxRate = f;
            }

            m_averageAccumulator += f;
            m_sampleCount++;
        }

        void reset()
        {
            minRate = std::numeric_limits<float>::max();
            maxRate = 0.f;

            m_averageAccumulator = 0.f;
            m_sampleCount = 1.f;
        }

    private:
        float m_averageAccumulator = 0.f;
        float m_sampleCount = 1.f;

        cro::HiResTimer m_timer;
    }m_benchmark;
    void dumpBenchmark();
};