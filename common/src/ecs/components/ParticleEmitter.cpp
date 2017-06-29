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

#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/graphics/TextureResource.hpp>
#include <crogine/core/ConfigFile.hpp>

using namespace cro;

ParticleEmitter::ParticleEmitter()
    : m_vbo             (0),
    m_nextFreeParticle  (0),
    m_running           (false)
{

}

//void ParticleEmitter::applySettings(const EmitterSettings& es)
//{
//    CRO_ASSERT(es.emitRate > 0, "Emit rate must be grater than 0");
//    CRO_ASSERT(es.lifetime > 0, "Lifetime must be greater than 0");
//    
//    m_emitterSettings = es;
//}

void ParticleEmitter::start()
{
    m_running = true;
}

void ParticleEmitter::stop()
{
    m_running = false;
}

bool EmitterSettings::loadFromFile(const std::string& path, cro::TextureResource& textures)
{
    ConfigFile cfg;
    if (!cfg.loadFromFile(path)) return false;

    if (cfg.getName() == "particle_system")
    {
        const auto& properties = cfg.getProperties();
        for (const auto& p : properties)
        {
            auto name = p.getName();
            if (name == "src")
            {
                textureID = textures.get(p.getValue<std::string>()).getGLHandle();
            }
            else if (name == "blendmode")
            {
                auto mode = p.getValue<std::string>();
                if (mode == "add")
                {
                    blendmode = EmitterSettings::Add;
                }
                else if (mode == "multiply")
                {
                    blendmode = EmitterSettings::Multiply;
                }
                else
                {
                    blendmode = EmitterSettings::Alpha;
                }
            }
            else if (name == "gravity")
            {
                gravity = p.getValue<glm::vec3>();
            }
            else if (name == "velocity")
            {
                initialVelocity = p.getValue<glm::vec3>();
            }
            else if (name == "lifetime")
            {
                lifetime = p.getValue<float>();
            }
            else if (name == "lifetime_variance")
            {
                lifetimeVariance = p.getValue<float>();
            }
            else if (name == "colour")
            {
                glm::vec4 c = p.getValue<glm::vec4>();
                colour = cro::Colour(c.r, c.g, c.b, c.a);
            }
            else if (name == "rotation_speed")
            {
                rotationSpeed = p.getValue<float>();
            }
            else if (name == "scale_affector")
            {
                scaleModifier = p.getValue<float>();
            }
            else if (name == "size")
            {
                size = p.getValue<float>();
            }
            else if (name == "emit_rate")
            {
                emitRate = p.getValue<float>();
            }
            else if (name == "spawn_radius")
            {
                spawnRadius = p.getValue<float>();
            }
        }

        const auto& objects = cfg.getObjects();
        for (const auto& o : objects)
        {
            //load force array
            if (o.getName() == "forces")
            {
                const auto& props = o.getProperties();
                std::size_t currentForce = 0;
                for (const auto& force : props)
                {
                    if (force.getName() == "force")
                    {
                        forces[currentForce++] = force.getValue<glm::vec3>();
                        if (currentForce == forces.size())
                        {
                            break;
                        }
                    }
                }
            }
        }

        if (textureID == 0)
        {
            Logger::log(path + ": no texture proeprty found", Logger::Type::Warning);
        }
    }

    return false;
}