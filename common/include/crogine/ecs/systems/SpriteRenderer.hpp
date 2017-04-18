/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef CRO_SPRITE_RENDERER_HPP_
#define CRO_SPRITE_RENDERER_HPP_

#include <crogine/Config.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <glm/mat4x4.hpp>

#include <map>
#include <set>

namespace cro
{
    class MessageBus;
    class Sprite;
    class Transform;

    /*!
    \brief Batches and renders the scene Sprite components
    */
    class CRO_EXPORT_API SpriteRenderer final : public System, public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor.
        \param mb Refernce to system message bus
        */
        explicit SpriteRenderer(MessageBus& mb);
        ~SpriteRenderer();

        //TODO setters for view/resolution

        enum class DepthAxis
        {
            Y, Z
        };
        /*!
        \brief Sets the depth sorting axis.
        When the depth axis is set to Y then sprites lower down on the
        screen will appear nearer the front. When set to Z they are sorted
        by the Z depth value of the entity transform. Defaults to Z
        */
        void setDepthAxis(DepthAxis d) { m_depthAxis = d; }

        /*!
        \brief Returns the current DepthAxis setting
        */
        DepthAxis getDepthAxis() const { return m_depthAxis; }

        /*!
        \brief Implements message handling
        */
        void handleMessage(const Message&) override;

        /*!
        \brief Implements the process which performs batching
        */
        void process(Time) override;

        /*!
        \brief Renders the Sprite components
        */
        void render();

    private:
        IntRect m_viewPort;
        void setViewPort(int32 x, int32 y);
        
        //maps VBO to textures
        struct Batch final
        {
            int32 texture = 0;
            uint32 start = 0;
            uint32 count = 0; //this is the COUNT not the final index
        };
        std::vector<std::pair<uint32, std::vector<Batch>>> m_buffers;
        std::vector<std::vector<glm::mat4>> m_bufferTransforms;

        enum AttribLocation
        {
            Position, Colour, UV0, UV1, Count
        };
        struct AttribData final
        {
            uint32 size = 0;
            uint32 location = 0;
            uint32 offset = 0;
        };
        std::array<AttribData, AttribLocation::Count> m_attribMap;

        glm::mat4 m_projectionMatrix;
        Shader m_shader;
        int32 m_matrixIndex;
        int32 m_textureIndex;
        int32 m_projectionIndex;

        DepthAxis m_depthAxis;

        bool m_pendingRebuild;
        void rebuildBatch();

        void updateGlobalBounds(Sprite&, const glm::mat4&);

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;

#ifdef _DEBUG_
        //Shader m_debugShader;
        int32 m_debugMatrixIndex;
        /*std::array<AttribData, 2u> m_debugAttribs;*/
        uint32 m_debugVBO;
        uint32 m_debugVertCount;

        void buildDebug();
        void drawDebug();
#endif //_DEBUG_
    };
}

#endif //CRO_SPRITE_RENDERER_HPP_