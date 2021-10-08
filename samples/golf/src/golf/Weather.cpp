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

#include "GolfState.hpp"
#include "PoissonDisk.hpp"
#include "GameConsts.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>

namespace
{
#include "WireframeShader.inl"

    const std::string WeatherVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec4 u_clipPlane;

    uniform float u_time;

#if defined (CULLED)
    uniform HIGH vec3 u_cameraWorldPosition;
#endif

    VARYING_OUT LOW vec4 v_colour;

const float SystemHeight = 10.0;

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;

position.y -= u_time;
position.y = mod(position.y, SystemHeight);
position.y -= (SystemHeight - position.y) * 5.0;

        gl_Position = wvp * position;

        vec4 worldPos = u_worldMatrix * position;
        v_colour = a_colour;

#if defined (CULLED)
        vec3 distance = worldPos.xyz - u_cameraWorldPosition;
        v_colour.a *= smoothstep(12.25, 16.0, dot(distance, distance));
#endif

        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    }
)";


    const std::array<float, 3u> AreaStart = { 0.f, 0.f, -10.f };
    const std::array<float, 3u> AreaEnd = { 10.f, 10.f, 0.f };
}

void GolfState::createWeather()
{
    cro::Clock clock;
    auto points = pd::PoissonDiskSampling(1.6f, AreaStart, AreaEnd, 30u, static_cast<std::uint32_t>(std::time(nullptr)));

    auto t = static_cast<float>(clock.elapsed().asMilliseconds()) / 1000.f;
    LogI << "Generated " << points.size() << " in " << t << " seconds" << std::endl;

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(meshID);
    std::vector<float> verts;
    std::vector<std::uint32_t> indices;
    for (auto i = 0u; i < points.size(); ++i)
    {
        verts.push_back(points[i][0]);
        verts.push_back(points[i][1]);
        verts.push_back(points[i][2]);
        verts.push_back(1.f);
        verts.push_back(0.f);
        verts.push_back(0.5f);
        verts.push_back(1.f);

        /*
        Problem with 2 verts is that when the first wraps
        around it stretches to the other still at the top.
        */

        /*verts.push_back(points[i][0]);
        verts.push_back(points[i][1] + 0.1f);
        verts.push_back(points[i][2]);
        verts.push_back(0.5f);
        verts.push_back(0.f);
        verts.push_back(1.f);
        verts.push_back(1.f);*/

        indices.push_back(i);
    }

    //for (auto i = 0u; i < points.size() * 2u; ++i)
    //{
    //    indices.push_back(i);
    //}

    meshData->vertexCount = points.size();
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    m_resources.shaders.loadFromString(ShaderID::Weather, WeatherVertex, WireframeFragment);
    auto& shader = m_resources.shaders.get(ShaderID::Weather);
    auto materialID = m_resources.materials.add(shader);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(250.f, 0.f, -28.f));
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialID));


    //hax to update uniform
    auto uniformID = shader.getUniformID("u_time");
    auto shaderID = shader.getGLHandle();
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [uniformID, shaderID](cro::Entity, float dt)
    {
        static float accum = 0.f;
        accum += dt;

        glCheck(glUseProgram(shaderID));
        glCheck(glUniform1f(uniformID, accum));
    };
}