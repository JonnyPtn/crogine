/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include "../detail/TextConstruction.hpp"
#include "../detail/glad.hpp"

#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/RenderTarget.hpp>

using namespace cro;

SimpleText::SimpleText()
    : m_lastTextureSize (0),
    m_fontTexture       (nullptr),
    m_dirtyFlags        (DirtyFlags::All)
{
    setPrimitiveType(GL_TRIANGLES);
}

SimpleText::SimpleText(const Font& font)
    : SimpleText()
{
    setFont(font);
}

//public
void SimpleText::setFont(const Font& font)
{
    m_context.font = &font;
    m_dirtyFlags |= DirtyFlags::All;
}

void SimpleText::setCharacterSize(std::uint32_t size)
{
    m_context.charSize = size;
    m_dirtyFlags |= DirtyFlags::All;
}

void SimpleText::setVerticalSpacing(float spacing)
{
    m_context.verticalSpacing = spacing;
    m_dirtyFlags |= DirtyFlags::All;
}

void SimpleText::setString(const String& str)
{
    if (m_context.string != str)
    {
        m_context.string = str;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void SimpleText::setFillColour(Colour colour)
{
    if (m_context.fillColour != colour)
    {
        //hmm if we cached vertex data we
        //could just update the colour property rather
        //than rebuild the entire thing if only
        //the colour flag is set
        m_context.fillColour = colour;
        m_dirtyFlags |= DirtyFlags::ColourInner;
    }
}

void SimpleText::setOutlineColour(Colour colour)
{
    if (m_context.outlineColour != colour)
    {
        m_context.outlineColour = colour;
        m_dirtyFlags |= DirtyFlags::ColourOuter;
    }
}

void SimpleText::setOutlineThickness(float thickness)
{
    if (m_context.outlineThickness != thickness)
    {
        m_context.outlineThickness = thickness;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

const Font* SimpleText::getFont() const
{
    return m_context.font;
}

std::uint32_t SimpleText::getCharacterSize() const
{
    return m_context.charSize;
}

float SimpleText::getVerticalSpacing() const
{
    return m_context.verticalSpacing;
}

const String& SimpleText::getString() const
{
    return m_context.string;
}

Colour SimpleText::getFillColour() const
{
    return m_context.fillColour;
}

Colour SimpleText::getOutlineColour() const
{
    return m_context.outlineColour;
}

float SimpleText::getOutlineThickness() const
{
    return m_context.outlineThickness;
}

FloatRect SimpleText::getLocalBounds()
{
    if (m_dirtyFlags)
    {
        m_dirtyFlags = 0;
        updateVertices();
    }
    return m_localBounds;
}

FloatRect SimpleText::getGlobalBounds()
{
    return getLocalBounds().transform(getTransform());
}

void SimpleText::draw(const RenderTarget& target)
{
    if (m_dirtyFlags
        || (m_fontTexture && m_fontTexture->getSize() != m_lastTextureSize))
    {
        m_dirtyFlags = 0;
        updateVertices();
    }
    drawGeometry(getTransform(), target.getSize());
}

//private
void SimpleText::updateVertices()
{
    if (m_context.font)
    {
        //do this first because the update below may
        //resize the texture and we want to know about it :)
        m_fontTexture = &m_context.font->getTexture(m_context.charSize);
        m_lastTextureSize = m_fontTexture->getSize();
        setTexture(*m_fontTexture);
    }
    
    std::vector<Vertex2D> verts;
    m_localBounds = Detail::Text::updateVertices(verts, m_context);
    setVertexData(verts);
}