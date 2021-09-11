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

#include "TutorialDirector.hpp"
#include "MessageIDs.hpp"
#include "SharedStateData.hpp"
#include "../StateIDs.hpp"

TutorialDirector::TutorialDirector(SharedStateData& sd)
    : m_sharedData(sd)
{

}

//public
void TutorialDirector::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::SceneMessage:
    {
        const auto& data = msg.getData<SceneEvent>();
        switch (data.type)
        {
        default: break;
        case SceneEvent::TransitionComplete:
        {
            //push first tutorial
            m_sharedData.tutorialIndex = 0;

            auto* msg2 = postMessage<SystemEvent>(MessageID::SystemMessage);
            msg2->data = StateID::Tutorial;
            msg2->type = SystemEvent::StateRequest;
        }
        break;
        }
    }
    }
}