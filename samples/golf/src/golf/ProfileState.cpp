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

#include "ProfileState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"

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
#include <crogine/ecs/components/Model.hpp>
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
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Main, Confirm
        };
    };

    constexpr glm::uvec2 BallTexSize(110u, 110u);
    constexpr glm::uvec2 AvatarTexSize(130u, 202u);
}

ProfileState::ProfileState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_modelScene        (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_viewScale         (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    addSystems();
    loadResources();
    buildScene();
}

//public
bool ProfileState::handleEvent(const cro::Event& evt)
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
        if (evt.cbutton.button == cro::GameController::ButtonB
            || evt.cbutton.button == cro::GameController::ButtonStart)
        {
            quitState();
            return false;
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

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_uiScene.forwardEvent(evt);
    m_modelScene.forwardEvent(evt);
    return false;
}

void ProfileState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
    m_modelScene.forwardMessage(msg);
}

bool ProfileState::simulate(float dt)
{
    m_modelScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ProfileState::render()
{
    m_ballTexture.clear(cro::Colour::Plum);
    m_modelScene.render();
    m_ballTexture.display();

    m_avatarTexture.clear(cro::Colour::CornflowerBlue);
    m_modelScene.render();
    m_avatarTexture.display();

    m_uiScene.render();
}

//private
void ProfileState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);

    m_modelScene.addSystem<cro::CameraSystem>(mb);
    m_modelScene.addSystem<cro::ModelRenderer>(mb);
}

void ProfileState::loadResources()
{
    //button audio
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

    //preview textures
    m_ballTexture.create(BallTexSize.x, BallTexSize.y);
    m_avatarTexture.create(AvatarTexSize.x, AvatarTexSize.y);


    
    m_sharedData.ballModels;

    m_sharedData.avatarInfo;

    m_sharedData.hairInfo;
}

void ProfileState::buildScene()
{
    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_uiScene.createEntity();
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
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_uiScene.createEntity();
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
    spriteSheet.loadFromFile("assets/golf/sprites/avatar_edit.spt", m_sharedData.sharedResources->textures);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();
    auto selected = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White); 
            e.getComponent<cro::Callback>().active = true;
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        });
    auto unselected = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });

    const auto createButton = [&](const std::string& spriteID, glm::vec2 position)
    {
        auto bounds = spriteSheet.getSprite(spriteID).getTextureBounds();

        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.1f));
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f ) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(spriteID);
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

#ifdef USE_GNS
    //TODO add workshop button
#endif

    //colour buttons
    auto hairColour = createButton("colour_highlight", glm::vec2(33.f, 167.f));
    hairColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto skinColour = createButton("colour_highlight", glm::vec2(33.f, 135.f));
    skinColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto topLightColour = createButton("colour_highlight", glm::vec2(17.f, 103.f));
    topLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto topDarkColour = createButton("colour_highlight", glm::vec2(49.f, 103.f));
    topDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto bottomLightColour = createButton("colour_highlight", glm::vec2(17.f, 69.f));
    bottomLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto bottomDarkColour = createButton("colour_highlight", glm::vec2(49.f, 69.f));
    bottomDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });

    //avatar arrow buttons
    auto hairLeft = createButton("arrow_left", glm::vec2(87.f, 156.f));
    hairLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto hairRight = createButton("arrow_right", glm::vec2(234.f, 156.f));
    hairRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto avatarLeft = createButton("arrow_left", glm::vec2(87.f, 110.f));
    avatarLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto avatarRight = createButton("arrow_right", glm::vec2(234.f, 110.f));
    avatarRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });

    //checkbox
    auto southPaw = createButton("check_highlight", glm::vec2(17.f, 42.f));
    southPaw.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });

    //name button
    auto nameButton = createButton("name_highlight", glm::vec2(264.f, 213.f));
    nameButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });

    //ball arrow buttons
    auto ballHairLeft = createButton("arrow_left", glm::vec2(311.f, 156.f));
    ballHairLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto ballHairRight = createButton("arrow_right", glm::vec2(440.f, 156.f));
    ballHairRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto ballLeft = createButton("arrow_left", glm::vec2(311.f, 110.f));
    ballLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto ballRight = createButton("arrow_right", glm::vec2(440.f, 110.f));
    ballRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });

    //save/quit buttons
    auto saveQuit = createButton("button_highlight", glm::vec2(269.f, 48.f));
    saveQuit.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    auto quit = createButton("button_highlight", glm::vec2(269.f, 24.f));
    quit.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });

    //TODO check for steamdeck and add mugshot button
    //TODO will this also break big picture mode?
    if (!m_sharedData.playerProfiles[m_sharedData.activeProfileIndex].mugshot.empty())
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 396.f, 24.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>(); //TODO get texture from shared resources
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    auto addCorners = [&](cro::Entity p, cro::Entity q)
    {
        auto bounds = q.getComponent<cro::Sprite>().getTextureBounds();
        auto offset = q.getComponent<cro::Transform>().getPosition();

        auto cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_bl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        auto cornerBounds = cornerEnt.getComponent<cro::Sprite>().getTextureBounds();

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tr");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_br");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());
    };


    //avatar preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 98.f, 27.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_avatarTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    addCorners(bgEnt, entity);

    //ball preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 323.f, 83.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_ballTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    addCorners(bgEnt, entity);


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
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void ProfileState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}