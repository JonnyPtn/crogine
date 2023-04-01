/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "KeyboardState.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/core/GameController.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct KeyboardCallbackData final
    {
        enum
        {
            In, Hold, Out
        }state = In;

    };

    struct SendCodepoint final
    {
        void operator()()
        {
            if (SDL_IsTextInputActive())
            {
                SDL_Event evt;
                evt.text.windowID = 0;
                evt.text.timestamp = 0;
                evt.type = SDL_TEXTINPUT;
                evt.text.text[0] = bytes[0];
                evt.text.text[1] = bytes[1];
                evt.text.text[2] = bytes[2];
                evt.text.text[3] = 0;

                SDL_PushEvent(&evt);
            }
        };

        SendCodepoint(std::int8_t first, std::int8_t second, std::int8_t third = 0)
            : bytes({ first, second, third }) {}
        const std::array<std::int8_t, 3u> bytes = {};
    };

    struct ButtonAnimationCallback final
    {
        void operator() (cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            float scale = cro::Util::Easing::easeInBounce(currTime) * 0.4f;
            scale += 1.f;
            e.getComponent<cro::Transform>().setScale({ scale, scale });

            currTime = std::max(0.f, currTime - (dt * 2.f));
            if (currTime == 0)
            {
                currTime = 1.f;
                e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                e.getComponent<cro::Callback>().active = false;
            }
        }
    };

    static constexpr glm::vec2 GridOffset(19.f, 108.f);
    static constexpr glm::vec2 GridSpacing(77.f, 88.f);

    static constexpr std::int32_t GridX = 10;
    static constexpr std::int32_t GridY = 3;
    static constexpr std::int32_t GridSize = GridX * GridY;
}

KeyboardState::KeyboardState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_sharedData    (sd),
    m_activeLayout  (KeyboardLayout::Lower),
    m_selectedIndex (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
    initCallbacks();

#ifdef CRO_DEBUG_
    for (auto& i : m_keyboardLayouts)
    {
        for (auto& j : i.callbacks)
        {
            if (!j)
            {
                j = [] {};
            }
        }
    }
#endif
}

//public
bool KeyboardState::handleEvent(const cro::Event& evt)
{
    //only handle input if not transitioning
    if (!m_keyboardEntity.getComponent<cro::Callback>().active)
    {
        switch (evt.type)
        {
        default: break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::DPadDown:
                down();
                break;
            case cro::GameController::DPadLeft:
                left();
                break;
            case cro::GameController::DPadRight:
                right();
                break;
            case cro::GameController::DPadUp:
                up();
                break;
            case cro::GameController::ButtonA:
                activate();
                break;
            case cro::GameController::ButtonB:
                sendBackspace();
                break;
            case cro::GameController::ButtonX:
                sendSpace();
                break;
            case cro::GameController::ButtonY:
                nextLayout();
                break;
            case cro::GameController::ButtonStart:
            case cro::GameController::ButtonBack:
                quitState();
                break;
            }
            return false;
        case SDL_CONTROLLERBUTTONUP:
            
            return false;
        case SDL_CONTROLLERAXISMOTION:
            
            return false;
#ifdef CRO_DEBUG_
        case SDL_KEYDOWN:
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_LEFT:
                left();
                break;
            case SDLK_RIGHT:
                right();
                break;
            case SDLK_UP:
                up();
                break;
            case SDLK_DOWN:
                down();
                break;
            case SDLK_RETURN:
                activate();
                return false;
            case SDLK_SPACE:
                sendSpace();
                return false;
            /*case SDLK_BACKSPACE:
                sendBackspace();
                return false;*/
            case SDLK_TAB:
                nextLayout();
                return false;
            }
            break;
        case SDL_KEYUP:
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_BACKSPACE:
                sendBackspace();
                [[fallthrough]];
            case SDLK_RETURN: 
            case SDLK_SPACE:
            case SDLK_TAB:
                return false;
            }
            break;
#endif
        }
    }

    if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        quitState();
    }

    m_scene.forwardEvent(evt);
    return true;
}

void KeyboardState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool KeyboardState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void KeyboardState::render()
{
    m_scene.render();
}

