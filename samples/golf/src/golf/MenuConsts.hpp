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

#include <crogine/graphics/Colour.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/detail/glm/vec2.hpp>

struct FontID final
{
    enum
    {
        UI,
        Info,

        Count
    };
};

static constexpr std::uint32_t LargeTextSize = 64;
static constexpr std::uint32_t MediumTextSize = 32;
static constexpr std::uint32_t SmallTextSize = 16;

static constexpr std::uint32_t UITextSize = 8;
static constexpr std::uint32_t InfoTextSize = 10;

static const cro::Colour TextNormalColour(0xfff8e1ff);
static const cro::Colour TextEditColour(0x6eb39dff);
static const cro::Colour TextHighlightColour(0xb83530ff); //red
//static const cro::Colour TextHighlightColour(0x005ab5ff); //cb blue
//static const cro::Colour TextHighlightColour(0x006cd1ff); //cb blue 2
static const cro::Colour TextGoldColour(0xf2cf5cff);
static const cro::Colour TextOrangeColour(0xec773dff);
static const cro::Colour TextGreenColour(0x6ebe70ff);
static const cro::Colour LeaderboardTextDark(0x1a1e2dff);
static const cro::Colour LeaderboardTextLight(0xfff8e1ff);

static constexpr float BackgroundAlpha = 0.7f;

static constexpr float UIBarHeight = 16.f;
static constexpr float UITextPosV = 12.f;
static constexpr glm::vec3 CursorOffset(-20.f, -7.f, 0.f);

//ui components are laid out as a normalised value
//relative to the window size.
struct UIElement final
{
    glm::vec2 absolutePosition = glm::vec2(0.f); //absolute in units
    glm::vec2 relativePosition = glm::vec2(0.f); //normalised relative to screen size
    float depth = 0.f;
};
static constexpr glm::vec2 UIHiddenPosition(-10000.f, -10000.f);

//spacing of each menu relative to root node
//see GolfMenuState::m_menuPositions/MenuCreation.cpp
static constexpr glm::vec2 MenuSpacing(1920.f, 1080.f);
static constexpr float MenuBottomBorder = 20.f;

static inline bool activated(const cro::ButtonEvent& evt)
{
    switch (evt.type)
    {
    default: return false;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
        return evt.button.button == SDL_BUTTON_LEFT;
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERBUTTONDOWN:
        return evt.cbutton.button == SDL_CONTROLLER_BUTTON_A;
    case SDL_FINGERUP:
    case SDL_FINGERDOWN:
        return true;
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        return (evt.key.keysym.sym == SDLK_KP_ENTER || evt.key.keysym.sym == SDLK_RETURN);
    }
}