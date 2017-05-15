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

using namespace cro;

ParticleEmitter::ParticleEmitter()
    : m_vbo             (0),
    m_nextFreeParticle  (0),
    m_running           (true)
{

}

void ParticleEmitter::applySettings(const EmitterSettings& es)
{
    CRO_ASSERT(es.emitRate > 0, "Emit rate must be grater than 0");
    CRO_ASSERT(es.lifetime > 0, "Lifetime must be greater than 0");
    
    m_emitterSettings = es;
}

void ParticleEmitter::start()
{
    m_running = true;
}

void ParticleEmitter::stop()
{
    m_running = false;
}