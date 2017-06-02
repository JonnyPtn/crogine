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

#include "GameState.hpp"
#include "ResourceIDs.hpp"
#include "BackgroundShader.hpp"
#include "BackgroundSystem.hpp"
#include "PostRadial.hpp"
#include "RotateSystem.hpp"
#include "TerrainChunk.hpp"
#include "ChunkBuilder.hpp"
#include "Messages.hpp"
#include "RockFallSystem.hpp"
#include "RandomTranslation.hpp"
#include "VelocitySystem.hpp"
#include "PlayerDirector.hpp"
#include "BackgroundDirector.hpp"
#include "ItemSystem.hpp"
#include "ItemsDirector.hpp"
#include "NPCSystem.hpp"
#include "NpcDirector.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/CommandID.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    BackgroundSystem* backgroundController = nullptr;
    const glm::vec2 backgroundSize(21.3f, 7.2f);
    std::size_t rockfallCount = 2;

    cro::Skeleton choppaSkel;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_scene             (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
    });
    //context.appInstance.setClearColour(cro::Colour::White());
    //context.mainWindow.setVsyncEnabled(false);

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_SPACE)
        {
            backgroundController->setMode(BackgroundSystem::Mode::Shake);
        }
        else if (evt.key.keysym.sym == SDLK_RETURN)
        {
            backgroundController->setMode(BackgroundSystem::Mode::Scroll);
            backgroundController->setScrollSpeed(0.2f);
        }
    }

    m_scene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
}

bool GameState::simulate(cro::Time dt)
{
    m_scene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_scene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::SceneGraph>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    backgroundController = &m_scene.addSystem<BackgroundSystem>(mb);
    backgroundController->setScrollSpeed(0.2f);
    m_scene.addSystem<ChunkSystem>(mb);
    m_scene.addSystem<RockFallSystem>(mb);
    m_scene.addSystem<RotateSystem>(mb);
    m_scene.addSystem<cro::ParticleSystem>(mb);
    m_scene.addSystem<Translator>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<VelocitySystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<ItemSystem>(mb);
    m_scene.addSystem<NpcSystem>(mb);

    m_scene.addDirector<PlayerDirector>();
    m_scene.addDirector<BackgroundDirector>();
    m_scene.addDirector<ItemDirector>();
    m_scene.addDirector<NpcDirector>();

    m_scene.addPostProcess<PostRadial>();
}

