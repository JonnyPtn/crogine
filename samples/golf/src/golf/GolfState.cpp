﻿/*-----------------------------------------------------------------------

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

#include "GolfState.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "SharedStateData.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "TextAnimCallback.hpp"
#include "ClientCollisionSystem.hpp"
#include "GolfParticleDirector.hpp"
#include "PlayerAvatar.hpp"
#include "GolfSoundDirector.hpp"
#include "TutorialDirector.hpp"
#include "BallSystem.hpp"
#include "FpsCameraSystem.hpp"
#include "NotificationSystem.hpp"
#include "TrophyDisplaySystem.hpp"
#include "FloatingTextSystem.hpp"
#include "CloudSystem.hpp"
#include "VatAnimationSystem.hpp"
#include "BeaconCallback.hpp"
#include "SpectatorSystem.hpp"
#include "PropFollowSystem.hpp"
#include "BallAnimationSystem.hpp"
#include "SpectatorAnimCallback.hpp"
#include "MiniBallSystem.hpp"
#include "CallbackData.hpp"
#include "XPAwardStrings.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

#include <crogine/audio/AudioScape.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/ecs/InfoFlags.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>
#include <crogine/ecs/systems/LightVolumeSystem.hpp>

#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/LightVolume.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/CircleMeshBuilder.hpp>

#include <crogine/network/NetClient.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/rotate_vector.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>
#include "../ErrorCheck.hpp"

#include <sstream>

using namespace cl;

namespace
{
#include "PointLightShader.inl"
#include "WaterShader.inl"
#include "CloudShader.inl"
#include "CelShader.inl"
#include "MinimapShader.inl"
#include "WireframeShader.inl"
#include "TransitionShader.inl"
#include "BillboardShader.inl"
#include "ShadowMapping.inl"
#include "TreeShader.inl"
#include "BeaconShader.inl"
#include "FogShader.inl"
#include "PostProcess.inl"
#include "ShaderIncludes.inl"

#ifdef CRO_DEBUG_
    std::int32_t debugFlags = 0;
    cro::Entity ballEntity;
    std::size_t bitrate = 0;
    std::size_t bitrateCounter = 0;
    glm::vec4 topSky;
    glm::vec4 bottomSky;

#endif // CRO_DEBUG_

    float godmode = 1.f;

    const cro::Time ReadyPingFreq = cro::seconds(1.f);

    constexpr float MaxRotation = 0.3f;// 0.13f;
    constexpr float MaxPuttRotation = 0.4f;// 0.24f;

    bool recordCam = false;

    bool isFastCPU(const SharedStateData& sd, const ActivePlayer& activePlayer)
    {
        return sd.connectionData[activePlayer.client].playerData[activePlayer.player].isCPU
            && sd.fastCPU;
    }
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus(), 768/*, cro::INFO_FLAG_SYSTEM_TIME | cro::INFO_FLAG_SYSTEMS_ACTIVE*/),
    m_skyScene          (context.appInstance.getMessageBus(), 512),
    m_uiScene           (context.appInstance.getMessageBus(), 1024),
    m_trophyScene       (context.appInstance.getMessageBus()),
    m_textChat          (m_uiScene, sd),
    m_inputParser       (sd, &m_gameScene),
    m_cpuGolfer         (m_inputParser, m_currentPlayer, m_collisionMesh),
    m_humanCount        (0),
    m_wantsGameState    (true),
    m_allowAchievements (false),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_windBuffer        ("WindValues"),
    m_holeToModelRatio  (1.f),
    m_currentHole       (0),
    m_distanceToHole    (1.f), //don't init to 0 in case we get div0
    m_terrainChunker    (m_gameScene),
    m_terrainBuilder    (sd, m_holeData, m_terrainChunker),
    m_audioPath         ("assets/golf/sound/ambience.xas"),
    m_currentCamera     (CameraID::Player),
    m_idleTime          (cro::seconds(180.f)),
    m_photoMode         (false),
    m_restoreInput      (false),
    m_activeAvatar      (nullptr),
    m_camRotation       (0.f),
    m_roundEnded        (false),
    m_newHole           (true),
    m_suddenDeath       (false),
    m_viewScale         (1.f),
    m_scoreColumnCount  (2),
    m_readyQuitFlags    (0),
    m_courseIndex       (getCourseIndex(sd.mapDirectory.toAnsiString())),
    m_emoteWheel        (sd, m_currentPlayer, m_textChat)
{
    sd.nightTime = 1;
    m_cpuGolfer.setFastCPU(m_sharedData.fastCPU);

    godmode = 1.f;
    registerCommand("god", [](const std::string&)
        {
            if (godmode == 1)
            {
                godmode = 4.f;
                cro::Console::print("godmode ON");
            }
            else
            {
                godmode = 1.f;
                cro::Console::print("godmode OFF");
            }
        });

    registerCommand("noclip", [&](const std::string&)
        {
            toggleFreeCam();
            if (m_photoMode)
            {
                cro::Console::print("noclip ON");
            }
            else
            {
                cro::Console::print("noclip OFF");
            }
        });

#ifdef CRO_DEBUG_
    registerCommand("fast_cpu", [&](const std::string& param)
        {
            if (m_sharedData.hosting)
            {
                const auto sendCmd = [&]()
                {
                    m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::FastCPU, m_sharedData.fastCPU ? 1 : 0, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    m_cpuGolfer.setFastCPU(m_sharedData.fastCPU);

                    //TODO set active or not if current player is CPU
                    
                };

                if (param == "0")
                {
                    m_sharedData.fastCPU = false;
                    sendCmd();
                }
                else if (param == "1")
                {
                    m_sharedData.fastCPU = true;
                    sendCmd();
                }
                else
                {
                    cro::Console::print("Usage: fast_cpu <0|1>");
                }
            }
        });

    /*registerCommand("fog", [&](const std::string& param)
        {
            if (param.empty())
            {
                cro::Console::print("Usage: fog <0.0 - 1.0>");
            }
            else
            {
                float f = 0.f;
                std::stringstream ss;
                ss << param;
                ss >> f;

                auto& shader = m_resources.shaders.get(ShaderID::Fog);
                auto uniform = shader.getUniformID("u_density");
                glUseProgram(shader.getGLHandle());
                glUniform1f(uniform, std::clamp(f, 0.f, 1.f));
            }
        });*/
#endif

    m_shadowQuality.update(sd.hqShadows);

    sd.baseState = StateID::Golf;

    std::int32_t humanCount = 0;
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        if (!m_sharedData.localConnectionData.playerData[i].isCPU)
        {
            humanCount++;
        }
    }
    m_allowAchievements = (humanCount == 1) && (getCourseIndex(sd.mapDirectory) != -1);
    m_humanCount = humanCount;

    /*switch (sd.scoreType)
    {
    default:
        m_allowAchievements = m_allowAchievements;
        break;
    case ScoreType::ShortRound:
        m_allowAchievements = false;
        break;
    }*/

    //This is set when setting active player.
    Achievements::setActive(m_allowAchievements);
    Social::getMonthlyChallenge().refresh();

    //do this first so scores are reset before scoreboard
    //is first created.
    std::int32_t clientCount = 0;
    std::int32_t cpuCount = 0;
    for (auto& c : sd.connectionData)
    {
        if (c.playerCount != 0)
        {
            clientCount++;

            for (auto i = 0u; i < c.playerCount; ++i)
            {
                c.playerData[i].matchScore = 0;
                c.playerData[i].skinScore = 0;

                if (c.playerData[i].isCPU)
                {
                    cpuCount++;
                }
            }
        }
    }
    if (clientCount > /*ConstVal::MaxClients*/3) //achievement is for 4
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::BetterWithFriends]);
    }
    m_cpuGolfer.setCPUCount(cpuCount, sd);

    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        buildTrophyScene();
        buildScene();

        createTransition();
        cacheState(StateID::Pause);
        cacheState(StateID::MapOverview);
        });

    //glLineWidth(1.5f);
#ifdef CRO_DEBUG_
    ballEntity = {};

    registerDebugWindows();
#endif
    registerDebugCommands(); //includes cubemap creation

    cro::App::getInstance().resetFrameTime();
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (evt.type != SDL_MOUSEMOTION
        && evt.type != SDL_CONTROLLERBUTTONDOWN
        && evt.type != SDL_CONTROLLERBUTTONUP)
    {
        if (ImGui::GetIO().WantCaptureKeyboard
            || ImGui::GetIO().WantCaptureMouse)
        {
            if (evt.type == SDL_KEYUP)
            {
                switch (evt.key.keysym.sym)
                {
                default: break;
                case SDLK_ESCAPE:
                    if (m_textChat.isVisible())
                    {
                        m_textChat.toggleWindow();
                    }
                    break;
                case SDLK_F8:
                    if (evt.key.keysym.mod & KMOD_SHIFT)
                    {
                        m_textChat.toggleWindow();
                    }
                    break;
                }                
            }
            return true;
        }
    }


    //handle this first in case the input parser is currently suspended
    if (m_photoMode)
    {
        m_gameScene.getSystem<FpsCameraSystem>()->handleEvent(evt);
    }
    else
    {
        if (!m_emoteWheel.handleEvent(evt))
        {
            m_inputParser.handleEvent(evt);
        }
    }


    const auto scrollScores = [&](std::int32_t step)
    {
        if (m_holeData.size() > 9)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::ScoreScroll;
            cmd.action = [step](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<std::int32_t>() = step;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    };

    const auto hideMouse = [&]()
    {
        if (getStateCount() == 1)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto closeMessage = [&]()
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::MessageBoard;
        cmd.action = [](cro::Entity e, float)
        {
            auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<MessageAnim>();
            if (state == MessageAnim::Hold)
            {
                currTime = 1.f;
                state = MessageAnim::Close;
            }
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    const auto showMapOverview = [&]()
    {
        if (m_sharedData.minimapData.active)
        {
            requestStackPush(StateID::MapOverview);
        }
    };

    if (evt.type == SDL_KEYUP)
    {
        //hideMouse(); //TODO this should only react to current keybindings
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_2:
            if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
            {
                if (m_inputParser.getActive())
                {
                    if (m_currentCamera == CameraID::Player)
                    {
                        setActiveCamera(CameraID::Bystander);
                    }
                    else if (m_currentCamera == CameraID::Bystander)
                    {
                        setActiveCamera(CameraID::Player);
                    }
                }
            }
            break;
        case SDLK_3:
            toggleFreeCam();
            break;
            //4&5 rotate camera
        case SDLK_6:
            showMapOverview();
            break;
        case SDLK_TAB:
            showScoreboard(false);
            break;
        case SDLK_SPACE: //TODO this should read the keymap... but it's not const
            closeMessage();
            break;
        case SDLK_F6:
            //logCSV();
            /*m_activeAvatar->model.getComponent<cro::Callback>().active = true;
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(false);*/

            break;
        case SDLK_F8:
            if (evt.key.keysym.mod & KMOD_SHIFT)
            {
                m_textChat.toggleWindow();
            }
            break;
        case SDLK_F9:
            if (evt.key.keysym.mod & KMOD_SHIFT)
            {
                cro::Console::doCommand("build_cubemaps");
            }
            break;
#ifdef CRO_DEBUG_
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::GotoHole), net::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextPlayer), net::NetFlag::Reliable);
            break;
        case SDLK_F4:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::GotoGreen), net::NetFlag::Reliable);
            break;
//        case SDLK_F6: //this is used to log CSV
//            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::EndGame), net::NetFlag::Reliable);
//#ifdef USE_GNS
//            //Social::resetAchievement(AchievementStrings[AchievementID::SkinOfYourTeeth]);
//#endif
//            break;
        case SDLK_F7:
            //m_sharedData.clientConnection.netClient.sendPacket(PacketID::SkipTurn, m_sharedData.localConnectionData.connectionID, net::NetFlag::Reliable);
            
            m_sharedData.connectionData[0].playerData[0].skinScore = 10;
            m_sharedData.connectionData[0].playerData[1].skinScore = 1;
            showCountdown(30);
            //showMessageBoard(MessageBoardID::Scrub);
            //requestStackPush(StateID::Tutorial);
            //showNotification("buns");
            //Achievements::awardAchievement(AchievementStrings[AchievementID::SkinOfYourTeeth]);
            break;
        case SDLK_F10:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::ChangeWind), net::NetFlag::Reliable);
            break;
        case SDLK_KP_0:
            setActiveCamera(CameraID::Idle);
        {
            /*static bool hidden = false;
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(!hidden);
            hidden = !hidden;*/
        }
            break;
        case SDLK_KP_1:
            //setActiveCamera(1);
            //m_cameras[CameraID::Sky].getComponent<CameraFollower>().state = CameraFollower::Zoom;
        {
            if (m_drone.isValid())
            {
                auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::DroneHit;
                msg->position = m_drone.getComponent<cro::Transform>().getPosition();
            }
        }
            break;
        case SDLK_KP_2:
            //setActiveCamera(2);
            //showCountdown(10);
            m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Swing], 1.f, 1.2f);
            break;
        case SDLK_KP_3:
            //predictBall(0.85f);
            m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Celebrate], 1.f, 1.2f);
            break;
        case SDLK_KP_4:
        {
            static bool hidden = false;
            hidden = !hidden;
            m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().setHidden(hidden);
        }
            break;
        case SDLK_KP_5:
            gamepadNotify(GamepadNotify::HoleInOne);
            break;
        case SDLK_KP_6:
        {
            auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
            msg->type = Social::SocialEvent::LevelUp;
            msg->level = 19;
        }
            break;
        case SDLK_KP_7:
        {
            auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg2->type = GolfEvent::BirdHit;
            msg2->position = m_currentPlayer.position;
            float rot = glm::eulerAngles(m_cameras[m_currentCamera].getComponent<cro::Transform>().getWorldRotation()).y;
            msg2->travelDistance = rot;
        }
            break;
            //used in font smoothing debug GolfGame.cpp
        /*case SDLK_KP_MULTIPLY:
        {
            auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::HoleInOne;
            msg->position = m_holeData[m_currentHole].pin;
        }
            showMessageBoard(MessageBoardID::HoleScore);*/
            break;
        case SDLK_PAGEUP:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Flag;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<FlagCallbackData>().targetHeight = FlagCallbackData::MaxHeight;
                e.getComponent<cro::Callback>().active = true;
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case SDLK_PAGEDOWN:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Flag;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<FlagCallbackData>().targetHeight = 0.f;
                e.getComponent<cro::Callback>().active = true;
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        break;
        case SDLK_HOME:
            debugFlags = (debugFlags == 0) ? BulletDebug::DebugFlags : 0;
            m_collisionMesh.setDebugFlags(debugFlags);
            break;
        case SDLK_END:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::ThinkBubble;
            cmd.action = [](cro::Entity e, float)
            {
                auto& [dir, _] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
                dir = (dir == 0) ? 1 : 0;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case SDLK_DELETE:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Crowd;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<VatAnimation>().applaud();
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::Spectator;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().setUserData<bool>(true);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
#endif
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym != SDLK_F12) //default screenshot key
        {
            resetIdle();
        }
        m_skipState.displayControllerMessage = false;

        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            if (!evt.key.repeat)
            {
                showScoreboard(true);
            }
            break;
        case SDLK_UP:
        case SDLK_LEFT:
            scrollScores(-19);
            break;
        case SDLK_DOWN:
        case SDLK_RIGHT:
            scrollScores(19);
            break;
        case SDLK_RETURN:
            showScoreboard(false);
            break;
        case SDLK_ESCAPE:
            if (m_textChat.isVisible())
            {
                m_textChat.toggleWindow();
                break;
            }
            [[fallthrough]];
        case SDLK_p:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
        case SDLK_SPACE:
            toggleQuitReady();
            break;
        case SDLK_7:
            m_textChat.quickEmote(TextChat::Applaud);
            break;
        case SDLK_8:
            m_textChat.quickEmote(TextChat::Happy);
            break;
        case SDLK_9:
            m_textChat.quickEmote(TextChat::Laughing);
            break;
        case SDLK_0:
            m_textChat.quickEmote(TextChat::Angry);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        resetIdle();
        m_skipState.displayControllerMessage = true;

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftStick:
            //showMapOverview();
            break;
        case cro::GameController::ButtonBack:
            showScoreboard(true);
            break;
        case cro::GameController::ButtonB:
            showScoreboard(false);
            break;
        case cro::GameController::DPadUp:
        case cro::GameController::DPadLeft:
            scrollScores(-19);
            break;
        case cro::GameController::DPadDown:
        case cro::GameController::DPadRight:
            scrollScores(19);
            break;
        case cro::GameController::ButtonA:
            toggleQuitReady();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        hideMouse();
        switch (evt.cbutton.button)
        {
        default: break;
        case SDL_CONTROLLER_BUTTON_MISC1:
            m_textChat.toggleWindow();
            break;
        case cro::GameController::ButtonTrackpad:
            showMapOverview();
            break;
        case cro::GameController::ButtonBack:
            showScoreboard(false);
            break;
        case cro::GameController::ButtonStart:
        case cro::GameController::ButtonGuide:
            requestStackPush(StateID::Pause);
            break;
        case cro::GameController::ButtonA:
            if (evt.cbutton.which == cro::GameController::deviceID(activeControllerID(m_currentPlayer.player)))
            {
                closeMessage();
            }
            break;
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            scrollScores(-19);
        }
        else if (evt.wheel.y < 0)
        {
            scrollScores(19);
        }
        resetIdle();
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            closeMessage();
        }
    }

    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        m_emoteWheel.refreshLabels(); //displays labels if no controllers connected

        //pause the game
        requestStackPush(StateID::Pause);
    }
    else if (evt.type == SDL_CONTROLLERDEVICEADDED)
    {
        //hides labels
        m_emoteWheel.refreshLabels();
    }

    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (!m_photoMode
            && (evt.motion.state & SDL_BUTTON_RMASK) == 0)
        {
            cro::App::getWindow().setMouseCaptured(false);
        }
    }
    else if (evt.type == SDL_JOYAXISMOTION)
    {
        m_skipState.displayControllerMessage = true;

        if (std::abs(evt.caxis.value) > 10000)
        {
            hideMouse();
            resetIdle();
        }

        switch (evt.caxis.axis)
        {
        default: break;
        case cro::GameController::AxisRightY:
            if (std::abs(evt.caxis.value) > 10000)
            {
                scrollScores(cro::Util::Maths::sgn(evt.caxis.value) * 19);
            }
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_skyScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    m_trophyScene.forwardEvent(evt);

    return true;
}

void GolfState::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case Social::MessageID::SocialMessage:
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::LevelUp)
        {
            std::uint64_t packet = 0;
            packet |= (static_cast<std::uint64_t>(m_sharedData.clientConnection.connectionID) << 40);
            packet |= (static_cast<std::uint64_t>(m_currentPlayer.player) << 32);
            packet |= data.level;
            m_sharedData.clientConnection.netClient.sendPacket<std::uint64_t>(PacketID::LevelUp, packet, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else if (data.type == Social::SocialEvent::XPAwarded)
        {
            auto xpStr = std::to_string(data.level) + " XP";
            if (data.reason > -1)
            {
                CRO_ASSERT(data.reason < XPStringID::Count, "");
                floatingMessage(XPStrings[data.reason] + " " + xpStr);
            }
            else
            {
                floatingMessage(xpStr);
            }
        }
    }
        break;
    case MessageID::SystemMessage:
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::StateRequest)
        {
            requestStackPush(data.data);
        }
        else if (data.type == SystemEvent::ShadowQualityChanged)
        {
            applyShadowQuality();
        }
    }
        break;
    case cro::Message::SkeletalAnimationMessage:
    {
        const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
        if (data.userType == SpriteAnimID::Swing)
        {
            //relay this message with the info needed for particle/sound effects
            auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg2->type = GolfEvent::ClubSwing;
            msg2->position = m_currentPlayer.position;
            msg2->terrain = m_currentPlayer.terrain;
            msg2->club = static_cast<std::uint8_t>(getClub());

            m_gameScene.getSystem<ClientCollisionSystem>()->setActiveClub(getClub());

            auto isCPU = m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU;
            if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !isCPU)
            {
                auto strLow = static_cast<std::uint16_t>(50000.f * m_inputParser.getPower()) * m_sharedData.enableRumble;
                auto strHigh = static_cast<std::uint16_t>(35000.f * m_inputParser.getPower()) * m_sharedData.enableRumble;

                cro::GameController::rumbleStart(activeControllerID(m_sharedData.inputBinding.playerID), strLow, strHigh, 200);
            }

            //auto skip if fast CPU is on
            if (m_sharedData.fastCPU && isCPU)
            {
                if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID)
                {
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::SkipTurn, m_sharedData.localConnectionData.connectionID, net::NetFlag::Reliable);
                    m_skipState.wasSkipped = true;
                }
            }

            //check if we hooked/sliced
            if (auto club = getClub(); club != ClubID::Putter
                && (!isCPU || (isCPU && !m_sharedData.fastCPU)))
            {
                //TODO this doesn't include any easing added when making the stroke
                //we should be using the value returned by getStroke() in hitBall()
                auto hook = m_inputParser.getHook() * m_activeAvatar->model.getComponent<cro::Transform>().getScale().x;
                if (hook < -0.08f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::HookedBall;
                    floatingMessage("Hook");
                }
                else if (hook > 0.08f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::SlicedBall;
                    floatingMessage("Slice");
                }

                auto power = m_inputParser.getPower();
                hook *= 20.f;
                hook = std::round(hook);
                hook /= 20.f;
                static constexpr float  PowerShot = 0.97f;

                if (power > 0.59f //hmm not sure why power should factor into this?
                    && std::abs(hook) < 0.05f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = (power > PowerShot && club < ClubID::NineIron) ? GolfEvent::PowerShot : GolfEvent::NiceShot;
                    msg3->position = m_currentPlayer.position;

                    /*if (msg3->type == GolfEvent::PowerShot)
                    {
                        m_activeAvatar->ballModel.getComponent<cro::ParticleEmitter>().start();
                    }*/

                    //award more XP for aiming straight
                    float dirAmount = /*cro::Util::Easing::easeOutExpo*/((m_inputParser.getMaxRotation() - std::abs(m_inputParser.getRotation())) / m_inputParser.getMaxRotation());

                    auto xp = std::clamp(static_cast<std::int32_t>(6.f * dirAmount), 0, 6);
                    if (xp)
                    {
                        Social::awardXP(xp, XPStringID::GreatAccuracy);
                        Social::getMonthlyChallenge().updateChallenge(ChallengeID::Eight, 0);
                    }
                }
                else if (power > PowerShot
                    && club < ClubID::NineIron)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::PowerShot;
                    msg3->position = m_currentPlayer.position;

                    m_activeAvatar->ballModel.getComponent<cro::ParticleEmitter>().start();
                }

                //hide the ball briefly to hack around the motion lag
                //(the callback automatically scales back)
                m_activeAvatar->ballModel.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                m_activeAvatar->ballModel.getComponent<cro::Callback>().active = true;
                m_activeAvatar->ballModel.getComponent<cro::Model>().setHidden(true);

                //see if we're doing something silly like facing the camera
                const auto camVec = cro::Util::Matrix::getForwardVector(m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldTransform());
                const auto playerVec = m_currentPlayer.position - m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldPosition();
                const auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), m_inputParser.getYaw(), cro::Transform::Y_AXIS);
                const auto ballDir = glm::toMat3(rotation) * cro::Transform::X_AXIS;
                if (glm::dot(camVec, ballDir) < -0.9f && glm::dot(camVec, playerVec) > 0.f
                    && !Achievements::getAchievement(AchievementStrings[AchievementID::BadSport])->achieved)
                {
                    auto* shader = &m_resources.shaders.get(ShaderID::Noise);
                    m_courseEnt.getComponent<cro::Drawable2D>().setShader(shader);
                    for (auto& cam : m_cameras)
                    {
                        cam.getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Noise];
                    }

                    auto entity = m_uiScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition({ 10.f, 32.f, 0.2f });
                    entity.addComponent<cro::Drawable2D>();
                    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString("FEED LOST");
                    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
                    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
                    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
                    entity.addComponent<cro::Callback>().active = true;
                    entity.getComponent<cro::Callback>().function =
                        [&](cro::Entity e, float dt)
                    {
                        static float state = 0.f;
                        static float accum = 0.f;
                        accum += dt;
                        if (accum > 0.5f)
                        {
                            accum -= 0.5f;
                            state = state == 1 ? 0.f : 1.f;
                        }

                        constexpr glm::vec2 pos(10.f, 32.f);
                        auto scale = m_viewScale / glm::vec2(m_courseEnt.getComponent<cro::Transform>().getScale());
                        e.getComponent<cro::Transform>().setScale(scale * state);
                        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos * scale, 0.2f));
                    };
                    m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

                    cro::App::postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::PlayerBad;
                    Achievements::awardAchievement(AchievementStrings[AchievementID::BadSport]);
                }
            }

            m_gameScene.setSystemActive<CameraFollowSystem>(!(isCPU && m_sharedData.fastCPU));

            //hide current terrain
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::TerrainType;
            cmd.action =
                [](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(" ");
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //show flight cam if not putting
            cmd.targetFlags = CommandID::UI::MiniGreen;
            cmd.action = [&](cro::Entity e, float)
                {
                    if (m_currentPlayer.terrain != TerrainID::Green)
                    {
                        e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 0;
                        e.getComponent<cro::Callback>().active = true;
                        //e.getComponent<cro::Sprite>().setTexture(m_flightTexture.getTexture());
                        e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                        m_flightCam.getComponent<cro::Camera>().active = true;
                    }
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //restore the ball origin if buried
            m_activeAvatar->ballModel.getComponent<cro::Transform>().setOrigin(glm::vec3(0.f));
        }
        else if (data.userType == cro::Message::SkeletalAnimationEvent::Stopped)
        {
            if (m_activeAvatar &&
                data.entity == m_activeAvatar->model)
            {
                //delayed ent to restore player cam
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(0.6f);
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt;

                    if (currTime < 0)
                    {
                        if (m_currentCamera == CameraID::Bystander)
                        {
                            setActiveCamera(CameraID::Player);
                        }

                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                };
            }
        }
    }
    break;
    case MessageID::SceneMessage:
    {
        const auto& data = msg.getData<SceneEvent>();
        switch(data.type)
        {
        default: break;
        case SceneEvent::PlayerRotate:
            if (m_activeAvatar)
            {
                m_activeAvatar->model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, data.rotation);
            }
            break;
        case SceneEvent::TransitionComplete:
        {
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
            break;
        case SceneEvent::RequestSwitchCamera:
            
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;

            if (data.data == CameraID::Drone)
            {
                //hide the stroke indicator
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Model>().setHidden(true); 
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
            else if (data.data == CameraID::Player
                && m_currentCamera == CameraID::Drone)
            {
                //show the stroke indicator if active player
                cmd.action = [&](cro::Entity e, float)
                {
                    auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                    e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }

            setActiveCamera(data.data);
        }
            break;
        }
    }
        break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();

        switch (data.type)
        {
        default: break;
        case GolfEvent::HitBall:
        {
            hitBall();
#ifdef PATH_TRACING
            beginBallDebug();
#endif
#ifdef CAMERA_TRACK
            recordCam = true;
            for (auto& v : m_cameraDebugPoints)
            {
                v.clear();
            }
#endif
        }
        break;
        case GolfEvent::ClubChanged:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeIndicator;
            cmd.action = [&](cro::Entity e, float)
            {
                float scale = std::max(0.25f, Clubs[getClub()].getPower(m_distanceToHole, m_sharedData.imperialMeasurements) / Clubs[ClubID::Driver].getPower(m_distanceToHole, m_sharedData.imperialMeasurements));
                e.getComponent<cro::Transform>().setScale({ scale, 1.f });
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //update the player with correct club
            if (m_activeAvatar
                && m_activeAvatar->hands)
            {
                if (getClub() <= ClubID::FiveWood)
                {
                    m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Wood]);
                }
                else
                {
                    m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Iron]);
                }
                m_activeAvatar->hands->getModel().getComponent<cro::Model>().setFacing(m_activeAvatar->model.getComponent<cro::Model>().getFacing());
            }

            //update club text colour based on distance
            cmd.targetFlags = CommandID::UI::ClubName;
            cmd.action = [&](cro::Entity e, float)
            {
                if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                {
                    e.getComponent<cro::Text>().setString(Clubs[getClub()].getName(m_sharedData.imperialMeasurements, m_distanceToHole));

                    auto dist = m_distanceToHole * 1.67f;
                    if (getClub() < ClubID::NineIron &&
                        Clubs[getClub()].getTarget(m_distanceToHole) > dist)
                    {
                        e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                    }
                    else
                    {
                        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
                    }
                }
                else
                {
                    e.getComponent<cro::Text>().setString(" ");
                }
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::UI::PuttPower;
            cmd.action = [&](cro::Entity e, float)
            {
                if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                {
                    auto club = getClub();
                    if (club == ClubID::Putter)
                    {
                        auto str = Clubs[ClubID::Putter].getName(m_sharedData.imperialMeasurements, m_distanceToHole);
                        e.getComponent<cro::Text>().setString(str.substr(str.find_last_of(' ') + 1));
                    }
                    else
                    {
                        e.getComponent<cro::Text>().setString(" ");
                    }
                }
                else
                {
                    e.getComponent<cro::Text>().setString(" ");
                }
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //hide wind indicator if club is less than min wind distance to hole
            cmd.targetFlags = CommandID::UI::WindHidden;
            cmd.action = [&](cro::Entity e, float)
            {
                std::int32_t dir = (getClub() > ClubID::PitchWedge) && (glm::length(m_holeData[m_currentHole].pin - m_currentPlayer.position) < 30.f) ? 0 : 1;
                e.getComponent<cro::Callback>().getUserData<WindHideData>().direction = dir;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


            if (m_currentPlayer.terrain != TerrainID::Green
                && m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
            {
                if (getClub() > ClubID::SevenIron)
                {
                    retargetMinimap(false);
                }
            }
        }
        break;
        case GolfEvent::BallLanded:
        {
            bool oob = false;
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Scrub:
            case TerrainID::Water:
                oob = true;
                [[fallthrough]];
            case TerrainID::Bunker:
                if (data.travelDistance > 100.f
                    && !isFastCPU(m_sharedData, m_currentPlayer))
                {
                    m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.2f);
                }
                break;
            case TerrainID::Green:
                if (getClub() != ClubID::Putter)
                {
                    if ((data.pinDistance < 1.f
                        || data.travelDistance > 10000.f)
                        && !isFastCPU(m_sharedData, m_currentPlayer))
                    {
                        m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Celebrate], 1.f, 1.2f);

                        if (data.pinDistance < 0.5f)
                        {
                            Social::awardXP(XPValues[XPID::Special] / 2, XPStringID::NiceChip);
                            Social::getMonthlyChallenge().updateChallenge(ChallengeID::Zero, 0);
                        }
                    }
                    else if (data.travelDistance < 9.f
                        && !isFastCPU(m_sharedData, m_currentPlayer))
                    {
                        m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.4f);
                    }

                    //check if we're still 2 under for achievement
                    if (m_sharedData.localConnectionData.connectionID == m_currentPlayer.client
                        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].isCPU)
                    {
                        if (m_holeData[m_currentHole].par - m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]
                            < 2)
                        {
                            m_achievementTracker.twoShotsSpare = false;
                        }
                    }
                }
                break;
            case TerrainID::Fairway:
                if (data.travelDistance < 16.f
                    && !isFastCPU(m_sharedData, m_currentPlayer))
                {
                    m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.4f);
                }
                break;
            }

            //creates a delay before switching back to player cam
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(1.5f);
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;

                if (currTime < 0)
                {
                    setActiveCamera(CameraID::Player);
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);


                    //hide the flight cam
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::UI::MiniGreen;
                    cmd.action = [&](cro::Entity en, float)
                        {
                            if (m_currentPlayer.terrain != TerrainID::Green)
                            {
                                en.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 1;
                                en.getComponent<cro::Callback>().active = true;
                            }
                        };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                }
            };

            if (data.terrain == TerrainID::Fairway)
            {
                Social::awardXP(1, XPStringID::OnTheFairway);
            }

            if (oob)
            {
                //red channel triggers noise effect
                m_miniGreenEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Cyan);
            }

