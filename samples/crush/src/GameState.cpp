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

#include "GameState.hpp"
#include "SharedStateData.hpp"
#include "PlayerSystem.hpp"
#include "PacketIDs.hpp"
#include "ActorIDs.hpp"
#include "ClientCommandIDs.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "DayNightDirector.hpp"
#include "MapData.hpp"
#include "ServerLog.hpp"
#include "GLCheck.hpp"
#include "Collision.hpp"
#include "DebugDraw.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Network.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace
{
    //for debug output
    cro::Entity playerEntity;
    std::size_t bitrate = 0;
    std::size_t bitrateCounter = 0;

    //render flags for reflection passes
    std::size_t NextPlayerPlane = 0; //index into this array when creating new player
    const std::uint64_t NoPlanes = 0xFFFFFFFFFFFFFF00;
    const std::array<std::uint64_t, 4u> PlayerPlanes =
    {
        0x1, 0x2,0x4,0x8
    };
    const std::uint64_t NoReflect = 0x10;
    const std::uint64_t NoRefract = 0x20;

    const std::array Colours =
    {
        cro::Colour::Red, cro::Colour::Magenta,
        cro::Colour::Green, cro::Colour::Yellow
    };
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_requestFlags      (ClientRequestFlags::All), //TODO remember to update this once we actually have stuff to request
    m_dataRequestCount  (0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    sd.clientConnection.ready = false;

    //skip requesting these because they're lag-tastic
    m_requestFlags |= (ClientRequestFlags::BushMap | ClientRequestFlags::TreeMap);

    //console commands
    registerCommand("sv_playermode", 
        [&](const std::string& params)
        {
            if (!params.empty())
            {
                ServerCommand cmd;
                cmd.target = 0;

                if (params == "fly")
                {
                    cmd.commandID = CommandPacket::SetModeFly;
                }
                else if (params == "walk")
                {
                    cmd.commandID = CommandPacket::SetModeWalk;
                }
                else
                {
                    cro::Console::print(params + ": unknown parameter.");
                    return;
                }
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, cmd, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else
            {
                cro::Console::print("Missing parameter. Usage: sv_playermode <mode>");
            }
        });

#ifdef CRO_DEBUG_
    //debug output
    registerWindow([&]()
        {
            ImGui::SetNextWindowSize({ 300.f, 320.f });
            if (ImGui::Begin("Info"))
            {
                if (playerEntity.isValid())
                {
                    ImGui::Text("Player ID: %d", m_sharedData.clientConnection.connectionID);

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();
                }

                ImGui::Text("Bitrate: %3.3fkbps", static_cast<float>(bitrate) / 1024.f);
            }
            ImGui::End();

            if (ImGui::Begin("Textures"))
            {
                ImGui::Image(m_debugViewTexture.getTexture(), { 512.f, 512.f }, { 0.f, 1.f }, { 1.f, 0.f });

                if (ImGui::Button("Perspective"))
                {
                    m_debugCam.getComponent<cro::Camera>().setPerspective(45.f * cro::Util::Const::degToRad, 1.f, 10.f, 60.f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Ortho"))
                {
                    m_debugCam.getComponent<cro::Camera>().setOrthographic(-10.f, 10.f, -10.f, 10.f, 10.f, 60.f);
                }

                ImGui::SameLine();
                if (ImGui::Button("Front"))
                {
                    m_debugCam.getComponent<cro::Transform>().setRotation(glm::vec3(1.f, 0.f, 0.f), 0.f);
                    m_debugCam.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 6.f, 30.f));
                }
                ImGui::SameLine();
                if (ImGui::Button("Top"))
                {
                    m_debugCam.getComponent<cro::Transform>().setRotation(glm::vec3(1.f, 0.f, 0.f), -90.f * cro::Util::Const::degToRad);
                    m_debugCam.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 30.f, 0.f));
                }
                ImGui::SameLine();
                if (ImGui::Button("Rear"))
                {
                    m_debugCam.getComponent<cro::Transform>().setRotation(glm::vec3(0.f, 1.f, 0.f), 180.f * cro::Util::Const::degToRad);
                    m_debugCam.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 6.f, -30.f));
                }

            }
            ImGui::End();
        });