void GameState::loadAssets()
{
    m_shaderResource.preloadFromString(Shaders::Background::Vertex, Shaders::Background::Fragment, ShaderID::Background);
    auto& farTexture = m_textureResource.get("assets/materials/background_far.png");
    farTexture.setRepeated(true);
    farTexture.setSmooth(true);
    auto& farMaterial = m_materialResource.add(MaterialID::GameBackgroundFar, m_shaderResource.get(ShaderID::Background));
    farMaterial.setProperty("u_diffuseMap", farTexture);

    auto& midTexture = m_textureResource.get("assets/materials/background_mid.png");
    midTexture.setRepeated(true);
    midTexture.setSmooth(true);
    auto& midMaterial = m_materialResource.add(MaterialID::GameBackgroundMid, m_shaderResource.get(ShaderID::Background));
    midMaterial.setProperty("u_diffuseMap", midTexture);
    midMaterial.blendMode = cro::Material::BlendMode::Alpha;

    auto& nearTexture = m_textureResource.get("assets/materials/background_near.png");
    nearTexture.setRepeated(true);
    nearTexture.setSmooth(true);
    auto& nearMaterial = m_materialResource.add(MaterialID::GameBackgroundNear, m_shaderResource.get(ShaderID::Background));
    nearMaterial.setProperty("u_diffuseMap", nearTexture);
    nearMaterial.blendMode = cro::Material::BlendMode::Alpha;

    cro::QuadBuilder qb(backgroundSize);
    m_meshResource.loadMesh(MeshID::GameBackground, qb);

    auto shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::BuiltIn::VertexLit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::NormalMap);
    auto& playerMaterial = m_materialResource.add(MaterialID::PlayerShip, m_shaderResource.get(shaderID));
    playerMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/player_diffuse.png"));
    playerMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/player_mask.png"));
    playerMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/player_normal.png"));

    cro::StaticMeshBuilder playerMesh("assets/models/player_ship.cmf");
    m_meshResource.loadMesh(MeshID::PlayerShip, playerMesh);

    auto& eliteMaterial = m_materialResource.add(MaterialID::NPCElite, m_shaderResource.get(shaderID));
    eliteMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/npc/elite_diffuse.png"));
    eliteMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/npc/elite_mask.png"));
    eliteMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/npc/elite_normal.png"));

    cro::StaticMeshBuilder eliteMesh("assets/models/elite.cmf");
    m_meshResource.loadMesh(MeshID::NPCElite, eliteMesh);


    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap);
    auto& turrBaseMat = m_materialResource.add(MaterialID::NPCTurretBase, m_shaderResource.get(shaderID));
    turrBaseMat.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/npc/turret_base_diffuse.png"));
    turrBaseMat.setProperty("u_maskMap", m_textureResource.get("assets/materials/npc/turret_base_mask.png"));

    auto& turrCanMat = m_materialResource.add(MaterialID::NPCTurretCannon, m_shaderResource.get(shaderID));
    turrCanMat.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/npc/turret_cannon_diffuse.png"));
    turrCanMat.setProperty("u_maskMap", m_textureResource.get("assets/materials/npc/turret_cannon_mask.png"));

    cro::StaticMeshBuilder turretBase("assets/models/turret_base.cmf");
    m_meshResource.loadMesh(MeshID::NPCTurretBase, turretBase);

    cro::StaticMeshBuilder turretCannon("assets/models/turret_cannon.cmf");
    m_meshResource.loadMesh(MeshID::NPCTurretCannon, turretCannon);


    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::Skinning);
    auto& choppaMat = m_materialResource.add(MaterialID::NPCChoppa, m_shaderResource.get(shaderID));
    choppaMat.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/npc/choppa_diffuse.png"));
    choppaMat.setProperty("u_maskMap", m_textureResource.get("assets/materials/npc/choppa_mask.png"));

    cro::IqmBuilder choppaMesh("assets/models/choppa_pod.iqm");
    m_meshResource.loadMesh(MeshID::NPCChoppa, choppaMesh);
    choppaSkel = choppaMesh.getSkeleton();
    choppaSkel.animations[0].playing = true;

    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::BuiltIn::Unlit, cro::ShaderResource::VertexColour);
    m_materialResource.add(MaterialID::TerrainChunk, m_shaderResource.get(shaderID));

    ChunkBuilder chunkBuilder;
    m_meshResource.loadMesh(MeshID::TerrainChunkA, chunkBuilder);
    m_meshResource.loadMesh(MeshID::TerrainChunkB, chunkBuilder);


    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::Subrects);
    for (auto i = 0u; i < rockfallCount; ++i)
    {
        auto& rockMat = m_materialResource.add(MaterialID::Rockfall + i, m_shaderResource.get(shaderID));
        rockMat.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/npc/tites.png"));
        rockMat.setProperty("u_subrect", glm::vec4(0.f, 0.f, 0.25f, 1.f));
        rockMat.blendMode = cro::Material::BlendMode::Alpha;
    }
    cro::QuadBuilder rockQuad({ 1.f, 1.f });
    m_meshResource.loadMesh(MeshID::RockQuad, rockQuad);

    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::NormalMap | cro::ShaderResource::Subrects);
    auto& battMaterial = m_materialResource.add(MaterialID::BattCollectable, m_shaderResource.get(shaderID));
    battMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/collectables/collectables01_diffuse.png"));
    battMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/collectables/collectables01_mask.png"));
    battMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/collectables/collectables01_normal.png"));
    battMaterial.setProperty("u_subrect", glm::vec4(0.f, 0.5f, 0.5f, 0.5f));

    auto& bombMaterial = m_materialResource.add(MaterialID::BombCollectable, m_shaderResource.get(shaderID));
    bombMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/collectables/collectables01_diffuse.png"));
    bombMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/collectables/collectables01_mask.png"));
    bombMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/collectables/collectables01_normal.png"));
    bombMaterial.setProperty("u_subrect", glm::vec4(0.5f, 0.5f, 0.5f, 0.5f));

    auto& botMaterial = m_materialResource.add(MaterialID::BotCollectable, m_shaderResource.get(shaderID));
    botMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/collectables/collectables01_diffuse.png"));
    botMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/collectables/collectables01_mask.png"));
    botMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/collectables/collectables01_normal.png"));
    botMaterial.setProperty("u_subrect", glm::vec4(0.f, 0.f, 0.5f, 0.5f));

    auto& heartMaterial = m_materialResource.add(MaterialID::HeartCollectable, m_shaderResource.get(shaderID));
    heartMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/collectables/collectables01_diffuse.png"));
    heartMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/collectables/collectables01_mask.png"));
    heartMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/collectables/collectables01_normal.png"));
    heartMaterial.setProperty("u_subrect", glm::vec4(0.5f, 0.f, 0.5f, 0.5f));

    auto& shieldMaterial = m_materialResource.add(MaterialID::ShieldCollectable, m_shaderResource.get(shaderID));
    shieldMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/collectables/collectables02_diffuse.png"));
    shieldMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/collectables/collectables02_mask.png"));
    shieldMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/collectables/collectables02_normal.png"));
    shieldMaterial.setProperty("u_subrect", glm::vec4(0.f, 0.5f, 0.5f, 0.5f));

    cro::StaticMeshBuilder collectableMesh("assets/models/collectable.cmf");
    m_meshResource.loadMesh(MeshID::Collectable, collectableMesh);
}

