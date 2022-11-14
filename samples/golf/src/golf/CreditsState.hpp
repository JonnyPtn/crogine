/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>

struct SharedStateData;
struct CreditEntry final
{
    cro::String title;
    std::vector<cro::String> names;
};

class CreditsState final : public cro::State
{
public:
    CreditsState(cro::StateStack&, cro::State::Context, SharedStateData&, const std::vector<CreditEntry>&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Credits; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;
    const std::vector<CreditEntry>& m_credits;


    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    void buildScene();

    void quitState();
};