//private
void KeyboardState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/osk.spt", m_sharedData.sharedResources->textures);

    cro::Entity entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lower");
    auto bounds = spriteSheet.getSprite("lower").getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<KeyboardCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const auto& data = e.getComponent<cro::Callback>().getUserData<KeyboardCallbackData>();
        auto pos = e.getComponent<cro::Transform>().getOrigin();
        float diff = 0.f;
        
        switch (data.state)
        {
        default: break;
        case KeyboardCallbackData::In:
        {
            diff = 0.f - pos.y;

            if (diff > -0.5f)
            {
                e.getComponent<cro::Callback>().active = false;
                diff = 0.f;
                pos.y = 0.f;
                e.getComponent<cro::Transform>().setOrigin(pos);
                m_highlightEntity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            }
        }
            break;
        case KeyboardCallbackData::Out:
        {
            auto height = e.getComponent<cro::Sprite>().getTextureBounds().height;
            diff = height - pos.y;

            if (diff < 0.5f)
            {
                e.getComponent<cro::Callback>().active = false;
                requestStackPop();
            }
        }
            break;
        }

        pos.y += diff * (dt * 8.f);
        e.getComponent<cro::Transform>().setOrigin(pos);
    };

    auto resize = [entity](cro::Camera& cam) mutable
    {
        //ensure keyboard is always less than half the height of the window
        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        auto keyboardSize = entity.getComponent<cro::Sprite>().getTextureBounds();

        float scale = 3.f - std::ceil((winSize.y / 2.f) / keyboardSize.height);
        winSize *= scale;
        cam.setOrthographic(0.f, winSize.x, 0.f, winSize.y, -0.2f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        entity.getComponent<cro::Transform>().setPosition({ winSize.x / 2.f, 0.f });
    };
    m_keyboardEntity = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = resize;
    resize(entity.getComponent<cro::Camera>());
    m_scene.setActiveCamera(entity);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(GridOffset, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().setUserData<float>(0.1f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            auto rect = e.getComponent<cro::Sprite>().getTextureRect();
            rect.left = 0.f;
            e.getComponent<cro::Sprite>().setTextureRect(rect);

            currTime = 0.1f;
            e.getComponent<cro::Callback>().active = false;
        }
    };
    m_keyboardEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_highlightEntity = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 78.f, 27.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour(1.f, 0.835f, 0.f));
    entity.addComponent<cro::Callback>().function = ButtonAnimationCallback();
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_keyboardEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::Shift] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 772.f, 27.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour(1.f, 0.f, 0.2f));
    entity.addComponent<cro::Callback>().function = ButtonAnimationCallback();
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_keyboardEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::Backspace] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 539.f, 27.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour(0.f, 0.4667f, 1.f));
    entity.addComponent<cro::Callback>().function = ButtonAnimationCallback();
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_keyboardEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::Space] = entity;

    m_keyboardLayouts[KeyboardLayout::Lower].bounds = spriteSheet.getSprite("lower").getTextureRect();
    m_keyboardLayouts[KeyboardLayout::Upper].bounds = spriteSheet.getSprite("upper").getTextureRect();
    m_keyboardLayouts[KeyboardLayout::Symbol].bounds = spriteSheet.getSprite("symbols").getTextureRect();
}