#ifdef PATH_TRACING
            endBallDebug();
#endif
#ifdef CAMERA_TRACK
            recordCam = false;
#endif
        }
        break;
        case GolfEvent::DroneHit:
        {
            Achievements::awardAchievement(AchievementStrings[AchievementID::HoleInOneMillion]);
            Social::awardXP(XPValues[XPID::Special] * 5, XPStringID::DroneHit);

            m_gameScene.destroyEntity(m_drone);
            m_drone = {};

            m_cameras[CameraID::Sky].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Noise];
            m_cameras[CameraID::Drone].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Noise];

            if (m_currentCamera == CameraID::Sky
                || m_currentCamera == CameraID::Drone) //although you shouldn't be able to switch to this with the ball moving
            {
                m_courseEnt.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::Noise));
            }

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 10.f, 32.f, 0.2f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString("FEED LOST");
            entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                static float state = 0.f;

                if (m_currentCamera == CameraID::Sky
                    || m_currentCamera == CameraID::Drone)
                {
                    static float accum = 0.f;
                    accum += dt;
                    if (accum > 0.5f)
                    {
                        accum -= 0.5f;
                        state = state == 1 ? 0.f : 1.f;
                    }
                }
                else
                {
                    state = 0.f;
                }

                constexpr glm::vec2 pos(10.f, 32.f);
                auto scale = m_viewScale / glm::vec2(m_courseEnt.getComponent<cro::Transform>().getScale());
                e.getComponent<cro::Transform>().setScale(scale * state);
                e.getComponent<cro::Transform>().setPosition(glm::vec3(pos * scale, 0.2f));

                if (m_drone.isValid())
                {
                    //camera was restored
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            };
            m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
        break;
        case GolfEvent::HoleInOne:
        {
            gamepadNotify(GamepadNotify::HoleInOne);
        }
        break;
        }
    }
    break;
    case cro::Message::ConsoleMessage:
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        switch (data.type)
        {
        default: break;
        case cro::Message::ConsoleEvent::Closed:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        case cro::Message::ConsoleEvent::Opened:
            cro::App::getWindow().setMouseCaptured(false);
            break;
        }
    }
        break;
    case cro::Message::StateMessage:
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            if (data.id == StateID::Pause
                || data.id == StateID::Tutorial)
            {
                cro::App::getWindow().setMouseCaptured(true);
            }
            else if (data.id == StateID::Options)
            {
                //update the beacon if settings changed
                cro::Command cmd;
                cmd.targetFlags = CommandID::Beacon;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().active = m_sharedData.showBeacon;
                    e.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //other items share the beacon colouring such as the putt assist
                cmd.targetFlags = CommandID::BeaconColour;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //and distance values
                cmd.targetFlags = CommandID::UI::ClubName;
                cmd.action = [&](cro::Entity e, float)
                {
                    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                    {
                        e.getComponent<cro::Text>().setString(Clubs[getClub()].getName(m_sharedData.imperialMeasurements, m_distanceToHole));
                    }
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::UI::PuttPower;
                cmd.action = [&](cro::Entity e, float)
                {
                    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                    {
                        auto club = getClub();
                        if (club == ClubID::Putter)
                        {
                            auto str = Clubs[ClubID::Putter].getName(m_sharedData.imperialMeasurements, m_distanceToHole);
                            e.getComponent<cro::Text>().setString(str.substr(str.find_last_of(' ') + 1));
                        }
                        else
                        {
                            e.getComponent<cro::Text>().setString(" ");
                        }
                    }
                    else
                    {
                        e.getComponent<cro::Text>().setString(" ");
                    }
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::UI::PinDistance;
                cmd.action =
                    [&](cro::Entity e, float)
                {
                    formatDistanceString(m_distanceToHole, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements);

                    auto bounds = cro::Text::getLocalBounds(e);
                    bounds.width = std::floor(bounds.width / 2.f);
                    e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //and putting grid
                cmd.targetFlags = CommandID::SlopeIndicator;
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().active = true;
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                m_emoteWheel.refreshLabels();
                m_ballTrail.setUseBeaconColour(m_sharedData.trailBeaconColour);
            }
        }
    }
        break;
    case MessageID::AchievementMessage:
    {
        const auto& data = msg.getData<AchievementEvent>();
        std::array<std::uint8_t, 2u> packet =
        {
            m_sharedData.localConnectionData.connectionID,
            data.id
        };
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::AchievementGet, packet, net::NetFlag::Reliable);
    }
    break;

    case MessageID::AIMessage:
    {
        const auto& data = msg.getData<AIEvent>();
        if (data.type == AIEvent::Predict)
        {
            predictBall(data.power);
        }
        else
        {
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::CPUThink, std::uint8_t(data.type), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
        break;
    case MessageID::CollisionMessage:
    {
        const auto& data = msg.getData<CollisionEvent>();
        if (data.terrain == TerrainID::Scrub)
        {
            if (cro::Util::Random::value(0, 2) == 0)
            {
                auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg2->type = GolfEvent::BirdHit;
                msg2->position = data.position;

                float rot = glm::eulerAngles(m_cameras[m_currentCamera].getComponent<cro::Transform>().getWorldRotation()).y;
                msg2->travelDistance = rot;
            }
        }

        if (data.type == CollisionEvent::NearMiss)
        {
            m_achievementTracker.nearMissChallenge = true;
            Social::awardXP(1, XPStringID::NearMiss);
        }
    }
        break;
    }

    m_cpuGolfer.handleMessage(msg);

    m_gameScene.forwardMessage(msg);
    m_skyScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
    m_trophyScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
#ifdef CRO_DEBUG_
    glm::vec3 move(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_UP))
    {
        move.z -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_DOWN))
    {
        move.z += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_LEFT))
    {
        move.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_RIGHT))
    {
        move.x += 1.f;
    }
    
    if (glm::length2(move) > 1)
    {
        move = glm::normalize(move);
    }
    m_waterEnt.getComponent<cro::Transform>().move(move * 10.f * dt);
#endif

    auto holeDir = m_holeData[m_currentHole].pin - m_currentPlayer.position;
    if (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit)
    {
        holeDir = m_holeData[m_currentHole].target - m_currentPlayer.position;
    }

    m_ballTrail.update();

    //this gets used a lot so we'll save on some calls to length()
    m_distanceToHole = glm::length(holeDir);

    m_depthMap.update(1);

    if (m_sharedData.clientConnection.connected)
    {
        //these may have accumulated during loading
        for (const auto& evt : m_sharedData.clientConnection.eventBuffer)
        {
            handleNetEvent(evt);
        }
        m_sharedData.clientConnection.eventBuffer.clear();

        net::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
#ifdef CRO_DEBUG_
            if (evt.type == cro::NetEvent::PacketReceived)
            {
                bitrateCounter += evt.packet.getSize() * 8;
            }
#endif
            //handle events
            handleNetEvent(evt);
        }

#ifdef CRO_DEBUG_
        static float bitrateTimer = 0.f;
        bitrateTimer += dt;
        if (bitrateTimer > 1.f)
        {
            bitrateTimer -= 1.f;
            bitrate = bitrateCounter;
            bitrateCounter = 0;
        }
#endif

        if (m_wantsGameState)
        {
            if (m_readyClock.elapsed() > ReadyPingFreq)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable);
                m_readyClock.restart();
            }
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
        m_sharedData.errorMessage = "Lost connection to host.";
        requestStackPush(StateID::Error);
    }

    //update the fade distance if needed
    if (m_resolutionUpdate.targetFade != m_resolutionUpdate.resolutionData.nearFadeDistance)
    {
        float diff = m_resolutionUpdate.targetFade - m_resolutionUpdate.resolutionData.nearFadeDistance;
        if (diff > 0.0001f)
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance += (diff * (dt * 1.2f));
        }
        else if (diff < 0.0001f)
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance += (diff * (dt * 8.f));
        }
        else
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance = m_resolutionUpdate.targetFade;
        }

        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);
    }

    //update time uniforms
    static float elapsed = dt;
    elapsed += dt;
    
    m_windUpdate.currentWindSpeed += (m_windUpdate.windVector.y - m_windUpdate.currentWindSpeed) * dt;
    m_windUpdate.currentWindVector += (m_windUpdate.windVector - m_windUpdate.currentWindVector) * dt;

    WindData data;
    data.direction[0] = m_windUpdate.currentWindVector.x;
    data.direction[1] = m_windUpdate.currentWindSpeed;
    data.direction[2] = m_windUpdate.currentWindVector.z;
    data.elapsedTime = elapsed;
    m_windBuffer.setData(data);

    glm::vec3 windVector(m_windUpdate.currentWindVector.x,
                        m_windUpdate.currentWindSpeed,
                        m_windUpdate.currentWindVector.z);
    m_gameScene.getSystem<CloudSystem>()->setWindVector(windVector);

    const auto& windEnts = m_skyScene.getSystem<cro::CallbackSystem>()->getEntities();
    for (auto e : windEnts)
    {
        e.getComponent<cro::Callback>().setUserData<float>(m_windUpdate.currentWindSpeed);
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::ParticleEmitter;
    cmd.action = [&](cro::Entity e, float)
    {
        static constexpr float Strength = 2.45f;
        e.getComponent<cro::ParticleEmitter>().settings.forces[0] =
        {
            m_windUpdate.currentWindVector.x * m_windUpdate.currentWindSpeed * Strength,
            0.f,
            m_windUpdate.currentWindVector.z * m_windUpdate.currentWindSpeed * Strength
        };
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //don't update the CPU or gamepad if there are any menus open
    if (getStateCount() == 1)
    {
        m_cpuGolfer.update(dt, windVector, m_distanceToHole);
        m_inputParser.update(dt);

        if (float movement = m_inputParser.getCamMotion(); movement != 0)
        {
            updateCameraHeight(movement * dt);
        }

        //if we're CPU or remote player check screen pos of ball and
        //move the cam
        /*if (m_currentPlayer.client != m_sharedData.localConnectionData.connectionID
            || m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
        {
            if (m_inputParser.isAiming())
            {
                auto pos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(m_currentPlayer.position, m_gameSceneTexture.getSize());
                if ((pos.y / m_gameSceneTexture.getSize().y) < 0.08f)
                {
                    updateCameraHeight(-dt);
                }
            }
        }*/

        float rotation = m_inputParser.getCamRotation() * dt;
        if (getClub() != ClubID::Putter
            && rotation != 0)
        {
            auto& tx = m_cameras[CameraID::Player].getComponent<cro::Transform>();

            auto axis = glm::inverse(tx.getRotation()) * cro::Transform::Y_AXIS;
            tx.rotate(axis, rotation);

            auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
            auto lookDir = targetInfo.currentLookAt - tx.getWorldPosition();
            lookDir = glm::rotate(lookDir, rotation, axis);
            targetInfo.currentLookAt = tx.getWorldPosition() + lookDir;
            targetInfo.targetLookAt = targetInfo.currentLookAt;

            m_camRotation += rotation;
        }

        updateSkipMessage(dt);
    }

    m_emoteWheel.update(dt);
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    //m_terrainChunker.update();//this needs to be done after the camera system is updated

    //do this last to ensure game scene camera is up to date
    updateSkybox(dt);


#ifndef CRO_DEBUG_
    if (m_roundEnded)
#endif
    {
        m_trophyScene.simulate(dt);
    }

    //tell the flag to raise or lower
    if (m_currentPlayer.terrain == TerrainID::Green)
    {
        cmd.targetFlags = CommandID::Flag;
        cmd.action = [&](cro::Entity e, float)
        {
            if (!e.getComponent<cro::Callback>().active)
            {
                auto camDist = glm::length2(m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition() - m_holeData[m_currentHole].pin);
                auto ballDist = FlagRaiseDistance * 2.f;
                if (m_activeAvatar)
                {
                    ballDist = glm::length2((m_activeAvatar->ballModel.getComponent<cro::Transform>().getWorldPosition() - m_holeData[m_currentHole].pin) * 3.f);
                }

                auto& data = e.getComponent<cro::Callback>().getUserData<FlagCallbackData>();
                if (data.targetHeight < FlagCallbackData::MaxHeight
                    && (camDist < FlagRaiseDistance || ballDist < FlagRaiseDistance))
                {
                    data.targetHeight = FlagCallbackData::MaxHeight;
                    e.getComponent<cro::Callback>().active = true;
                }
                else if (data.targetHeight > 0
                    && (camDist > FlagRaiseDistance && ballDist > FlagRaiseDistance))
                {
                    data.targetHeight = 0.f;
                    e.getComponent<cro::Callback>().active = true;
                }
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }

#ifndef CAMERA_TRACK
    //play avatar sound if player idles
    if (!m_sharedData.tutorial
        && !m_roundEnded)
    {
        if (m_idleTimer.elapsed() > m_idleTime)
        {
            m_idleTimer.restart();
            m_idleTime = cro::seconds(std::max(20.f, m_idleTime.asSeconds() / 2.f));


            //horrible hack to make the coughing less frequent
            static std::int32_t coughCount = 0;
            if ((coughCount++ % 8) == 0)
            {
                auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
                msg->type = SceneEvent::PlayerIdle;

                gamepadNotify(GamepadNotify::NewPlayer);
            }

            auto* skel = &m_activeAvatar->model.getComponent<cro::Skeleton>();
            auto animID = m_activeAvatar->animationIDs[AnimationID::Impatient];
            auto idleID = m_activeAvatar->animationIDs[AnimationID::Idle];
            skel->play(animID, 1.f, 0.8f);

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, skel, animID, idleID](cro::Entity e, float)
            {
                auto [currAnim, nextAnim] = skel->getActiveAnimations();
                if (currAnim == animID
                    || nextAnim == animID)
                {
                    if (skel->getState() == cro::Skeleton::Stopped)
                    {
                        skel->play(idleID, 1.f, 0.8f);
                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                }
                else
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
            };
        }

        //switch to idle cam if more than half time
        if (m_idleTimer.elapsed() > (m_idleTime * 0.65f)
            && m_currentCamera == CameraID::Player
            && m_inputParser.getActive()) //hmm this stops this happening on remote clients
        {
            setActiveCamera(CameraID::Idle);
            m_inputParser.setSuspended(true);

            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;
            cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Model>().setHidden(true); };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    }
#else
    if (recordCam)
    {
        const auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
        bool hadUpdate = false;
        if (m_gameScene.getActiveCamera().hasComponent<CameraFollower>())
        {
            hadUpdate = m_gameScene.getActiveCamera().getComponent<CameraFollower>().hadUpdate;
            m_gameScene.getActiveCamera().getComponent<CameraFollower>().hadUpdate = false;
        }
        m_cameraDebugPoints[m_currentCamera].emplace_back(tx.getRotation(), tx.getPosition(), hadUpdate);
    }

#endif
    return true;
}

void GolfState::render()
{
    m_benchmark.update();

    //TODO we probably only need to do this once after the scene is built
    m_scaleBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_windBuffer.bind(2);

    //render reflections first
    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto oldVP = cam.viewport;

    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);
    cam.renderFlags = RenderFlags::Reflection;

    auto& skyCam = m_skyCameras[SkyCam::Main].getComponent<cro::Camera>();
    skyCam.setActivePass(cro::Camera::Pass::Reflection);
    skyCam.renderFlags = RenderFlags::Reflection;
    skyCam.viewport = { 0.f,0.f,1.f,1.f };
    m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Main]);

    cam.reflectionBuffer.clear(cro::Colour::Red);
    //don't want to test against skybox depth values.
    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    //glCheck(glEnable(GL_PROGRAM_POINT_SIZE));
    m_gameScene.render();
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    cam.renderFlags = RenderFlags::All;
    cam.viewport = oldVP;

    skyCam.setActivePass(cro::Camera::Pass::Final);
    skyCam.renderFlags = RenderFlags::All;
    skyCam.viewport = oldVP;

    //then render scene
    if (m_holeData[m_currentHole].puttFromTee)
    {
        glUseProgram(m_gridShaders[1].shaderID);
        glUniform1f(m_gridShaders[1].transparency, m_sharedData.gridTransparency);
    }
    else
    {
        glUseProgram(m_gridShaders[0].shaderID);
        glUniform1f(m_gridShaders[0].transparency, m_sharedData.gridTransparency * (1.f - m_terrainBuilder.getSlopeAlpha()));
    }    
    m_renderTarget.clear(cro::Colour::Black);
    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    glCheck(glEnable(GL_LINE_SMOOTH));
    m_gameScene.render();
#ifdef CAMERA_TRACK
    //if (recordCam)
    //{
    //    const auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
    //    m_cameraDebugPoints[m_currentCamera].emplace_back(tx.getRotation(), tx.getPosition());
    //}
#endif
    glCheck(glDisable(GL_LINE_SMOOTH));
#ifdef CRO_DEBUG_
    //m_collisionMesh.renderDebug(cam.getActivePass().viewProjectionMatrix, m_gameSceneTexture.getSize());
#endif
    m_renderTarget.display();

    cro::Entity nightCam;

    //update mini green if ball is there
    if (m_currentPlayer.terrain == TerrainID::Green)
    {
        glUseProgram(m_gridShaders[1].shaderID);
        glUniform1f(m_gridShaders[1].transparency, 0.f);

        auto oldCam = m_gameScene.setActiveCamera(m_greenCam);
        m_overheadBuffer.clear();
        m_gameScene.render();
        m_overheadBuffer.display();
        m_gameScene.setActiveCamera(oldCam);
        nightCam = m_greenCam;
    }
    else if (m_flightCam.getComponent<cro::Camera>().active)
    {
        auto resolutionData = m_resolutionUpdate.resolutionData;
        resolutionData.nearFadeDistance = 0.15f;
        m_resolutionBuffer.setData(resolutionData);

        m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Flight]);

        //update the flight view
        auto oldCam = m_gameScene.setActiveCamera(m_flightCam);
        m_overheadBuffer.clear(cro::Colour::Magenta);
        m_skyScene.render();
        glClear(GL_DEPTH_BUFFER_BIT);
        m_gameScene.render();
        m_overheadBuffer.display();
        m_gameScene.setActiveCamera(oldCam);

        m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Main]);
        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);

        nightCam = m_flightCam;
    }
 

    //TODO remove conditional branch?
    if (m_sharedData.nightTime)
    {
        auto& lightVolSystem = *m_gameScene.getSystem<cro::LightVolumeSystem>();
        lightVolSystem.setSourceBuffer(m_gameSceneMRTexture.getTexture(MRTIndex::Normal), cro::LightVolumeSystem::BufferID::Normal);
        lightVolSystem.setSourceBuffer(m_gameSceneMRTexture.getTexture(MRTIndex::Position), cro::LightVolumeSystem::BufferID::Position);
        lightVolSystem.updateTarget(m_gameScene.getActiveCamera(), m_lightMaps[LightMapID::Scene]);

        if (nightCam.isValid())
        {
            lightVolSystem.setSourceBuffer(m_overheadBuffer.getTexture(MRTIndex::Normal), cro::LightVolumeSystem::BufferID::Normal);
            lightVolSystem.setSourceBuffer(m_overheadBuffer.getTexture(MRTIndex::Position), cro::LightVolumeSystem::BufferID::Position);
            lightVolSystem.updateTarget(nightCam, m_lightMaps[LightMapID::Overhead]);
        }
    }


#ifndef CRO_DEBUG_
    if (m_roundEnded /* && !m_sharedData.tutorial */)
#endif
    {
        m_trophySceneTexture.clear(cro::Colour::Transparent);
        m_trophyScene.render();
        m_trophySceneTexture.display();
    }

    //m_uiScene.setActiveCamera(uiCam);
    m_uiScene.render();
}

