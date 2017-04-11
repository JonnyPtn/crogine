/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/Texture.hpp>

using namespace cro;

Sprite::Sprite()
    : m_textureID(-1)
{
    for (auto& q : m_quad)
    {
        q.colour = { 1.f, 1.f, 1.f, 1.f };
        q.position.w = 1.f;
    }
}

//public
void Sprite::setTexture(const Texture& t)
{
    m_textureID = t.getGLHandle();
    FloatRect size = { 0.f, 0.f, static_cast<float>(t.getSize().x), static_cast<float>(t.getSize().y) };
    setSize(size);
    setTextureRect(size);
}

void Sprite::setSize(FloatRect size)
{
    /*
      1-------4
      |       |
      |       |
      0-------3
    */

    m_quad[0].position.x = size.left;
    m_quad[0].position.y = size.bottom;

    m_quad[1].position.x = size.left;
    m_quad[1].position.y = size.bottom + size.height;

    m_quad[2].position.x = size.left + size.width;
    m_quad[2].position.y = size.bottom;

    m_quad[3].position.x = size.left + size.width;
    m_quad[3].position.y = size.bottom + size.height;
}

void Sprite::setTextureRect(FloatRect subRect)
{
    float width = m_quad[3].position.x - m_quad[0].position.x;
    float height = m_quad[1].position.y - m_quad[0].position.y;
    
    CRO_ASSERT(width != 0 && height != 0, "Invalid width or height - is the texture set?");

    m_quad[0].UV.x = subRect.left / width;
    m_quad[0].UV.y = subRect.bottom / height;

    m_quad[1].UV.x = subRect.left / width;
    m_quad[1].UV.y = (subRect.bottom + subRect.height) / height;

    m_quad[2].UV.x = (subRect.left + subRect.width) / width;
    m_quad[2].UV.y = subRect.bottom / height;

    m_quad[3].UV.x = (subRect.left + subRect.width) / width;
    m_quad[3].UV.y = (subRect.bottom + subRect.height) / height;
}

void Sprite::setColour(Colour colour)
{
    for (auto& v : m_quad)
    {
        v.colour = { colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha() };
    }
}