void KeyboardState::initCallbacks()
{
    //each callback starts at bottom left
    //and works across then up

    auto& lowerCallbacks = m_keyboardLayouts[KeyboardLayout::Lower].callbacks;
    
    //<zxcvbnm,.
    lowerCallbacks[0] = SendCodepoint(0x3c, 0);
    lowerCallbacks[1] = SendCodepoint(0x7a, 0);
    lowerCallbacks[2] = SendCodepoint(0x78, 0);
    lowerCallbacks[3] = SendCodepoint(0x63, 0);
    lowerCallbacks[4] = SendCodepoint(0x76, 0);
    lowerCallbacks[5] = SendCodepoint(0x62, 0);
    lowerCallbacks[6] = SendCodepoint(0x6e, 0);
    lowerCallbacks[7] = SendCodepoint(0x6d, 0);
    lowerCallbacks[8] = SendCodepoint(0x2c, 0);
    lowerCallbacks[9] = SendCodepoint(0x2e, 0);


    //asdfghjkl>
    lowerCallbacks[10] = SendCodepoint(0x61, 0);
    lowerCallbacks[11] = SendCodepoint(0x73, 0);
    lowerCallbacks[12] = SendCodepoint(0x64, 0);
    lowerCallbacks[13] = SendCodepoint(0x66, 0);
    lowerCallbacks[14] = SendCodepoint(0x67, 0);
    lowerCallbacks[15] = SendCodepoint(0x68, 0);
    lowerCallbacks[16] = SendCodepoint(0x6a, 0);
    lowerCallbacks[17] = SendCodepoint(0x6b, 0);
    lowerCallbacks[18] = SendCodepoint(0x6c, 0);
    lowerCallbacks[19] = SendCodepoint(0x3e, 0);

    //qwertyuiop
    lowerCallbacks[20] = SendCodepoint(0x71, 0);
    lowerCallbacks[21] = SendCodepoint(0x77, 0);
    lowerCallbacks[22] = SendCodepoint(0x65, 0);
    lowerCallbacks[23] = SendCodepoint(0x72, 0);
    lowerCallbacks[24] = SendCodepoint(0x74, 0);
    lowerCallbacks[25] = SendCodepoint(0x79, 0);
    lowerCallbacks[26] = SendCodepoint(0x75, 0);
    lowerCallbacks[27] = SendCodepoint(0x69, 0);
    lowerCallbacks[28] = SendCodepoint(0x6f, 0);
    lowerCallbacks[29] = SendCodepoint(0x70, 0);

    auto& upperCallbacks = m_keyboardLayouts[KeyboardLayout::Upper].callbacks;
    
    //\ZXCVBNM/?
    upperCallbacks[0] = SendCodepoint(0x5c, 0);
    upperCallbacks[1] = SendCodepoint(0x5a, 0);
    upperCallbacks[2] = SendCodepoint(0x58, 0);
    upperCallbacks[3] = SendCodepoint(0x43, 0);
    upperCallbacks[4] = SendCodepoint(0x56, 0);
    upperCallbacks[5] = SendCodepoint(0x42, 0);
    upperCallbacks[6] = SendCodepoint(0x4e, 0);
    upperCallbacks[7] = SendCodepoint(0x4d, 0);
    upperCallbacks[8] = SendCodepoint(0x2f, 0);
    upperCallbacks[9] = SendCodepoint(0x3f, 0);

    //ASDFGHJKL:
    upperCallbacks[10] = SendCodepoint(0x41, 0);
    upperCallbacks[11] = SendCodepoint(0x53, 0);
    upperCallbacks[12] = SendCodepoint(0x44, 0);
    upperCallbacks[13] = SendCodepoint(0x46, 0);
    upperCallbacks[14] = SendCodepoint(0x47, 0);
    upperCallbacks[15] = SendCodepoint(0x48, 0);
    upperCallbacks[16] = SendCodepoint(0x4a, 0);
    upperCallbacks[17] = SendCodepoint(0x4b, 0);
    upperCallbacks[18] = SendCodepoint(0x4c, 0);
    upperCallbacks[19] = SendCodepoint(0x3a, 0);

    //QWERTYUIOP
    upperCallbacks[20] = SendCodepoint(0x51, 0);
    upperCallbacks[21] = SendCodepoint(0x57, 0);
    upperCallbacks[22] = SendCodepoint(0x45, 0);
    upperCallbacks[23] = SendCodepoint(0x52, 0);
    upperCallbacks[24] = SendCodepoint(0x54, 0);
    upperCallbacks[25] = SendCodepoint(0x59, 0);
    upperCallbacks[26] = SendCodepoint(0x55, 0);
    upperCallbacks[27] = SendCodepoint(0x49, 0);
    upperCallbacks[28] = SendCodepoint(0x4f, 0);
    upperCallbacks[29] = SendCodepoint(0x50, 0);

    auto& symbolCallbacks = m_keyboardLayouts[KeyboardLayout::Symbol].callbacks;
    
    //~_-+=;@# euro.
    symbolCallbacks[0] = SendCodepoint(0x7e, 0);
    symbolCallbacks[1] = SendCodepoint(0x5f, 0);
    symbolCallbacks[2] = SendCodepoint(0x2d, 0);
    symbolCallbacks[3] = SendCodepoint(0x2b, 0);
    symbolCallbacks[4] = SendCodepoint(0x3d, 0);
    symbolCallbacks[5] = SendCodepoint(0x3b, 0);
    symbolCallbacks[6] = SendCodepoint(0x40, 0);
    symbolCallbacks[7] = SendCodepoint(0x23, 0);
    symbolCallbacks[8] = SendCodepoint(-30, -126, -84);
    symbolCallbacks[9] = SendCodepoint(0x2e, 0);

    //1234567890
    symbolCallbacks[10] = SendCodepoint(0x31, 0);
    symbolCallbacks[11] = SendCodepoint(0x32, 0);
    symbolCallbacks[12] = SendCodepoint(0x33, 0);
    symbolCallbacks[13] = SendCodepoint(0x34, 0);
    symbolCallbacks[14] = SendCodepoint(0x35, 0);
    symbolCallbacks[15] = SendCodepoint(0x36, 0);
    symbolCallbacks[16] = SendCodepoint(0x37, 0);
    symbolCallbacks[17] = SendCodepoint(0x38, 0);
    symbolCallbacks[18] = SendCodepoint(0x39, 0);
    symbolCallbacks[19] = SendCodepoint(0x30, 0);

    //!"�$%^&*()
    symbolCallbacks[20] = SendCodepoint(0x21, 0);
    symbolCallbacks[21] = SendCodepoint(0x22, 0);
    symbolCallbacks[22] = SendCodepoint(-62, -93);
    symbolCallbacks[23] = SendCodepoint(0x24, 0);
    symbolCallbacks[24] = SendCodepoint(0x25, 0);
    symbolCallbacks[25] = SendCodepoint(0x5e, 0);
    symbolCallbacks[26] = SendCodepoint(0x26, 0);
    symbolCallbacks[27] = SendCodepoint(0x2a, 0);
    symbolCallbacks[28] = SendCodepoint(0x28, 0);
    symbolCallbacks[29] = SendCodepoint(0x29, 0);
}