//private
void GolfState::loadAssets()
{
    m_reflectionMap.loadFromFile("assets/golf/images/skybox/billiards/trophy.ccm");

    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    //load materials
    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }

    //cel shaded material
    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n" + wobble);
    auto* shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelSkinned, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n#define SKINNED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelSkinned] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Flag, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define SKINNED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Flag);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Flag] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Ball, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Ball);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Ball] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Trophy, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define REFLECTIONS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Trophy);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Trophy] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Trophy]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]).setProperty("u_noiseTexture", noiseTex);

    //custom shadow map so shadows move with wind too...
    m_resources.shaders.loadFromString(ShaderID::ShadowMap, ShadowVertex, ShadowFragment, "#define DITHERED\n#define WIND_WARP\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::ShadowMap);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::ShadowMap] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::ShadowMap]).setProperty("u_noiseTexture", noiseTex);

    m_resources.shaders.loadFromString(ShaderID::BillboardShadow, BillboardVertexShader, ShadowFragment, "#define DITHERED\n#define SHADOW_MAPPING\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::BillboardShadow);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::Leaderboard, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define SUBRECT\n");
    shader = &m_resources.shaders.get(ShaderID::Leaderboard);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Leaderboard] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinned, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SKINNED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedSkinned] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Player, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define SKINNED\n#define NOCHEX\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Player);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Player] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Hair, CelVertexShader, CelFragmentShader, "#define USER_COLOUR\n#define NOCHEX\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Hair);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Hair] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TERRAIN\n#define COMP_SHADE\n#define COLOUR_LEVELS 5.0\n#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Course] = m_resources.materials.add(*shader);


    m_resources.shaders.loadFromString(ShaderID::CourseGreen, CelVertexShader, CelFragmentShader, "#define HOLE_HEIGHT\n#define TERRAIN\n#define COMP_SHADE\n#define COLOUR_LEVELS 5.0\n#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CourseGreen);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    auto* greenShader = shader; //greens use wither this or the gridShader below for their material (set after parsing hole data)
    m_gridShaders[0].shaderID = shader->getGLHandle();
    m_gridShaders[0].transparency = shader->getUniformID("u_transparency");
    m_gridShaders[0].holeHeight = shader->getUniformID("u_holeHeight");


    m_resources.shaders.loadFromString(ShaderID::CourseGrid, CelVertexShader, CelFragmentShader, "#define HOLE_HEIGHT\n#define TEXTURED\n#define RX_SHADOWS\n#define CONTOUR\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CourseGrid);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    auto* gridShader = shader; //used below when testing for putt holes.
    m_gridShaders[1].shaderID = shader->getGLHandle();
    m_gridShaders[1].transparency = shader->getUniformID("u_transparency");
    m_gridShaders[1].holeHeight = shader->getUniformID("u_holeHeight");

    if (m_sharedData.nightTime)
    {
        m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader, "#define USE_MRT\n");;
    }
    else
    {
        m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader);
    }
    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);


    //shaders used by terrain
    m_resources.shaders.loadFromString(ShaderID::CelTexturedInstanced, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::CelTexturedInstanced);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::ShadowMapInstanced, ShadowVertex, ShadowFragment, "#define DITHERED\n#define WIND_WARP\n#define ALPHA_CLIP\n#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::ShadowMapInstanced);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::Crowd, CelVertexShader, CelFragmentShader, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define NOCHEX\n#define TEXTURED\n");
    shader = &m_resources.shaders.get(ShaderID::Crowd);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::CrowdShadow, ShadowVertex, ShadowFragment, "#define DITHERED\n#define INSTANCING\n#define VATS\n");
    m_resolutionBuffer.addShader(m_resources.shaders.get(ShaderID::CrowdShadow));

    m_resources.shaders.loadFromString(ShaderID::CrowdArray, CelVertexShader, CelFragmentShader, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define NOCHEX\n#define TEXTURED\n#define ARRAY_MAPPING\n");
    shader = &m_resources.shaders.get(ShaderID::CrowdArray);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::CrowdShadowArray, ShadowVertex, ShadowFragment, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define ARRAY_MAPPING\n");
    m_resolutionBuffer.addShader(m_resources.shaders.get(ShaderID::CrowdShadowArray));


    if (m_sharedData.treeQuality == SharedStateData::High)
    {
        std::string mrt;
        if (m_sharedData.nightTime)
        {
            mrt = "#define USE_MRT\n";
        }

        m_resources.shaders.loadFromString(ShaderID::TreesetBranch, BranchVertex, BranchFragment, "#define ALPHA_CLIP\n#define INSTANCING\n" + wobble + mrt);
        shader = &m_resources.shaders.get(ShaderID::TreesetBranch);
        m_scaleBuffer.addShader(*shader);
        m_resolutionBuffer.addShader(*shader);
        m_windBuffer.addShader(*shader);

        m_resources.shaders.loadFromString(ShaderID::TreesetLeaf, BushVertex, /*BushGeom,*/ BushFragment, "#define POINTS\n#define INSTANCING\n#define HQ\n" + wobble + mrt);
        shader = &m_resources.shaders.get(ShaderID::TreesetLeaf);
        m_scaleBuffer.addShader(*shader);
        m_resolutionBuffer.addShader(*shader);
        m_windBuffer.addShader(*shader);


        m_resources.shaders.loadFromString(ShaderID::TreesetShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define TREE_WARP\n#define ALPHA_CLIP\n" + wobble);
        shader = &m_resources.shaders.get(ShaderID::TreesetShadow);
        m_windBuffer.addShader(*shader);
        //m_resolutionBuffer.addShader(*shader);

        std::string alphaClip;
        if (m_sharedData.hqShadows)
        {
            alphaClip = "#define ALPHA_CLIP\n";
        }
        m_resources.shaders.loadFromString(ShaderID::TreesetLeafShadow, ShadowVertex, /*ShadowGeom,*/ ShadowFragment, "#define POINTS\n#define INSTANCING\n#define LEAF_SIZE\n" + alphaClip + wobble);
        shader = &m_resources.shaders.get(ShaderID::TreesetLeafShadow);
        m_windBuffer.addShader(*shader);
        //m_resolutionBuffer.addShader(*shader);
    }


    //scanline transition
    m_resources.shaders.loadFromString(ShaderID::Transition, MinimapVertex, ScanlineTransition);

    //noise effect
    m_resources.shaders.loadFromString(ShaderID::Noise, MinimapVertex, NoiseFragment);
    shader = &m_resources.shaders.get(ShaderID::Noise);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_postProcesses[PostID::Noise].shader = shader;

    //fog
    std::string desat;
    if (Social::isGreyscale())
    {
        desat = "#define DESAT 1.0\n";
    }
    if (m_sharedData.nightTime)
    {
        desat += "#define LIGHT_COLOUR\n";
    }
    m_resources.shaders.loadFromString(ShaderID::Fog, FogVert, FogFrag, "#define ZFAR 320.0\n" + desat);
    shader = &m_resources.shaders.get(ShaderID::Fog);
    m_postProcesses[PostID::Fog].shader = shader;
    //depth uniform is set after creating the UI once we know the render texture is created

    //wireframe
    m_resources.shaders.loadFromString(ShaderID::Wireframe, WireframeVertex, WireframeFragment);
    m_materialIDs[MaterialID::WireFrame] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Wireframe));
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]).blendMode = cro::Material::BlendMode::Alpha;

    m_resources.shaders.loadFromString(ShaderID::WireframeCulled, WireframeVertex, WireframeFragment, "#define CULLED\n");
    m_materialIDs[MaterialID::WireFrameCulled] = m_resources.materials.add(m_resources.shaders.get(ShaderID::WireframeCulled));
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]).blendMode = cro::Material::BlendMode::Alpha;
    //shader = &m_resources.shaders.get(ShaderID::WireframeCulled);
    //m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::BallTrail, WireframeVertex, WireframeFragment, "#define HUE\n");
    m_materialIDs[MaterialID::BallTrail] = m_resources.materials.add(m_resources.shaders.get(ShaderID::BallTrail));
    m_resources.materials.get(m_materialIDs[MaterialID::BallTrail]).blendMode = cro::Material::BlendMode::Additive;
    m_resources.materials.get(m_materialIDs[MaterialID::BallTrail]).setProperty("u_colourRotation", m_sharedData.beaconColour);

    //minimap - green overhead
    m_resources.shaders.loadFromString(ShaderID::Minimap, MinimapVertex, MinimapFragment);
    shader = &m_resources.shaders.get(ShaderID::Minimap);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    //minimap - course view
    m_resources.shaders.loadFromString(ShaderID::MinimapView, MinimapViewVertex, MinimapViewFragment);
    shader = &m_resources.shaders.get(ShaderID::MinimapView);
    m_minimapZoom.shaderID = shader->getGLHandle();
    m_minimapZoom.matrixUniformID = shader->getUniformID("u_coordMatrix");

    //water
    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment);
    shader = &m_resources.shaders.get(ShaderID::Water);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(*shader);
    //m_resources.materials.get(m_materialIDs[MaterialID::Water]).setProperty("u_noiseTexture", noiseTex);
    //forces rendering last to reduce overdraw - overdraws stroke indicator though(??)
    //also suffers the black banding effect where alpha  < 1
    //m_resources.materials.get(m_materialIDs[MaterialID::Water]).blendMode = cro::Material::BlendMode::Alpha; 
    
    //this version is affected by the sunlight colour of the scene
    m_resources.shaders.loadFromString(ShaderID::HorizonSun, HorizonVert, HorizonFrag, "#define SUNLIGHT\n");
    shader = &m_resources.shaders.get(ShaderID::HorizonSun);
    m_materialIDs[MaterialID::HorizonSun] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Horizon, HorizonVert, HorizonFrag);
    shader = &m_resources.shaders.get(ShaderID::Horizon);
    m_materialIDs[MaterialID::Horizon] = m_resources.materials.add(*shader);

    //mmmm... bacon
    m_resources.shaders.loadFromString(ShaderID::Beacon, BeaconVertex, BeaconFragment, "#define TEXTURED\n");
    m_materialIDs[MaterialID::Beacon] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Beacon));


    //model definitions
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }
    m_modelDefs[ModelID::BallShadow]->loadFromFile("assets/golf/models/ball_shadow.cmt");
    m_modelDefs[ModelID::PlayerShadow]->loadFromFile("assets/golf/models/player_shadow.cmt");
    m_modelDefs[ModelID::BullsEye]->loadFromFile("assets/golf/models/target.cmt"); //TODO we can only load this if challenge month or game mode requires

    //ball models - the menu should never have let us get this far if it found no ball files
    for (const auto& [colour, uid, path, _1, _2] : m_sharedData.ballInfo)
    {
        std::unique_ptr<cro::ModelDefinition> def = std::make_unique<cro::ModelDefinition>(m_resources);
        m_ballModels.insert(std::make_pair(uid, std::move(def)));
    }

    //UI stuffs
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    m_sprites[SpriteID::PowerBar] = spriteSheet.getSprite("power_bar_wide");
    m_sprites[SpriteID::PowerBarInner] = spriteSheet.getSprite("power_bar_inner_wide");
    m_sprites[SpriteID::HookBar] = spriteSheet.getSprite("hook_bar");
    m_sprites[SpriteID::SlopeStrength] = spriteSheet.getSprite("slope_indicator");
    m_sprites[SpriteID::BallSpeed] = spriteSheet.getSprite("ball_speed");
    m_sprites[SpriteID::MapFlag] = spriteSheet.getSprite("flag03");
    m_sprites[SpriteID::MapTarget] = spriteSheet.getSprite("multitarget");
    m_sprites[SpriteID::MiniFlag] = spriteSheet.getSprite("putt_flag");
    m_sprites[SpriteID::WindIndicator] = spriteSheet.getSprite("wind_dir");
    m_sprites[SpriteID::WindSpeed] = spriteSheet.getSprite("wind_speed");
    m_sprites[SpriteID::WindSpeedBg] = spriteSheet.getSprite("wind_text_bg");
    m_sprites[SpriteID::Thinking] = spriteSheet.getSprite("thinking");
    m_sprites[SpriteID::MessageBoard] = spriteSheet.getSprite("message_board");
    m_sprites[SpriteID::Bunker] = spriteSheet.getSprite("bunker");
    m_sprites[SpriteID::Foul] = spriteSheet.getSprite("foul");
    m_sprites[SpriteID::SpinBg] = spriteSheet.getSprite("spin_bg");
    m_sprites[SpriteID::SpinFg] = spriteSheet.getSprite("spin_fg");
    m_sprites[SpriteID::BirdieLeft] = spriteSheet.getSprite("birdie_left");
    m_sprites[SpriteID::BirdieRight] = spriteSheet.getSprite("birdie_right");
    m_sprites[SpriteID::EagleLeft] = spriteSheet.getSprite("eagle_left");
    m_sprites[SpriteID::EagleRight] = spriteSheet.getSprite("eagle_right");
    m_sprites[SpriteID::AlbatrossLeft] = spriteSheet.getSprite("albatross_left");
    m_sprites[SpriteID::AlbatrossRight] = spriteSheet.getSprite("albatross_right");
    m_sprites[SpriteID::Hio] = spriteSheet.getSprite("hio");

    spriteSheet.loadFromFile("assets/golf/sprites/bounce.spt", m_resources.textures);
    m_sprites[SpriteID::BounceAnim] = spriteSheet.getSprite("bounce");


    spriteSheet.loadFromFile("assets/golf/sprites/emotes.spt", m_resources.textures);
    m_sprites[SpriteID::EmoteHappy] = spriteSheet.getSprite("happy_small");
    m_sprites[SpriteID::EmoteGrumpy] = spriteSheet.getSprite("grumpy_small");
    m_sprites[SpriteID::EmoteLaugh] = spriteSheet.getSprite("laughing_small");
    m_sprites[SpriteID::EmoteSad] = spriteSheet.getSprite("sad_small");
    m_sprites[SpriteID::EmotePulse] = spriteSheet.getSprite("pulse");


    //load audio from avatar info
    for (const auto& avatar : m_sharedData.avatarInfo)
    {
        m_gameScene.getDirector<GolfSoundDirector>()->addAudioScape(avatar.audioscape, m_resources.audio);
    }

    //TODO we don't actually need to load *every* sprite sheet, just look up the index first
    //and load it as necessary...
    //however: while it may load unnecessary audioscapes, it does ensure they are loaded in the correct order :S

    //copy into active player slots
    const auto indexFromSkinID = [&](std::uint32_t skinID)->std::size_t
    {
        auto result = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
            [skinID](const SharedStateData::AvatarInfo& ai) 
            {
                return skinID == ai.uid;
            });

        if (result != m_sharedData.avatarInfo.end())
        {
            return std::distance(m_sharedData.avatarInfo.begin(), result);
        }
        return 0;
    };

    const auto indexFromHairID = [&](std::uint32_t hairID)
    {
        if (auto hair = std::find_if(m_sharedData.hairInfo.begin(), m_sharedData.hairInfo.end(),
            [hairID](const SharedStateData::HairInfo& h) {return h.uid == hairID;});
            hair != m_sharedData.hairInfo.end())
        {
            return static_cast<std::int32_t>(std::distance(m_sharedData.hairInfo.begin(), hair));
        }

        return 0;
    };

    //player avatars
    cro::ModelDefinition md(m_resources);
    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            auto skinID = m_sharedData.connectionData[i].playerData[j].skinID;
            auto avatarIndex = indexFromSkinID(skinID);

            m_gameScene.getDirector<GolfSoundDirector>()->setPlayerIndex(i, j, static_cast<std::int32_t>(avatarIndex));
            m_avatars[i][j].flipped = m_sharedData.connectionData[i].playerData[j].flipped;

            //player avatar model
            //TODO we might want error checking here, but the model files
            //should have been validated by the menu state.
            if (md.loadFromFile(m_sharedData.avatarInfo[avatarIndex].modelPath))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>();
                entity.addComponent<cro::Callback>().setUserData<PlayerCallbackData>();
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                {
                    auto& [direction, scale] = e.getComponent<cro::Callback>().getUserData<PlayerCallbackData>();
                    const auto xScale = e.getComponent<cro::Transform>().getScale().x; //might be flipped

                    if (direction == 0)
                    {
                        scale = std::min(1.f, scale + (dt * 3.f));
                        if (scale == 1)
                        {
                            direction = 1;
                            e.getComponent<cro::Callback>().active = false;
                        }
                    }
                    else
                    {
                        scale = std::max(0.f, scale - (dt * 3.f));

                        if (scale == 0)
                        {
                            direction = 0;
                            e.getComponent<cro::Callback>().active = false;
                            e.getComponent<cro::Model>().setHidden(true);

                            //seems counter-intuitive to search the avatar here
                            //but we have a, uh.. 'handy' handle (to the hands)
                            if (m_activeAvatar->hands)
                            {
                                //we have to free this up alse the model might
                                //become attached to two avatars...
                                m_activeAvatar->hands->setModel({});
                            }
                        }
                    }
                    auto yScale = cro::Util::Easing::easeOutBack(scale);
                    e.getComponent<cro::Transform>().setScale(glm::vec3(xScale, yScale, yScale));
                };
                md.createModel(entity);

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Player]);
                material.setProperty("u_diffuseMap", m_sharedData.avatarTextures[i][j]);
                entity.getComponent<cro::Model>().setMaterial(0, material);

                if (m_avatars[i][j].flipped)
                {
                    entity.getComponent<cro::Transform>().setScale({ -1.f, 0.f, 0.f });
                    entity.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                }
                else
                {
                    entity.getComponent<cro::Transform>().setScale({ 1.f, 0.f, 0.f });
                }

                //this should assert in debug, however oor IDs will just be ignored
                //in release so this is the safest way to handle missing animations
                std::fill(m_avatars[i][j].animationIDs.begin(), m_avatars[i][j].animationIDs.end(), AnimationID::Invalid);
                if (entity.hasComponent<cro::Skeleton>())
                {
                    auto& skel = entity.getComponent<cro::Skeleton>();
                    
                    //find attachment points for club model
                    auto id = skel.getAttachmentIndex("hands");
                    if (id > -1)
                    {
                        m_avatars[i][j].hands = &skel.getAttachments()[id];
                    }                    
                    
                    const auto& anims = skel.getAnimations();
                    for (auto k = 0u; k < std::min(anims.size(), static_cast<std::size_t>(AnimationID::Count)); ++k)
                    {
                        if (anims[k].name == "idle")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Idle] = k;
                            skel.play(k);
                        }
                        else if (anims[k].name == "drive")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Swing] = k;
                        }
                        else if (anims[k].name == "chip")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Chip] = k;
                        }
                        else if (anims[k].name == "putt")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Putt] = k;
                        }
                        else if (anims[k].name == "celebrate")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Celebrate] = k;
                        }
                        else if (anims[k].name == "disappointment")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Disappoint] = k;
                        }
                        else if (anims[k].name == "impatient")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Impatient] = k;
                        }
                    }

                    //and attachment for hair/hats
                    id = skel.getAttachmentIndex("head");
                    if (id > -1)
                    {
                        //look to see if we have a hair model to attach
                        auto hairID = indexFromHairID(m_sharedData.connectionData[i].playerData[j].hairID);
                        if (hairID != 0
                            && md.loadFromFile(m_sharedData.hairInfo[hairID].modelPath))
                        {
                            auto hairEnt = m_gameScene.createEntity();
                            hairEnt.addComponent<cro::Transform>();
                            md.createModel(hairEnt);

                            //set material and colour
                            material = m_resources.materials.get(m_materialIDs[MaterialID::Hair]);
                            applyMaterialData(md, material); //applies double sidedness
                            material.setProperty("u_hairColour", pc::Palette[m_sharedData.connectionData[i].playerData[j].avatarFlags[pc::ColourKey::Hair]]);
                            hairEnt.getComponent<cro::Model>().setMaterial(0, material);
                            hairEnt.getComponent<cro::Model>().setRenderFlags(~RenderFlags::CubeMap);

                            skel.getAttachments()[id].setModel(hairEnt);

                            if (m_avatars[i][j].flipped)
                            {
                                hairEnt.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                            }

                            hairEnt.addComponent<cro::Callback>().active = true;
                            hairEnt.getComponent<cro::Callback>().function =
                                [entity](cro::Entity e, float)
                            {
                                e.getComponent<cro::Model>().setHidden(entity.getComponent<cro::Model>().isHidden());
                            };
                        }
                    }
                }

                entity.getComponent<cro::Model>().setHidden(true);
                entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::CubeMap);
                m_avatars[i][j].model = entity;
            }
        }
    }
    //m_activeAvatar = &m_avatars[0][0]; //DON'T DO THIS! WE MUST BE NULL WHEN THE MAP LOADS

    //club models
    m_clubModels[ClubModel::Wood] = m_gameScene.createEntity();
    m_clubModels[ClubModel::Wood].addComponent<cro::Transform>();
    if (md.loadFromFile("assets/golf/models/club_wood.cmt"))
    {
        md.createModel(m_clubModels[ClubModel::Wood]);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material, 0);
        m_clubModels[ClubModel::Wood].getComponent<cro::Model>().setMaterial(0, material);
        m_clubModels[ClubModel::Wood].getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::CubeMap));
    }
    else
    {
        createFallbackModel(m_clubModels[ClubModel::Wood], m_resources);
    }
    m_clubModels[ClubModel::Wood].addComponent<cro::Callback>().active = true;
    m_clubModels[ClubModel::Wood].getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_activeAvatar)
        {
            bool hidden = !(!m_activeAvatar->model.getComponent<cro::Model>().isHidden() &&
                m_activeAvatar->hands->getModel() == e);

            e.getComponent<cro::Model>().setHidden(hidden);
        }
    };


    m_clubModels[ClubModel::Iron] = m_gameScene.createEntity();
    m_clubModels[ClubModel::Iron].addComponent<cro::Transform>();
    if (md.loadFromFile("assets/golf/models/club_iron.cmt"))
    {
        md.createModel(m_clubModels[ClubModel::Iron]);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material, 0);
        m_clubModels[ClubModel::Iron].getComponent<cro::Model>().setMaterial(0, material);
        m_clubModels[ClubModel::Iron].getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::CubeMap));
    }
    else
    {
        createFallbackModel(m_clubModels[ClubModel::Iron], m_resources);
        m_clubModels[ClubModel::Iron].getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Cyan);
    }
    m_clubModels[ClubModel::Iron].addComponent<cro::Callback>().active = true;
    m_clubModels[ClubModel::Iron].getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_activeAvatar)
        {
            bool hidden = !(!m_activeAvatar->model.getComponent<cro::Model>().isHidden() &&
                m_activeAvatar->hands->getModel() == e);

            e.getComponent<cro::Model>().setHidden(hidden);
        }
    };


    //ball resources - ball is rendered as a single point
    //at a distance, and as a model when closer
    //glCheck(glPointSize(BallPointSize)); - this is set in resize callback based on the buffer resolution/pixel scale
    m_ballResources.materialID = m_materialIDs[MaterialID::WireFrameCulled];
    m_ballResources.ballMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    m_ballResources.shadowMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(m_ballResources.ballMeshID);
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,   1.f, 1.f, 1.f, 1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0
    };

    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData = &m_resources.meshes.getMesh(m_ballResources.shadowMeshID);
    verts =
    {
        0.f, 0.f, 0.f,    0.f, 0.f, 0.f, 0.25f,
    };
    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    m_ballTrail.create(m_gameScene, m_resources, m_materialIDs[MaterialID::BallTrail]);
    m_ballTrail.setUseBeaconColour(m_sharedData.trailBeaconColour);


    //used when parsing holes
    auto addCrowd = [&](HoleData& holeData, glm::vec3 position, glm::vec3 lookAt, float rotation)
    {
        constexpr auto MapOrigin = glm::vec3(MapSize.x / 2.f, 0.f, -static_cast<float>(MapSize.y) / 2.f);

        //used by terrain builder to create instanced geom
        glm::vec3 offsetPos(-8.f, 0.f, 0.f);
        const glm::mat4 rotMat = glm::rotate(glm::mat4(1.f), rotation * cro::Util::Const::degToRad, cro::Transform::Y_AXIS);

        for (auto i = 0; i < 16; ++i)
        {
            auto offset = glm::vec3(rotMat * glm::vec4(offsetPos, 1.f));

            auto tx = glm::translate(glm::mat4(1.f), position - MapOrigin);
            tx = glm::translate(tx, offset);

            auto lookDir = lookAt - (glm::vec3(tx[3]) + MapOrigin);
            if (float len = glm::length2(lookDir); len < 1600.f)
            {
                rotation = std::atan2(-lookDir.z, lookDir.x) + (90.f * cro::Util::Const::degToRad);
                tx = glm::rotate(tx, rotation, glm::vec3(0.f, 1.f, 0.f));
            }
            else
            {
                tx = glm::rotate(tx, cro::Util::Random::value(-0.25f, 0.25f) + (rotation * cro::Util::Const::degToRad), glm::vec3(0.f, 1.f, 0.f));
            }

            float scale = static_cast<float>(cro::Util::Random::value(95, 110)) / 100.f;
            tx = glm::scale(tx, glm::vec3(scale));

            holeData.crowdPositions.push_back(tx);

            offsetPos.x += 0.3f + (static_cast<float>(cro::Util::Random::value(2, 5)) / 10.f);
            offsetPos.z = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;
        }        
    };


    //load the map data
    bool error = false;
    bool hasSpectators = false;
    auto mapDir = m_sharedData.mapDirectory.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    bool isUser = false;
    if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + mapPath))
    {
        auto coursePath = cro::App::getPreferencePath() + ConstVal::UserMapPath;
        if (!cro::FileSystem::directoryExists(coursePath))
        {
            cro::FileSystem::createDirectory(coursePath);
        }

        mapPath = cro::App::getPreferencePath() + ConstVal::UserMapPath + mapDir + "/course.data";
        isUser = true;

        if (!cro::FileSystem::fileExists(mapPath))
        {
            LOG("Course file doesn't exist", cro::Logger::Type::Error);
            error = true;
        }
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath, !isUser))
    {
        error = true;
    }

    if (auto* title = courseFile.findProperty("title"); title)
    {
        m_courseTitle = title->getValue<cro::String>();
    }

    std::string skyboxPath;

    ThemeSettings theme;
    std::vector<std::string> holeStrings;
    const auto& props = courseFile.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "hole"
            && holeStrings.size() < MaxHoles)
        {
            holeStrings.push_back(prop.getValue<std::string>());
        }
        else if (name == "skybox")
        {
            skyboxPath = prop.getValue<std::string>();

            //if set to night check for night path (appended with _n)
            if (m_sharedData.nightTime)
            {
                auto ext = cro::FileSystem::getFileExtension(skyboxPath);
                auto nightPath = skyboxPath.substr(0, skyboxPath.find(ext)) + "_n" + ext;
                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + nightPath))
                {
                    skyboxPath = nightPath;

                    m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour(SkyNight);
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(SkyNight);
                }
            }
        }
        else if (name == "shrubbery")
        {
            cro::ConfigFile shrubbery;
            if (shrubbery.loadFromFile(prop.getValue<std::string>()))
            {
                const auto& shrubProps = shrubbery.getProperties();
                for (const auto& sp : shrubProps)
                {
                    const auto& shrubName = sp.getName();
                    if (shrubName == "model")
                    {
                        theme.billboardModel = sp.getValue<std::string>();
                    }
                    else if (shrubName == "sprite")
                    {
                        theme.billboardSprites = sp.getValue<std::string>();
                    }
                    else if (shrubName == "treeset")
                    {
                        Treeset ts;
                        if (theme.treesets.size() < ThemeSettings::MaxTreeSets
                            && ts.loadFromFile(sp.getValue<std::string>()))
                        {
                            theme.treesets.push_back(ts);
                        }
                    }
                    else if (shrubName == "grass")
                    {
                        theme.grassColour = sp.getValue<cro::Colour>();
                    }
                    else if (shrubName == "grass_tint") 
                    {
                        theme.grassTint = sp.getValue<cro::Colour>();
                    }
                }
            }
        }
        else if (name == "audio")
        {
            auto audioPath = prop.getValue<std::string>();

            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + audioPath))
            {
                m_audioPath = audioPath;
            }
        }
        else if (name == "instance_model")
        {
            theme.instancePath = prop.getValue<std::string>();
        }
    }
    
    //use old sprites if user so wishes
    if (m_sharedData.treeQuality == SharedStateData::Classic)
    {
        std::string classicModel;
        std::string classicSprites;

        //assume we have the correct file extension, because it's an invalid path anyway if not.
        classicModel = theme.billboardModel.substr(0,theme.billboardModel.find_last_of('.')) + "_low.cmt";
        classicSprites = theme.billboardSprites.substr(0,theme.billboardSprites.find_last_of('.')) + "_low.spt";

        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + classicModel))
        {
            classicModel.clear();
        }
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + classicSprites))
        {
            classicSprites.clear();
        }

        if (!classicModel.empty() && !classicSprites.empty())
        {
            theme.billboardModel = classicModel;
            theme.billboardSprites = classicSprites;
        }
    }

    SkyboxMaterials materials;
    materials.horizon = m_materialIDs[MaterialID::Horizon];
    materials.horizonSun = m_materialIDs[MaterialID::HorizonSun];
    materials.skinned = m_materialIDs[MaterialID::CelTexturedSkinned];

    auto cloudRing = loadSkybox(skyboxPath, m_skyScene, m_resources, materials);
    if (cloudRing.isValid()
        && cloudRing.hasComponent<cro::Model>())
    {
        m_resources.shaders.loadFromString(ShaderID::CloudRing, CloudOverheadVertex, CloudOverheadFragment, "#define REFLECTION\n#define POINT_LIGHT\n");
        auto& shader = m_resources.shaders.get(ShaderID::CloudRing);

        auto matID = m_resources.materials.add(shader);
        auto material = m_resources.materials.get(matID);
        material.setProperty("u_skyColourTop", m_skyScene.getSkyboxColours().top);
        material.setProperty("u_skyColourBottom", m_skyScene.getSkyboxColours().middle);
        cloudRing.getComponent<cro::Model>().setMaterial(0, material);
    }

    if (m_sharedData.nightTime)
    {
        auto skyDark = SkyBottom.getVec4() * SkyNight.getVec4();
        auto colours = m_skyScene.getSkyboxColours();
        colours.bottom = skyDark;
        m_skyScene.setSkyboxColours(colours);
    }

#ifdef CRO_DEBUG_
    auto& colours = m_skyScene.getSkyboxColours();
    topSky = colours.top.getVec4();
    bottomSky = colours.middle.getVec4();