void GameState::createScene()
{
    //background layers
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -18.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundFar));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -14.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundMid));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -11.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundNear));
    entity.addComponent<BackgroundComponent>();

    //terrain chunks
    entity = m_scene.createEntity();
    auto& chunkTxA = entity.addComponent<cro::Transform>();
    chunkTxA.setPosition({ 0.f, 0.f, -8.8f });
    //chunkTxA.setScale({ 0.5f, 0.5f, 1.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::TerrainChunkA), m_materialResource.get(MaterialID::TerrainChunk));
    entity.addComponent<TerrainChunk>();

    auto chunkEntityA = entity; //keep this so we can attach turrets to it

    entity = m_scene.createEntity();
    auto& chunkTxB = entity.addComponent<cro::Transform>();
    chunkTxB.setPosition({ backgroundSize.x, 0.f, -8.8f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::TerrainChunkB), m_materialResource.get(MaterialID::TerrainChunk));
    entity.addComponent<TerrainChunk>();

    auto chunkEntityB = entity;

    //some rockfall parts
    for (auto i = 0u; i < rockfallCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<RockFall>();
        auto& tx = entity.addComponent<cro::Transform>();
        tx.setScale({ 0.6f, 1.2f, 1.f });
        tx.setPosition({ 0.f, 3.4f, -9.1f });

        entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RockQuad), m_materialResource.get(MaterialID::Rockfall + i));
    }

    //player ship
    entity = m_scene.createEntity();
    auto& playerTx = entity.addComponent<cro::Transform>();
    playerTx.setPosition({ -3.4f, 0.f, -9.25f });
    playerTx.setScale({ 0.6f, 0.6f, 0.6f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::PlayerShip), m_materialResource.get(MaterialID::PlayerShip));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
    entity.addComponent<Velocity>().friction = 2.5f;
    //auto& rotator = entity.addComponent<Rotator>();
    //rotator.axis.x = 1.f;
    //rotator.speed = 1.f;


    //collectables
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.41f, 1.4f, -9.3f });
    entity.getComponent<cro::Transform>().setScale({ 0.25f, 0.25f, 0.25f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Collectable), m_materialResource.get(MaterialID::BattCollectable));
    auto& battSpin = entity.addComponent<Rotator>();
    battSpin.axis.y = 1.f;
    battSpin.speed = 3.2f;
    entity.addComponent<CollectableItem>().type = CollectableItem::EMP;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.42f, 0.6f, -9.3f });
    entity.getComponent<cro::Transform>().setScale({ 0.25f, 0.25f, 0.25f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Collectable), m_materialResource.get(MaterialID::BombCollectable));
    auto& bombSpin = entity.addComponent<Rotator>();
    bombSpin.axis.y = 1.f;
    bombSpin.speed = 2.9f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Bomb;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.6f, -0.2f, -9.3f });
    entity.getComponent<cro::Transform>().setScale({ 0.25f, 0.25f, 0.25f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Collectable), m_materialResource.get(MaterialID::BotCollectable));
    auto& botSpin = entity.addComponent<Rotator>();
    botSpin.axis.y = 1.f;
    botSpin.speed = 2.994f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Buddy;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.38f, -1.f, -9.3f });
    entity.getComponent<cro::Transform>().setScale({ 0.25f, 0.25f, 0.25f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Collectable), m_materialResource.get(MaterialID::HeartCollectable));
    auto& heartSpin = entity.addComponent<Rotator>();
    heartSpin.axis.y = 1.f;
    heartSpin.speed = 2.873f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Life;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.4f, -1.7f, -9.3f });
    entity.getComponent<cro::Transform>().setScale({ 0.25f, 0.25f, 0.25f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Collectable), m_materialResource.get(MaterialID::ShieldCollectable));
    auto& shieldSpin = entity.addComponent<Rotator>();
    shieldSpin.axis.y = 1.f;
    shieldSpin.speed = 3.028f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Shield;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;


    //NPCs
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.9f, 1.5f, -9.3f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::NPCElite), m_materialResource.get(MaterialID::NPCElite));
    entity.addComponent<Npc>().type = Npc::Elite;
    
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.9f, 0.5f, -9.3f });
    entity.getComponent<cro::Transform>().setRotation({ -cro::Util::Const::PI / 2.f, 0.f, 0.f });
    //entity.getComponent<cro::Transform>().setScale({ 0.02f, 0.02f, 0.02f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::NPCChoppa), m_materialResource.get(MaterialID::NPCChoppa));
    entity.addComponent<cro::Skeleton>() = choppaSkel;
    entity.addComponent<Npc>().type = Npc::Choppa;

    //attach turret to each of the terrain chunks
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 0.f, 0.f }); //places off screen to start
    entity.getComponent<cro::Transform>().setScale({ 0.3f, 0.3f, 0.3f });
    entity.getComponent<cro::Transform>().setParent(chunkEntityA); //attach to scenery
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::NPCTurretBase), m_materialResource.get(MaterialID::NPCTurretBase));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Turret;
    
    auto canEnt = m_scene.createEntity();
    canEnt.addComponent<cro::Transform>().setParent(entity);
    canEnt.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::NPCTurretCannon), m_materialResource.get(MaterialID::NPCTurretCannon));
    canEnt.addComponent<Npc>().type = Npc::Turret;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 0.f, 0.f });
    entity.getComponent<cro::Transform>().setScale({ 0.3f, 0.3f, 0.3f });
    entity.getComponent<cro::Transform>().setParent(chunkEntityB); //attach to scenery
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::NPCTurretBase), m_materialResource.get(MaterialID::NPCTurretBase));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Turret;

    canEnt = m_scene.createEntity();
    canEnt.addComponent<cro::Transform>().setParent(entity);
    canEnt.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::NPCTurretCannon), m_materialResource.get(MaterialID::NPCTurretCannon));
    canEnt.addComponent<Npc>().type = Npc::Turret;


    //particle systems
    entity = m_scene.createEntity();
    auto& snowEmitter = entity.addComponent<cro::ParticleEmitter>();
    auto& settings = snowEmitter.emitterSettings;
    settings.emitRate = 30.f;
    settings.initialVelocity = { -2.4f, -0.1f, 0.f };
    settings.gravity = { 0.f, -1.f, 0.f };
    settings.colour = cro::Colour::White();
    settings.lifetime = 5.f;
    settings.rotationSpeed = 5.f;
    settings.size = 0.03f;
    settings.spawnRadius = 0.8f;
    settings.textureID = m_textureResource.get("assets/particles/snowflake.png").getGLHandle();
    settings.blendmode = cro::EmitterSettings::Add;

    snowEmitter.start();
    entity.addComponent<cro::Transform>();
    auto& translator = entity.addComponent<RandomTranslation>();
    for (auto& p : translator.translations)
    {
        p.x = cro::Util::Random::value(-3.5f, 12.1f);
        p.y = 3.1f;
        p.z = -9.2f;
    }
    entity.addComponent<cro::CommandTarget>().ID = CommandID::SnowParticles;

    //rock fragments from ceiling
    entity = m_scene.createEntity();
    auto& rockEmitter = entity.addComponent<cro::ParticleEmitter>();
    auto& rockSettings = rockEmitter.emitterSettings;
    rockSettings.emitRate = 4.f;
    rockSettings.initialVelocity = {};
    rockSettings.lifetime = 2.f;
    rockSettings.rotationSpeed = 8.f;
    rockSettings.size = 0.06f;
    rockSettings.textureID = m_textureResource.get("assets/particles/rock_fragment.png").getGLHandle();
    rockSettings.blendmode = cro::EmitterSettings::Alpha;
    rockSettings.gravity = { 0.f, -9.f, 0.f };

    entity.addComponent<cro::Transform>();
    auto& rockTrans = entity.addComponent<RandomTranslation>();
    for (auto& p : rockTrans.translations)
    {
        p.x = cro::Util::Random::value(-4.2f, 4.2f);
        p.y = 3.f;
        p.z = -8.6f;
    }
    entity.addComponent<cro::CommandTarget>().ID = CommandID::RockParticles;


    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Camera>();
    m_scene.setActiveCamera(ent);
}

void GameState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projection = glm::perspective(0.6f, 16.f / 9.f, 0.1f, 100.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    /*auto& cam2D = m_menuScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;*/
}