#endif
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() 
        || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_PAUSE:
        case SDLK_ESCAPE:
        case SDLK_p:
            requestStackPush(States::ID::Pause);
            break;
#ifdef CRO_DEBUG_
        case SDLK_F4:
        {
            auto flags = m_gameScene.getSystem<cro::RenderSystem2D>().getFilterFlags();
            if (flags & TwoDeeFlags::Debug)
            {
                flags &= ~TwoDeeFlags::Debug;
            }
            else
            {
                flags |= TwoDeeFlags::Debug;
            }
            m_gameScene.getSystem<cro::RenderSystem2D>().setFilterFlags(flags);
        }
            break;
        /*case SDLK_1:
        {
            ServerCommand cmd;
            cmd.target = 0;
            cmd.commandID = CommandPacket::SetModeWalk;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, cmd, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
            break;
        case SDLK_2:
        {
            ServerCommand cmd;
            cmd.target = 0;
            cmd.commandID = CommandPacket::SetModeFly;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, cmd, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        break;*/
#endif //CRO_DEBUG_
        }
    }

    for (auto& [id, parser] : m_inputParsers)
    {
        parser.handleEvent(evt);
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GameState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            if (evt.type == cro::NetEvent::PacketReceived)
            {
                bitrateCounter += evt.packet.getSize() * 8;
                handlePacket(evt.packet);
            }
            else if (evt.type == cro::NetEvent::ClientDisconnect)
            {
                m_sharedData.errorMessage = "Disconnected from server.";
                requestStackPush(States::Error);
            }
        }

        if (m_bitrateClock.elapsed().asMilliseconds() > 1000)
        {
            //TODO convert this to a moving average
            m_bitrateClock.restart();
            bitrate = bitrateCounter;
            bitrateCounter = 0;
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
    }

    if (m_dataRequestCount == MaxDataRequests)
    {
        m_sharedData.errorMessage = "Failed to download\ndata from the server";
        requestStackPush(States::Error);

        return false;
    }

    //if we haven't had all the data yet ask for it
    if (m_requestFlags != ClientRequestFlags::All)
    {
        if (m_sceneRequestClock.elapsed().asMilliseconds() > 1000)
        {
            m_sceneRequestClock.restart();
            m_dataRequestCount++;


            //TODO this should have alrady been done in the lobby
            /*if ((m_requestFlags & ClientRequestFlags::MapName) == 0)
            {
                std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::MapName;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
            }*/
            //these require the heightmap has already been received
            /*else
            {
                if ((m_requestFlags & ClientRequestFlags::TreeMap) == 0)
                {
                    std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::TreeMap;
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
                }

                if ((m_requestFlags & ClientRequestFlags::BushMap) == 0)
                {
                    std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::BushMap;
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
                }
            }*/
        }
    }
    else if (!m_sharedData.clientConnection.ready)
    {
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable);
        m_sharedData.clientConnection.ready = true;
    }

    for (auto& [id, parser] : m_inputParsers)
    {
        parser.update();
    }

static float timeAccum = 0.f;
timeAccum += dt;
m_gameScene.setWaterLevel(std::sin(timeAccum * 0.9f) * 0.08f);

m_gameScene.simulate(dt);
m_uiScene.simulate(dt);
return true;
}