#endif

    if (theme.billboardModel.empty()
        || !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + theme.billboardModel))
    {
        LogE << "Missing or invalid billboard model definition" << std::endl;
        error = true;
    }
    if (theme.billboardSprites.empty()
        || !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + theme.billboardSprites))
    {
        LogE << "Missing or invalid billboard sprite sheet" << std::endl;
        error = true;
    }
    if (holeStrings.empty())
    {
        LOG("No hole files in course data", cro::Logger::Type::Error);
        error = true;
    }

    //check the rules and truncate hole list
    //if requested - 1 front holes, 1 back holes
    if (m_sharedData.holeCount == 1)
    {
        auto size = std::max(std::size_t(1), holeStrings.size() / 2);
        holeStrings.resize(size);
    }
    else if (m_sharedData.holeCount == 2)
    {
        auto start = holeStrings.size() / 2;
        std::vector<std::string> newStrings(holeStrings.begin() + start, holeStrings.end());
        holeStrings.swap(newStrings);
    }

    //and reverse if rules request
    if (m_sharedData.reverseCourse)
    {
        std::reverse(holeStrings.begin(), holeStrings.end());
    }


    cro::ConfigFile holeCfg;
    cro::ModelDefinition modelDef(m_resources);
    std::string prevHoleString;
    cro::Entity prevHoleEntity;
    std::vector<cro::Entity> prevProps;
    std::vector<cro::Entity> prevParticles;
    std::vector<cro::Entity> prevAudio;
    std::vector<cro::Entity> leaderboardProps;
    std::int32_t holeModelCount = 0; //use this to get a guestimate of how many holes per model there are to adjust the camera offset

    cro::AudioScape propAudio;
    propAudio.loadFromFile("assets/golf/sound/props.xas", m_resources.audio);

    for (const auto& hole : holeStrings)
    {
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + hole))
        {
            LOG("Hole file is missing", cro::Logger::Type::Error);
            error = true;
        }

        if (!holeCfg.loadFromFile(hole))
        {
            LOG("Failed opening hole file", cro::Logger::Type::Error);
            error = true;
        }

        static constexpr std::int32_t MaxProps = 6;
        std::int32_t propCount = 0;
        auto& holeData = m_holeData.emplace_back();
        bool duplicate = false;

        const auto& holeProps = holeCfg.getProperties();
        for (const auto& holeProp : holeProps)
        {
            const auto& name = holeProp.getName();
            if (name == "map")
            {
                auto path = holeProp.getValue<std::string>();
                if (!m_currentMap.loadFromFile(path)
                    || m_currentMap.getFormat() != cro::ImageFormat::RGBA)
                {
                    LogE << path << ": image file not RGBA" << std::endl;
                    error = true;
                }
                holeData.mapPath = holeProp.getValue<std::string>();
                propCount++;
            }
            else if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                holeData.pin = holeProp.getValue<glm::vec3>();
                holeData.pin.x = glm::clamp(holeData.pin.x, 0.f, 320.f);
                holeData.pin.z = glm::clamp(holeData.pin.z, -200.f, 0.f);
                propCount++;
            }
            else if (name == "tee")
            {
                holeData.tee = holeProp.getValue<glm::vec3>();
                holeData.tee.x = glm::clamp(holeData.tee.x, 0.f, 320.f);
                holeData.tee.z = glm::clamp(holeData.tee.z, -200.f, 0.f);
                propCount++;
            }
            else if (name == "target")
            {
                holeData.target = holeProp.getValue<glm::vec3>();
                if (glm::length2(holeData.target) > 0)
                {
                    propCount++;
                }
            }
            else if (name == "par")
            {
                holeData.par = holeProp.getValue<std::int32_t>();
                if (holeData.par < 1 || holeData.par > 10)
                {
                    LOG("Invalid PAR value", cro::Logger::Type::Error);
                    error = true;
                }
                propCount++;
            }
            else if (name == "model")
            {
                auto modelPath = holeProp.getValue<std::string>();

                if (modelPath != prevHoleString)
                {
                    //attept to load model
                    if (modelDef.loadFromFile(modelPath))
                    {
                        holeData.modelPath = modelPath;

                        holeData.modelEntity = m_gameScene.createEntity();
                        holeData.modelEntity.addComponent<cro::Transform>().setPosition(OriginOffset);
                        holeData.modelEntity.getComponent<cro::Transform>().setOrigin(OriginOffset);
                        holeData.modelEntity.addComponent<cro::Callback>();
                        modelDef.createModel(holeData.modelEntity);
                        holeData.modelEntity.getComponent<cro::Model>().setHidden(true);
                        for (auto m = 0u; m < holeData.modelEntity.getComponent<cro::Model>().getMeshData().submeshCount; ++m)
                        {
                            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Course]);
                            applyMaterialData(modelDef, material, m);
                            holeData.modelEntity.getComponent<cro::Model>().setMaterial(m, material);
                        }
                        propCount++;

                        prevHoleString = modelPath;
                        prevHoleEntity = holeData.modelEntity;

                        holeModelCount++;
                    }
                    else
                    {
                        LOG("Failed loading model file", cro::Logger::Type::Error);
                        error = true;
                    }
                }
                else
                {
                    //duplicate the hole by copying the previous model entitity
                    holeData.modelPath = prevHoleString;
                    holeData.modelEntity = prevHoleEntity;
                    duplicate = true;
                    propCount++;
                }
            }
        }

        if (propCount != MaxProps)
        {
            LOG("Missing hole property", cro::Logger::Type::Error);
            error = true;
        }
        else
        {
            if (!duplicate) //this hole wasn't a duplicate of the previous
            {
                //look for prop models (are optional and can fail to load no problem)
                const auto& propObjs = holeCfg.getObjects();
                for (const auto& obj : propObjs)
                {
                    const auto& name = obj.getName();
                    if (name == "prop")
                    {
                        const auto& modelProps = obj.getProperties();
                        glm::vec3 position(0.f);
                        float rotation = 0.f;
                        glm::vec3 scale(1.f);
                        std::string path;

                        std::vector<glm::vec3> curve;
                        bool loopCurve = true;
                        float loopDelay = 4.f;
                        float loopSpeed = 6.f;

                        std::string particlePath;
                        std::string emitterName;

                        for (const auto& modelProp : modelProps)
                        {
                            auto propName = modelProp.getName();
                            if (propName == "position")
                            {
                                position = modelProp.getValue<glm::vec3>();
                            }
                            else if (propName == "model")
                            {
                                path = modelProp.getValue<std::string>();
                            }
                            else if (propName == "rotation")
                            {
                                rotation = modelProp.getValue<float>();
                            }
                            else if (propName == "scale")
                            {
                                scale = modelProp.getValue<glm::vec3>();
                            }
                            else if (propName == "particles")
                            {
                                particlePath = modelProp.getValue<std::string>();
                            }
                            else if (propName == "emitter")
                            {
                                emitterName = modelProp.getValue<std::string>();
                            }
                        }

                        const auto modelObjs = obj.getObjects();
                        for (const auto& o : modelObjs)
                        {
                            if (o.getName() == "path")
                            {
                                const auto points = o.getProperties();
                                for (const auto& p : points)
                                {
                                    if (p.getName() == "point")
                                    {
                                        curve.push_back(p.getValue<glm::vec3>());
                                    }
                                    else if (p.getName() == "loop")
                                    {
                                        loopCurve = p.getValue<bool>();
                                    }
                                    else if (p.getName() == "delay")
                                    {
                                        loopDelay = std::max(0.f, p.getValue<float>());
                                    }
                                    else if (p.getName() == "speed")
                                    {
                                        loopSpeed = std::max(0.f, p.getValue<float>());
                                    }
                                }

                                break;
                            }
                        }

                        if (!path.empty() && Social::isValid(path)
                            && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + path))
                        {
                            if (modelDef.loadFromFile(path))
                            {
                                auto ent = m_gameScene.createEntity();
                                ent.addComponent<cro::Transform>().setPosition(position);
                                ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                                ent.getComponent<cro::Transform>().setScale(scale);
                                modelDef.createModel(ent);
                                if (modelDef.hasSkeleton())
                                {
                                    for (auto i = 0u; i < modelDef.getMaterialCount(); ++i)
                                    {
                                        auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
                                        applyMaterialData(modelDef, texturedMat, i);
                                        ent.getComponent<cro::Model>().setMaterial(i, texturedMat);
                                    }

                                    auto& skel = ent.getComponent<cro::Skeleton>();
                                    if (!skel.getAnimations().empty())
                                    {
                                        //this is the default behaviour
                                        const auto& anims = skel.getAnimations();
                                        if (anims.size() == 1)
                                        {
                                            //ent.getComponent<cro::Skeleton>().play(0); // don't play this until unhidden
                                            skel.getAnimations()[0].looped = true;
                                            skel.setMaxInterpolationDistance(100.f);
                                        }
                                        //however spectator models need fancier animation
                                        //control... and probably a smaller interp distance
                                        else
                                        {
                                            //TODO we could improve this by disabling when hidden?
                                            ent.addComponent<cro::Callback>().active = true;
                                            ent.getComponent<cro::Callback>().function = SpectatorCallback(anims);
                                            ent.getComponent<cro::Callback>().setUserData<bool>(false);
                                            ent.addComponent<cro::CommandTarget>().ID = CommandID::Spectator;

                                            skel.setMaxInterpolationDistance(80.f);
                                        }
                                    }
                                }
                                else
                                {
                                    for (auto i = 0u; i < modelDef.getMaterialCount(); ++i)
                                    {
                                        auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                                        applyMaterialData(modelDef, texturedMat, i);
                                        ent.getComponent<cro::Model>().setMaterial(i, texturedMat);

                                        auto shadowMat = m_resources.materials.get(m_materialIDs[MaterialID::ShadowMap]);
                                        applyMaterialData(modelDef, shadowMat);
                                        //shadowMat.setProperty("u_alphaClip", 0.5f);
                                        ent.getComponent<cro::Model>().setShadowMaterial(i, shadowMat);
                                    }
                                }
                                ent.getComponent<cro::Model>().setHidden(true);
                                ent.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                                holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                                holeData.propEntities.push_back(ent);

                                //special case for leaderboard model, cos, y'know
                                if (cro::FileSystem::getFileName(path) == "leaderboard.cmt")
                                {
                                    leaderboardProps.push_back(ent);
                                }

                                //add path if it exists
                                if (curve.size() > 3)
                                {
                                    Path propPath;
                                    for (auto p : curve)
                                    {
                                        propPath.addPoint(p);
                                    }

                                    ent.addComponent<PropFollower>().path = propPath;
                                    ent.getComponent<PropFollower>().loop = loopCurve;
                                    ent.getComponent<PropFollower>().idleTime = loopDelay;
                                    ent.getComponent<PropFollower>().speed = loopSpeed;
                                    ent.getComponent<cro::Transform>().setPosition(curve[0]);
                                }

                                //add child particles if they exist
                                if (!particlePath.empty())
                                {
                                    cro::EmitterSettings settings;
                                    if (settings.loadFromFile(particlePath, m_resources.textures))
                                    {
                                        auto pEnt = m_gameScene.createEntity();
                                        pEnt.addComponent<cro::Transform>();
                                        pEnt.addComponent<cro::ParticleEmitter>().settings = settings;
                                        pEnt.getComponent<cro::ParticleEmitter>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                        pEnt.addComponent<cro::CommandTarget>().ID = CommandID::ParticleEmitter;
                                        ent.getComponent<cro::Transform>().addChild(pEnt.getComponent<cro::Transform>());
                                        holeData.particleEntities.push_back(pEnt);
                                    }
                                }

                                //and child audio
                                if (propAudio.hasEmitter(emitterName))
                                {
                                    struct AudioCallbackData final
                                    {
                                        glm::vec3 prevPos = glm::vec3(0.f);
                                        float fadeAmount = 0.f;
                                        float currentVolume = 0.f;
                                    };

                                    auto audioEnt = m_gameScene.createEntity();
                                    audioEnt.addComponent<cro::Transform>();
                                    audioEnt.addComponent<cro::AudioEmitter>() = propAudio.getEmitter(emitterName);
                                    auto baseVolume = audioEnt.getComponent<cro::AudioEmitter>().getVolume();
                                    audioEnt.addComponent<cro::CommandTarget>().ID = CommandID::AudioEmitter;
                                    audioEnt.addComponent<cro::Callback>().setUserData<AudioCallbackData>();
                                    
                                    if (ent.hasComponent<PropFollower>())
                                    {
                                        audioEnt.getComponent<cro::Callback>().function =
                                            [&, ent, baseVolume](cro::Entity e, float dt)
                                        {
                                            auto& [prevPos, fadeAmount, currentVolume] = e.getComponent<cro::Callback>().getUserData<AudioCallbackData>();
                                            auto pos = ent.getComponent<cro::Transform>().getPosition();
                                            auto velocity = (pos - prevPos) * 60.f; //frame time
                                            prevPos = pos;
                                            e.getComponent<cro::AudioEmitter>().setVelocity(velocity);

                                            const float speed = ent.getComponent<PropFollower>().speed + 0.001f; //prevent div0
                                            float pitch = std::min(1.f, glm::length2(velocity) / (speed * speed));
                                            e.getComponent<cro::AudioEmitter>().setPitch(pitch);

                                            //fades in when callback first started
                                            fadeAmount = std::min(1.f, fadeAmount + dt);

                                            //rather than just jump to volume, we move towards it for a
                                            //smoother fade
                                            auto targetVolume = (baseVolume * 0.1f) + (pitch * (baseVolume * 0.9f));
                                            auto diff = targetVolume - currentVolume;
                                            if (std::abs(diff) > 0.001f)
                                            {
                                                currentVolume += (diff * dt);
                                            }
                                            else
                                            {
                                                currentVolume = targetVolume;
                                            }

                                            e.getComponent<cro::AudioEmitter>().setVolume(currentVolume * fadeAmount);
                                        };
                                    }
                                    else
                                    {
                                        //add a dummy function which will still be updated on hole end to remove this ent
                                        audioEnt.getComponent<cro::Callback>().function = [](cro::Entity,float) {};
                                    }
                                    ent.getComponent<cro::Transform>().addChild(audioEnt.getComponent<cro::Transform>());
                                    holeData.audioEntities.push_back(audioEnt);
                                }
                            }
                        }
                    }
                    else if (name == "particles")
                    {
                        const auto& particleProps = obj.getProperties();
                        glm::vec3 position(0.f);
                        std::string path;

                        for (auto particleProp : particleProps)
                        {
                            auto propName = particleProp.getName();
                            if (propName == "path")
                            {
                                path = particleProp.getValue<std::string>();
                            }
                            else if (propName == "position")
                            {
                                position = particleProp.getValue<glm::vec3>();
                            }
                        }

                        if (!path.empty())
                        {
                            cro::EmitterSettings settings;
                            if (settings.loadFromFile(path, m_resources.textures))
                            {
                                auto ent = m_gameScene.createEntity();
                                ent.addComponent<cro::Transform>().setPosition(position);
                                ent.addComponent<cro::ParticleEmitter>().settings = settings;
                                ent.getComponent<cro::ParticleEmitter>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                ent.addComponent<cro::CommandTarget>().ID = CommandID::ParticleEmitter;
                                holeData.particleEntities.push_back(ent);
                                holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                            }
                        }
                    }
                    else if (name == "crowd")
                    {
                        const auto& modelProps = obj.getProperties();
                        glm::vec3 position(0.f);
                        float rotation = 0.f;
                        glm::vec3 lookAt = holeData.pin;

                        for (const auto& modelProp : modelProps)
                        {
                            auto propName = modelProp.getName();
                            if (propName == "position")
                            {
                                position = modelProp.getValue<glm::vec3>();
                            }
                            else if (propName == "rotation")
                            {
                                rotation = modelProp.getValue<float>();
                            }
                            else if (propName == "lookat")
                            {
                                lookAt = modelProp.getValue<glm::vec3>();
                            }
                        }

                        std::vector<glm::vec3> curve;
                        const auto& modelObjs = obj.getObjects();
                        for (const auto& o : modelObjs)
                        {
                            if (o.getName() == "path")
                            {
                                const auto& points = o.getProperties();
                                for (const auto& p : points)
                                {
                                    if (p.getName() == "point")
                                    {
                                        curve.push_back(p.getValue<glm::vec3>());
                                    }
                                }

                                break;
                            }
                        }

                        if (curve.size() < 4)
                        {
                            addCrowd(holeData, position, lookAt, rotation);
                        }
                        else
                        {
                            auto& spline = holeData.crowdCurves.emplace_back();
                            for (auto p : curve)
                            {
                                spline.addPoint(p);
                            }
                            hasSpectators = true;
                        }
                    }
                    else if (name == "speaker")
                    {
                        std::string emitterName;
                        glm::vec3 position = glm::vec3(0.f);

                        const auto& speakerProps = obj.getProperties();
                        for (const auto& speakerProp : speakerProps)
                        {
                            const auto& propName = speakerProp.getName();
                            if (propName == "emitter")
                            {
                                emitterName = speakerProp.getValue<std::string>();
                            }
                            else if (propName == "position")
                            {
                                position = speakerProp.getValue<glm::vec3>();
                            }
                        }

                        if (!emitterName.empty() &&
                            propAudio.hasEmitter(emitterName))
                        {
                            auto emitterEnt = m_gameScene.createEntity();
                            emitterEnt.addComponent<cro::Transform>().setPosition(position);
                            emitterEnt.addComponent<cro::AudioEmitter>() = propAudio.getEmitter(emitterName);
                            float baseVol = emitterEnt.getComponent<cro::AudioEmitter>().getVolume();
                            emitterEnt.getComponent<cro::AudioEmitter>().setVolume(0.f);
                            emitterEnt.addComponent<cro::Callback>().function =
                                [baseVol](cro::Entity e, float dt)
                            {
                                auto vol = e.getComponent<cro::AudioEmitter>().getVolume();
                                vol = std::min(baseVol, vol + dt);
                                e.getComponent<cro::AudioEmitter>().setVolume(vol);

                                if (vol == baseVol)
                                {
                                    e.getComponent<cro::Callback>().active = false;
                                }
                            };
                            holeData.audioEntities.push_back(emitterEnt);
                        }
                    }
                }

                prevProps = holeData.propEntities;
                prevParticles = holeData.particleEntities;
                prevAudio = holeData.audioEntities;
            }
            else
            {
                holeData.propEntities = prevProps;
                holeData.particleEntities = prevParticles;
                holeData.audioEntities = prevAudio;
            }

            //cos you know someone is dying to try and break the game :P
            if (holeData.pin != holeData.tee)
            {
                holeData.distanceToPin = glm::length(holeData.pin - holeData.tee);
            }
        }
        std::shuffle(holeData.crowdPositions.begin(), holeData.crowdPositions.end(), cro::Util::Random::rndEngine);
    }

    //add the dynamically updated model to any leaderboard props
    if (!leaderboardProps.empty())
    {
        if (md.loadFromFile("assets/golf/models/leaderboard_panel.cmt"))
        {
            for (auto lb : leaderboardProps)
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>();
                md.createModel(entity);
                
                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Leaderboard]);
                material.setProperty("u_subrect", glm::vec4(0.f, 0.5f, 1.f, 0.5f));
                entity.getComponent<cro::Model>().setMaterial(0, material);
                m_leaderboardTexture.addTarget(entity);

                //updates the texture rect depending on hole number
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<std::size_t>(m_currentHole);
                entity.getComponent<cro::Callback>().function =
                    [&,lb](cro::Entity e, float)
                {
                    //the leaderboard might get tidies up on hole change
                    //so remove this ent if that's the case
                    if (lb.destroyed())
                    {
                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                    else
                    {
                        auto currentHole = e.getComponent<cro::Callback>().getUserData<std::size_t>();
                        if (currentHole != m_currentHole)
                        {
                            if (m_currentHole > 8)
                            {
                                e.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", glm::vec4(0.f, 0.f, 1.f, 0.5f));
                            }
                        }
                        currentHole = m_currentHole;
                        e.getComponent<cro::Model>().setHidden(lb.getComponent<cro::Model>().isHidden());
                    }
                };
                entity.getComponent<cro::Model>().setHidden(true);

                lb.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            }
        }
    }


    //remove holes which failed to load - TODO should we delete partially loaded props here?
    m_holeData.erase(std::remove_if(m_holeData.begin(), m_holeData.end(), 
        [](const HoleData& hd)
        {
            return !hd.modelEntity.isValid();
        }), m_holeData.end());

    //if we're running a short round, crop the number of holes
    if (m_sharedData.scoreType == ScoreType::ShortRound
        && m_courseIndex != -1)
    {
        switch (m_sharedData.holeCount)
        {
        default:
        case 0:
        {
            auto size = std::min(m_holeData.size(), std::size_t(12));
            m_holeData.resize(size);
        }
            break;
        case 1:
        case 2:
        {
            auto size = std::min(m_holeData.size(), std::size_t(6));
            m_holeData.resize(size);
        }
            break;
        }
    }


    //check the crowd positions on every hole and set the height
    for (auto& hole : m_holeData)
    {
        m_collisionMesh.updateCollisionMesh(hole.modelEntity.getComponent<cro::Model>().getMeshData());

        for (auto& m : hole.crowdPositions)
        {
            glm::vec3 pos = m[3];
            pos.x += MapSize.x / 2;
            pos.z -= MapSize.y / 2;

            auto result = m_collisionMesh.getTerrain(pos);
            m[3][1] = result.height;
        }

        for (auto& c : hole.crowdCurves)
        {
            for (auto& p : c.getPoints())
            {
                auto result = m_collisionMesh.getTerrain(p);
                p.y = result.height;
            }
        }

        //make sure the hole position matches the terrain
        auto result = m_collisionMesh.getTerrain(hole.pin);
        hole.pin.y = result.height;

        //while we're here check if this is a putting
        //course by looking to see if the tee is on the green
        hole.puttFromTee = m_collisionMesh.getTerrain(hole.tee).terrain == TerrainID::Green;

        //update the green material with grid shader
        auto& model = hole.modelEntity.getComponent<cro::Model>();
        auto matCount = model.getMeshData().submeshCount;
        for (auto i = 0u; i < matCount; ++i)
        {
            auto mat = model.getMaterialData(cro::Mesh::IndexData::Final, i);
            if (mat.name == "green")
            {
                if (hole.puttFromTee)
                {
                    mat.setShader(*gridShader);
                }
                else
                {
                    mat.setShader(*greenShader);
                }
                model.setMaterial(i, mat);
            }
        }


        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::MultiTarget:
            if (!hole.puttFromTee)
            {
                hole.par++;
            }
            break;
        }
    }


    if (error)
    {
        m_sharedData.errorMessage = "Failed to load course data\nSee console for more information";
        requestStackPush(StateID::Error);
    }
    else
    {
        m_holeToModelRatio = static_cast<float>(holeModelCount) / m_holeData.size();

        if (hasSpectators)
        {
            loadSpectators();
        }

        m_depthMap.setModel(m_holeData[0]);
        m_depthMap.update(40);
    }


    m_terrainBuilder.create(m_resources, m_gameScene, theme);

    //terrain builder will have loaded some shaders from which we need to capture some uniforms
    shader = &m_resources.shaders.get(ShaderID::Terrain);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    shader = &m_resources.shaders.get(ShaderID::Slope);
    m_windBuffer.addShader(*shader);

    createClouds();

    //reserve the slots for each hole score
    for (auto& client : m_sharedData.connectionData)
    {
        for (auto& player : client.playerData)
        {
            player.score = 0;
            player.holeScores.clear();
            player.holeScores.resize(holeStrings.size());
            std::fill(player.holeScores.begin(), player.holeScores.end(), 0);

            player.holeComplete.clear();
            player.holeComplete.resize(holeStrings.size());
            std::fill(player.holeComplete.begin(), player.holeComplete.end(), false);

            player.distanceScores.clear();
            player.distanceScores.resize(holeStrings.size());
            std::fill(player.distanceScores.begin(), player.distanceScores.end(), 0.f);
        }
    }

    for (auto& data : m_sharedData.timeStats)
    {
        data.totalTime = 0.f;
        data.holeTimes.clear();
        data.holeTimes.resize(holeStrings.size());
        std::fill(data.holeTimes.begin(), data.holeTimes.end(), 0.f);
    }

    initAudio(theme.treesets.size() > 2);

    //md.loadFromFile("assets/golf/models/sphere.cmt");
    //auto entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //md.createModel(entity);
    //entity.addComponent<cro::Callback>().active = true;
    //entity.getComponent<cro::Callback>().function =
    //    [&](cro::Entity e, float)
    //{
    //    auto cam = m_gameScene.getActiveCamera();
    //    if (cam.hasComponent<CameraFollower>())
    //    {
    //        e.getComponent<cro::Transform>().setPosition(cam.getComponent<CameraFollower>().currentTarget);
    //    }
    //};
}

void GolfState::loadSpectators()
{
    cro::ModelDefinition md(m_resources);
    std::array modelPaths =
    {
        "assets/golf/models/spectators/01.cmt",
        "assets/golf/models/spectators/02.cmt",
        "assets/golf/models/spectators/03.cmt",
        "assets/golf/models/spectators/04.cmt"
    };


    for (auto i = 0; i < 2; ++i)
    {
        for (const auto& path : modelPaths)
        {
            if (md.loadFromFile(path))
            {
                for (auto j = 0; j < 3; ++j)
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);
                    entity.getComponent<cro::Model>().setHidden(true);
                    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                    if (md.hasSkeleton())
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
                        applyMaterialData(md, material);

                        glm::vec4 rect((1.f / 3.f) * j, 0.f, (1.f / 3.f), 1.f);
                        material.setProperty("u_subrect", rect);
                        entity.getComponent<cro::Model>().setMaterial(0, material);

                        auto& skel = entity.getComponent<cro::Skeleton>();
                        if (!skel.getAnimations().empty())
                        {
                            auto& spectator = entity.addComponent<Spectator>();
                            for(auto k = 0u; k < skel.getAnimations().size(); ++k)
                            {
                                if (skel.getAnimations()[k].name == "Walk")
                                {
                                    spectator.anims[Spectator::AnimID::Walk] = k;
                                }
                                else if (skel.getAnimations()[k].name == "Idle")
                                {
                                    spectator.anims[Spectator::AnimID::Idle] = k;
                                }
                            }

                            skel.setMaxInterpolationDistance(30.f);
                        }
                    }

                    m_spectatorModels.push_back(entity);
                }
            }
        }
    }

    std::shuffle(m_spectatorModels.begin(), m_spectatorModels.end(), cro::Util::Random::rndEngine);
}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem<InterpolationType::Linear>>(mb);
    m_gameScene.addSystem<CloudSystem>(mb);
    m_gameScene.addSystem<ClientCollisionSystem>(mb, m_holeData, m_collisionMesh);
    m_gameScene.addSystem<SpectatorSystem>(mb, m_collisionMesh);
    m_gameScene.addSystem<PropFollowSystem>(mb, m_collisionMesh);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<VatAnimationSystem>(mb);
    m_gameScene.addSystem<BallAnimationSystem>(mb);
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem3D>(mb, 10.f); //water rings sprite :D
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<CameraFollowSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb)->setRenderInterval(m_sharedData.hqShadows ? 2 : 3);
//#ifdef CRO_DEBUG_
    m_gameScene.addSystem<FpsCameraSystem>(mb);
//#endif
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    if (m_sharedData.nightTime)
    {
        m_gameScene.addSystem<cro::LightVolumeSystem>(mb, cro::LightVolume::WorldSpace);
    }

    //m_gameScene.setSystemActive<InterpolationSystem<InterpolationType::Linear>>(false);
    m_gameScene.setSystemActive<CameraFollowSystem>(false);
#ifdef CRO_DEBUG_
    m_gameScene.setSystemActive<FpsCameraSystem>(false);
    m_gameScene.setSystemActive<cro::ParticleSystem>(false);
    //m_gameScene.setSystemActive<cro::SkeletalAnimator>(false); //can't do this because we rely on player animation events
#endif

    m_gameScene.addDirector<GolfParticleDirector>(m_resources.textures, m_sharedData);
    m_gameScene.addDirector<GolfSoundDirector>(m_resources.audio, m_sharedData);


    if (m_sharedData.tutorial)
    {
        m_gameScene.addDirector<TutorialDirector>(m_sharedData, m_inputParser);
    }

    m_skyScene.addSystem<cro::CallbackSystem>(mb);
    m_skyScene.addSystem<cro::SkeletalAnimator>(mb);
    m_skyScene.addSystem<cro::CameraSystem>(mb);
    m_skyScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<NotificationSystem>(mb);
    m_uiScene.addSystem<FloatingTextSystem>(mb);
    m_uiScene.addSystem<MiniBallSystem>(mb, m_minimapZoom);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);

    m_trophyScene.addSystem<TrophyDisplaySystem>(mb);
    m_trophyScene.addSystem<cro::SpriteSystem3D>(mb, 300.f);
    m_trophyScene.addSystem<cro::SpriteAnimator>(mb);
    m_trophyScene.addSystem<cro::ParticleSystem>(mb);
    m_trophyScene.addSystem<cro::CameraSystem>(mb);
    m_trophyScene.addSystem<cro::ModelRenderer>(mb);
    m_trophyScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void GolfState::buildScene()
{
    Club::setClubLevel(m_sharedData.tutorial ? 0 : m_sharedData.clubSet);

    m_achievementTracker.noGimmeUsed = (m_sharedData.gimmeRadius != 0);
    m_achievementTracker.noHolesOverPar = (m_sharedData.scoreType == ScoreType::Stroke);

    if (m_holeData.empty())
    {
        //use dummy data to get scene standing
        //but make sure to push error state too
        auto& holeDummy = m_holeData.emplace_back();
        holeDummy.modelEntity = m_gameScene.createEntity();
        holeDummy.modelEntity.addComponent<cro::Transform>();
        holeDummy.modelEntity.addComponent<cro::Callback>();
        createFallbackModel(holeDummy.modelEntity, m_resources);

        m_sharedData.errorMessage = "No Hole Data Loaded.";
        requestStackPush(StateID::Error);
    }

    //quality holing
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/cup.cmt");
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 1.1f, 1.f, 1.1f });
    md.createModel(entity);

    auto holeEntity = entity; //each of these entities are added to the entity with CommandID::Hole - below

    md.loadFromFile("assets/golf/models/flag.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Flag;
    entity.addComponent<float>() = 0.f;
    md.createModel(entity);
    if (md.hasSkeleton())
    {
        entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Flag]));
        entity.getComponent<cro::Skeleton>().play(0);
    }
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FlagCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto pos = e.getComponent<cro::Transform>().getPosition();
        auto& data = e.getComponent<cro::Callback>().getUserData<FlagCallbackData>();

        const float  speed = dt * 0.5f;

        if (data.currentHeight < data.targetHeight)
        {
            data.currentHeight = std::min(FlagCallbackData::MaxHeight, data.currentHeight + speed);
            pos.y = cro::Util::Easing::easeOutExpo(data.currentHeight / FlagCallbackData::MaxHeight);
        }
        else
        {
            data.currentHeight = std::max(0.f, data.currentHeight - speed);
            pos.y = cro::Util::Easing::easeInExpo(data.currentHeight / FlagCallbackData::MaxHeight);
        }
        
        e.getComponent<cro::Transform>().setPosition(pos);

        if (data.currentHeight == data.targetHeight)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    auto flagEntity = entity;

    md.loadFromFile("assets/golf/models/beacon.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Beacon | CommandID::BeaconColour;
    entity.addComponent<cro::Callback>().active = m_sharedData.showBeacon;
    entity.getComponent<cro::Callback>().function = BeaconCallback(m_gameScene);
    md.createModel(entity);

    auto beaconMat = m_resources.materials.get(m_materialIDs[MaterialID::Beacon]);
    applyMaterialData(md, beaconMat);

    entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    entity.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::FlightCam));
    auto beaconEntity = entity;

#ifdef CRO_DEBUG_
    //entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //md.createModel(entity);
    //entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", 0.f);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
    //CPUTarget = entity;

    //entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //md.createModel(entity);
    //entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", 0.35f);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
    //PredictedTarget = entity;
