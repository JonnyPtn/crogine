/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "PlayerColours.hpp"

#include <crogine/core/String.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <array>
#include <memory>

struct PlayerData final
{
    cro::String name;
    std::array<std::uint8_t, 4u> avatarFlags = { 1,0,3,6 }; //indices into colour pairs
    std::uint32_t ballID = 0;
    std::uint32_t hairID = 0;
    std::uint32_t skinID = 0; //as loaded from the avatar data file
    bool flipped = false; //whether or not avatar flipped
    bool isCPU = false; //these bools are flagged as bits in a single byte when serialised

    //these aren't included in serialise/deserialise
    std::vector<std::uint8_t> holeScores;
    std::uint8_t score = 0;
    std::uint8_t matchScore = 0;
    std::uint8_t skinScore = 0;
    std::int32_t parScore = 0;
    glm::vec3 currentTarget = glm::vec3(0.f);
    cro::Colour ballTint;

    mutable std::string profileID; //saving file generates this
    bool saveProfile() const;
    bool loadProfile(const std::string& path, const std::string& uid);
};

struct ProfileTexture final
{
    explicit ProfileTexture(const std::string&);

    void setColour(pc::ColourKey::Index, std::int8_t);
    std::pair<cro::Colour, cro::Colour> getColour(pc::ColourKey::Index) const;
    void apply(cro::Texture* dst = nullptr);

    cro::TextureID getTexture() const { return cro::TextureID(*m_texture); }

private:
    //make these pointers because this struct is
    //stored in a vector
    std::unique_ptr<cro::Image> m_imageBuffer;
    std::unique_ptr<cro::Texture> m_texture;

    std::array<std::vector<std::uint32_t>, pc::ColourKey::Count> m_keyIndicesLight;
    std::array<std::vector<std::uint32_t>, pc::ColourKey::Count> m_keyIndicesDark;

    std::array<cro::Colour, pc::ColourKey::Count> m_lightColours;
    std::array<cro::Colour, pc::ColourKey::Count> m_darkColours;
};