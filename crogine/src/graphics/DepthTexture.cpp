/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/graphics/DepthTexture.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

DepthTexture::DepthTexture()
    : m_fboID   (0),
    m_textureID (0),
    m_size      (0,0),
    m_layerCount(0)
{

}

DepthTexture::~DepthTexture()
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
    }

    if (m_textureID)
    {
        glCheck(glDeleteTextures(1, &m_textureID));
    }
}

DepthTexture::DepthTexture(DepthTexture&& other) noexcept
    : DepthTexture()
{
    m_fboID = other.m_fboID;
    m_textureID = other.m_textureID;
    setViewport(other.getViewport());
    setView(other.getView());
    m_layerCount = other.m_layerCount;

    other.m_fboID = 0;
    other.m_textureID = 0;
    other.setViewport({ 0, 0, 0, 0 });
    other.setView({ 0.f, 0.f });
    other.m_layerCount = 0;
}

DepthTexture& DepthTexture::operator=(DepthTexture&& other) noexcept
{
    if (&other != this)
    {
        //tidy up anything we own first!
        if (m_fboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_fboID));
        }

        if (m_textureID)
        {
            glCheck(glDeleteTextures(1, &m_textureID));
        }

        m_fboID = other.m_fboID;
        m_textureID = other.m_textureID;
        setViewport(other.getViewport());
        setView(other.getView());
        m_layerCount = other.m_layerCount;

        other.m_fboID = 0;
        other.m_textureID = 0;
        other.setViewport({ 0, 0, 0, 0 });
        other.setView({ 0.f, 0.f });
        other.m_layerCount = 0;
    }
    return *this;
}

//public
bool DepthTexture::create(std::uint32_t width, std::uint32_t height, std::uint32_t layers)
{
#ifdef PLATFORM_MOBILE
    LogE << "Depth Textures are not available on mobile platforms" << std::endl;
    return false;
#else
    CRO_ASSERT(layers > 0, "");

    if (m_fboID &&
        m_textureID)
    {
        //resize the buffer
        glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID));
        glCheck(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, width, height, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

        setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
        setView(FloatRect(getViewport()));
        m_size = { width, height };
        m_layerCount = layers;

        return true;
    }

    //else create it
    m_size = { 0, 0 };
    setViewport({ 0, 0, 0, 0 });

    //create the texture
    glCheck(glGenTextures(1, &m_textureID));
    glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID));
    glCheck(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, width, height, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
    const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glCheck(glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor));


    //create the frame buffer
    glCheck(glGenFramebuffers(1, &m_fboID));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));
    //glCheck(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_textureID, 0));
    glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_textureID, 0, 0));
    glCheck(glDrawBuffer(GL_NONE));
    glCheck(glReadBuffer(GL_NONE));

    bool result = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (result)
    {
        setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
        setView(FloatRect(getViewport()));
        m_size = { width, height };
        m_layerCount = layers;
    }

    return result;
#endif
}

glm::uvec2 DepthTexture::getSize() const
{
    return m_size;
}

void DepthTexture::clear(std::uint32_t layer)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(m_fboID, "No FBO created!");
    CRO_ASSERT(m_layerCount > layer, "");

    //store active buffer and bind this one
    setActive(true);

    //TODO does checking to see we're not already on the
    //active layer take less time than just setting it every time?
    glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_textureID, 0, layer));

    glCheck(glColorMask(false, false, false, false));

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glClear(GL_DEPTH_BUFFER_BIT));
#endif
}

void DepthTexture::display()
{
#ifdef PLATFORM_DESKTOP
    glCheck(glColorMask(true, true, true, true));

    //unbind buffer
    setActive(false);
#endif
}

TextureID DepthTexture::getTexture() const
{
    return TextureID(m_textureID);
}