#endif

    //arrow pointing to hole
    md.loadFromFile("assets/golf/models/hole_arrow.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Beacon | CommandID::BeaconColour;
    entity.addComponent<cro::Callback>().active = m_sharedData.showBeacon;
    entity.getComponent<cro::Callback>().function =
        [flagEntity](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);

            const auto& data = flagEntity.getComponent<cro::Callback>().getUserData<FlagCallbackData>();
            float amount = cro::Util::Easing::easeOutCubic(data.currentHeight / FlagCallbackData::MaxHeight);
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour(amount, amount, amount));
        };
    md.createModel(entity);

    applyMaterialData(md, beaconMat);

    entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    entity.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection));
    auto arrowEntity = entity;

    //displays the stroke direction
    auto pos = m_holeData[0].tee;
    pos.y += 0.01f;
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getYaw());
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeIndicator;

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    material.blendMode = cro::Material::BlendMode::Additive;
    material.enableDepthTest = false;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

    //glm::vec3 c(1.f, 0.f, 1.f);
    glm::vec3 c(1.f, 0.97f, 0.88f);
    std::vector<float> verts =
    {
        0.1f, Ball::Radius, 0.005f, c.r * IndicatorLightness, c.g * IndicatorLightness, c.b * IndicatorLightness, 1.f,
        5.f, Ball::Radius, 0.f,    c.r * IndicatorDarkness,  c.g * IndicatorDarkness,  c.b * IndicatorDarkness, 1.f,
        0.1f, Ball::Radius, -0.005f,c.r * IndicatorLightness, c.g * IndicatorLightness, c.b * IndicatorLightness, 1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0,1,2
    };
    meshData->boundingBox = { glm::vec3(0.1f, 0.f, 0.005f), glm::vec3(5.f, Ball::Radius, -0.005f) };
    meshData->boundingSphere = meshData->boundingBox;

    auto vertStride = (meshData->vertexSize / sizeof(float));
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    entity.getComponent<cro::Model>().setHidden(true);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection | RenderFlags::FlightCam | RenderFlags::CubeMap));


    //a 'fan' which shows max rotation
    material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    material.blendMode = cro::Material::BlendMode::Additive;
    material.enableDepthTest = false;
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLE_FAN));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeArc;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection | RenderFlags::FlightCam | RenderFlags::CubeMap));
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        float scale = m_currentPlayer.terrain != TerrainID::Green ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getDirection());
    };

    const std::int32_t pointCount = 6;
    const float arc = MaxRotation * 2.f;
    const float step = arc / pointCount;
    const float radius = 2.5f;

    std::vector<glm::vec2> points;
    for(auto i = 0; i <= pointCount; ++i)
    {
        auto t = -MaxRotation + (i * step);
        auto& p = points.emplace_back(std::cos(t), std::sin(t));
        p *= radius;
    }

    c = { TextGoldColour.getVec4() };
    c *= IndicatorLightness / 10.f;
    meshData = &entity.getComponent<cro::Model>().getMeshData();
    verts =
    {
        0.f, Ball::Radius, 0.f, c.r, c.g, c.b, 1.f
    };
    indices = { 0 };

    for (auto i = 0u; i < points.size(); ++i)
    {
        verts.push_back(points[i].x);
        verts.push_back(Ball::Radius);
        verts.push_back(-points[i].y);
        verts.push_back(c.r);
        verts.push_back(c.g);
        verts.push_back(c.b);
        verts.push_back(1.f);

        indices.push_back(i + 1);
    }

    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));


    //ring effect when holing par or under
    material = m_resources.materials.get(m_materialIDs[MaterialID::BallTrail]);
    material.enableDepthTest = true;
    material.doubleSided = true;
    material.setProperty("u_colourRotation", m_sharedData.beaconColour);
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::HoleRing | CommandID::BeaconColour;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection));
    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
    
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const float Speed = dt;
        static constexpr float MaxTime = 1.f;
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(MaxTime, progress + Speed);

        float scale = cro::Util::Easing::easeOutQuint(progress / MaxTime);
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        float colour = 1.f - cro::Util::Easing::easeOutQuad(progress / MaxTime);
        e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", glm::vec4(colour));

        if (progress == MaxTime)
        {
            progress = 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            e.getComponent<cro::Callback>().active = false;
        }
    };
    
    verts.clear();
    indices.clear();
    //vertex colour of the beacon model. I don't remember why I chose this specifically,
    //but it needs to match so that the hue rotation in the shader outputs the same result
    c = glm::vec3(1.f,0.f,0.781f);

    static constexpr float RingRadius = 1.f;
    auto j = 0u;
    for (auto i = 0.f; i < cro::Util::Const::TAU; i += (cro::Util::Const::TAU / 16.f))
    {
        auto x = std::cos(i) * RingRadius;
        auto z = -std::sin(i) * RingRadius;

        verts.push_back(x);
        verts.push_back(Ball::Radius);
        verts.push_back(z);
        verts.push_back(c.r);
        verts.push_back(c.g);
        verts.push_back(c.b);
        verts.push_back(1.f);
        indices.push_back(j++);

        verts.push_back(x);
        verts.push_back(Ball::Radius + 0.15f);
        verts.push_back(z);
        verts.push_back(0.02f);
        verts.push_back(0.02f);
        verts.push_back(0.02f);
        verts.push_back(1.f);
        indices.push_back(j++);
    }
    indices.push_back(indices[0]);
    indices.push_back(indices[1]);

    meshData = &entity.getComponent<cro::Model>().getMeshData();

    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    meshData->boundingBox = { glm::vec3(-7.f, 0.2f, -0.7f), glm::vec3(7.f, 0.f, 7.f) };
    meshData->boundingSphere = meshData->boundingBox;
    auto ringEntity = entity;

    //draw the flag pole as a single line which can be
    //seen from a distance - hole and model are also attached to this
    material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]);
    material.setProperty("u_colour", cro::Colour::White);
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Hole;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    entity.getComponent<cro::Transform>().addChild(holeEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(flagEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(beaconEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(arrowEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(ringEntity.getComponent<cro::Transform>());

    meshData = &entity.getComponent<cro::Model>().getMeshData();
    verts =
    {
        0.f, 2.f, 0.f,      LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 1.66f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,

        0.f, 1.66f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,
        0.f, 1.33f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,

        0.f, 1.33f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 1.f, 0.f,      LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,

        0.f, 1.f, 0.f,      0.05f, 0.043f, 0.05f, 1.f,
        0.f, 0.66f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,

        0.f, 0.66f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 0.33f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,

        0.f, 0.33f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,
        0.f, 0.f, 0.f,      0.05f, 0.043f, 0.05f, 1.f,
    };
    indices =
    {
        0,1,2,3,4,5,6,7,8,9,10,11,12
    };
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));



    //water plane. Updated by various camera callbacks
    meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(240.f, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    waterEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, -30.f });
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Water]));
    waterEnt.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::Refraction | RenderFlags::FlightCam));
    waterEnt.addComponent<cro::Callback>().active = true;
    waterEnt.getComponent<cro::Callback>().setUserData<glm::vec3>(m_holeData[0].pin);
    waterEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto target = e.getComponent<cro::Callback>().getUserData<glm::vec3>();
        target.y = WaterLevel;

        auto& tx = e.getComponent<cro::Transform>();
        auto diff = target - tx.getPosition();
        tx.move(diff * 5.f * dt);
    };
    m_waterEnt = waterEnt;
    m_gameScene.setWaterLevel(WaterLevel);
    m_skyScene.setWaterLevel(WaterLevel);

    //we never use the reflection buffer fot the skybox cam, but
    //it needs for one to be created for the culling to consider
    //the scene's objects for reflection map pass...
    m_skyScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.create(2, 2);

    //tee marker
    material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
    md.loadFromFile("assets/golf/models/tee_balls.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].tee);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Tee;
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    
    auto targetDir = m_holeData[m_currentHole].target - m_holeData[0].tee;
    m_camRotation = std::atan2(-targetDir.z, targetDir.x);
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation);

    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        const auto& target = e.getComponent<cro::Callback>().getUserData<float>();
        float scale = e.getComponent<cro::Transform>().getScale().x;
        if (target < scale)
        {
            scale = std::max(target, scale - dt);
        }
        else
        {
            scale = std::min(target, scale + dt);
        }

        if (scale == target)
        {
            e.getComponent<cro::Callback>().active = false;
        }
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };


    auto teeEnt = entity;

    //golf bags
    material.doubleSided = true;
    md.loadFromFile("assets/golf/models/golfbag02.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -0.6f, 0.f, 3.1f });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.2f);
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    if (m_sharedData.localConnectionData.playerCount > 2
        || m_sharedData.connectionData[2].playerCount > 0)
    {
        md.loadFromFile("assets/golf/models/golfbag01.cmt");
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -0.2f, 0.f, -2.8f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 3.2f);
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Ball]));
        entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
        teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //carts
    md.loadFromFile("assets/golf/models/cart.cmt");
    auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    applyMaterialData(md, texturedMat);
    std::array cartPositions =
    {
        glm::vec3(-0.4f, 0.f, -5.9f),
        glm::vec3(2.6f, 0.f, -6.9f),
        glm::vec3(2.2f, 0.f, 6.9f),
        glm::vec3(-1.2f, 0.f, 5.2f)
    };

    //add a cart for each connected client :3
    for (auto i = 0u; i < /*m_sharedData.connectionData.size()*/4u; ++i)
    {
        if (m_sharedData.connectionData[i].playerCount > 0)
        {
            auto r = i + cro::Util::Random::value(0, 8);
            float rotation = (cro::Util::Const::PI / 4.f) * r;

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(cartPositions[i]);
            entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
            entity.addComponent<cro::CommandTarget>().ID = CommandID::Cart;
            md.createModel(entity);
            entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
            entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
            teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }


    createCameras();


    //drone model to follow camera
    createDrone();
   
    m_currentPlayer.position = m_holeData[m_currentHole].tee; //prevents the initial camera movement

    buildUI(); //put this here because we don't want to do this if the map data didn't load
    setCurrentHole(4); //need to do this here to make sure everything is loaded for rendering

    auto sunEnt = m_gameScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -130.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -75.f * cro::Util::Const::degToRad);

    if (auto month = cro::SysTime::now().months(); month == 12)
    {
        if (cro::Util::Random::value(0, 20) == 0)
        {
            createWeather(WeatherType::Snow);
        }
    }
    else if (month == 6 && !m_sharedData.nightTime)
    {
        if (cro::Util::Random::value(0, 8) == 0)
        {
            buildBow();
        }
    }

    

    //m_resources.shaders.loadFromString(ShaderID::PointLight,
    //    cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), PointLightFrag, "#define RIMMING\n");
    //auto* shader = &m_resources.shaders.get(ShaderID::PointLight);
    //m_windBuffer.addShader(*shader);
    //auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    //auto matID = m_resources.materials.add(*shader);
    //material = m_resources.materials.get(matID);
    //material.setProperty("u_noiseTexture", noiseTex);

    //md.loadFromFile("assets/golf/models/light_sphere.cmt");
    //applyMaterialData(md, material);

    //entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ 40.831287f,1.996931f,-100.147438f });
    //entity.getComponent<cro::Transform>().setScale(glm::vec3(5.f));
    //md.createModel(entity);
    //entity.getComponent<cro::Model>().setMaterial(0, material);

    //entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ 124.924004f,2.089445f,-139.739395f });
    //entity.getComponent<cro::Transform>().setScale(glm::vec3(5.f));
    //md.createModel(entity);
    //entity.getComponent<cro::Model>().setMaterial(0, material);
}

void GolfState::initAudio(bool loadTrees)
{
    if (m_sharedData.nightTime)
    {
        auto ext = cro::FileSystem::getFileExtension(m_audioPath);
        auto nightPath = m_audioPath.substr(0, m_audioPath.find(ext)) + "_n" + ext;

        if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + nightPath))
        {
            m_audioPath = nightPath;
        }
    }


    //6 evenly spaced points with ambient audio
    auto envOffset = glm::vec2(MapSize) / 3.f;
    cro::AudioScape as;
    if (as.loadFromFile(m_audioPath, m_resources.audio))
    {
        std::array emitterNames =
        {
            std::string("01"),
            std::string("02"),
            std::string("03"),
            std::string("04"),
            std::string("05"),
            std::string("06"),
            std::string("03"),
            std::string("04"),
        };

        for (auto i = 0; i < 2; ++i)
        {
            for (auto j = 0; j < 2; ++j)
            {
                static constexpr float height = 4.f;
                glm::vec3 position(envOffset.x * (i + 1), height, -envOffset.y * (j + 1));

                auto idx = i * 2 + j;

                if (as.hasEmitter(emitterNames[idx + 4]))
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(position);
                    entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx + 4]);
                    entity.getComponent<cro::AudioEmitter>().play();
                    entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(5.f));
                }

                position = { i * MapSize.x, height, -static_cast<float>(MapSize.y) * j };

                if (as.hasEmitter(emitterNames[idx]))
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(position);
                    entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx]);
                    entity.getComponent<cro::AudioEmitter>().play();
                }
            }
        }

        //random incidental audio
        if (as.hasEmitter("incidental01")
            && as.hasEmitter("incidental02"))
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("incidental01");
            entity.getComponent<cro::AudioEmitter>().setLooped(false);
            auto plane01 = entity;

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("incidental02");
            entity.getComponent<cro::AudioEmitter>().setLooped(false);
            auto plane02 = entity;

            //we'll shoehorn the plane in here. won't make much sense
            //if the audioscape has different audio but hey...
            cro::ModelDefinition md(m_resources);
            cro::Entity planeEnt;
            if (md.loadFromFile("assets/golf/models/plane.cmt"))
            {
                static constexpr glm::vec3 Start(-32.f, 60.f, 20.f);
                static constexpr glm::vec3 End(352.f, 60.f, -220.f);

                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition(Start);
                entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 32.f * cro::Util::Const::degToRad);
                entity.getComponent<cro::Transform>().setScale({ 0.01f, 0.01f, 0.01f });
                md.createModel(entity);

                entity.addComponent<cro::Callback>().function =
                    [](cro::Entity e, float dt)
                {
                    static constexpr float Speed = 10.f;
                    const float MaxLen = glm::length2((Start - End) / 2.f);

                    auto& tx = e.getComponent<cro::Transform>();
                    auto dir = glm::normalize(tx.getRightVector()); //scaling means this isn't normalised :/
                    tx.move(dir * Speed * dt);

                    float currLen = glm::length2((Start + ((Start + End) / 2.f)) - tx.getPosition());
                    float scale = std::max(1.f - (currLen / MaxLen), 0.001f); //can't scale to 0 because it breaks normalizing the right vector above
                    tx.setScale({ scale, scale, scale });

                    if (tx.getPosition().x > End.x)
                    {
                        tx.setPosition(Start);
                        e.getComponent<cro::Callback>().active = false;
                    }
                };

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                applyMaterialData(md, material);
                entity.getComponent<cro::Model>().setMaterial(0, material);

                //engine
                entity.addComponent<cro::AudioEmitter>(); //always needs one in case audio doesn't exist
                if (as.hasEmitter("plane"))
                {
                    entity.getComponent<cro::AudioEmitter>() = as.getEmitter("plane");
                    entity.getComponent<cro::AudioEmitter>().setLooped(false);
                }

                planeEnt = entity;
            }

            struct AudioData final
            {
                float currentTime = 0.f;
                float timeout = static_cast<float>(cro::Util::Random::value(32, 64));
                cro::Entity activeEnt;
            };

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<AudioData>();
            entity.getComponent<cro::Callback>().function =
                [plane01, plane02, planeEnt](cro::Entity e, float dt) mutable
            {
                auto& [currTime, timeOut, activeEnt] = e.getComponent<cro::Callback>().getUserData<AudioData>();
                
                if (!activeEnt.isValid()
                    || activeEnt.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                {
                    currTime += dt;

                    if (currTime > timeOut)
                    {
                        currTime = 0.f;
                        timeOut = static_cast<float>(cro::Util::Random::value(120, 240));

                        auto id = cro::Util::Random::value(0, 2);
                        if (id == 0)
                        {
                            //fly the plane
                            if (planeEnt.isValid())
                            {
                                planeEnt.getComponent<cro::Callback>().active = true;
                                planeEnt.getComponent<cro::AudioEmitter>().play();
                                activeEnt = planeEnt;
                            }
                        }
                        else
                        {
                            auto ent = (id == 1) ? plane01 : plane02;
                            if (ent.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                            {
                                ent.getComponent<cro::AudioEmitter>().play();
                                activeEnt = ent;
                            }
                        }
                    }
                }
            };
        }

        //put the new hole music on the cam for accessibilty
        //this is done *before* m_cameras is updated 
        if (as.hasEmitter("music"))
        {
            m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>() = as.getEmitter("music");
            m_gameScene.getActiveCamera().getComponent<cro::AudioEmitter>().setLooped(false);
        }
    }
    else
    {
        //still needs an emitter to stop crash playing non-loaded music
        m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>();
        LogE << "Invalid AudioScape file was found" << std::endl;
    }
    createMusicPlayer(m_gameScene, m_sharedData, m_gameScene.getActiveCamera());


    if (loadTrees)
    {
        const std::array<std::string, 3u> paths =
        {
            "assets/golf/sound/ambience/trees01.ogg",
            "assets/golf/sound/ambience/trees03.ogg",
            "assets/golf/sound/ambience/trees02.ogg"
        };

        /*const std::array positions =
        {
            glm::vec3(80.f, 4.f, -66.f),
            glm::vec3(240.f, 4.f, -66.f),
            glm::vec3(160.f, 4.f, -66.f),
            glm::vec3(240.f, 4.f, -123.f),
            glm::vec3(160.f, 4.f, -123.f),
            glm::vec3(80.f, 4.f, -123.f)
        };

        auto callback = [&](cro::Entity e, float)
        {
            float amount = std::min(1.f, m_windUpdate.currentWindSpeed);
            float pitch = 0.5f + (0.8f * amount);
            float volume = 0.05f + (0.3f * amount);

            e.getComponent<cro::AudioEmitter>().setPitch(pitch);
            e.getComponent<cro::AudioEmitter>().setVolume(volume);
        };

        //this works but... meh
        for (auto i = 0u; i < paths.size(); ++i)
        {
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + paths[i]))
            {
                for (auto j = 0u; j < 2u; ++j)
                {
                    auto id = m_resources.audio.load(paths[i], true);

                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(positions[i + (j * paths.size())]);
                    entity.addComponent<cro::AudioEmitter>().setSource(m_resources.audio.get(id));
                    entity.getComponent<cro::AudioEmitter>().setVolume(0.f);
                    entity.getComponent<cro::AudioEmitter>().setLooped(true);
                    entity.getComponent<cro::AudioEmitter>().setRolloff(0.f);
                    entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    entity.getComponent<cro::AudioEmitter>().play();

                    entity.addComponent<cro::Callback>().active = true;
                    entity.getComponent<cro::Callback>().function = callback;
                }
            }
        }*/
    }


    //fades in the audio
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + dt);

        cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::Effects);
        cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::Environment);
        cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::UserMusic);

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };
}

void GolfState::createDrone()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/drone.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec3(2.f));
        entity.getComponent<cro::Transform>().setPosition({ 160.f, 1.f, -100.f }); //lazy man's half map size
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        //material.doubleSided = true;
        applyMaterialData(md, material, 1);
        material.blendMode = cro::Material::BlendMode::Alpha;
        entity.getComponent<cro::Model>().setMaterial(1, material);

        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

        entity.addComponent<cro::AudioEmitter>();

        cro::AudioScape as;
        if (as.loadFromFile("assets/golf/sound/drone.xas", m_resources.audio)
            && as.hasEmitter("drone"))
        {
            entity.getComponent<cro::AudioEmitter>() = as.getEmitter("drone");
            entity.getComponent<cro::AudioEmitter>().play();
        }

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<DroneCallbackData>();
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto oldPos = e.getComponent<cro::Transform>().getPosition();
            auto playerPos = m_currentPlayer.position;

            //rotate towards active player
            glm::vec2 dir = glm::vec2(oldPos.x - playerPos.x, oldPos.z - playerPos.z);
            float rotation = std::atan2(dir.y, dir.x) + (cro::Util::Const::PI / 2.f);


            auto& [currRotation, acceleration, target, _] = e.getComponent<cro::Callback>().getUserData<DroneCallbackData>();

            //move towards skycam target
            static constexpr float MoveSpeed = 6.f;
            static constexpr float MinRadius = MoveSpeed * MoveSpeed;
            //static constexpr float AccelerationRadius = 7.f;// 40.f;

            auto movement = target.getComponent<cro::Transform>().getPosition() - oldPos;
            if (auto len2 = glm::length2(movement); len2 > MinRadius)
            {
                //const float len = std::sqrt(len2);
                //movement /= len;
                //movement *= MoveSpeed;

                ////go slower over short distances
                //const float multiplier = 0.6f + (0.4f * std::min(1.f, len / AccelerationRadius));

                //acceleration = std::min(1.f, acceleration + ((dt / 2.f) * multiplier));
                //movement *= cro::Util::Easing::easeInSine(acceleration);

                currRotation += cro::Util::Maths::shortestRotation(currRotation, rotation) * dt;
            }
            else
            {
                acceleration = 0.f;
                currRotation = std::fmod(currRotation + (dt * 0.5f), cro::Util::Const::TAU);
            }
            //e.getComponent<cro::Transform>().move(movement * dt);
            
            
            //---------------
            static glm::vec3 vel(0.f);
            auto pos = cro::Util::Maths::smoothDamp(e.getComponent<cro::Transform>().getPosition(), target.getComponent<cro::Transform>().getPosition(), vel, 3.f, dt, 24.f);
            e.getComponent<cro::Transform>().setPosition(pos);
            //--------------

            e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, currRotation);

            m_cameras[CameraID::Sky].getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
            m_cameras[CameraID::Drone].getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());

            //update emitter based on velocity
            auto velocity = oldPos - e.getComponent<cro::Transform>().getPosition();
            e.getComponent<cro::AudioEmitter>().setVelocity(velocity * 60.f);

            //update the pitch based on height above hole
            static constexpr float MaxHeight = 10.f;
            float height = oldPos.y - m_holeData[m_currentHole].pin.y;
            height = std::min(1.f, height / MaxHeight);

            float pitch = 0.5f + (0.5f * height);
            e.getComponent<cro::AudioEmitter>().setPitch(pitch);
        };

        m_drone = entity;

        //make sure this is actually valid...
        auto targetEnt = m_gameScene.createEntity();
        targetEnt.addComponent<cro::Transform>().setPosition({ 160.f, 30.f, -100.f });
        targetEnt.addComponent<cro::Callback>().active = true; //also used to make the drone orbit the flag (see showCountdown())
        targetEnt.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            if (!m_drone.isValid())
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            else
            {
                auto wind = m_windUpdate.currentWindSpeed * (m_windUpdate.currentWindVector * 0.3f);
                wind += m_windUpdate.currentWindVector * 0.7f;

                auto resetPos = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().resetPosition;

                //clamp to radius
                const float Radius = m_holeData[m_currentHole].puttFromTee ? 25.f : 50.f;
                if (glm::length2(resetPos - e.getComponent<cro::Transform>().getPosition()) < (Radius * Radius))
                {
                    e.getComponent<cro::Transform>().move(wind * dt);
                }
                else
                {
                    e.getComponent<cro::Transform>().setPosition(resetPos);
                }
            }
        };

        m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().target = targetEnt;
        m_cameras[CameraID::Sky].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Fog];
    }
    m_cameras[CameraID::Drone].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Fog];
}

void GolfState::spawnBall(const ActorInfo& info)
{
    auto ballID = m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballID;

    //render the ball as a point so no perspective is applied to the scale
    cro::Colour miniBallColour;
    bool rollAnimation = true;
    auto material = m_resources.materials.get(m_ballResources.materialID);
    auto ball = std::find_if(m_sharedData.ballInfo.begin(), m_sharedData.ballInfo.end(),
        [ballID](const SharedStateData::BallInfo& ballPair)
        {
            return ballPair.uid == ballID;
        });
    if (ball != m_sharedData.ballInfo.end())
    {
        material.setProperty("u_colour", ball->tint);
        miniBallColour = ball->tint;
        rollAnimation = ball->rollAnimation;
    }
    else
    {
        //this should at least line up with the fallback model
        material.setProperty("u_colour", m_sharedData.ballInfo.cbegin()->tint);
        miniBallColour = m_sharedData.ballInfo.cbegin()->tint;
    }

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    //entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f }); //pushes the ent above the ground a bit to stop Z fighting
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent<InterpolationType::Linear>>(
        InterpolationPoint(info.position, glm::vec3(0.f), cro::Util::Net::decompressQuat(info.rotation), info.timestamp)).id = info.serverID;
    entity.addComponent<ClientCollider>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.ballMeshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    
    //revisit this if we can interpolate spawn positions
    //entity.addComponent<cro::ParticleEmitter>().settings.loadFromFile("assets/golf/particles/rockit.cps", m_resources.textures);

    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto scale = e.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale + (dt * 1.5f));
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        if (scale == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            e.getComponent<cro::Model>().setHidden(false);
        }
    };
    m_avatars[info.clientID][info.playerID].ballModel = entity;



    //ball shadow
    auto ballEnt = entity;
    material.setProperty("u_colour", cro::Colour::White);
    //material.blendMode = cro::Material::BlendMode::Multiply; //causes shadow to actually get darker as alpha reaches zero.. duh

    bool showTrail = !(m_sharedData.connectionData[info.clientID].playerData[info.playerID].isCPU && m_sharedData.fastCPU);

    //point shadow seen from distance
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition(info.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, info, showTrail](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            //only do this when active player.
            if (ballEnt.getComponent<ClientCollider>().state != std::uint8_t(Ball::State::Idle)
                || ballEnt.getComponent<cro::Transform>().getPosition() == m_holeData[m_currentHole].tee)
            {
                auto ballPos = ballEnt.getComponent<cro::Transform>().getPosition();
                auto ballHeight = ballPos.y;

                auto c = cro::Colour::White;
                if (ballPos.y > WaterLevel)
                {
                    //rays have limited length so might miss from high up (making shadow disappear)
                    auto rayPoint = ballPos;
                    rayPoint.y = 10.f;
                    auto height = m_collisionMesh.getTerrain(rayPoint).height;
                    c.setAlpha(smoothstep(0.2f, 0.8f, (ballPos.y - height) / 0.25f));

                    ballPos.y = 0.00001f + (height - ballHeight);
                }
                e.getComponent<cro::Transform>().setPosition({ 0.f, ballPos.y, 0.f });
                e.getComponent<cro::Model>().setHidden((m_currentPlayer.terrain == TerrainID::Green) || ballEnt.getComponent<cro::Model>().isHidden());
                e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", c);

                if (showTrail)
                {
                    if (m_sharedData.showBallTrail && (info.playerID == m_currentPlayer.player && info.clientID == m_currentPlayer.client)
                        && ballEnt.getComponent<ClientCollider>().state == static_cast<std::uint8_t>(Ball::State::Flight))
                    {
                        m_ballTrail.addPoint(ballEnt.getComponent<cro::Transform>().getPosition());
                    }
                }
            }
        }
    };
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.shadowMeshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //large shadow seen close up
    auto shadowEnt = entity;
    entity = m_gameScene.createEntity();
    shadowEnt.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 0.003f, 0.f });
    m_modelDefs[ModelID::BallShadow]->createModel(entity);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    //entity.getComponent<cro::Transform>().setScale(glm::vec3(1.3f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        e.getComponent<cro::Model>().setHidden(ballEnt.getComponent<cro::Model>().isHidden());
        e.getComponent<cro::Transform>().setScale(ballEnt.getComponent<cro::Transform>().getScale()/* * 0.95f*/);
    };


    //adding a ball model means we see something a bit more reasonable when close up
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    if (rollAnimation)
    {
        entity.getComponent<cro::Transform>().setPosition({ 0.f, Ball::Radius, 0.f });
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f });
        entity.addComponent<BallAnimation>().parent = ballEnt;
    }
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt](cro::Entity e, float dt)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };

    const auto loadDefaultBall = [&]()
    {
        auto& defaultBall = m_ballModels.begin()->second;
        if (!defaultBall->isLoaded())
        {
            defaultBall->loadFromFile(m_sharedData.ballInfo[0].modelPath);
        }

        //a bit dangerous assuming we're not empty, but we
        //shouldn't have made it this far if there are no ball files
        //as they are vetted by the menu state.
        LogW << "Ball with ID " << (int)ballID << " not found" << std::endl;
        defaultBall->createModel(entity);
        applyMaterialData(*m_ballModels.begin()->second, material);
    };

    material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
    if (m_ballModels.count(ballID) != 0
        && ball != m_sharedData.ballInfo.end())
    {
        if (!m_ballModels[ballID]->isLoaded())
        {
            m_ballModels[ballID]->loadFromFile(ball->modelPath);
        }

        if (m_ballModels[ballID]->isLoaded())
        {
            m_ballModels[ballID]->createModel(entity);
            applyMaterialData(*m_ballModels[ballID], material);
        }
        else
        {
            loadDefaultBall();
        }
    }
    else
    {
        loadDefaultBall();
    }
    //clamp scale of balls in case someone got funny with a large model
    const float scale = std::min(1.f, MaxBallRadius / entity.getComponent<cro::Model>().getBoundingSphere().radius);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

    entity.getComponent<cro::Model>().setMaterial(0, material);
    if (entity.getComponent<cro::Model>().getMeshData().submeshCount > 1)
    {
        //this assumes the model loaded successfully, otherwise
        //there wouldn't be two submeshes.
        auto mat = m_resources.materials.get(m_materialIDs[MaterialID::Trophy]);
        applyMaterialData(*m_ballModels[ballID], mat);
        entity.getComponent<cro::Model>().setMaterial(1, mat);
    }
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    if (m_sharedData.nightTime)
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile("assets/golf/models/light_sphere.cmt"))
        {
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec3(0.5f));
            entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.1f, 0.f });
            md.createModel(entity);

            entity.addComponent<cro::LightVolume>().radius = 0.5f;
            entity.getComponent<cro::LightVolume>().colour = miniBallColour.getVec4() / 2.f;
            entity.getComponent<cro::Model>().setHidden(true);

            ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }



    //name label for the ball's owner
    glm::vec2 texSize(LabelTextureSize);
    texSize.y -= (LabelIconSize.y * 4.f);

    const auto playerID = info.playerID;
    const auto clientID = info.clientID;
    const auto depthOffset = ((clientID * ConstVal::MaxPlayers) + playerID) + 1; //must be at least 1 (if you change this, note that it breaks anything which refers to this uid)
    const cro::FloatRect textureRect(0.f, playerID * (texSize.y / ConstVal::MaxPlayers), texSize.x, texSize.y / ConstVal::MaxPlayers);
    const cro::FloatRect uvRect(0.f, textureRect.bottom / static_cast<float>(LabelTextureSize.y),
                            1.f, textureRect.height / static_cast<float>(LabelTextureSize.y));

    constexpr glm::vec2 AvatarSize(16.f);
    const glm::vec2 AvatarOffset((textureRect.width - AvatarSize.x) / 2.f, textureRect.height + 2.f);
    cro::FloatRect avatarUV(0.f, texSize.y / static_cast<float>(LabelTextureSize.y),
                    LabelIconSize.x / static_cast<float>(LabelTextureSize.x), 
                    LabelIconSize.y / static_cast<float>(LabelTextureSize.y));

    float xOffset = (playerID % 2) * avatarUV.width;
    float yOffset = (playerID / 2) * avatarUV.height;
    avatarUV.left += xOffset;
    avatarUV.bottom += yOffset;

    static constexpr cro::Colour BaseColour(1.f, 1.f, 1.f, 0.f);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ texSize.x / 2.f, 0.f, -0.1f - (0.01f * depthOffset) });
    entity.addComponent<cro::Drawable2D>().setTexture(&m_sharedData.nameTextures[info.clientID].getTexture());
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, textureRect.height), glm::vec2(0.f, uvRect.bottom + uvRect.height), BaseColour),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, uvRect.bottom), BaseColour),
            cro::Vertex2D(glm::vec2(textureRect.width, textureRect.height), glm::vec2(uvRect.width, uvRect.bottom + uvRect.height), BaseColour),

            cro::Vertex2D(glm::vec2(textureRect.width, textureRect.height), glm::vec2(uvRect.width, uvRect.bottom + uvRect.height), BaseColour),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, uvRect.bottom), BaseColour),
            cro::Vertex2D(glm::vec2(textureRect.width, 0.f), glm::vec2(uvRect.width, uvRect.bottom), BaseColour),

            cro::Vertex2D(AvatarOffset + glm::vec2(0.f, AvatarSize.y), glm::vec2(avatarUV.left, avatarUV.bottom + avatarUV.height), BaseColour),
            cro::Vertex2D(AvatarOffset + glm::vec2(0.f), glm::vec2(avatarUV.left, avatarUV.bottom), BaseColour),
            cro::Vertex2D(AvatarOffset + AvatarSize, glm::vec2(avatarUV.left + avatarUV.width, avatarUV.bottom + avatarUV.height), BaseColour),

            cro::Vertex2D(AvatarOffset + AvatarSize, glm::vec2(avatarUV.left + avatarUV.width, avatarUV.bottom + avatarUV.height), BaseColour),
            cro::Vertex2D(AvatarOffset + glm::vec2(0.f), glm::vec2(avatarUV.left, avatarUV.bottom), BaseColour),
            cro::Vertex2D(AvatarOffset + glm::vec2(AvatarSize.x, 0.f), glm::vec2(avatarUV.left + avatarUV.width, avatarUV.bottom), BaseColour),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, playerID, clientID](cro::Entity e, float dt)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }

        auto terrain = ballEnt.getComponent<ClientCollider>().terrain;
        auto& colour = e.getComponent<cro::Callback>().getUserData<float>();

        auto position = ballEnt.getComponent<cro::Transform>().getPosition();
        position.y += Ball::Radius * 3.f;

        auto labelPos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(position, m_renderTarget.getSize());
        const float halfWidth = m_renderTarget.getSize().x / 2.f;

        e.getComponent<cro::Transform>().setPosition(labelPos);

        if (terrain == TerrainID::Green)
        {
            if (m_currentPlayer.player == playerID
                && m_sharedData.clientConnection.connectionID == clientID)
            {
                //set target fade to zero
                colour = std::max(0.f, colour - dt);
            }
            else
            {
                //calc target fade based on distance to the camera
                const auto& camTx = m_cameras[CameraID::Player].getComponent<cro::Transform>();
                auto camPos = camTx.getPosition();
                auto ballVec = position - camPos;
                auto len2 = glm::length2(ballVec);
                static constexpr float MinLength = 64.f; //8m^2
                float alpha = smoothstep(0.05f, 0.5f, 1.f - std::min(1.f, std::max(0.f, len2 / MinLength)));

                //fade slightly near the centre of the screen
                //to prevent blocking the view
                float halfPos = labelPos.x - halfWidth;
                float amount = std::min(1.f, std::max(0.f, std::abs(halfPos) / halfWidth));
                amount = 0.1f + (smoothstep(0.12f, 0.26f, amount) * 0.85f); //remember tex size is probably a lot wider than the window
                alpha *= amount;

                //and hide if near the tee
                alpha *= std::min(1.f, glm::length2(m_holeData[m_currentHole].tee - position));

                float currentAlpha = colour;
                colour = std::max(0.f, std::min(1.f, currentAlpha + (dt * cro::Util::Maths::sgn(alpha - currentAlpha))));
            }

            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour.setAlpha(colour);
            }
        }
        else
        {
            colour = std::max(0.f, colour - dt);
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour.setAlpha(colour);
            }
        }

        float scale = m_sharedData.pixelScale ? 1.f : m_viewScale.x;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //miniball for player
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-2.f, 2.f), miniBallColour),
        cro::Vertex2D(glm::vec2(-2.f), miniBallColour),
        cro::Vertex2D(glm::vec2(2.f), miniBallColour),
        cro::Vertex2D(glm::vec2(2.f, -2.f), miniBallColour)
    };
    entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniBall;
    entity.addComponent<MiniBall>().playerID = depthOffset;
    entity.getComponent<MiniBall>().parent = ballEnt;
    entity.getComponent<MiniBall>().minimap = m_minimapEnt;

    m_minimapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //and indicator icon when putting
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-1.f, 4.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(0.f,0.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(1.f, 4.5f), miniBallColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, depthOffset](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
            return;
        }

        //if (m_miniGreenEnt.getComponent<cro::Sprite>().getTexture() &&
        //    m_miniGreenEnt.getComponent<cro::Sprite>().getTexture()->getGLHandle() ==
        //    m_flightTexture.getTexture().getGLHandle())
        //{
        //    //hide
        //    e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        //}
        //else
        {
            if (m_currentPlayer.terrain == TerrainID::Green)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                auto pos = ballEnt.getComponent<cro::Transform>().getWorldPosition();
                auto iconPos = m_greenCam.getComponent<cro::Camera>().coordsToPixel(pos, m_overheadBuffer.getSize());

                const glm::vec2 Centre = glm::vec2(m_overheadBuffer.getSize() / 2u);

                iconPos -= Centre;
                iconPos *= std::min(1.f, Centre.x / glm::length(iconPos));
                iconPos += Centre;

                auto terrain = ballEnt.getComponent<ClientCollider>().terrain;
                float scale = terrain == TerrainID::Green ? m_viewScale.x / m_miniGreenEnt.getComponent<cro::Transform>().getScale().x : 0.f;

                e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                e.getComponent<cro::Transform>().setPosition(glm::vec3(iconPos, static_cast<float>(depthOffset) / 100.f));

                const auto activePlayer = ((m_currentPlayer.client * ConstVal::MaxPlayers) + m_currentPlayer.player) + 1;
                if (m_inputParser.getActive()
                    && activePlayer == depthOffset)
                {
                    m_miniGreenIndicatorEnt.getComponent<cro::Transform>().setPosition(glm::vec3(iconPos, 0.05f));
                }
            }
            else
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        }
    };

    m_miniGreenEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballTint = miniBallColour;

