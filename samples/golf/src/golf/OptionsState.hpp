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

#include <crogine/core/State.hpp>

#include <crogine/ecs/Scene.hpp>

struct SharedStateData;

namespace cro
{
    class SpriteSheet;
}

class OptionsState final : public cro::State
{
public:
    OptionsState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Options; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

    struct VideoSettings final
    {
        std::size_t resolutionIndex = 0;
        bool fullScreen = false;
    }m_videoSettings;

    bool m_updatingKeybind;
    float m_lastMousePos;
    std::vector<cro::Entity> m_sliders;
    cro::Entity m_activeSlider;
    void pickSlider();
    void updateSlider();

    std::int32_t m_bindingIndex;
    void updateKeybind(SDL_Keycode);

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    void buildScene();

    void buildAVMenu(cro::Entity, const cro::SpriteSheet&);
    void buildControlMenu(cro::Entity, const cro::SpriteSheet&);

    void createButtons(cro::Entity, std::int32_t, std::uint32_t, std::uint32_t);

    void quitState();
};