void KeyboardState::quitState()
{
    m_highlightEntity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

    m_keyboardEntity.getComponent<cro::Callback>().getUserData<KeyboardCallbackData>().state = KeyboardCallbackData::Out;
    m_keyboardEntity.getComponent<cro::Callback>().active = true;
}

void KeyboardState::setCursorPosition()
{
    auto x = m_selectedIndex % GridX;
    auto y = m_selectedIndex / GridX;

    glm::vec2 pos = { x * GridSpacing.x, y * GridSpacing.y };
    pos += GridOffset;

    m_highlightEntity.getComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
}

void KeyboardState::left()
{
    m_selectedIndex = ((m_selectedIndex + (GridSize - 1)) % GridX) + ((m_selectedIndex / GridX) * GridX);
    setCursorPosition();
}

void KeyboardState::right()
{
    m_selectedIndex = ((m_selectedIndex + 1) % GridX) + ((m_selectedIndex / GridX) * GridX);
    setCursorPosition();
}

void KeyboardState::up()
{
    m_selectedIndex = (m_selectedIndex + GridX) % GridSize;
    setCursorPosition();
}

void KeyboardState::down()
{
    m_selectedIndex = (m_selectedIndex + (GridSize - GridX)) % GridSize;
    setCursorPosition();
}

void KeyboardState::activate()
{
    m_keyboardLayouts[m_activeLayout].callbacks[m_selectedIndex]();

    auto rect = m_highlightEntity.getComponent<cro::Sprite>().getTextureRect();
    rect.left = rect.width;
    m_highlightEntity.getComponent<cro::Sprite>().setTextureRect(rect);
    m_highlightEntity.getComponent<cro::Callback>().active = true;
}

void KeyboardState::nextLayout()
{
    m_activeLayout = (m_activeLayout + 1) % KeyboardLayout::Count;
    m_keyboardEntity.getComponent<cro::Sprite>().setTextureRect(m_keyboardLayouts[m_activeLayout].bounds);

    m_buttonEnts[ButtonID::Shift].getComponent<cro::Callback>().active = true;
    m_buttonEnts[ButtonID::Shift].getComponent<cro::Callback>().setUserData<float>(1.f);
};

void KeyboardState::sendKeystroke(std::int32_t key)
{
    SDL_Event evt;
    evt.type = SDL_KEYDOWN;
    evt.key.keysym.sym = key;
    evt.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    evt.key.timestamp = 0;
    evt.key.repeat = 0;
    evt.key.windowID = 0;
    evt.key.state = SDL_RELEASED;

    SDL_PushEvent(&evt);
};

void KeyboardState::sendBackspace()
{
    sendKeystroke(SDLK_BACKSPACE);
    m_buttonEnts[ButtonID::Backspace].getComponent<cro::Callback>().active = true;
    m_buttonEnts[ButtonID::Backspace].getComponent<cro::Callback>().setUserData<float>(1.f);
}

void KeyboardState::sendSpace()
{
    //sendKeystroke(SDLK_SPACE);

    SendCodepoint space(0x20, 0);
    space();

    m_buttonEnts[ButtonID::Space].getComponent<cro::Callback>().active = true;
    m_buttonEnts[ButtonID::Space].getComponent<cro::Callback>().setUserData<float>(1.f);
}