#ifdef CRO_DEBUG_
    ballEntity = ballEnt;
#endif
}

void GolfState::spawnBullsEye(const BullsEye& b)
{
    if (b.spawn)
    {
        auto targetScale = b.diametre;

        auto position = b.position;
        position.y = m_collisionMesh.getTerrain(b.position).height;

        //create new model
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::BullsEye;
        m_modelDefs[ModelID::BullsEye]->createModel(entity);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<BullsEyeData>();
        entity.getComponent<cro::Callback>().function =
            [&, targetScale](cro::Entity e, float dt)
        {
            auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<BullsEyeData>();
            if (direction == AnimDirection::Grow)
            {
                progress = std::min(1.f, progress + dt);
                if (progress == 1)
                {
                    direction = AnimDirection::Hold;
                }
            }
            else if (direction == AnimDirection::Hold)
            {
                //idle while rotating
            }
            else
            {
                progress = std::max(0.f, progress - dt);
                if (progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    
                    //setting to 2 just hides
                    if (direction == AnimDirection::Destroy)
                    {
                        m_gameScene.destroyEntity(e);
                    }
                }
            }

            float scale = cro::Util::Easing::easeOutBack(progress) * targetScale;
            e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.2f);
        };
    }
    else
    {
        //remove existing
        cro::Command cmd;
        cmd.targetFlags = CommandID::BullsEye;
        cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<BullsEyeData>().direction = AnimDirection::Destroy;
            e.getComponent<cro::Callback>().active = true;
        };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }
}

void GolfState::handleNetEvent(const net::NetEvent& evt)
{
    switch (evt.type)
    {
    case net::NetEvent::PacketReceived:
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::WarnTime:
        {
            float warnTime = static_cast<float>(evt.packet.as<std::uint8_t>());

            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::AFKWarn;
            cmd.action = [warnTime](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().setUserData<float>(warnTime + 0.1f);
                e.getComponent<cro::Callback>().active = true;
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::MaxClubs:
        {
            std::uint8_t clubSet = evt.packet.as<std::uint8_t>();
            if (clubSet < m_sharedData.clubSet)
            {
                m_sharedData.clubSet = clubSet;
                Club::setClubLevel(clubSet);

                //hmm this should be read from whichever player is setting the limit
                switch (clubSet)
                {
                default: break;
                case 0:
                    m_sharedData.inputBinding.clubset = ClubID::DefaultSet;
                    break;
                case 1:
                    m_sharedData.inputBinding.clubset = ClubID::DefaultSet | ClubID::FiveWood | ClubID::FourIron | ClubID::SixIron;
                    break;
                case 2:
                    m_sharedData.inputBinding.clubset = ClubID::FullSet;
                    break;
                }
            }
        }
        break;
        case PacketID::ChatMessage:
            m_textChat.handlePacket(evt.packet);
            {
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;
            }
            break;
        case PacketID::FlagHit:
        {
            auto data = evt.packet.as<BullHit>();
            data.player = std::clamp(data.player, std::uint8_t(0), ConstVal::MaxPlayers);
            if (!m_sharedData.localConnectionData.playerData[data.player].isCPU)
            {
                Social::getMonthlyChallenge().updateChallenge(ChallengeID::Eleven, 0);
                Achievements::incrementStat(StatStrings[StatID::FlagHits]);
            }
        }
            break;
        case PacketID::BullHit:
            handleBullHit(evt.packet.as<BullHit>());
            break;
        case PacketID::BullsEye:
        {
            spawnBullsEye(evt.packet.as<BullsEye>());
        }
            break;
        case PacketID::FastCPU:
            m_sharedData.fastCPU = evt.packet.as<std::uint8_t>() != 0;
            m_cpuGolfer.setFastCPU(m_sharedData.fastCPU);
            break;
        case PacketID::PlayerXP:
        {
            auto value = evt.packet.as<std::uint16_t>();
            std::uint8_t client = value & 0xff;
            std::uint8_t level = value >> 8;
            m_sharedData.connectionData[client].level = level;
        }
        break;
        case PacketID::BallPrediction:
        {
            auto pos = evt.packet.as<glm::vec3>();
            m_cpuGolfer.setPredictionResult(pos, m_collisionMesh.getTerrain(pos).terrain);
#ifdef CRO_DEBUG_
            //PredictedTarget.getComponent<cro::Transform>().setPosition(evt.packet.as<glm::vec3>());
#endif
        }
            break;
        case PacketID::LevelUp:
            showLevelUp(evt.packet.as<std::uint64_t>());
            break;
        case PacketID::SetPar:
        {
            std::uint16_t holeInfo = evt.packet.as<std::uint16_t>();
            std::uint8_t hole = (holeInfo & 0xff00) >> 8;
            m_holeData[hole].par = (holeInfo & 0x00ff);

            if (hole == m_currentHole)
            {
                updateScoreboard();
            }
        }
            break;
        case PacketID::Emote:
            showEmote(evt.packet.as<std::uint32_t>());
            break;
        case PacketID::MaxStrokes:
            handleMaxStrokes(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::PingTime:
        {
            auto data = evt.packet.as<std::uint32_t>();
            auto pingTime = data & 0xffff;
            auto client = (data & 0xffff0000) >> 16;

            m_sharedData.connectionData[client].pingTime = pingTime;
        }
            break;
        case PacketID::CPUThink:
        {
            auto direction = evt.packet.as<std::uint8_t>();

            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::ThinkBubble;
            cmd.action = [direction](cro::Entity e, float)
            {
                auto& [dir, _] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
                dir = direction;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::ReadyQuitStatus:
            m_readyQuitFlags = evt.packet.as<std::uint8_t>();
            break;
        case PacketID::AchievementGet:
            notifyAchievement(evt.packet.as<std::array<std::uint8_t, 2u>>());
            break;
        case PacketID::ServerAchievement:
        {
            auto [client, player, achID] = evt.packet.as<std::array<std::uint8_t, 3u>>();
            if (client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[player].isCPU
                && achID < AchievementID::Count)
            {
                Achievements::awardAchievement(AchievementStrings[achID]);
            }
        }
            break;
        case PacketID::Gimme:
            {
                auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::Gimme;

                auto data = evt.packet.as<std::uint16_t>();
                auto client = (data >> 8);
                auto player = (data & 0x0f);

                showNotification(m_sharedData.connectionData[client].playerData[player].name + " took a Gimme");

                //inflate this so that the message board is correct - the update will come
                //in to assert this is correct afterwards
                m_sharedData.connectionData[client].playerData[player].holeScores[m_currentHole]++;
                m_sharedData.connectionData[client].playerData[player].holeComplete[m_currentHole] = true;
                showMessageBoard(MessageBoardID::Gimme);

                if (client == m_sharedData.localConnectionData.connectionID)
                {
                    if (m_sharedData.gimmeRadius == 1)
                    {
                        Achievements::incrementStat(StatStrings[StatID::LeatherGimmies]);
                    }
                    else
                    {
                        Achievements::incrementStat(StatStrings[StatID::PutterGimmies]);
                    }

                    if (!m_sharedData.connectionData[client].playerData[player].isCPU)
                    {
                        m_achievementTracker.noGimmeUsed = false;
                        m_achievementTracker.gimmes++;

                        if (m_achievementTracker.gimmes == 18)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::GimmeGimmeGimme]);
                        }

                        if (m_achievementTracker.nearMissChallenge)
                        {
                            Social::getMonthlyChallenge().updateChallenge(ChallengeID::Seven, 0);
                            m_achievementTracker.nearMissChallenge = false;
                        }
                    }
                }
            }
            break;
        case PacketID::BallLanded:
        {
            auto update = evt.packet.as<BallUpdate>();
            switch (update.terrain)
            {
            default: break;
            case TerrainID::Bunker:
                showMessageBoard(MessageBoardID::Bunker);
                break;
            case TerrainID::Scrub:
                showMessageBoard(MessageBoardID::Scrub);
                m_achievementTracker.hadFoul = (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
                    && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU);
                break;
            case TerrainID::Water:
                showMessageBoard(MessageBoardID::Water);
                m_achievementTracker.hadFoul = (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
                    && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU);
                break;
            case TerrainID::Hole:
                if (m_sharedData.tutorial)
                {
                    Achievements::setActive(true);
                    Achievements::awardAchievement(AchievementStrings[AchievementID::CluedUp]);
                    Achievements::setActive(m_allowAchievements);
                }

                bool special = false;
                std::int32_t score = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole];
                if (score == 1)
                {
                    auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::HoleInOne;
                    msg->position = m_holeData[m_currentHole].pin;
                }

                //check if this is our own score
                if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                {
                    if (m_currentHole == m_holeData.size() - 1)
                    {
                        //just completed the course
                        if (m_currentHole == 17) //full round
                        {
                            Achievements::incrementStat(StatStrings[StatID::HolesPlayed]);
                            Achievements::awardAchievement(AchievementStrings[AchievementID::JoinTheClub]);
                        }

                        if (m_sharedData.scoreType != ScoreType::Skins)
                        {
                            Social::awardXP(XPValues[XPID::CompleteCourse] / (18 / m_holeData.size()), XPStringID::CourseComplete);
                        }
                    }

                    //check putt distance / if this was in fact a putt
                    if (getClub() == ClubID::Putter)
                    {
                        if (glm::length(update.position - m_currentPlayer.position) > LongPuttDistance)
                        {
                            Achievements::incrementStat(StatStrings[StatID::LongPutts]);
                            Achievements::awardAchievement(AchievementStrings[AchievementID::PuttStar]);
                            Social::awardXP(XPValues[XPID::Special] / 2, XPStringID::LongPutt);
                            Social::getMonthlyChallenge().updateChallenge(ChallengeID::One, 0);
                            special = true;
                        }
                    }
                    else
                    {
                        if (getClub() > ClubID::NineIron)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::TopChip]);
                            Achievements::incrementStat(StatStrings[StatID::ChipIns]);
                            Social::awardXP(XPValues[XPID::Special], XPStringID::TopChip);
                        }

                        if (m_achievementTracker.hadBackspin)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::SpinClass]);
                            Social::awardXP(XPValues[XPID::Special] * 2, XPStringID::BackSpinSkill);
                        }
                        else if (m_achievementTracker.hadTopspin)
                        {
                            Social::awardXP(XPValues[XPID::Special] * 2, XPStringID::TopSpinSkill);
                        }
                    }
                }

                showMessageBoard(MessageBoardID::HoleScore, special);
                m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeComplete[m_currentHole] = true;

                break;
            }

            auto* msg = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::BallLanded;
            msg->terrain = update.position.y <= WaterLevel ? TerrainID::Water : update.terrain;
            msg->club = getClub();
            msg->travelDistance = glm::length2(update.position - m_currentPlayer.position);
            msg->pinDistance = glm::length2(update.position - m_holeData[m_currentHole].pin);

            //update achievements if this is local player
            if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
            {
                m_sharedData.timeStats[m_currentPlayer.player].holeTimes[m_currentHole] += m_turnTimer.elapsed().asSeconds();

                if (msg->terrain == TerrainID::Bunker)
                {
                    Achievements::incrementStat(StatStrings[StatID::SandTrapCount]);
                }
                else if (msg->terrain == TerrainID::Water)
                {
                    Achievements::incrementStat(StatStrings[StatID::WaterTrapCount]);
                }

                //this won't register if player 0 is CPU...
                if (m_currentPlayer.player == 0
                    && !m_sharedData.localConnectionData.playerData[0].isCPU)
                {
                    m_playTime += m_playTimer.elapsed();
                }

                //track other achievements awarded at the end of the round
                if (!m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
                {
                    if ((msg->terrain != TerrainID::Green
                        && msg->terrain != TerrainID::Fairway)
                        && m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] == 1) //first stroke
                    {
                        m_achievementTracker.alwaysOnTheCourse = false;
                    }
                }

                //update profile stats for distance
                if (msg->club == ClubID::Putter
                    && msg->terrain == TerrainID::Hole)
                {
                    m_personalBests[m_currentPlayer.player][m_currentHole].longestPutt = 
                        std::max(std::sqrt(msg->travelDistance), m_personalBests[m_currentPlayer.player][m_currentHole].longestPutt);

                    m_personalBests[m_currentPlayer.player][m_currentHole].wasPuttAssist = m_sharedData.showPuttingPower ? 1 : 0;
                }
                else if (msg->club < ClubID::FourIron)
                {
                    m_personalBests[m_currentPlayer.player][m_currentHole].longestDrive =
                        std::max(std::sqrt(msg->travelDistance), m_personalBests[m_currentPlayer.player][m_currentHole].longestDrive);
                }
            }
        }
            break;
        case PacketID::ClientDisconnected:
            removeClient(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::ServerError:
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                m_sharedData.errorMessage = "Server Error (Unknown)";
                break;
            case MessageType::MapNotFound:
                m_sharedData.errorMessage = "Server Failed To Load Map";
                break;
            }
            requestStackPush(StateID::Error);
            break;
        case PacketID::SetPlayer:
            if (m_photoMode)
            {
                toggleFreeCam();
            }
            else
            {
                resetIdle();
            }
            m_wantsGameState = false;
            m_newHole = false; //not necessarily a new hole, but the server has said player wants to go, regardless
            {
                auto playerData = evt.packet.as<ActivePlayer>();
                createTransition(playerData);
            }
            break;
        case PacketID::ActorSpawn:
            spawnBall(evt.packet.as<ActorInfo>());
            break;
        case PacketID::ActorUpdate:
            updateActor( evt.packet.as<ActorInfo>());            
            break;
        case PacketID::ActorAnimation:
        {
            if (m_activeAvatar)
            {
                auto animID = evt.packet.as<std::uint8_t>();
                if (animID == AnimationID::Celebrate)
                {
                    m_clubModels[ClubModel::Wood].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                    m_clubModels[ClubModel::Iron].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                }
                else
                {
                    m_clubModels[ClubModel::Wood].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                    m_clubModels[ClubModel::Iron].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                }

                /*if (animID == AnimationID::Swing)
                {
                    if(m_inputParser.getPower() * Clubs[getClub()].target < 20.f)
                    {
                        animID = AnimationID::Chip;
                    }
                }*/

                //TODO scale club model to zero if not idle or swing

                auto anim = m_activeAvatar->animationIDs[animID];
                auto& skel = m_activeAvatar->model.getComponent<cro::Skeleton>();

                if (skel.getState() == cro::Skeleton::Stopped
                    || (skel.getActiveAnimations().first != anim && skel.getActiveAnimations().second != anim))
                {
                    skel.play(anim, /*isCPU ? 2.f :*/ 1.f, 0.4f);
                }
            }
        }
            break;
        case PacketID::WindDirection:
            updateWindDisplay(cro::Util::Net::decompressVec3(evt.packet.as<std::array<std::int16_t, 3u>>()));
            break;
        case PacketID::SetHole:
            if (m_photoMode)
            {
                toggleFreeCam();
            }
            else
            {
                resetIdle();
            }
            setCurrentHole(evt.packet.as<std::uint16_t>());
            break;
        case PacketID::ScoreUpdate:
        {
            auto su = evt.packet.as<ScoreUpdate>();
            auto& player = m_sharedData.connectionData[su.client].playerData[su.player];
            
            if (su.hole < player.holeScores.size())
            {
                player.score = su.score;
                player.matchScore = su.matchScore;
                player.skinScore = su.skinsScore;
                player.holeScores[su.hole] = su.stroke;
                player.distanceScores[su.hole] = su.distanceScore;

                if (su.client == m_sharedData.localConnectionData.connectionID)
                {
                    if (getClub() == ClubID::Putter)
                    {
                        Achievements::incrementStat(StatStrings[StatID::PuttDistance], su.strokeDistance);
                    }
                    else
                    {
                        Achievements::incrementStat(StatStrings[StatID::StrokeDistance], su.strokeDistance);
                    }

                    m_personalBests[su.player][su.hole].hole = su.hole;
                    m_personalBests[su.player][su.hole].course = m_courseIndex;
                    m_personalBests[su.player][su.hole].score = su.stroke;
                }
                updateScoreboard(false);
            }
        }
            break;
        case PacketID::HoleWon:
            updateHoleScore(evt.packet.as<std::uint16_t>());
        break;
        case PacketID::GameEnd:
            showCountdown(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Lobby)
            {
                requestStackClear();
                requestStackPush(StateID::Menu);
            }
            break;
        case PacketID::EntityRemoved:
        {
            auto idx = evt.packet.as<std::uint32_t>();
            cro::Command cmd;
            cmd.targetFlags = CommandID::Ball;
            cmd.action = [&,idx](cro::Entity e, float)
            {
                if (e.getComponent<InterpolationComponent<InterpolationType::Linear>>().id == idx)
                {
                    //this just does some effects
                    auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::PowerShot;
                    msg->position = e.getComponent<cro::Transform>().getWorldPosition();

                    m_gameScene.destroyEntity(e);
                    LOG("Packet removed ball entity", cro::Logger::Type::Warning);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        }
        break;
    case net::NetEvent::ClientDisconnect:
        m_sharedData.errorMessage = "Disconnected From Server (Host Quit)";
        requestStackPush(StateID::Error);
        break;
    default: break;
    }
}

void GolfState::handleBullHit(const BullHit& bh)
{
    if (bh.client == m_sharedData.localConnectionData.connectionID)
    {
        if (!m_sharedData.localConnectionData.playerData[bh.player].isCPU)
        {
            if (!m_achievementTracker.bullseyeChallenge
                && m_sharedData.scoreType != ScoreType::MultiTarget)
            {
                Social::getMonthlyChallenge().updateChallenge(ChallengeID::Two, 0);
            }
            m_achievementTracker.bullseyeChallenge = true;

            if (!m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit)
            {
                auto xp = static_cast<std::int32_t>(80.f * bh.accuracy);
                if (xp)
                {
                    Social::awardXP(xp, XPStringID::BullsEyeHit);
                }
                else
                {
                    floatingMessage("Target Hit!");
                }

                auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::TargetHit;
                msg->position = bh.position;
            }
        }
        else if (!m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit)
        {
            auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::TargetHit;
            msg->position = bh.position;

            floatingMessage("Target Hit!");
        }
    }
    else if (!m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit)
    {
        auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
        msg->type = GolfEvent::TargetHit;
        msg->position = bh.position;

        floatingMessage("Target Hit!");
    }
    
    //hide the target
    cro::Command cmd;
    cmd.targetFlags = CommandID::BullsEye;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<BullsEyeData>().direction = AnimDirection::Shrink;
            e.getComponent<cro::Callback>().active = true;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit = true;
}

void GolfState::handleMaxStrokes(std::uint8_t reason)
{
    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeComplete[m_currentHole] = true;
    switch (m_sharedData.scoreType)
    {
    default:
        showNotification("Stroke Limit Reached.");
        break;
    case ScoreType::MultiTarget:
        if (reason == MaxStrokeID::Forfeit)
        {
            showNotification("Hole Forfeit: No Target Hit.");
        }
        else
        {
            showNotification("Stroke Limit Reached.");
        }
        break;
    case ScoreType::LongestDrive:
    case ScoreType::NearestThePin:
        //do nothing, this is integral behavior
        return;
    }

    auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
    msg->type = Social::SocialEvent::PlayerAchievement;
    msg->level = 1; //sad sound
}

void GolfState::removeClient(std::uint8_t clientID)
{
    cro::String str = m_sharedData.connectionData[clientID].playerData[0].name;
    for (auto i = 1u; i < m_sharedData.connectionData[clientID].playerCount; ++i)
    {
        str += ", " + m_sharedData.connectionData[clientID].playerData[i].name;
    }
    str += " left the game";

    showNotification(str);

    for (auto i = m_netStrengthIcons.size() - 1; i >= (m_netStrengthIcons.size() - m_sharedData.connectionData[clientID].playerCount); i--)
    {
        m_uiScene.destroyEntity(m_netStrengthIcons[i]);
    }
    m_netStrengthIcons.resize(m_netStrengthIcons.size() - m_sharedData.connectionData[clientID].playerCount);

    m_sharedData.connectionData[clientID].playerCount = 0;
    m_sharedData.connectionData[clientID].pingTime = 1000; //just so existing net indicators don't show green

    updateScoreboard();
}