void GameState::render()
{
    if (m_cameras.empty())
    {
        return;
    }

    /*for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        auto ent = m_cameras[i];
        m_gameScene.setActiveCamera(ent);

        auto& cam = ent.getComponent<cro::Camera>();
        cam.renderFlags = NoPlanes | NoRefract;
        auto oldVP = cam.viewport;

        cam.viewport = { 0.f,0.f,1.f,1.f };

        cam.setActivePass(cro::Camera::Pass::Reflection);
        cam.reflectionBuffer.clear(cro::Colour::Red);
        m_gameScene.render(cam.reflectionBuffer);
        cam.reflectionBuffer.display();

        cam.renderFlags = NoPlanes | NoReflect;
        cam.setActivePass(cro::Camera::Pass::Refraction);
        cam.refractionBuffer.clear(cro::Colour::Blue);
        m_gameScene.render(cam.refractionBuffer);
        cam.refractionBuffer.display();

        cam.renderFlags = NoPlanes | PlayerPlanes[i] | NoReflect | NoRefract;
        cam.setActivePass(cro::Camera::Pass::Final);
        cam.viewport = oldVP;
    }*/

    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt, m_cameras);
    m_uiScene.render(rt);


#ifdef CRO_DEBUG_
    //render a far view of the scene in debug to a render texture
    auto oldCam = m_gameScene.setActiveCamera(m_debugCam);
    m_debugViewTexture.clear();
    m_gameScene.render(m_debugViewTexture);
    m_debugViewTexture.display();
    m_gameScene.setActiveCamera(oldCam);
#endif
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::DynamicTreeSystem>(mb);
    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<PlayerSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);

    m_gameScene.addDirector<DayNightDirector>();


    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GameState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/cubemap/beach02.hdr");
    m_skyMap.loadFromFile("assets/images/cubemap/skybox.hdr");
    m_gameScene.setCubemap(m_environmentMap);
    //m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");


    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::PBR, cro::ShaderResource::DiffuseColour | cro::ShaderResource::RxShadows);
    m_materialIDs[MaterialID::Default] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_colour", cro::Colour(1.f, 1.f, 1.f));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_maskColour", cro::Colour(0.f, 1.f, 1.f));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_brdfMap", m_environmentMap.getBRDFMap());
    //m_resources.materials.get(m_materialIDs[MaterialID::Default]).blendMode = cro::Material::BlendMode::Alpha;

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::DepthMap);
    m_materialIDs[MaterialID::DefaultShadow] = m_resources.materials.add(m_resources.shaders.get(shaderID));

#ifdef CRO_DEBUG_
    m_debugViewTexture.create(512, 512);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 6.f, 30.f));
    entity.addComponent<cro::Camera>().setOrthographic(-10.f, 10.f, -10.f, 10.f, 10.f, 60.f);
    entity.getComponent<cro::Camera>().depthBuffer.create(1024, 1024);

    m_debugCam = entity;
#endif
}

void GameState::createScene()
{
    createDayCycle();

    loadMap();

    //ground plane
    cro::ModelDefinition modelDef;
    modelDef.loadFromFile("assets/models/ground_plane.cmt", m_resources, &m_environmentMap);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    modelDef.createModel(entity, m_resources);
}

void GameState::createUI()
{
    cro::Entity entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);

    m_splitScreenNode = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>(); //the game scene camera callback will also update this
    m_uiScene.setActiveCamera(entity);
}

void GameState::createDayCycle()
{
    auto rootNode = m_gameScene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::CommandTarget>().ID = Client::CommandID::SunMoonNode;
    auto& children = rootNode.addComponent<ChildNode>();

    //moon
    auto entity = m_gameScene.createEntity();
    //we want to always be beyond the 'horizon', but we're fixed relative to the centre of the island
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -SunOffset });
    children.moonNode = entity;

    cro::ModelDefinition definition;
    definition.loadFromFile("assets/models/moon.cmt", m_resources);
    definition.createModel(entity, m_resources);
    entity.getComponent<cro::Model>().setRenderFlags(NoRefract);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //sun
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, SunOffset });
    children.sunNode = entity;

    definition.loadFromFile("assets/models/sun.cmt", m_resources);
    definition.createModel(entity, m_resources);
    entity.getComponent<cro::Model>().setRenderFlags(NoRefract);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //add the shadow caster node to the day night cycle
    auto sunNode = m_gameScene.getSunlight();
    sunNode.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 50.f });
    sunNode.addComponent<TargetTransform>();
    rootNode.getComponent<cro::Transform>().addChild(sunNode.getComponent<cro::Transform>());
}

