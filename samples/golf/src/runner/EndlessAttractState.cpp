/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "../golf/SharedStateData.hpp"
#include "../golf/MenuConsts.hpp"

#include "EndlessAttractState.hpp"
#include "EndlessConsts.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>

EndlessAttractState::EndlessAttractState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_uiScene       (context.appInstance.getMessageBus())
{
    addSystems();
    loadAssets();
    createUI();
}

//public
bool EndlessAttractState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    const auto startGame = [&]()
        {
            requestStackClear();
            requestStackPush(StateID::EndlessRunner);
        };
    const auto quitGame = [&]()
        {
            requestStackClear();
            requestStackPush(StateID::Clubhouse);
        };

    const auto updateTextPrompt = [&](bool controller)
        {
            static bool prevController = false;
            if (controller != prevController)
            {
                prevController = controller;

                //TODO actual update
            }

            cro::App::getWindow().setMouseCaptured(true);
        };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
#ifndef CRO_DEBUG_
            quitGame();
#endif
            break;
        }

        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            startGame();
        }

        updateTextPrompt(false);
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftShoulder:
            //TODO goto prev page
            break;
        case cro::GameController::ButtonRightShoulder:
            //TODO goto next page
            break;
        case cro::GameController::ButtonA:
            startGame();
            break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonBack:
            quitGame();
            break;
        }

        updateTextPrompt(true);
    }

    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        updateTextPrompt(true);
    }

    m_uiScene.forwardEvent(evt);
    return true;
}

void EndlessAttractState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool EndlessAttractState::simulate(float dt)
{
    m_uiScene.simulate(dt);
    return true;
}

void EndlessAttractState::render()
{
    m_uiScene.render();
}

//private
void EndlessAttractState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void EndlessAttractState::loadAssets()
{
}

void EndlessAttractState::createUI()
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    m_rootNode = entity;

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //text prompt to start
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press Space to Start");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -16.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = TextFlashCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_startTextPrompt = entity;


    //text prompt to quit
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press Escape to Quit");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 32.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = TextFlashCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_quitTextPrompt = entity;


    //TODO create a proper set of nodes to cycle through title, leaderboards, rules, and Game Over
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("GAME\nOVER");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 10);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 80.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //scaled independently of root node (see callback, below)
    cro::Colour bgColour(0.f, 0.f, 0.f, BackgroundAlpha);
    auto bgEnt = m_uiScene.createEntity();
    bgEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -9.f });
    bgEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 1.f), bgColour),
            cro::Vertex2D(glm::vec2(0.f), bgColour),
            cro::Vertex2D(glm::vec2(1.f), bgColour),
            cro::Vertex2D(glm::vec2(1.f, 0.f), bgColour),
        }
    );

    auto resize = [&, bgEnt](cro::Camera& cam) mutable
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);

        bgEnt.getComponent<cro::Transform>().setScale(size);

        const float scale = getViewScale(size);
        m_rootNode.getComponent<cro::Transform>().setScale(glm::vec2(scale));

        size /= scale;

        cro::Command cmd;
        cmd.targetFlags = CommandID::UIElement;
        cmd.action =
            [size](cro::Entity e, float)
            {
                const auto& element = e.getComponent<UIElement>();
                auto pos = size * element.relativePosition;
                pos.x = std::round(pos.x);
                pos.y = std::round(pos.y);

                pos += element.absolutePosition;
                e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}