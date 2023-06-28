/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "LeaderboardState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"

#include <HallOfFame.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Dummy, Leaderboard,
            MonthSelect
        };
    };

    const std::array<std::string, 13u> PageNames =
    {
        "All Time",
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
    };

    const std::array<std::string, 3u> HoleNames =
    {
        "All Holes", "Front 9", "Back 9"
    };
}

LeaderboardState::LeaderboardState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_viewScale         (2.f)
{
    Social::updateHallOfFame();
    parseCourseDirectory();
    buildScene();

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Stats"))
    //        {
    //            ImGui::Text("Hole count %d", m_displayContext.holeCount);
    //        }
    //        ImGui::End();
    //    });
}

//public
bool LeaderboardState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonStart:
            quitState();
            return false;
        case cro::GameController::ButtonLeftShoulder:
            if (m_displayContext.boardIndex == BoardIndex::Course)
            {
                m_displayContext.courseIndex = (m_displayContext.courseIndex + (m_courseStrings.size() - 1)) % m_courseStrings.size();
                Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                refreshDisplay();
            }
            break;
        case cro::GameController::ButtonRightShoulder:
            if (m_displayContext.boardIndex == BoardIndex::Course)
            {
                m_displayContext.courseIndex = (m_displayContext.courseIndex + 1) % m_courseStrings.size();
                Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                refreshDisplay();
            }
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void LeaderboardState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::HOFReceived)
        {
            refreshDisplay();
        }
    }

    m_scene.forwardMessage(msg);
}

bool LeaderboardState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void LeaderboardState::render()
{
    m_scene.render();
}

//private
void LeaderboardState::parseCourseDirectory()
{
    m_resources.textures.setFallbackColour(cro::Colour::Transparent);

    const std::string coursePath = cro::FileSystem::getResourcePath() + "assets/golf/courses/";
    auto dirs = cro::FileSystem::listDirectories(coursePath);
    
    std::sort(dirs.begin(), dirs.end());

    for (const auto& dir : dirs)
    {
        if (dir.find("course_") != std::string::npos)
        {
            auto filePath = coursePath + dir + "/course.data";
            if (cro::FileSystem::fileExists(filePath))
            {
                cro::ConfigFile cfg;
                cfg.loadFromFile(filePath);
                if (auto* prop = cfg.findProperty("title"); prop != nullptr)
                {
                    const auto courseTitle = prop->getValue<std::string>();
                    m_courseStrings.emplace_back(std::make_pair(dir, cro::String::fromUtf8(courseTitle.begin(), courseTitle.end())));


                    filePath = coursePath + dir + "/preview.png";
                    //if this fails we still need the fallback to pad the vector
                    m_courseThumbs.push_back(&m_resources.textures.get(filePath));
                }
            }
        }
    }
}

void LeaderboardState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);// ->setActiveControllerID(m_sharedData.inputBinding.controllerID);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");


    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<RootCallbackData>();

        switch (state)
        {
        default: break;
        case RootCallbackData::FadeIn:
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Leaderboard);
                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();


                //apply the selected course data from shared data
                //so we get the current leaderboards eg from the lobby
                m_displayContext.courseIndex = m_sharedData.courseIndex;
                m_displayContext.holeCount = m_sharedData.holeCount;
                m_displayContext.page = 0;
                m_displayContext.monthText.getComponent<cro::Text>().setString(PageNames[0]);
                centreText(m_displayContext.monthText);
                Social::refreshHallOfFame(m_courseStrings[m_sharedData.courseIndex].first);
                refreshDisplay();
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float)
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setScale(size);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

   
    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/leaderboard_browser.spt", m_resources.textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, (bounds.height / 2.f) - 10.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());
    const float bgCentre = bounds.width / 2.f;
    auto bgNode = entity;


    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //course name
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgCentre, bounds.height - 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Westington Links, Isle Of Pendale");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.courseTitle = entity;

    //personal best
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 182.f, 76.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("No Personal Score");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    centreText(entity);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.personalBest = entity;

    //leaderboard values text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 22.f, 216.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setString("Waiting for data...");
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.leaderboardText = entity;


    //course thumbnail
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 356.f, 116.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.2125f)); //yeah IDK
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*m_courseThumbs[0]);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.thumbnail = entity;

    createFlyout(bgNode);

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White); 
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e) 
        { 
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    
    
    //button to set hole count index / refresh
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 424.f, 107.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("All Holes");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    auto textEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 423.f, 94.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flyout_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    bounds.bottom += 4.f;
    bounds.height -= 8.f;
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.holeCount = (m_displayContext.holeCount + 1) % 3;
                    textEnt.getComponent<cro::Text>().setString(HoleNames[m_displayContext.holeCount]);
                    centreText(textEnt);
                    refreshDisplay();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //button to set month index / refresh
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 424.f, 87.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(PageNames[0]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    m_displayContext.monthText = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 423.f, 74.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flyout_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    bounds.bottom += 4.f;
    bounds.height -= 8.f;
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_flyout.background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    //set column/row count for menu
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::MonthSelect);
                    m_scene.getSystem<cro::UISystem>()->setColumnCount(3);
                    m_scene.getSystem<cro::UISystem>()->selectAt(std::max(12, m_displayContext.page - 1));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //button to enable nearest scores / refresh
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 365.f ,65.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_centre");
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto innerEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 363.f ,63.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_highlight");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, innerEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.showNearest = !m_displayContext.showNearest;
                    float scale = m_displayContext.showNearest ? 1.f : 0.f;
                    innerEnt.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                    refreshDisplay();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //prev button - decrement course index and selectCourse()
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgCentre - 42.f, 10.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_displayContext.courseIndex = (m_displayContext.courseIndex + (m_courseStrings.size() - 1)) % m_courseStrings.size();
                    Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                    refreshDisplay();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //close button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgCentre, 10.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            { 
                if (activated(evt))
                {
                    quitState();
                }
            });

    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //next button - increment course index and select course
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgCentre + 41.f, 10.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_displayContext.courseIndex = (m_displayContext.courseIndex + 1) % m_courseStrings.size();
                    Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                    refreshDisplay();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //dummy ent for dummy menu
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);


    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f);
}