void GameState::loadMap()
{
    MapData mapData;
    if (mapData.loadFromFile("assets/maps/" + m_sharedData.mapName.toAnsiString()))
    {
        //build the platform geometry
        for (auto i = 0; i < 2; ++i)
        {
            float layerDepth = LayerDepth * (1 + (i * -2));
            auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Normal/* | cro::VertexProperty::UV0*/, 1, GL_TRIANGLES));

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Default]));
            entity.getComponent<cro::Model>().setShadowMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::DefaultShadow]));
            entity.addComponent<cro::ShadowCaster>();

            std::vector<float> verts;
            std::vector<std::uint32_t> indices; //u16 should be fine but the mesh builder expects 32...

            //0------1
            //     /
            //    /
            //   /
            //  /
            //2------3

            const std::size_t vertComponentCount = 6; //MUST UPDATE THIS WITH VERT COMPONENT COUNT!
            const auto& rects = mapData.getCollisionRects(i);
            for (auto rect : rects)
            {
                auto indexOffset = static_cast<std::uint32_t>(verts.size() / vertComponentCount);

                //front face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness); //position
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f);


                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);


                //rear face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness); //position
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f);


                indexOffset += 4;
                indices.push_back(indexOffset + 1);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 0);

                indices.push_back(indexOffset + 1);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 2);


                //TODO we could properly calculate the outline of combined shapes
                //although this probably only saves us some small amount of overdraw

                //north face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness); //position
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f);

                indexOffset += 4;
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);

                //east face
                verts.push_back(rect.left+ rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness); //position
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f);

                indexOffset += 4;
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);


                //west face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness); //position
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f); //normal

                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f);

                indexOffset += 4;
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);

                //TODO only add the bottom face if the rect pos > half screen height (ie only when we can see the bottom, potentially)


                //might as well add the dynamic tree components...
                auto collisionEnt = m_gameScene.createEntity();
                collisionEnt.addComponent<cro::Transform>().setPosition({ rect.left, rect.bottom, layerDepth });
                collisionEnt.addComponent<cro::DynamicTreeComponent>().setArea({ glm::vec3(0.f, 0.f, LayerThickness), glm::vec3(rect.width, rect.height, -LayerThickness) });
                collisionEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags(i + 1);

                collisionEnt.addComponent<CollisionComponent>().rectCount = 1;
                collisionEnt.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Solid;
                collisionEnt.getComponent<CollisionComponent>().rects[0].bounds = { 0.f, 0.f, rect.width, rect.height };
                collisionEnt.getComponent<CollisionComponent>().calcSum();

#ifdef CRO_DEBUG_
                if (i == 0) addBoxDebug(collisionEnt, m_gameScene, cro::Colour::Blue);
#endif
            }

            auto& mesh = entity.getComponent<cro::Model>().getMeshData();

            glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexData[0].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            mesh.boundingBox[0] = { -10.5f, 0.f, 7.f };
            mesh.boundingBox[1] = { 10.5f, 12.f, 0.5f };
            mesh.boundingSphere.radius = 10.5f;
            mesh.vertexCount = static_cast<std::uint32_t>(verts.size() / vertComponentCount);
            mesh.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());
        }
    }
    else
    {
        m_sharedData.errorMessage = "Failed to load map " + m_sharedData.mapName.toAnsiString();
        requestStackPush(States::Error);
    }
}

