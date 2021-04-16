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

#include "PlayerState.hpp"
#include "PlayerSystem.hpp"
#include "GameConsts.hpp"
#include "CommonConsts.hpp"
#include "Collision.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/detail/glm/gtx/vector_angle.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Network.hpp>

PlayerStateWalking::PlayerStateWalking()
{

}

//public
void PlayerStateWalking::processMovement(cro::Entity entity, Input input)
{
    auto& player = entity.getComponent<Player>();
    auto& tx = entity.getComponent<cro::Transform>();

    const float multiplier = cro::Util::Net::decompressFloat(input.analogueMultiplier, 8);

    if (input.buttonFlags & InputFlag::Left)
    {
        //moving right, so slow down
        if (player.velocity.x > 0)
        {
            player.velocity.x -= Deceleration * multiplier;
            if (player.velocity.x <= 0)
            {
                player.velocity.x = -0.1f;
            }
        }
        //moving left, so speed up to max vel
        else if (player.velocity.x > -MaxVelocity)
        {
            player.velocity.x = std::max(-MaxVelocity, player.velocity.x - (Acceleration * multiplier));
        }
    }

    if (input.buttonFlags & InputFlag::Right)
    {
        //moving left, so slow down
        if (player.velocity.x < 0)
        {
            player.velocity.x += Deceleration * multiplier;
            if (player.velocity.x >= 0)
            {
                player.velocity.x = 0.1f;
            }
        }
        //moving right, so speed up to max vel
        else if (player.velocity.x < MaxVelocity)
        {
            player.velocity.x = std::min(MaxVelocity, player.velocity.x + (Acceleration * multiplier));
        }
    }

    if ((input.buttonFlags & (InputFlag::Left | InputFlag::Right)) == 0)
    {
        player.velocity.x -= std::min(std::abs(player.velocity.x), Friction) * cro::Util::Maths::sgn(player.velocity.x);
    }

    //jumpment
    if (player.collisionFlags & (1 << CollisionMaterial::Foot))
    {
        if ((input.buttonFlags & InputFlag::Action)
            && (player.previousInputFlags & InputFlag::Action) == 0)
        {
            player.velocity.y += JumpImpulse;
            player.state = Player::State::Falling;
        }
    }
    
    //adjust for the fact we're looking from behind on the second layer
    auto movement = player.velocity;
    movement.x *= Util::direction(player.collisionLayer);
    tx.move(movement * ConstVal::FixedGameUpdate);


    //check if we're over a teleport
    if (player.collisionFlags & (1 << CollisionMaterial::Teleport))
    {
        if ((input.buttonFlags & InputFlag::Up)
            && (player.previousInputFlags & InputFlag::Up) == 0)
        {
            player.state = Player::State::Teleport;
        }
    }


    player.previousInputFlags = input.buttonFlags;
}

void PlayerStateWalking::processCollision(cro::Entity entity, const std::vector<cro::Entity>& collisions)
{
    //narrow phase

    auto& player = entity.getComponent<Player>();
    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();
    
    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    glm::vec2 centre = { bodyRect.left + (bodyRect.width / 2.f), bodyRect.bottom + (bodyRect.height / 2.f) };

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            glm::vec2 otherCentre = { otherRect.left + (otherRect.width / 2.f), otherRect.bottom + (otherRect.height / 2.f) };

            glm::vec2 direction = otherCentre - centre;
            cro::FloatRect overlap;

            //foot collision
            if (footRect.intersects(otherRect, overlap))
            {
                player.collisionFlags |= (1 << CollisionMaterial::Foot);
            }

            //body collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //track which objects we're touching
                player.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));

                auto manifold = calcManifold(direction, overlap);
                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Solid:
                    entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                    if (manifold.normal.x != 0)
                    {
                        player.velocity = glm::reflect(player.velocity, glm::vec3(manifold.normal, 0.f)) * 0.1f;
                    }
                    break;
                case CollisionMaterial::Teleport:

                    break;
                }
            }
        }
    }

    if (player.collisionFlags == 0)
    {
        player.state = Player::State::Falling;
    }
}