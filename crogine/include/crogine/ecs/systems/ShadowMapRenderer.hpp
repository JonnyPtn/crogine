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

#pragma once

#include <crogine/graphics/MaterialData.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/DepthTexture.hpp>

namespace cro
{
    class Texture;

    /*!
    \brief Shadow map renderer.
    Any entities with a shadow caster component and
    appropriate shadow map material assigned to the Model
    component will be rendered by this system into a
    render target. This system should be added to the scene
    before the ModelRenderer system (or any system which
    employs the depth map rendered by this system) in order
    that the depth map data be up to date.
    */
    class CRO_EXPORT_API ShadowMapRenderer final : public cro::System
    {
    public:
        /*!
        \brief Constructor.
        \param mb Message bus instance
        \param size Resolution of the depth buffer.
        */
        ShadowMapRenderer(MessageBus& mb, glm::uvec2 size = glm::uvec2(2048, 2048));

        void process(float) override;

        /*!
        \brief Returns a reference to the texture used to render the depth map
        */
        TextureID getDepthMapTexture() const;

    private:
#ifdef PLATFORM_DESKTOP
        DepthTexture m_target;
#else
        RenderTexture m_target;
#endif
        std::vector<std::pair<Entity, float>> m_visibleEntities;

        void updateDrawList();
        void render();

        void onEntityAdded(cro::Entity) override;
    };
}