void GameState::handlePacket(const cro::NetEvent::Packet& packet)
{
    switch (packet.getID())
    {
    default: break;
    case PacketID::ConnectionRefused:
        if (packet.as<std::uint8_t>() == MessageType::ServerQuit)
        {
            m_sharedData.errorMessage = "Server Closed The Connection";
            requestStackPush(States::Error);
        }
        break;
    case PacketID::StateChange:
        if (packet.as<std::uint8_t>() == sv::StateID::Lobby)
        {
            //TODO push some sort of round summary state

            requestStackPop();
            requestStackPush(States::MainMenu);
        }
        break;
    case PacketID::LogMessage:
        LogW << "Server: " << sv::LogStrings[packet.as<std::int32_t>()] << std::endl;
        break;
    case PacketID::DayNightUpdate:
        m_gameScene.getDirector<DayNightDirector>().setTimeOfDay(cro::Util::Net::decompressFloat(packet.as<std::int16_t>(), 8));
        break;
    case PacketID::EntityRemoved:
    {
        auto entityID = packet.as<std::uint32_t>();
        cro::Command cmd;
        cmd.targetFlags = Client::CommandID::Interpolated;
        cmd.action = [&,entityID](cro::Entity e, float)
        {
            if (e.getComponent<Actor>().serverEntityId == entityID)
            {
                m_gameScene.destroyEntity(e);
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::PlayerSpawn:
        //TODO we want to flag all these + world data
        //so we know to stop requesting world data
        m_sharedData.clientConnection.ready = true;
        spawnPlayer(packet.as<PlayerInfo>());
        break;
    case PacketID::PlayerUpdate:
        //we assume we're receiving local
    {
        auto update = packet.as<PlayerUpdate>();
        if (m_inputParsers.count(update.playerID))
        {
            m_gameScene.getSystem<PlayerSystem>().reconcile(m_inputParsers.at(update.playerID).getEntity(), update);
        }
    }
        break;
    case PacketID::ActorUpdate:
    {
        auto update = packet.as<ActorUpdate>();
        cro::Command cmd;
        cmd.targetFlags = Client::CommandID::Interpolated;
        cmd.action = [update](cro::Entity e, float)
        {
            if (e.isValid() &&
                e.getComponent<Actor>().serverEntityId == update.serverID)
            {
                auto& interp = e.getComponent<InterpolationComponent>();
                interp.setTarget({ update.position, cro::Util::Net::decompressQuat(update.rotation), update.timestamp });
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::ServerCommand:
    {
        auto data = packet.as<ServerCommand>();
        if (m_inputParsers.count(data.target) != 0)
        {
            auto playerEnt = m_inputParsers.at(data.target).getEntity();
            if (playerEnt.isValid() && playerEnt.getComponent<Player>().id == data.target)
            {
                switch (data.commandID)
                {
                default: break;

                }
            }
        }
    }
    break;
    case PacketID::ClientDisconnected:
        m_sharedData.playerData[packet.as<std::uint8_t>()].name.clear();
        break;
    }
}

void GameState::spawnPlayer(PlayerInfo info)
{
    //used for both local and remote
    auto createActor = [&]()->cro::Entity
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
        entity.getComponent<cro::Transform>().setRotation(cro::Util::Net::decompressQuat(info.rotation));
            
        entity.addComponent<Actor>().id = info.playerID;
        entity.getComponent<Actor>().serverEntityId = info.serverID;
        return entity;
    };

    //placeholder for player scale
    cro::ModelDefinition md;
    md.loadFromFile("assets/models/player_box.cmt", m_resources, &m_environmentMap);


    if (info.connectionID == m_sharedData.clientConnection.connectionID)
    {
        if (m_inputParsers.count(info.playerID) == 0)
        {
            //this is us - the root is controlled by the player and the
            //avatar is connected as a child
            auto root = createActor();
            
            root.addComponent<Player>().id = info.playerID;
            root.getComponent<Player>().connectionID = info.connectionID;
            root.getComponent<Player>().spawnPosition = info.spawnPosition;
            root.getComponent<Player>().collisionLayer = info.playerID / 2;

            root.addComponent<cro::DynamicTreeComponent>().setArea(PlayerBounds);
            root.getComponent<cro::DynamicTreeComponent>().setFilterFlags((info.playerID / 2) + 1);

            root.addComponent<CollisionComponent>().rectCount = 2;
            root.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Body;
            root.getComponent<CollisionComponent>().rects[0].bounds = { -PlayerSize.x / 2.f, 0.f, PlayerSize.x, PlayerSize.y };
            root.getComponent<CollisionComponent>().rects[1].material = CollisionMaterial::Foot;
            root.getComponent<CollisionComponent>().rects[1].bounds = FootBounds;
            root.getComponent<CollisionComponent>().calcSum();

            m_inputParsers.insert(std::make_pair(info.playerID, InputParser(m_sharedData.clientConnection.netClient, m_sharedData.inputBindings[info.playerID])));
            m_inputParsers.at(info.playerID).setEntity(root);


            m_cameras.emplace_back();
            m_cameras.back() = m_gameScene.createEntity();
            m_cameras.back().addComponent<cro::Transform>().setPosition({ 0.f, CameraHeight, CameraDistance });

            auto rotation = glm::lookAt(m_cameras.back().getComponent<cro::Transform>().getPosition(), glm::vec3(0.f, 0.f, -50.f), cro::Transform::Y_AXIS);
            m_cameras.back().getComponent<cro::Transform>().rotate(glm::inverse(rotation));

            auto& cam = m_cameras.back().addComponent<cro::Camera>();
            cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.reflectionBuffer.setSmooth(true);
            cam.refractionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.refractionBuffer.setSmooth(true);
            cam.depthBuffer.create(4096, 4096);


            //placeholder for player model
            //don't worry about mis-matching colour IDs... well be setting avatars differently

            auto playerEnt = m_gameScene.createEntity();
            playerEnt.addComponent<cro::Transform>().setOrigin({ 0.f, -0.4f, 0.f });
            md.createModel(playerEnt, m_resources);
            playerEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[info.playerID]);

            root.getComponent<Player>().avatar = playerEnt;
            root.getComponent<cro::Transform>().addChild(m_cameras.back().getComponent<cro::Transform>());
            root.getComponent<cro::Transform>().addChild(playerEnt.getComponent<cro::Transform>());


#ifdef CRO_DEBUG_
            addBoxDebug(root, m_gameScene);
            root.addComponent<DebugInfo>();

            registerWindow([root]()
                {
                    const auto& player = root.getComponent<Player>();
                    std::string label = "Player " + std::to_string(player.id);
                    if (ImGui::Begin(label.c_str()))
                    {
                        const auto& debug = root.getComponent<DebugInfo>();
                        ImGui::Text("Nearby: %d", debug.nearbyEnts);
                        ImGui::Text("Colliding: %d", debug.collidingEnts);
                        /*ImGui::Text("Bounds: %3.3f, %3.3f, %3.3f, - %3.3f, %3.3f, %3.3f",
                            debug.bounds[0].x, debug.bounds[0].y, debug.bounds[0].z,
                            debug.bounds[1].x, debug.bounds[1].y, debug.bounds[1].z);*/

                        switch (player.state)
                        {
                        default:
                            ImGui::Text("State: unknown");
                            break;
                        case Player::State::Falling:
                            ImGui::Text("State: Falling");
                            break;
                        case Player::State::Walking:
                            ImGui::Text("State: Walking");
                            break;
                        }

                        ImGui::Text("Vel X: %3.3f", player.velocity.x);
                    }
                    ImGui::End();
                });
#endif
        }


        if (m_cameras.size() == 1)
        {
            //this is the first to join
            m_gameScene.setActiveCamera(m_cameras[0]);

            //main camera - updateView() updates all cameras so we only need one callback
            auto camEnt = m_gameScene.getActiveCamera();
            updateView(camEnt.getComponent<cro::Camera>());
            camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&GameState::updateView, this, std::placeholders::_1);
        }
        else
        {
            //resizes the client views to match number of players
            updateView(m_cameras[0].getComponent<cro::Camera>());
        }
    }
    else
    {
        //spawn an avatar
        //TODO check this avatar doesn't already exist
        auto entity = createActor();
        md.createModel(entity, m_resources);

        auto rotation = entity.getComponent<cro::Transform>().getRotation();
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.4f, 0.f });
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[info.playerID]);

        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
        entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rotation, info.timestamp));

        entity.addComponent<cro::DynamicTreeComponent>().setArea(PlayerBounds);
        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags((info.playerID / 2) + 1);

        entity.addComponent<CollisionComponent>().rectCount = 1;// 2;
        entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Body;
        entity.getComponent<CollisionComponent>().rects[0].bounds = { -PlayerSize.x / 2.f, 0.f, PlayerSize.x, PlayerSize.y };
        /*entity.getComponent<CollisionComponent>().rects[1].material = CollisionMaterial::Foot;
        entity.getComponent<CollisionComponent>().rects[1].bounds = FootBounds;*/
        entity.getComponent<CollisionComponent>().calcSum();