void LeaderboardState::createFlyout(cro::Entity parent)
{
    static constexpr glm::vec2 TextPadding(2.f, 10.f);
    static constexpr glm::vec2 IconSize(68.f, 12.f);
    static constexpr float IconPadding = 4.f;

    static constexpr std::int32_t ColumnCount = 3;
    static constexpr std::int32_t RowCount = 4;

    static constexpr float BgWidth = (ColumnCount * (IconSize.x + IconPadding)) + IconPadding;
    static constexpr float BgHeight = ((RowCount + 1.f) * IconSize.y) + (RowCount * IconPadding) + IconPadding;

    //background
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(138.f, 88.f, 0.2f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    auto verts = createMenuBackground({ BgWidth, BgHeight });
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);

    m_flyout.background = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconPadding, IconPadding, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    m_flyout.highlight = entity;
    m_flyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //buttons/menu items
    m_flyout.activateCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            auto quitMenu = [&]()
            {
                m_flyout.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Leaderboard);
                m_scene.getSystem<cro::UISystem>()->setColumnCount(1);
            };

            if (activated(evt))
            {
                m_displayContext.page = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                m_displayContext.monthText.getComponent<cro::Text>().setString(PageNames[m_displayContext.page]);
                centreText(m_displayContext.monthText);
                refreshDisplay();

                quitMenu();
            }
            else if (deactivated(evt))
            {
                quitMenu();
            }
        });
    m_flyout.selectCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            m_flyout.highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() - glm::vec3(TextPadding, 0.f));
            e.getComponent<cro::AudioEmitter>().play();
        });

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    for (auto j = 0u; j < 12u; ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = j % ColumnCount;
        std::size_t y = (RowCount - 1) - (j / ColumnCount);

        glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
        pos += TextPadding;
        pos += IconPadding;

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(PageNames[(j + 1)]);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

        entity.addComponent<cro::UIInput>().setGroup(MenuID::MonthSelect);
        entity.getComponent<cro::UIInput>().area = { -TextPadding, IconSize };
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_flyout.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_flyout.activateCallback;
        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(j + 1);

        m_flyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //add 'all' option
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(78.f, 76.f, 0.1f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(PageNames[0]);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

    entity.addComponent<cro::UIInput>().setGroup(MenuID::MonthSelect);
    entity.getComponent<cro::UIInput>().area = { -TextPadding, IconSize };
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_flyout.selectCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_flyout.activateCallback;
    entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(0);

    m_flyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void LeaderboardState::refreshDisplay()
{
    switch (m_displayContext.boardIndex)
    {
    default:
    case BoardIndex::Course:
    {
        const auto& title = m_courseStrings[m_displayContext.courseIndex].second;

        m_displayContext.courseTitle.getComponent<cro::Text>().setString(title);
        m_displayContext.thumbnail.getComponent<cro::Sprite>().setTexture(*m_courseThumbs[m_displayContext.courseIndex]);

        const auto& entry = Social::getHallOfFame(m_courseStrings[m_displayContext.courseIndex].first, m_displayContext.page, m_displayContext.holeCount);
        
        if (entry.hasData)
        {
            if (m_displayContext.showNearest)
            {
                if (entry.nearestTen.empty())
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString("No Attempt");
                }
                else
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString(entry.nearestTen);
                }
            }
            else
            {
                if (entry.topTen.empty())
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString("No Scores");
                }
                else
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString(entry.topTen);
                }
            }

            if (!entry.personalBest.empty())
            {
                m_displayContext.personalBest.getComponent<cro::Text>().setString(entry.personalBest);
            }
            else
            {
                m_displayContext.personalBest.getComponent<cro::Text>().setString("No Personal Best");
            }
            centreText(m_displayContext.personalBest);
        }
        else
        {
            m_displayContext.personalBest.getComponent<cro::Text>().setString(" ");
            m_displayContext.leaderboardText.getComponent<cro::Text>().setString("Waiting...");
        }
    }
        break;
    case BoardIndex::Hio:
        m_displayContext.courseTitle.getComponent<cro::Text>().setString("Most Holes In One");
        break;
    case BoardIndex::Rank:
        m_displayContext.courseTitle.getComponent<cro::Text>().setString("Highest Ranked Players");
        break;
    case BoardIndex::Streak:
        m_displayContext.courseTitle.getComponent<cro::Text>().setString("Longest Daily Streak");
        break;

    }
    centreText(m_displayContext.courseTitle);
};

void LeaderboardState::quitState()
{
    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}