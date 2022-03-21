/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "BilliardsInput.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    constexpr float CamRotationSpeed = 0.05f;
    constexpr float CamRotationSpeedFast = CamRotationSpeed * 5.f;
    constexpr float CueRotationSpeed = 0.025f;
    constexpr float MaxCueRotation = cro::Util::Const::PI / 16.f;
}

BilliardsInput::BilliardsInput(const InputBinding& ip)
    : m_inputBinding(ip),
    m_inputFlags    (0),
    m_prevFlags     (0),
    m_mouseWheel    (0),
    m_prevMouseWheel(0),
    m_mouseMove     (0.f),
    m_prevMouseMove (0.f),
    m_active        (true)
{

}

//public
void BilliardsInput::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags |= InputFlag::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags |= InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags |= InputFlag::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags |= InputFlag::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags |= InputFlag::Action;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
        {
            m_inputFlags |= InputFlag::NextClub;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
        {
            m_inputFlags |= InputFlag::PrevClub;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags &= ~InputFlag::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags &= ~InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags &= ~InputFlag::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags &= ~InputFlag::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags &= ~InputFlag::Action;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
        {
            m_inputFlags &= ~InputFlag::NextClub;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
        {
            m_inputFlags &= ~InputFlag::PrevClub;
        }
    }
    
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(m_inputBinding.controllerID))
        {
            if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags |= InputFlag::Action;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::NextClub])
            {
                m_inputFlags |= InputFlag::NextClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::PrevClub])
            {
                m_inputFlags |= InputFlag::PrevClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CamModifier])
            {
                m_inputFlags |= InputFlag::CamModifier;
            }

            else if (evt.cbutton.button == cro::GameController::DPadLeft)
            {
                m_inputFlags |= InputFlag::Left;
            }
            else if (evt.cbutton.button == cro::GameController::DPadRight)
            {
                m_inputFlags |= InputFlag::Right;
            }
            else if (evt.cbutton.button == cro::GameController::DPadUp)
            {
                m_inputFlags |= InputFlag::Up;
            }
            else if (evt.cbutton.button == cro::GameController::DPadDown)
            {
                m_inputFlags |= InputFlag::Down;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(m_inputBinding.controllerID))
        {
            if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags &= ~InputFlag::Action;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::NextClub])
            {
                m_inputFlags &= ~InputFlag::NextClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::PrevClub])
            {
                m_inputFlags &= ~InputFlag::PrevClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CamModifier])
            {
                m_inputFlags &= ~InputFlag::CamModifier;
            }

            else if (evt.cbutton.button == cro::GameController::DPadLeft)
            {
                m_inputFlags &= ~InputFlag::Left;
            }
            else if (evt.cbutton.button == cro::GameController::DPadRight)
            {
                m_inputFlags &= ~InputFlag::Right;
            }
            else if (evt.cbutton.button == cro::GameController::DPadUp)
            {
                m_inputFlags &= ~InputFlag::Up;
            }
            else if (evt.cbutton.button == cro::GameController::DPadDown)
            {
                m_inputFlags &= ~InputFlag::Down;
            }
        }
    }

    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_inputFlags |= InputFlag::Action;
            break;
        case SDL_BUTTON_RIGHT:
            m_inputFlags |= InputFlag::CamModifier;
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_inputFlags &= ~InputFlag::Action;
            break;
        case SDL_BUTTON_RIGHT:
            m_inputFlags &= ~InputFlag::CamModifier;
            break;
        }
    }

    else if (evt.type == SDL_MOUSEWHEEL)
    {
        m_mouseWheel += evt.wheel.y;
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        m_mouseMove = static_cast<float>(evt.motion.xrel);
    }
}

void BilliardsInput::update(float dt)
{
    if (m_active)
    {
        if (m_mouseMove)
        {
            if (*m_camEntity.getComponent<ControllerRotation>().activeCamera)
            {
                //this is the cue view
                if (m_inputFlags & InputFlag::CamModifier)
                {
                    //move cam
                    m_camEntity.getComponent<ControllerRotation>().rotation += CamRotationSpeed * m_mouseMove * dt;
                    m_camEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camEntity.getComponent<ControllerRotation>().rotation);
                }
                else
                {
                    //move cue
                    auto rotation = m_cueEntity.getComponent<ControllerRotation>().rotation + CueRotationSpeed * m_mouseMove * dt;
                    rotation = std::max(-MaxCueRotation, std::min(MaxCueRotation, rotation));
                    m_cueEntity.getComponent<ControllerRotation>().rotation = rotation;
                    m_cueEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                }
            }
            else
            {
                //some other view so just rotate the camera
                m_camEntity.getComponent<ControllerRotation>().rotation += CamRotationSpeedFast * m_mouseMove * dt;
                m_camEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camEntity.getComponent<ControllerRotation>().rotation);
            }
        }
    }

    m_prevFlags = m_inputFlags;
    
    m_prevMouseMove = m_mouseMove;
    m_mouseMove = 0;

    m_prevMouseWheel = m_mouseWheel;
    m_mouseWheel = 0;
}

void BilliardsInput::setControlEntities(cro::Entity camera, cro::Entity cue)
{
    CRO_ASSERT(camera.hasComponent<ControllerRotation>(), "");
    CRO_ASSERT(cue.hasComponent<ControllerRotation>(), "");

    m_camEntity = camera;
    m_cueEntity = cue;
}

std::pair<glm::vec3, glm::vec3> BilliardsInput::getImpulse() const
{
    const float TotalRotation = m_camEntity.getComponent<ControllerRotation>().rotation + m_cueEntity.getComponent<ControllerRotation>().rotation;

    glm::vec4 direction(1.f, 0.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::mat4(1.f), TotalRotation, cro::Transform::Y_AXIS);

    direction = rotation * direction;

    //TODO multiply by strength
    //TODO read offset

    return { glm::vec3(direction), glm::vec3(0.f) };
}