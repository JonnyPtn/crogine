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

#ifndef CRO_SPRITE_SHEET_HPP_
#define CRO_SPRITE_SHEET_HPP_

#include <crogine/Config.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <unordered_map>
#include <string>

namespace cro
{
    class TextureResource;

    /*!
    \brief Supports loading multiple sprites from a single
    texture atlas via the ConfigFile format.
    */
    class CRO_EXPORT_API SpriteSheet final
    {
    public:
        SpriteSheet();

        /*!
        \brief Attempts to load a ConfigFile from the given path.
        A reference to a valid texture resource is required to load
        the sprite sheet texture.
        \returns true if successful, else false
        */
        bool loadFromFile(const std::string& path, TextureResource& rx);

        /*!
        \brief Returns a sprite component with the given name as it
        appears in the sprite sheet. If the sprite does not exist an
        empty sprite is returned.
        */
        Sprite getSprite(const std::string& name) const;

    private:
        mutable std::unordered_map<std::string, Sprite> m_sprites;

    };
}

#endif //CRO_SPRITE_SHEET_HPP_