#ifdef CRO_DEBUG_
        addBoxDebug(entity, m_gameScene, cro::Colour::Magenta);
#endif
    }
}

void GameState::updateView(cro::Camera&)
{
    CRO_ASSERT(!m_cameras.empty(), "Need at least one camera!");

    const float fov = 56.f * cro::Util::Const::degToRad;
    const float nearPlane = 0.1f;
    const float farPlane = 170.f;
    float aspect = 16.f / 9.f;

    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;
    cro::FloatRect viewport(0.f, (1.f - size.y) / 2.f, size.x, size.y);

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = viewport;

    //set up the viewports
    switch (m_cameras.size())
    {
    case 1:
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;
        break;
    case 2:
        aspect = 8.f / 9.f;
        viewport.width /= 2.f;
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;
        viewport.left = viewport.width;
        m_cameras[1].getComponent<cro::Camera>().viewport = viewport;

        //add the screen split
        {
            auto& verts = m_splitScreenNode.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, 0.f), cro::Colour::Black),
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y), cro::Colour::Black),
            };
            m_splitScreenNode.getComponent<cro::Drawable2D>().updateLocalBounds();
        }


        break;

    case 3:
    case 4:
        viewport.width /= 2.f;
        viewport.height /= 2.f;
        viewport.bottom += viewport.height;
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;

        viewport.left = viewport.width;
        m_cameras[1].getComponent<cro::Camera>().viewport = viewport;

        viewport.left = 0.f;
        viewport.bottom -= viewport.height;
        m_cameras[2].getComponent<cro::Camera>().viewport = viewport;

        if (m_cameras.size() == 4)
        {
            viewport.left = viewport.width;
            m_cameras[3].getComponent<cro::Camera>().viewport = viewport;
        }

        //add the screen split
        {
            auto& verts = m_splitScreenNode.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, 0.f), cro::Colour::Black),
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y), cro::Colour::Black),

                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y), cro::Colour::Transparent),
                cro::Vertex2D(glm::vec2(0.f, cro::DefaultSceneSize.y / 2.f), cro::Colour::Transparent),

                cro::Vertex2D(glm::vec2(0.f, cro::DefaultSceneSize.y / 2.f), cro::Colour::Black),
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x, cro::DefaultSceneSize.y / 2.f), cro::Colour::Black),
            };
            m_splitScreenNode.getComponent<cro::Drawable2D>().updateLocalBounds();
        }


        break;

    default:
        break;
    }

    //set up projection
    for (auto cam : m_cameras)
    {
        cam.getComponent<cro::Camera>().setPerspective(fov, aspect, nearPlane, farPlane);
    }



    //update the UI camera
    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();

    cam.setOrthographic(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;

}