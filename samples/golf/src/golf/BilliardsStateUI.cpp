/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "BilliardsState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "../GolfGame.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/graphics/ModelDefinition.hpp>

namespace
{
#include "PaletteSwap.inl"

    constexpr glm::vec3 TopSpinPosition(50.f, 0.f, 0.f);
    constexpr glm::vec3 TargetBallPosition(100.f, 0.f, 0.f);
    constexpr glm::vec3 CamOffset(0.f, 0.f, 0.06f);
}

void BilliardsState::createUI()
{
    createMiniballScenes();
    updateTargetTexture(0, -1);
    updateTargetTexture(1, -1);

    auto bufferUpdateCallback = [](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };

    //displays the background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.5f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_gameSceneTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;

    auto backgroundEnt = entity;

    /*if (m_gameSceneShader.loadFromString(PaletteSwapVertex, PaletteSwapFragment)
        && m_lutTexture.loadFromFile("assets/golf/images/lut.png"))
    {
        auto uniformID = m_gameSceneShader.getUniformID("u_palette");

        if (uniformID != -1)
        {
            entity.getComponent<cro::Drawable2D>().setShader(&m_gameSceneShader);
            entity.getComponent<cro::Drawable2D>().bindUniform("u_palette", m_lutTexture);
        }
    }*/

    //displays the topspin ball
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_topspinTexture.getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    auto topspinEnt = entity;


    //displays the player's current target ball
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_targetTextures[0].getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    auto targetEnt0 = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_targetTextures[1].getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    auto targetEnt1 = entity;


    //attach UI elements to this so they scale to their parent
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({0.f, 0.f, 0.1f});
    auto rootNode = entity;


    //player names
    auto& menuFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    
    auto createText = [&](const std::string& str, glm::vec2 relPos)
    {
        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>();
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Text>(menuFont).setString(str);
        ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
        ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
        ent.addComponent<UIElement>().relativePosition = relPos;
        ent.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;

        centreText(ent);
        rootNode.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };

    static constexpr float NameVertPos = 0.95f;
    static constexpr float NameHorPos = 0.13f;
    createText(m_sharedData.connectionData[0].playerData[0].name, glm::vec2(NameHorPos, NameVertPos));

    if (m_sharedData.connectionData[0].playerCount == 2)
    {
        createText(m_sharedData.connectionData[0].playerData[1].name, glm::vec2(1.f - NameHorPos, NameVertPos));
    }
    else
    {
        createText(m_sharedData.connectionData[1].playerData[0].name, glm::vec2(1.f - NameHorPos, NameVertPos));
    }

    CommandID::UI::StrengthMeter;
    //also mouse cursor for rotating


    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, backgroundEnt, topspinEnt, targetEnt0, targetEnt1](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        backgroundEnt.getComponent<cro::Transform>().setScale(courseScale);
        backgroundEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        backgroundEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -5.f));

        topspinEnt.getComponent<cro::Transform>().setScale(courseScale);
        topspinEnt.getComponent<cro::Callback>().active = true;
        topspinEnt.getComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y * NameVertPos, -0.3f });

        targetEnt0.getComponent<cro::Transform>().setScale(courseScale);
        targetEnt0.getComponent<cro::Callback>().active = true;
        targetEnt0.getComponent<cro::Transform>().setPosition({ size.x * 0.425f, size.y * NameVertPos, -0.3f });

        targetEnt1.getComponent<cro::Transform>().setScale(courseScale);
        targetEnt1.getComponent<cro::Callback>().active = true;
        targetEnt1.getComponent<cro::Transform>().setPosition({ size.x * 0.575f, size.y * NameVertPos, -0.3f });

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void BilliardsState::showReadyNotify(const BilliardsPlayer& player)
{
    m_wantsNotify = (player.client == m_sharedData.localConnectionData.connectionID);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 400.f, 300.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.connectionData[player.client].playerData[player.player].name + "'s Turn");
    entity.addComponent<UIElement>().relativePosition= glm::vec2(0.5f);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard | CommandID::UI::UIElement;

    if (m_wantsNotify)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 400.f, 200.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("Press Button"); //TODO read controller/keyboard status for more meaningful string
        entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 0.33f);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard | CommandID::UI::UIElement;
    }
}

void BilliardsState::showGameEnd(const BilliardsPlayer& player)
{
    //this may be from a player quitting mid-summary
    if (!m_gameEnded)
    {
        //show summary screen

        auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 400.f, 300.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("Game Ended");


        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 200.f, 10.f, 0.23f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::uint8_t>>(1.f, ConstVal::SummaryTimeout);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto& [current, sec] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::uint8_t>>();
            current -= dt;
            if (current < 0)
            {
                current += 1.f;
                sec--;
            }

            e.getComponent<cro::Text>().setString("Returning to lobby in: " + std::to_string(sec));

            auto bounds = cro::Text::getLocalBounds(e);
            bounds.width = std::floor(bounds.width / 2.f);
            e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        };


        m_gameEnded = true;
    }
}

void BilliardsState::toggleQuitReady()
{
    if (m_gameEnded)
    {
        m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::ReadyQuit, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void BilliardsState::createMiniballScenes()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/hole_19/topspin.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(TopSpinPosition);
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material);

        glm::vec4 rect;
        rect.x = 0.f;
        rect.y = 0.f;
        rect.z = 1.f;
        rect.w = 1.f;
        material.setProperty("u_subrect", rect);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setRotation(m_inputParser.getSpinOffset());
        };
    }

    auto camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(TopSpinPosition + CamOffset);

    auto& cam = camEnt.addComponent<cro::Camera>();
    auto resizeCallback = [&](cro::Camera& cam)
    {
        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, 1.f, 0.01f, 0.5f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    cam.resizeCallback = resizeCallback;
    cam.isStatic = true;
    resizeCallback(cam);

    m_topspinCamera = camEnt;


    //renders the player's current target ball
    if (md.loadFromFile("assets/golf/models/hole_19/billiard_ball.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(TargetBallPosition);
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        m_targetBall = entity;
    }

    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(TargetBallPosition + CamOffset);
    auto& cam2 = camEnt.addComponent<cro::Camera>();
    cam2.resizeCallback = resizeCallback;
    cam2.isStatic = true;
    resizeCallback(cam2);

    m_targetCamera = camEnt;
}

void BilliardsState::updateTargetTexture(std::int32_t playerIndex, std::int8_t ballID)
{
    m_targetTextures[playerIndex].clear(cro::Colour::Transparent);
    if (ballID > 0 && m_targetBall.isValid())
    {
        //set material UV
        auto rect = getSubrect(ballID);
        m_targetBall.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);

        auto oldCam = m_gameScene.setActiveCamera(m_targetCamera);
        m_gameScene.render();
        m_gameScene.setActiveCamera(oldCam);
    }
    m_targetTextures[playerIndex].display();
}
