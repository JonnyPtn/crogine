/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/Message.hpp>

using namespace cro;

SpriteAnimator::SpriteAnimator(MessageBus& mb)
    : System(mb, typeid(SpriteAnimator))
{
    requireComponent<Sprite>();
    requireComponent<SpriteAnimation>();
}

//public
void SpriteAnimator::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities) 
    {
        auto& animation = entity.getComponent<SpriteAnimation>();
        if (animation.playing)
        {
            auto& sprite = entity.getComponent<Sprite>();
            CRO_ASSERT(animation.id < sprite.m_animations.size(), "");

            //TODO this should be an assertion as we should never have
            //tried playing the animation in the first place...
            if (sprite.m_animations[animation.id].frames.empty())
            {
                animation.stop();
                continue;
            }
            

            animation.currentFrameTime -= dt;
            if (animation.currentFrameTime < 0)
            {
                animation.currentFrameTime += (1.f / sprite.m_animations[animation.id].framerate);

                auto lastFrame = animation.frameID;
                animation.frameID = (animation.frameID + 1) % sprite.m_animations[animation.id].frames.size();

                if (animation.frameID < lastFrame)
                {
                    if (!sprite.m_animations[animation.id].looped)
                    {
                        animation.stop();
                        continue;
                    }
                    else
                    {
                        animation.frameID = std::max(animation.frameID, sprite.m_animations[animation.id].loopStart);
                    }
                }

                sprite.setTextureRect(sprite.m_animations[animation.id].frames[animation.frameID]);
            }
        }
    }
}