void GolfState::setCurrentHole(std::uint16_t holeInfo)
{
    std::uint8_t hole = (holeInfo & 0xff00) >> 8;
    m_holeData[hole].par = (holeInfo & 0x00ff);

    //mark all holes complete - this fudges any missing
    //scores on the scoreboard... we shouldn't really have to do this :(
    if (hole > m_currentHole)
    {
        for (auto& client : m_sharedData.connectionData)
        {
            for (auto& player : client.playerData)
            {
                player.holeComplete[m_currentHole] = true;
            }
        }
    }


    if (hole != m_currentHole
        && m_sharedData.logBenchmarks)
    {
        dumpBenchmark();
    }


    //if this is the final hole repeated then we're in skins sudden death
    if (!m_sharedData.tutorial
        && m_sharedData.scoreType == ScoreType::Skins)
    {
        //TODO this will show if we're playing a custom course with
        //only one hole in skins mode (for some reason)
        if (hole == m_currentHole
            && hole == m_holeData.size() - 1)
        {
            showNotification("Sudden Death Round!");
            showNotification("First to hole wins!");
            m_suddenDeath = true;
        }
    }


    //update all the total hole times
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        m_sharedData.timeStats[i].totalTime += m_sharedData.timeStats[i].holeTimes[m_currentHole];

        Achievements::incrementStat(StatStrings[StatID::TimeOnTheCourse], m_sharedData.timeStats[i].holeTimes[m_currentHole]);
    }

    //update the look-at target in multi-target mode
    for (auto& client : m_sharedData.connectionData)
    {
        for (auto& player : client.playerData)
        {
            player.targetHit = false;
        }
    }

    updateScoreboard();
    m_achievementTracker.hadFoul = false;
    m_achievementTracker.bullseyeChallenge = false;
    m_achievementTracker.puttCount = 0;

    //can't get these when putting else it's
    //far too easy (we're technically always on the green)
    if (m_holeData[hole].puttFromTee)
    {
        m_achievementTracker.alwaysOnTheCourse = false;
        m_achievementTracker.twoShotsSpare = false;
    }

    //CRO_ASSERT(hole < m_holeData.size(), "");
    if (hole >= m_holeData.size())
    {
        m_sharedData.errorMessage = "Server requested hole\nnot found";
        requestStackPush(StateID::Error);
        return;
    }

    m_terrainBuilder.update(hole);
    m_gameScene.getSystem<ClientCollisionSystem>()->setMap(hole);
    m_gameScene.getSystem<ClientCollisionSystem>()->setPinPosition(m_holeData[hole].pin);
    m_collisionMesh.updateCollisionMesh(m_holeData[hole].modelEntity.getComponent<cro::Model>().getMeshData());

    //create hole model transition
    bool rescale = (hole == 0) || (m_holeData[hole - 1].modelPath != m_holeData[hole].modelPath);
    auto* propModels = &m_holeData[m_currentHole].propEntities;
    auto* particles = &m_holeData[m_currentHole].particleEntities;
    auto* audio = &m_holeData[m_currentHole].audioEntities;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().active = true;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().setUserData<float>(0.f);
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().function =
        [&, propModels, particles, audio, rescale](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + (dt / 2.f));

        if (rescale)
        {
            float scale = 1.f - progress;
            e.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });
            m_waterEnt.getComponent<cro::Transform>().setScale({ scale, scale, scale });
        }

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            e.getComponent<cro::Model>().setHidden(rescale);

            for (auto i = 0u; i < propModels->size(); ++i)
            {
                //if we're not rescaling we're recycling the model so don't hide its props
                propModels->at(i).getComponent<cro::Model>().setHidden(rescale);
            }

            if (rescale)
            {
                for (auto i = 0u; i < particles->size(); ++i)
                {
                    particles->at(i).getComponent<cro::ParticleEmitter>().stop();
                }
                //should already be started otherwise...

                for (auto i = 0u; i < audio->size(); ++i)
                {
                    audio->at(i).getComponent<cro::AudioEmitter>().stop();
                }

                for (auto spectator : m_spectatorModels)
                {
                    spectator.getComponent<cro::Model>().setHidden(true);
                    spectator.getComponent<Spectator>().path = nullptr;
                    spectator.getComponent<cro::Skeleton>().stop();
                }
            }

            //index should be updated by now (as this is a callback)
            //so we're actually targetting the next hole entity
            auto entity = m_holeData[m_currentHole].modelEntity;
            entity.getComponent<cro::Model>().setHidden(false);

            if (rescale)
            {
                entity.getComponent<cro::Transform>().setScale({ 0.f, 1.f, 0.f });
            }
            else
            {
                entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f, 1.f });
            }
            entity.getComponent<cro::Callback>().setUserData<float>(0.f);
            entity.getComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, rescale](cro::Entity ent, float dt)
            {
                auto& progress = ent.getComponent<cro::Callback>().getUserData<float>();
                progress = std::min(1.f, progress + (dt / 2.f));

                if (rescale)
                {
                    float scale = progress;
                    ent.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });
                    m_waterEnt.getComponent<cro::Transform>().setScale({ scale, scale, scale });
                }

                if (progress == 1)
                {
                    ent.getComponent<cro::Callback>().active = false;
                    updateMiniMap();
                }
            };

            //unhide any prop models
            for (auto prop : m_holeData[m_currentHole].propEntities)
            {
                prop.getComponent<cro::Model>().setHidden(false);

                if (prop.hasComponent<cro::Skeleton>()
                    //&& !prop.getComponent<cro::Skeleton>().getAnimations().empty())
                    && prop.getComponent<cro::Skeleton>().getAnimations().size() == 1)
                {
                    //this is a bit kludgy without animation index mapping - 
                    //basically props with one anim are idle, others might be
                    //specialised like clapping, which we don't want to trigger
                    //on a new hole
                    //if (prop.getComponent<cro::Skeleton>().getAnimations().size() == 1)
                    {
                        prop.getComponent<cro::Skeleton>().play(0);
                    }
                }
            }

            for (auto particle : m_holeData[m_currentHole].particleEntities)
            {
                particle.getComponent<cro::ParticleEmitter>().start();
            }

            for (auto audio : m_holeData[m_currentHole].audioEntities)
            {
                audio.getComponent<cro::AudioEmitter>().play();
                audio.getComponent<cro::Callback>().active = true;
            }

            //check hole for any crowd paths and assign any free
            //spectator models we have
            if (rescale &&
                !m_holeData[m_currentHole].crowdCurves.empty())
            {
                auto modelsPerPath = std::min(std::size_t(6), m_spectatorModels.size() / m_holeData[m_currentHole].crowdCurves.size());
                std::size_t assignedModels = 0;
                for (const auto& curve : m_holeData[m_currentHole].crowdCurves)
                {
                    for (auto i = 0u; i < modelsPerPath && assignedModels < m_spectatorModels.size(); i++, assignedModels++)
                    {
                        auto model = m_spectatorModels[assignedModels];
                        model.getComponent<cro::Model>().setHidden(false);
                        
                        auto& spectator = model.getComponent<Spectator>();
                        spectator.path = &curve;
                        spectator.target = i % curve.getPoints().size();
                        spectator.stateTime = 0.f;
                        spectator.state = Spectator::State::Pause;
                        spectator.direction = spectator.target < (curve.getPoints().size() / 2) ? -1 : 1;
                        //spectator.walkSpeed = 1.f;// +cro::Util::Random::value(-0.1f, 0.15f);

                        model.getComponent<cro::Skeleton>().play(spectator.anims[Spectator::AnimID::Idle]);
#ifdef CRO_DEBUG_
                        model.getComponent<cro::Skeleton>().setInterpolationEnabled(false);
                        model.getComponent<cro::ShadowCaster>().active = false;
#endif
                        model.getComponent<cro::Transform>().setPosition(curve.getPoint(spectator.target));
                        m_holeData[m_currentHole].modelEntity.getComponent<cro::Transform>().addChild(model.getComponent<cro::Transform>());
                    }
                }
            }

            m_gameScene.getSystem<SpectatorSystem>()->updateSpectatorGroups();


            if (entity != e)
            {
                //new hole model so remove old props
                for (auto i = 0u; i < propModels->size(); ++i)
                {
                    m_gameScene.destroyEntity(propModels->at(i));
                }
                propModels->clear();

                for (auto i = 0u; i < particles->size(); ++i)
                {
                    m_gameScene.destroyEntity(particles->at(i));
                }
                particles->clear();
                
                for (auto i = 0u; i < audio->size(); ++i)
                {
                    //m_gameScene.destroyEntity(audio->at(i));
                    audio->at(i).getComponent<cro::Callback>().function =
                        [&](cro::Entity e, float dt)
                    {
                        //fade out then remove oneself
                        auto vol = e.getComponent<cro::AudioEmitter>().getVolume();
                        vol = std::max(0.f, vol - dt);
                        e.getComponent<cro::AudioEmitter>().setVolume(vol);

                        if (vol == 0)
                        {
                            e.getComponent<cro::AudioEmitter>().stop();
                            e.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(e);
                        }
                    };
                    audio->at(i).getComponent<cro::Callback>().active = true;
                }
                audio->clear();
            }
        }
    };

    m_currentHole = hole;
    startFlyBy(); //requires current hole

    //restore the drone if someone hit it
    if (!m_drone.isValid())
    {
        createDrone();
    }


    //map collision data
    m_currentMap.loadFromFile(m_holeData[m_currentHole].mapPath);

    //make sure we have the correct target position
    m_cameras[CameraID::Player].getComponent<TargetInfo>().targetHeight = m_holeData[m_currentHole].puttFromTee ? CameraPuttHeight : CameraStrokeHeight;
    m_cameras[CameraID::Player].getComponent<TargetInfo>().targetOffset = m_holeData[m_currentHole].puttFromTee ? CameraPuttOffset : CameraStrokeOffset;
    m_cameras[CameraID::Player].getComponent<TargetInfo>().targetLookAt = m_holeData[m_currentHole].target;

    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point the ent destroys itself - also interps the position of the tee/flag
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_currentPlayer.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<glm::vec3>(m_currentPlayer.position);
    entity.getComponent<cro::Callback>().function =
        [&, hole](cro::Entity e, float dt)
    {
        auto currPos = e.getComponent<cro::Transform>().getPosition();
        auto travel = m_holeData[m_currentHole].tee - currPos;

        auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();

        //if we're moving on to any other than the first hole, interp the
        //tee and hole position based on how close to the tee the camera is
        float percent = 1.f;
        if (hole > 0)
        {
            auto startPos = e.getComponent<cro::Callback>().getUserData<glm::vec3>();
            auto totalDist = glm::length(m_holeData[m_currentHole].tee - startPos);
            auto currentDist = glm::length(travel);

            percent = (totalDist - currentDist) / totalDist;
            percent = std::min(1.f, std::max(0.f, percent));

            targetInfo.currentLookAt = targetInfo.prevLookAt + ((targetInfo.targetLookAt - targetInfo.prevLookAt) * percent);

            auto pinMove = m_holeData[m_currentHole].pin - m_holeData[m_currentHole - 1].pin;
            auto pinPos = m_holeData[m_currentHole - 1].pin + (pinMove * percent);

            cro::Command cmd;
            cmd.targetFlags = CommandID::Hole;
            cmd.action = [pinPos](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition(pinPos);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //updates the height shaders on the greens
            for (auto& s : m_gridShaders)
            {
                glUseProgram(s.shaderID);
                glUniform1f(s.holeHeight, pinPos.y);
            }

            auto teeMove = m_holeData[m_currentHole].tee - m_holeData[m_currentHole - 1].tee;
            auto teePos = m_holeData[m_currentHole - 1].tee + (teeMove * percent);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&, teePos](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition(teePos);

                auto pinDir = m_holeData[m_currentHole].target - teePos;
                auto rotation = std::atan2(-pinDir.z, pinDir.x);
                e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            auto targetDir = m_holeData[m_currentHole].target - currPos;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);

            //randomise the cart positions a bit
            cmd.targetFlags = CommandID::Cart;
            cmd.action = [&](cro::Entity e, float)
            {
                //move to ground level
                auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                auto result = m_collisionMesh.getTerrain(pos);
                float diff = result.height - pos.y;

                e.getComponent<cro::Transform>().move({ 0.f, diff, 0.f });

                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);

                //and orientate to slope
                /*auto r = glm::rotation(cro::Transform::Y_AXIS, result.normal);
                e.getComponent<cro::Transform>().setRotation(r);
                e.getComponent<cro::Transform>().rotate(glm::inverse(r) * cro::Transform::Y_AXIS, cro::Util::Const::PI);*/
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        else
        {
            auto targetDir = m_holeData[m_currentHole].target - currPos;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);
        }


        if (glm::length2(travel) < 0.0001f)
        {
            float height = m_holeData[m_currentHole].pin.y;
            for (auto& s : m_gridShaders)
            {
                glUseProgram(s.shaderID);
                glUniform1f(s.holeHeight, height);
            }

            targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
            targetInfo.startHeight = targetInfo.targetHeight;
            targetInfo.startOffset = targetInfo.targetOffset;

            auto targetDir = m_holeData[m_currentHole].target - m_holeData[m_currentHole].tee;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);

            //we're there
            setCameraPosition(m_holeData[m_currentHole].tee, targetInfo.targetHeight, targetInfo.targetOffset);

            //set tee / flag
            cro::Command cmd;
            cmd.targetFlags = CommandID::Hole;
            cmd.action = [&](cro::Entity en, float)
            {
                auto pos = m_holeData[m_currentHole].pin;
                pos.y += 0.0001f;

                en.getComponent<cro::Transform>().setPosition(pos);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&](cro::Entity en, float)
            {
                en.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].tee);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //remove the transition ent
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            auto height = targetInfo.targetHeight - targetInfo.startHeight;
            auto offset = targetInfo.targetOffset - targetInfo.startOffset;

            static constexpr float Speed = 4.f;
            e.getComponent<cro::Transform>().move(travel * Speed * dt);
            setCameraPosition(e.getComponent<cro::Transform>().getPosition(),
                targetInfo.startHeight + (height * percent),
                targetInfo.startOffset + (offset * percent));
        }
    };

    m_currentPlayer.position = m_holeData[m_currentHole].tee;


    m_inputParser.setHoleDirection(m_holeData[m_currentHole].target - m_currentPlayer.position);
    m_currentPlayer.terrain = m_holeData[m_currentHole].puttFromTee ? TerrainID::Green : TerrainID::Fairway; //this will be overwritten from the server but setting this to non-green makes sure the mini cam stops updating in time
    m_inputParser.setMaxClub(m_holeData[m_currentHole].distanceToPin, true); //limits club selection based on hole size

    //hide the slope indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [](cro::Entity e, float)
    {
        //e.getComponent<cro::Model>().setHidden(true);
        e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    m_terrainBuilder.setSlopePosition(m_holeData[m_currentHole].pin);

    //update the UI
    cmd.targetFlags = CommandID::UI::HoleNumber;
    cmd.action =
        [&](cro::Entity e, float)
    {
        auto holeNumber = m_currentHole + 1;
        if (m_sharedData.reverseCourse)
        {
            holeNumber = static_cast<std::uint32_t>(m_holeData.size() + 1) - holeNumber;

            if (m_sharedData.scoreType == ScoreType::ShortRound)
            {
                switch (m_sharedData.holeCount)
                {
                default:
                case 0:
                    holeNumber += 6;
                    break;
                case 1:
                case 2:
                    holeNumber += 3;
                    break;
                }
            }
        }

        if (m_sharedData.holeCount == 2)
        {
            if (m_sharedData.scoreType == ScoreType::ShortRound
                && m_courseIndex != -1)
            {
                holeNumber += 9;
            }
            else
            {
                holeNumber += static_cast<std::uint32_t>(m_holeData.size());
            }
        }

        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = "Hole: " + std::to_string(holeNumber);
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //hide the overhead view
    cmd.targetFlags = CommandID::UI::MiniGreen;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide current terrain
    cmd.targetFlags = CommandID::UI::TerrainType;
    cmd.action =
        [](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(" ");
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //tell the tee and its children to scale depending on if they should be visible
    cmd.targetFlags = CommandID::Tee;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().setUserData<float>(m_holeData[m_currentHole].puttFromTee ? 0.f : 1.f);
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //set green cam position
    auto holePos = m_holeData[m_currentHole].pin;
    m_greenCam.getComponent<cro::Transform>().setPosition({ holePos.x, holePos.y + 0.5f, holePos.z });


    //and the uh, other green cam. The spectator one
    setGreenCamPosition();


    //reset the flag
    cmd.targetFlags = CommandID::Flag;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<FlagCallbackData>().targetHeight = 0.f;
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //update status
    const std::size_t MaxTitleLen = 220;

    auto title = m_sharedData.tutorial ? cro::String("Tutorial").toUtf8() : m_courseTitle.substr(0, MaxTitleLen).toUtf8();
    auto holeNumber = std::to_string(m_currentHole + 1);
    auto holeTotal = std::to_string(m_holeData.size());
    //well... this is awful.
    Social::setStatus(Social::InfoID::Course, { reinterpret_cast<const char*>(title.c_str()), holeNumber.c_str(), holeTotal.c_str() });


    //cue up next depth map
    auto next = m_currentHole + 1;
    if (next < m_holeData.size())
    {
        m_depthMap.setModel(m_holeData[next]);
    }
    else
    {
        m_depthMap.forceSwap(); //make sure we're reading the correct texture anyway
    }
    m_waterEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_depthMap", m_depthMap.getTexture());

    m_sharedData.minimapData.teePos = m_holeData[m_currentHole].tee;
    m_sharedData.minimapData.pinPos = m_holeData[m_currentHole].pin;
    //m_sharedData.minimapData.holeNumber = m_currentHole;
    
    if (m_sharedData.reverseCourse)
    {
        if (m_sharedData.holeCount == 0)
        {
            m_sharedData.minimapData.holeNumber = 17 - m_sharedData.minimapData.holeNumber;
        }
        else
        {
            m_sharedData.minimapData.holeNumber = 8 - m_sharedData.minimapData.holeNumber;
        }
    }
    if (m_sharedData.holeCount == 2)
    {
        m_sharedData.minimapData.holeNumber += 9;
    }


    m_sharedData.minimapData.courseName = m_courseTitle;
    m_sharedData.minimapData.courseName += "\n";
#ifdef USE_GNS
    m_sharedData.minimapData.courseName += Social::getLeader(m_sharedData.mapDirectory, m_sharedData.holeCount);
#endif
    m_sharedData.minimapData.courseName += "\nHole: " + std::to_string(m_sharedData.minimapData.holeNumber + 2); //this isn't updated until the map texture is 
    m_sharedData.minimapData.courseName += "\nPar: " + std::to_string(m_holeData[m_currentHole].par);
    m_gameScene.getDirector<GolfSoundDirector>()->setCrowdPositions(m_holeData[m_currentHole].crowdPositions);
}

void GolfState::setCameraPosition(glm::vec3 position, float height, float viewOffset)
{
    static constexpr float MinDist = 6.f;
    static constexpr float MaxDist = 270.f;
    static constexpr float DistDiff = MaxDist - MinDist;
    float heightMultiplier = 1.f; //goes to -1.f at max dist

    auto camEnt = m_cameras[CameraID::Player];
    auto& targetInfo = camEnt.getComponent<TargetInfo>();
    auto target = targetInfo.currentLookAt - position;

    auto dist = glm::length(target);
    float distNorm = std::min(1.f, (dist - MinDist) / DistDiff);
    heightMultiplier -= (2.f * distNorm);

    target *= 1.f - ((1.f - 0.08f) * distNorm);
    target += position;

    auto result = m_collisionMesh.getTerrain(position);

    camEnt.getComponent<cro::Transform>().setPosition({ position.x, result.height + height, position.z });


    auto currentCamTarget = glm::vec3(target.x, result.height + (height * heightMultiplier), target.z);
    camEnt.getComponent<TargetInfo>().finalLookAt = currentCamTarget;

    auto oldPos = camEnt.getComponent<cro::Transform>().getPosition();
    camEnt.getComponent<cro::Transform>().setRotation(lookRotation(oldPos, currentCamTarget));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * viewOffset);

    //clamp above ground height and hole radius
    auto newPos = camEnt.getComponent<cro::Transform>().getPosition();

    static constexpr float MinRad = 0.6f + CameraPuttOffset;
    static constexpr float MinRadSqr = MinRad * MinRad;

    auto holeDir = m_holeData[m_currentHole].pin - newPos;
    auto holeDist = glm::length2(holeDir);
    /*if (holeDist < MinRadSqr)
    {
        auto len = std::sqrt(holeDist);
        auto move = MinRad - len;
        holeDir /= len;
        holeDir *= move;
        newPos -= holeDir;
    }*/

    //lower height as we get closer to hole
    heightMultiplier = std::min(1.f, std::max(0.f, holeDist / MinRadSqr));
    //if (!m_holeData[m_currentHole].puttFromTee)
    {
        heightMultiplier *= CameraTeeMultiplier;
    }

    auto groundHeight = m_collisionMesh.getTerrain(newPos).height;
    newPos.y = std::max(groundHeight + (CameraPuttHeight * heightMultiplier), newPos.y);

    camEnt.getComponent<cro::Transform>().setPosition(newPos);
    
    //hmm this stops the putt cam jumping when the position has been offset
    //however the look at point is no longer the hole, so the rotation
    //is weird and we need to interpolate back through the offset
    camEnt.getComponent<TargetInfo>().finalLookAt += newPos - oldPos;
    camEnt.getComponent<TargetInfo>().finalLookAtOffset = newPos - oldPos;

    //also updated by camera follower...
    if (targetInfo.waterPlane.isValid())
    {
        targetInfo.waterPlane.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
    }
}

void GolfState::requestNextPlayer(const ActivePlayer& player)
{
    if (!m_sharedData.tutorial)
    {
        m_currentPlayer = player;
        Club::setClubLevel(0); //always use the default set for the tutorial
        //setCurrentPlayer() is called when the sign closes

        showMessageBoard(MessageBoardID::PlayerName);
        showScoreboard(false);
    }
    else
    {
        setCurrentPlayer(player);
    }

    //raise message for particles
    auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg->position = player.position;
    msg->type = GolfEvent::RequestNewPlayer;
    msg->terrain = player.terrain;
}

void GolfState::setCurrentPlayer(const ActivePlayer& player)
{
    cro::App::getWindow().setMouseCaptured(true);
    m_achievementTracker.hadBackspin = false;
    m_achievementTracker.hadTopspin = false;
    m_achievementTracker.nearMissChallenge = false;
    m_turnTimer.restart();
    m_idleTimer.restart();
    m_playTimer.restart();
    m_idleTime = cro::seconds(90.f);
    m_skipState = {};
    m_ballTrail.setNext();


    auto localPlayer = (player.client == m_sharedData.clientConnection.connectionID);
    auto isCPU = m_sharedData.localConnectionData.playerData[player.player].isCPU;

    m_gameScene.getDirector<GolfSoundDirector>()->setActivePlayer(player.client, player.player, isCPU && m_sharedData.fastCPU);
    m_avatars[player.client][player.player].ballModel.getComponent<cro::Transform>().setScale(glm::vec3(1.f));

    m_resolutionUpdate.targetFade = player.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;

    updateScoreboard(false);
    showScoreboard(false);

    Club::setClubLevel(isCPU ? m_sharedData.clubLimit ? m_sharedData.clubSet : m_cpuGolfer.getClubLevel() : m_sharedData.clubSet); //do this first else setActive has the wrong estimation distance
    auto lie = m_avatars[player.client][player.player].ballModel.getComponent<ClientCollider>().lie;

    m_sharedData.inputBinding.playerID = localPlayer ? player.player : 0; //this also affects who can emote, so if we're currently emoting when it's not our turn always be player 0(??)
    m_inputParser.setActive(localPlayer && !m_photoMode, m_currentPlayer.terrain, isCPU, lie);
    m_restoreInput = localPlayer; //if we're in photo mode should we restore input parser?
    Achievements::setActive(localPlayer && !isCPU && m_allowAchievements);

    if (player.terrain == TerrainID::Bunker)
    {
        auto clubID = lie == 0 ? ClubID::SevenIron : ClubID::FourIron;
        m_inputParser.setMaxClub(clubID);
    }
    else
    {
        m_inputParser.setMaxClub(m_holeData[m_currentHole].distanceToPin, glm::length2(player.position - m_holeData[m_currentHole].tee) < 1.f);
    }

    //player UI name
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name;
        e.getComponent<cro::Callback>().active = true;
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PlayerIcon;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
        e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PinDistance;
    cmd.action =
        [&](cro::Entity e, float)
    {
        formatDistanceString(m_distanceToHole, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements);

        auto bounds = cro::Text::getLocalBounds(e);
        bounds.width = std::floor(bounds.width / 2.f);
        e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action =
        [&,player](cro::Entity e, float)
    {
        auto pid = ((player.client * ConstVal::MaxPlayers) + player.player) + 1;

        if (e.getComponent<MiniBall>().playerID == pid)
        {
            //play the callback animation
            e.getComponent<MiniBall>().state = MiniBall::Animating;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //display current terrain
    cmd.targetFlags = CommandID::UI::TerrainType;
    cmd.action =
        [&,player, lie](cro::Entity e, float)
    {
        if (player.terrain == TerrainID::Bunker)
        {
            static const std::array<std::string, 2u> str = { u8"Bunker ↓", u8"Bunker ↑" };
            e.getComponent<cro::Text>().setString(cro::String::fromUtf8(str[lie].begin(), str[lie].end()));
        }
        else
        {
            e.getComponent<cro::Text>().setString(TerrainStrings[player.terrain]);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show ui if this is our client    
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [&,localPlayer, isCPU](cro::Entity e, float)
    {
        //only show CPU power to beginners
        std::int32_t show = localPlayer && (!isCPU || Social::getLevel() < 10) ? 0 : 1;

        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = show;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //reset any warning
    cmd.targetFlags = CommandID::UI::AFKWarn;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            e.getComponent<cro::Callback>().active = false;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //stroke indicator is in model scene...
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [&,localPlayer, player](cro::Entity e, float)
    {
        auto position = player.position;
        position.y += 0.014f; //z-fighting
        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[player.player].isCPU));
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getYaw());
        e.getComponent<cro::Callback>().active = localPlayer;
        e.getComponent<cro::Model>().setDepthTestEnabled(0, /*player.terrain == TerrainID::Green*/false);

        e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);

        //fudgy way of changing the render type when putting
        if (e.getComponent<cro::CommandTarget>().ID & CommandID::StrokeIndicator)
        {
            if (player.terrain == TerrainID::Green)
            {
                e.getComponent<cro::Model>().getMeshData().indexData[0].primitiveType = GL_TRIANGLES;
            }
            else
            {
                e.getComponent<cro::Model>().getMeshData().indexData[0].primitiveType = GL_LINE_STRIP;
            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //if client is ours activate input/set initial stroke direction
    auto target = m_cameras[CameraID::Player].getComponent<TargetInfo>().targetLookAt;
    m_inputParser.resetPower();
    m_inputParser.setHoleDirection(target - player.position);
    
    //we can set the CPU skill extra wide without worrying about
    //rotating the player model as the aim is hidden anyway
    auto rotation = isCPU ? m_cpuGolfer.getSkillIndex() > 2 ? cro::Util::Const::PI : MaxRotation / 3.f : MaxRotation;
    m_inputParser.setMaxRotation(m_holeData[m_currentHole].puttFromTee ? MaxPuttRotation : 
        player.terrain == TerrainID::Green ? rotation / 3.f : rotation);


    //set this separately because target might not necessarily be the pin.
    bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit);
    auto clubTarget = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
    m_inputParser.setClub(glm::length(clubTarget - player.position));


    cmd.targetFlags = CommandID::BullsEye;
    cmd.action = [isMultiTarget](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<BullsEyeData>().direction = isMultiTarget ? AnimDirection::Grow : AnimDirection::Shrink;
            e.getComponent<cro::Callback>().active = true;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //check if input is CPU
    if (localPlayer
        && isCPU)
    {
        //if the CPU is smart enough always go for the hole if we can
        if (m_cpuGolfer.getSkillIndex() > 3
            /*&& !m_holeData[m_currentHole].puttFromTee*/)
        {
            //fallback is used when repeatedly launching the ball into the woods...
            m_cpuGolfer.activate(/*m_holeData[m_currentHole].pin*/clubTarget, m_holeData[m_currentHole].target, m_holeData[m_currentHole].puttFromTee);
        }

        else
        {
            //if the player can rotate enough prefer the hole as the target
            auto pin = /*m_holeData[m_currentHole].pin*/clubTarget - player.position;
            auto targetPoint = target - player.position;

            auto p = glm::normalize(glm::vec2(pin.x, -pin.z));
            auto t = glm::normalize(glm::vec2(targetPoint.x, -targetPoint.z));

            float dot = glm::dot(p, t);
            float det = (p.x * t.y) - (p.y * t.x);
            float targetAngle = std::abs(std::atan2(det, dot));

            if (targetAngle < m_inputParser.getMaxRotation()/*cro::Util::Const::PI / 2.f*/)
            {
                m_cpuGolfer.activate(/*m_holeData[m_currentHole].pin*/clubTarget, m_holeData[m_currentHole].target, m_holeData[m_currentHole].puttFromTee);
            }
            else
            {
                //aim for whichever is closer (target or pin)
                //if (glm::length2(target - player.position) < glm::length2(m_holeData[m_currentHole].pin - player.position))
                {
                    m_cpuGolfer.activate(target, m_holeData[m_currentHole].target, m_holeData[m_currentHole].puttFromTee);
                }
                /*else
                {
                    m_cpuGolfer.activate(m_holeData[m_currentHole].pin, target, m_holeData[m_currentHole].puttFromTee);
                }*/
            }
#ifdef CRO_DEBUG_
            //CPUTarget.getComponent<cro::Transform>().setPosition(m_cpuGolfer.getTarget());
#endif
        }
    }


    //this just makes sure to update the direction indicator
    //regardless of whether or not we actually switched clubs
    //it's a hack where above case tells the input parser not to update the club (because we're the same player)
    //but we've also landed on th green and therefor auto-switched to a putter
    auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;


    //show the new player model
    m_activeAvatar = &m_avatars[m_currentPlayer.client][m_currentPlayer.player];
    m_activeAvatar->model.getComponent<cro::Transform>().setPosition(player.position);
    if (player.terrain != TerrainID::Green)
    {
        auto playerRotation = m_camRotation - (cro::Util::Const::PI / 2.f);

        m_activeAvatar->model.getComponent<cro::Model>().setHidden(false);
        m_activeAvatar->model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, playerRotation);
        m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 0;
        m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().scale = 0.f;
        m_activeAvatar->model.getComponent<cro::Callback>().active = true;

        if (m_activeAvatar->hands)
        {
            if (getClub() < ClubID::FiveIron)
            {
                m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Wood]);
            }
            else
            {
                m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Iron]);
            }
            m_activeAvatar->hands->getModel().getComponent<cro::Model>().setFacing(m_activeAvatar->model.getComponent<cro::Model>().getFacing());
        }

        //set just far enough the flag shows in the distance
        m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);


        //update the position of the bystander camera
        //make sure to reset any zoom
        auto& zoomData = m_cameras[CameraID::Bystander].getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>();
        zoomData.progress = 0.f;
        zoomData.fov = 1.f;
        m_cameras[CameraID::Bystander].getComponent<cro::Camera>().resizeCallback(m_cameras[CameraID::Bystander].getComponent<cro::Camera>());

        //then set new position
        auto eye = CameraBystanderOffset;
        if (m_activeAvatar->model.getComponent<cro::Model>().getFacing() == cro::Model::Facing::Back)
        {
            eye.x *= -1.f;
        }
        eye = glm::rotateY(eye, playerRotation);
        eye += player.position;

        auto result = m_collisionMesh.getTerrain(eye);
        auto terrainHeight = result.height + CameraBystanderOffset.y;
        if (terrainHeight > eye.y)
        {
            eye.y = terrainHeight;
        }

        auto target = player.position;
        target.y += 1.f;

        m_cameras[CameraID::Bystander].getComponent<cro::Transform>().setRotation(lookRotation(eye, target));
        m_cameras[CameraID::Bystander].getComponent<cro::Transform>().setPosition(eye);

        //if this is a CPU player or a remote player, show a bystander cam automatically
        if (player.client != m_sharedData.localConnectionData.connectionID
            || 
            (player.client == m_sharedData.localConnectionData.connectionID &&
            m_sharedData.localConnectionData.playerData[player.player].isCPU &&
                !m_sharedData.fastCPU))
        {
            static constexpr float MinCamDist = 25.f;
            if (cro::Util::Random::value(0,2) != 0 &&
                glm::length2(player.position - m_holeData[m_currentHole].pin) > (MinCamDist * MinCamDist))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(2.7f);
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt;
                    if (currTime < 0)
                    {
                        setActiveCamera(CameraID::Bystander);

                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                };
            }
        }
    }
    else
    {
        m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

        //and use closer shadow mapping
        m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowNearDistance);
    }
    setActiveCamera(CameraID::Player);

    //show or hide the slope indicator depending if we're on the green
    //or if we're on a putting map (in which case we're using the contour material)
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [&,player](cro::Entity e, float)
    {
        bool hidden = ((player.terrain != TerrainID::Green) /*&& m_distanceToHole > Clubs[ClubID::GapWedge].getBaseTarget()*/) || m_holeData[m_currentHole].puttFromTee;

        if (!hidden)
        {
            e.getComponent<cro::Model>().setHidden(hidden);
            e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 0;
            e.getComponent<cro::Callback>().active = true;
        }
        else
        {
            e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
            e.getComponent<cro::Callback>().active = true;
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //also check if we need to display mini map for green
    cmd.targetFlags = CommandID::UI::MiniGreen;
    cmd.action = [&, player](cro::Entity e, float)
    {
        bool hidden = (player.terrain != TerrainID::Green);

        if (!hidden)
        {
            e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 0;
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            m_greenCam.getComponent<cro::Camera>().active = true;
        }
        else
        {
            e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 1;
            e.getComponent<cro::Callback>().active = true;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    m_currentPlayer = player;

    //announce player has changed
    auto* msg2 = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg2->position = m_currentPlayer.position;
    msg2->terrain = m_currentPlayer.terrain;
    msg2->type = GolfEvent::SetNewPlayer;

    m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::NewPlayer, 0, net::NetFlag::Reliable, ConstVal::NetChannelReliable);



    //this is just so that the particle director knows if we're on a new hole
    if (glm::length2(m_currentPlayer.position - m_holeData[m_currentHole].tee) < (0.05f * 0.05f))
    {
        msg2->travelDistance = -1.f;
    }

    m_gameScene.setSystemActive<CameraFollowSystem>(false);

    const auto setCamTarget = [&](glm::vec3 pos)
    {
        if (m_drone.isValid())
        {
            auto& data = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>();
            data.target.getComponent<cro::Transform>().setPosition(pos);
            data.resetPosition = pos;
        }
        else
        {
            m_cameras[CameraID::Sky].getComponent<cro::Transform>().setPosition(pos);
        }
    };

    //see where the player is and move the sky cam if possible
    //else set it to the default position
    static constexpr float MinPlayerDist = 40.f * 40.f; //TODO should this be the sky cam radius^2?
    auto dir = m_holeData[m_currentHole].pin - player.position;
    if (auto len2 = glm::length2(dir); len2 > MinPlayerDist)
    {
        static constexpr float MaxHeightMultiplier = static_cast<float>(MapSize.x) * 0.8f; //probably should be diagonal but meh
        
        auto camHeight = SkyCamHeight * std::min(1.2f, (std::sqrt(len2) / MaxHeightMultiplier));
        dir /= 2.f;
        dir.y = camHeight;

        dir += player.position;

        /*auto centre = glm::vec3(static_cast<float>(MapSize.x) / 2.f, camHeight, -static_cast<float>(MapSize.y) / 2.f);
        dir += (centre - dir) / 2.f;*/
        auto perp = glm::vec3( dir.z, dir.y, -dir.x );
        if (cro::Util::Random::value(0, 1) == 0)
        {
            perp.x *= -1.f;
            perp.z *= -1.f;
        }
        perp *= static_cast<float>(cro::Util::Random::value(7, 9)) / 100.f;
        perp *= m_holeToModelRatio;
        dir += perp;

        //make sure we're not too close to the player else we won't switch to this cam
        auto followerRad = m_cameras[CameraID::Sky].getComponent<CameraFollower>().radius;
        if (len2 = glm::length2(player.position - dir); len2 < followerRad)
        {
            auto diff = std::sqrt(followerRad) - std::sqrt(len2);
            dir += glm::normalize(dir - player.position) * (diff * 1.01f);

            //clamp the height - if we're so close this needs to happen
            //then we don't want to switch to this cam anyway...
            dir.y = std::min(dir.y, SkyCamHeight);
        }

        setCamTarget(dir);
    }
    else
    {
       // auto pos = m_holeData[m_currentHole].puttFromTee ? glm::vec3(160.f, SkyCamHeight, -100.f) : DefaultSkycamPosition;
        setCamTarget(DefaultSkycamPosition);
    }

    setGreenCamPosition();

    //set the player controlled drone cam to look at the player
    //(although this will drift as the drone moves)
    auto orientation = lookRotation(m_cameras[CameraID::Drone].getComponent<cro::Transform>().getPosition(), player.position);
    m_cameras[CameraID::Drone].getComponent<cro::Transform>().setRotation(orientation);


    if ((cro::GameController::getControllerCount() > 1
        && (m_sharedData.localConnectionData.playerCount > 1 && !isCPU))
        || m_sharedData.connectionData[1].playerCount != 0) //doesn't account if someone leaves mid-game however
    {
        gamepadNotify(GamepadNotify::NewPlayer);
    }
    else
    {
        cro::GameController::setLEDColour(activeControllerID(player.player), m_sharedData.connectionData[player.client].playerData[player.player].ballTint);
    }

    retargetMinimap(false); //must do this after current player position is set...
}

void GolfState::predictBall(float powerPct)
{
    auto club = getClub();
    if (club != ClubID::Putter)
    {
        powerPct = cro::Util::Easing::easeOutSine(powerPct);
    }
    auto pitch = Clubs[club].getAngle();
    auto yaw = m_inputParser.getYaw();
    auto power = Clubs[club].getPower(m_distanceToHole, m_sharedData.imperialMeasurements) * powerPct;

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    auto lie = m_avatars[m_currentPlayer.client][m_currentPlayer.player].ballModel.getComponent<ClientCollider>().lie;

    impulse *= power;
    impulse *= Dampening[m_currentPlayer.terrain] * LieDampening[m_currentPlayer.terrain][lie];
    impulse *= godmode;

    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::BallPrediction, update, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void GolfState::hitBall()
{
    auto club = getClub();
    auto facing = cro::Util::Maths::sgn(m_activeAvatar->model.getComponent<cro::Transform>().getScale().x);
    auto lie = m_avatars[m_currentPlayer.client][m_currentPlayer.player].ballModel.getComponent<ClientCollider>().lie;

    auto [impulse, spin, _] = m_inputParser.getStroke(club, facing, m_distanceToHole);
    impulse *= Dampening[m_currentPlayer.terrain] * LieDampening[m_currentPlayer.terrain][lie];
    impulse *= godmode;

    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;
    update.spin = spin;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, update, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_inputParser.setActive(false, m_currentPlayer.terrain);
    m_restoreInput = false;
    m_achievementTracker.hadBackspin = (spin.y < 0);
    m_achievementTracker.hadTopspin = (spin.y > 0);

    //increase the local stroke count so that the UI is updated
    //the server will set the actual value
    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]++;

    //hide the indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = false;

        //temp ent to delay hiding slightly then pop itself
        static constexpr float FadeTime = 1.5;
        auto tempEnt = m_gameScene.createEntity();
        tempEnt.addComponent<cro::Callback>().active = true;
        tempEnt.getComponent<cro::Callback>().setUserData<float>(FadeTime);
        tempEnt.getComponent<cro::Callback>().function =
            [&, e](cro::Entity f, float dt) mutable
        {
            auto& currTime = f.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::max(0.f, currTime - dt);

            float c = currTime / FadeTime;
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour(c, c, c));

            if (currTime == 0)
            {
                e.getComponent<cro::Model>().setHidden(true);

                f.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(f);
            }
        };
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //reset any warning
    cmd.targetFlags = CommandID::UI::AFKWarn;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            e.getComponent<cro::Callback>().active = false;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    if (m_currentCamera == CameraID::Bystander
        && cro::Util::Random::value(0,1) == 0)
    {
        //activate the zoom
        m_cameras[CameraID::Bystander].getComponent<cro::Callback>().active = true;
    }

    //track achievements for not using more than two putts
    if (club == ClubID::Putter &&
        !m_sharedData.connectionData[m_sharedData.localConnectionData.connectionID].playerData[m_currentPlayer.player].isCPU)
    {
        m_achievementTracker.puttCount++;
        if (m_achievementTracker.puttCount > 2)
        {
            m_achievementTracker.underTwoPutts = false;
        }
    }
}

void GolfState::updateActor(const ActorInfo& update)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [&, update](cro::Entity e, float)
    {
        if (e.isValid())
        {
            auto& interp = e.getComponent<InterpolationComponent<InterpolationType::Linear>>();
            bool active = (interp.id == update.serverID);
            if (active)
            {
                //cro::Transform::QUAT_IDENTITY;
                interp.addPoint({ update.position, glm::vec3(0.f), cro::Util::Net::decompressQuat(update.rotation), update.timestamp});

                //update spectator camera
                cro::Command cmd2;
                cmd2.targetFlags = CommandID::SpectatorCam;
                cmd2.action = [&, e](cro::Entity en, float)
                {
                    en.getComponent<CameraFollower>().target = e;
                    en.getComponent<CameraFollower>().playerPosition = m_currentPlayer.position;
                    en.getComponent<CameraFollower>().holePosition = m_holeData[m_currentHole].pin;
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);

                //set this ball as the flight cam target
                m_flightCam.getComponent<cro::Callback>().setUserData<cro::Entity>(e);

                //if the dest point moves the ball out of a certain radius
                //then set the drone cam to follow (if it's active)
                if (m_currentCamera == CameraID::Sky
                    && m_drone.isValid())
                {
                    auto p = m_cameras[CameraID::Sky].getComponent<cro::Transform>().getPosition();
                    glm::vec2 camPos(p.x, p.z);
                    p = e.getComponent<cro::Transform>().getPosition();
                    glm::vec2 ballPos(p.x, p.z);
                    glm::vec2 destPos(update.position.x, update.position.z);

                    //Later note: I found this half written code^^ and can't remember what
                    //I was planning, so here we go:
                    static constexpr float MaxRadius = 15.f * 15.f;
                    if (glm::length2(camPos - ballPos) < MaxRadius
                        && glm::length2(camPos - destPos) > MaxRadius)
                    {
                        auto& data = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>();
                        auto d = glm::normalize(ballPos - camPos) * 8.f;
                        data.resetPosition += glm::vec3(d.x, 0.f, d.y);
                        data.target.getComponent<cro::Transform>().setPosition(data.resetPosition);
                    }
                }
            }

            e.getComponent<ClientCollider>().active = active;
            e.getComponent<ClientCollider>().state = update.state;
            e.getComponent<ClientCollider>().lie = update.lie;
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    if (update == m_currentPlayer)
    {
        //shows how much effect the wind is currently having
        cmd.targetFlags = CommandID::UI::WindEffect;
        cmd.action = [update](cro::Entity e, float) 
        {
            e.getComponent<cro::Callback>().getUserData<WindEffectData>().first = update.windEffect;
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //set the green cam zoom as appropriate
        bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
            && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit);
        auto ballTarget = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
        float ballDist = glm::length(update.position - ballTarget);

#ifdef PATH_TRACING
        updateBallDebug(update.position);
#endif // CRO_DEBUG_

        m_greenCam.getComponent<cro::Callback>().getUserData<MiniCamData>().targetSize =
            interpolate(MiniCamData::MinSize, MiniCamData::MaxSize, smoothstep(MiniCamData::MinSize, MiniCamData::MaxSize, ballDist + 0.4f)); //pad the dist so ball is always in view

        //this is the active ball so update the UI
        cmd.targetFlags = CommandID::UI::PinDistance;
        cmd.action = [&, ballDist, isMultiTarget](cro::Entity e, float)
        {
            formatDistanceString(ballDist, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements, isMultiTarget);

            auto bounds = cro::Text::getLocalBounds(e);
            bounds.width = std::floor(bounds.width / 2.f);
            e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //set the skip state so we can tell if we're allowed to skip
        m_skipState.state = (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID) ? update.state : -1;
    }
    else
    {
        m_skipState.state = -1;
    }


    if (m_drone.isValid() 
        && !m_roundEnded
        && glm::length2(m_drone.getComponent<cro::Transform>().getPosition() - update.position) < 4.f)
    {
        auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
        msg->position = m_drone.getComponent<cro::Transform>().getPosition();
        msg->type = GolfEvent::DroneHit;
    }
}

void GolfState::createTransition(const ActivePlayer& playerData)
{
    //float targetDistance = glm::length2(playerData.position - m_currentPlayer.position);

    //set the target zoom on the player camera
    float zoom = 1.f;
    if (playerData.terrain == TerrainID::Green)
    {
        const float dist = 1.f - std::min(1.f, glm::length(playerData.position - m_holeData[m_currentHole].pin));
        zoom = m_holeData[m_currentHole].puttFromTee ? (PuttingZoom * (1.f - (0.56f * dist))) : GolfZoom;

        //reduce the zoom within the final metre
        float diff = 1.f - zoom;
        zoom += diff * dist;
    }

    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = zoom;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;

    //hide player avatar
    if (m_activeAvatar)
    {
        //check distance and animate 
        if (/*targetDistance > 0.01
            ||*/ playerData.terrain == TerrainID::Green)
        {
            auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
            scale.y = 0.f;
            scale.z = 0.f;
            m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

            if (m_activeAvatar->hands)
            {
                //we have to free this up alse the model might
                //become attached to two avatars...
                m_activeAvatar->hands->setModel({});
            }
        }
        else
        {
            m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 1;
            m_activeAvatar->model.getComponent<cro::Callback>().active = true;
        }
    }

    
    //hide hud
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float) 
    {
        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f)); //also hides attached icon
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = " ";
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PlayerIcon;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
    if (playerData.terrain == TerrainID::Green)
    {
        targetInfo.targetHeight = CameraPuttHeight;
        //if (!m_holeData[m_currentHole].puttFromTee)
        {
            targetInfo.targetHeight *= CameraTeeMultiplier;
        }
        targetInfo.targetOffset = CameraPuttOffset;
    }
    else
    {
        targetInfo.targetHeight = CameraStrokeHeight;
        targetInfo.targetOffset = CameraStrokeOffset;
    }

    auto targetDir = m_holeData[m_currentHole].target - playerData.position;
    auto pinDir = m_holeData[m_currentHole].pin - playerData.position;
    targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
    
    //always look at the target in mult-target mode and target not yet hit
    if (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[playerData.client].playerData[playerData.player].targetHit)
    {
        targetInfo.targetLookAt = m_holeData[m_currentHole].target;
    }
    else
    {
        //if both the pin and the target are in front of the player
        if (glm::dot(glm::normalize(targetDir), glm::normalize(pinDir)) > 0.4)
        {
            //set the target depending on how close it is
            auto pinDist = glm::length2(pinDir);
            auto targetDist = glm::length2(targetDir);
            if (pinDist < targetDist)
            {
                //always target pin if its closer
                targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
            }
            else
            {
                //target the pin if the target is too close
                //TODO this is really to do with whether or not we're putting
                //when this check happens, but it's unlikely to have
                //a target on the green in other cases.
                const float MinDist = m_holeData[m_currentHole].puttFromTee ? 9.f : 2500.f;
                if (targetDist < MinDist) //remember this in len2
                {
                    targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
                }
                else
                {
                    targetInfo.targetLookAt = m_holeData[m_currentHole].target;
                }
            }
        }
        else
        {
            //else set the pin as the target
            targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
        }
    }

    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point we update the active player and
    //the ent destroys itself
    auto startPos = m_currentPlayer.position;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(startPos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<ActivePlayer>(playerData);
    entity.getComponent<cro::Callback>().function =
        [&, startPos](cro::Entity e, float dt)
    {
        const auto& playerData = e.getComponent<cro::Callback>().getUserData<ActivePlayer>();

        auto currPos = e.getComponent<cro::Transform>().getPosition();
        auto travel = playerData.position - currPos;
        auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();

        auto targetDir = targetInfo.currentLookAt - currPos;
        m_camRotation = std::atan2(-targetDir.z, targetDir.x);

        float minTravel = playerData.terrain == TerrainID::Green ? 0.000001f : 0.005f;
        if (glm::length2(travel) < minTravel)
        {
            //we're there
            targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
            targetInfo.startHeight = targetInfo.targetHeight;
            targetInfo.startOffset = targetInfo.targetOffset;

            //hmm the final result is not always the same as the flyby - so snapping this here
            //can cause a jump in view

            //setCameraPosition(playerData.position, targetInfo.targetHeight, targetInfo.targetOffset);
            requestNextPlayer(playerData);

            m_gameScene.getActiveListener().getComponent<cro::AudioListener>().setVelocity(glm::vec3(0.f));

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            const auto totalDist = glm::length(playerData.position - startPos);
            const auto currentDist = glm::length(travel);
            const auto percent = 1.f - (currentDist / totalDist);

            targetInfo.currentLookAt = targetInfo.prevLookAt + ((targetInfo.targetLookAt - targetInfo.prevLookAt) * percent);

            auto height = targetInfo.targetHeight - targetInfo.startHeight;
            auto offset = targetInfo.targetOffset - targetInfo.startOffset;

            static constexpr float Speed = 4.f;
            e.getComponent<cro::Transform>().move(travel * Speed * dt);
            setCameraPosition(e.getComponent<cro::Transform>().getPosition(), 
                targetInfo.startHeight + (height * percent), 
                targetInfo.startOffset + (offset * percent));

            m_gameScene.getActiveListener().getComponent<cro::AudioListener>().setVelocity(travel * Speed);
        }
    };
}

void GolfState::startFlyBy()
{
    m_idleTimer.restart();
    m_idleTime = cro::seconds(90.f);

    //reset the zoom if not putting from tee
    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = m_holeData[m_currentHole].puttFromTee ? PuttingZoom : 1.f;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;
    m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);


    //static for lambda capture
    static constexpr float MoveSpeed = 50.f; //metres per sec
    static constexpr float MaxHoleDistance = 275.f; //this scales the move speed based on the tee-pin distance
    float SpeedMultiplier = (0.25f + ((m_holeData[m_currentHole].distanceToPin / MaxHoleDistance) * 0.75f));
    float heightMultiplier = 1.f;

    //only slow down if current and previous were putters - in cases of custom courses
    bool previousPutt = (m_currentHole > 0) ? m_holeData[m_currentHole - 1].puttFromTee : m_holeData[m_currentHole].puttFromTee;
    if (m_holeData[m_currentHole].puttFromTee)
    {
        if (previousPutt)
        {
            SpeedMultiplier /= 3.f;
        }
        heightMultiplier = 0.35f;
    }

    struct FlyByTarget final
    {
        std::function<float(float)> ease = std::bind(&cro::Util::Easing::easeInOutQuad, std::placeholders::_1);
        std::int32_t currentTarget = 0;
        float progress = 0.f;
        std::array<glm::mat4, 4u> targets = {};
        std::array<float, 4u> speeds = {};
    }targetData;

    targetData.targets[0] = m_cameras[CameraID::Player].getComponent<cro::Transform>().getLocalTransform();


    static constexpr glm::vec3 BaseOffset(10.f, 5.f, 0.f);
    const auto& holeData = m_holeData[m_currentHole];

    //calc offset based on direction of initial target to tee
    glm::vec3 dir = holeData.tee - holeData.pin;
    float rotation = std::atan2(-dir.z, dir.x);
    glm::quat q = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rotation, cro::Transform::Y_AXIS);
    glm::vec3 offset = q * BaseOffset;
    offset.y *= heightMultiplier;

    //set initial transform to look at pin from offset position
    auto transform = glm::inverse(glm::lookAt(offset + holeData.pin, holeData.pin, cro::Transform::Y_AXIS));
    targetData.targets[1] = transform;

    float moveDist = glm::length(glm::vec3(targetData.targets[0][3]) - glm::vec3(targetData.targets[1][3]));
    targetData.speeds[0] = moveDist / MoveSpeed;
    targetData.speeds[0] *= 1.f / SpeedMultiplier;

    //translate the transform to look at target point or half way point if not set
    constexpr float MinTargetMoveDistance = 100.f;
    auto diff = holeData.target - holeData.pin;
    if (glm::length2(diff) < MinTargetMoveDistance)
    {
        diff = (holeData.tee - holeData.pin) / 2.f;
    }
    diff.y = 10.f * SpeedMultiplier;


    transform[3] += glm::vec4(diff, 0.f);
    targetData.targets[2] = transform;

    moveDist = glm::length(glm::vec3(targetData.targets[1][3]) - glm::vec3(targetData.targets[2][3]));
    auto moveSpeed = MoveSpeed;
    if (!m_holeData[m_currentHole].puttFromTee)
    {
        moveSpeed *= std::min(1.f, (moveDist / (MinTargetMoveDistance / 2.f)));
    }
    targetData.speeds[1] = moveDist / moveSpeed;
    targetData.speeds[1] *= 1.f / SpeedMultiplier;

    //the final transform is set to what should be the same as the initial player view
    //this is actually more complicated than it seems, so the callback interrogates the
    //player camera when it needs to.

    //set to initial position
    m_cameras[CameraID::Transition].getComponent<cro::Transform>().setLocalTransform(targetData.targets[0]);


    //interp the transform targets
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FlyByTarget>(targetData);
    entity.getComponent<cro::Callback>().function =
        [&, SpeedMultiplier](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<FlyByTarget>();
        data.progress = /*std::min*/(data.progress + (dt / data.speeds[data.currentTarget])/*, 1.f*/);

        auto& camTx = m_cameras[CameraID::Transition].getComponent<cro::Transform>();

        //find out 'lookat' point as it would appear on the water plane and set the water there
        glm::vec3 intersection(0.f);
        if (planeIntersect(camTx.getLocalTransform(), intersection))
        {
            intersection.y = WaterLevel;
            m_cameras[CameraID::Transition].getComponent<TargetInfo>().waterPlane = m_waterEnt; //TODO this doesn't actually update the parent if already attached somewhere else...
            m_cameras[CameraID::Transition].getComponent<TargetInfo>().waterPlane.getComponent<cro::Transform>().setPosition(intersection);
        }

        if (data.progress >= 1)
        {
            data.progress -= 1.f;
            data.currentTarget++;
            //camTx.setLocalTransform(data.targets[data.currentTarget]);

            switch (data.currentTarget)
            {
            default: break;
            case 2:
                //hope the player cam finished...
                //which it hasn't on smaller holes, and annoying.
                //not game-breaking. but annoying.
            {
                data.targets[3] = m_cameras[CameraID::Player].getComponent<cro::Transform>().getLocalTransform();
                //data.ease = std::bind(&cro::Util::Easing::easeInSine, std::placeholders::_1);
                float moveDist = glm::length(glm::vec3(data.targets[2][3]) - glm::vec3(data.targets[3][3]));
                data.speeds[2] = moveDist / MoveSpeed;
                data.speeds[2] *= 1.f / SpeedMultiplier;

                //play the transition music
                if (m_sharedData.tutorial)
                {
                    m_cameras[CameraID::Player].getComponent<cro::AudioEmitter>().play();
                }
                //else we'll play it when the score board shows, below
            }
                break;
            case 3:
                //we're done here
                camTx.setLocalTransform(data.targets[data.currentTarget]);

                m_gameScene.getSystem<CameraFollowSystem>()->resetCamera();
                setActiveCamera(CameraID::Player);
            {
                if (m_sharedData.tutorial)
                {
                    auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
                    msg->type = SceneEvent::TransitionComplete;
                }
                else
                {
                    showScoreboard(true);
                    m_newHole = true;
                    m_cameras[CameraID::Player].getComponent<cro::AudioEmitter>().play();

                    //delayed ent just to show the score board for a while
                    auto de = m_gameScene.createEntity();
                    de.addComponent<cro::Callback>().active = true;
                    de.getComponent<cro::Callback>().setUserData<float>(0.2f);
                    de.getComponent<cro::Callback>().function =
                        [&](cro::Entity ent, float dt)
                    {
                        auto& currTime = ent.getComponent<cro::Callback>().getUserData<float>();
                        currTime -= dt;
                        if (currTime < 0)
                        {
                            auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
                            msg->type = SceneEvent::TransitionComplete;

                            ent.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(ent);
                        }
                    };
                }
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
                break;
            }
        }

        if (data.currentTarget < 3)
        {
            auto rot = glm::slerp(glm::quat_cast(data.targets[data.currentTarget]), glm::quat_cast(data.targets[data.currentTarget + 1]), data.progress);
            camTx.setRotation(rot);

            auto pos = interpolate(glm::vec3(data.targets[data.currentTarget][3]), glm::vec3(data.targets[data.currentTarget + 1][3]), data.ease(data.progress));
            camTx.setPosition(pos);
        }
    };


    setActiveCamera(CameraID::Transition);


    //hide the minimap ball
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide the mini flag
    cmd.targetFlags = CommandID::UI::MiniFlag;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide hud
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show wait message
    cmd.targetFlags = CommandID::UI::WaitMessage;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide player
    if (m_activeAvatar)
    {
        auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
        scale.y = 0.f;
        scale.z = 0.f;
        m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
        m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

        if (m_activeAvatar->hands)
        {
            //we have to free this up alse the model might
            //become attached to two avatars...
            m_activeAvatar->hands->setModel({});
        }
    }

    //hide stroke indicator
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Model>().setHidden(true);
        e.getComponent<cro::Callback>().active = false;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide ball models so they aren't seen floating mid-air
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
    msg->type = SceneEvent::TransitionStart;
}

std::int32_t GolfState::getClub() const
{
    switch (m_currentPlayer.terrain)
    {
    default: 
        return m_inputParser.getClub();
    case TerrainID::Green: 
        return ClubID::Putter;
    }
}

void GolfState::gamepadNotify(std::int32_t type)
{
    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
    {
        struct CallbackData final
        {
            float flashRate = 1.f;
            float currentTime = 0.f;
            std::int32_t state = 0; //on or off
            
            std::size_t colourIndex = 0;
            std::vector<cro::Colour> colours;
        };

        auto controllerID = activeControllerID(m_currentPlayer.player);
        auto colour = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].ballTint;

        auto ent = m_gameScene.createEntity();
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().function =
            [&, controllerID, colour](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<CallbackData>();
            if (data.colourIndex < data.colours.size())
            {
                data.currentTime += dt;
                if (data.currentTime > data.flashRate)
                {
                    data.currentTime -= data.flashRate;

                    if (data.state == 0)
                    {
                        data.state = 1;

                        //switch on effect
                        cro::GameController::rumbleStart(controllerID, 0, 60000 * m_sharedData.enableRumble, static_cast<std::uint32_t>(data.flashRate / 1000.f));
                        cro::GameController::setLEDColour(controllerID, data.colours[data.colourIndex++]);
                    }
                    else
                    {
                        data.state = 0;

                        //switch off effect
                        cro::GameController::rumbleStop(controllerID);
                        cro::GameController::setLEDColour(controllerID, data.colours[data.colourIndex++]);
                    }
                }
            }
            else
            {
                cro::GameController::setLEDColour(controllerID, colour);

                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
        };

        CallbackData data;

        switch (type)
        {
        default: break;
        case GamepadNotify::NewPlayer:
        {
            data.flashRate = 0.15f;
            data.colours = { colour, cro::Colour::Black, colour, cro::Colour::Black };
        }
            break;
        case GamepadNotify::Hole:

            break;
        case GamepadNotify::HoleInOne:
        {
            data.flashRate = 0.1f;
            data.colours = 
            {
                colour,
                cro::Colour::Red,
                colour, 
                cro::Colour::Green,
                colour,
                cro::Colour::Blue,
                colour,
                cro::Colour::Cyan,
                colour,
                cro::Colour::Magenta,
                colour,
                cro::Colour::Yellow,
                colour,
                cro::Colour::Red,
                colour,
                cro::Colour::Green,
                colour,
                cro::Colour::Blue,
                colour,
                cro::Colour::Cyan,
                colour,
                cro::Colour::Magenta,
                colour,
                cro::Colour::Yellow
            };
        }
            break;
        }
        ent.getComponent<cro::Callback>().setUserData<CallbackData>(data);
    }
}