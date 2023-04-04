/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "MenuState.hpp"
#include "MenuCallbacks.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "Utility.hpp"
#include "NameScrollSystem.hpp"
#include "CommandIDs.hpp"
#include "spooky2.hpp"
#include "Clubs.hpp"
#include "UnlockItems.hpp"
#include "../ErrorCheck.hpp"
#include "server/ServerPacketData.hpp"

#include <AchievementStrings.hpp>
#include <Social.hpp>
#include <Input.hpp>

#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/audio/AudioScape.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/OpenGL.hpp>

#include <cstring>
#include <iomanip>

namespace
{
#include "RandNames.hpp"
#include "PostProcess.inl"

    struct CursorAnimationCallback final
    {
        static std::vector<float> WaveTable;
        std::size_t index = 0;

        void operator()(cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().move({ WaveTable[index] * dt, 0.f });
            index = (index + 1) % WaveTable.size();
        }

        CursorAnimationCallback()
        {
            if (WaveTable.empty())
            {
                WaveTable = cro::Util::Wavetable::sine(3.f, 40.f);
            }
        }
    };
    std::vector<float> CursorAnimationCallback::WaveTable;
}

constexpr std::array<glm::vec2, MenuState::MenuID::Count> MenuState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, 0.f),
    glm::vec2(0.f, 0.f)
};

void MenuState::parseCourseDirectory(const std::string& rootDir, bool isUser)
{
    auto root = rootDir;
    if (!isUser)
    {
        //macOS shenanigans.
        root = cro::FileSystem::getResourcePath() + root;
    }

    auto directories = cro::FileSystem::listDirectories(root);

    //at least be consistent across platforms
    std::sort(directories.begin(), directories.end(), [](const  std::string& a, const std::string& b) {return a < b; });

    m_courseIndices[m_currentRange].start = m_courseData.size();

    std::int32_t courseNumber = 1;
    for (const auto& dir : directories)
    {
        if (dir == "tutorial")
        {
            continue;
        }

        auto courseFile = rootDir + dir + "/course.data";

        //because macs are special, obvs
        auto testPath = courseFile;
        if (!isUser)
        {
            testPath = cro::FileSystem::getResourcePath() + testPath;
        }

        if (cro::FileSystem::fileExists(testPath))
        {
            cro::String title;
            std::string description;
            std::int32_t holeCount = 0;

            cro::ConfigFile cfg;
            cfg.loadFromFile(courseFile, !isUser);

            const auto& props = cfg.getProperties();
            for (const auto& prop : props)
            {
                const auto& propName = prop.getName();
                if (propName == "description")
                {
                    description = prop.getValue<std::string>();
                }
                else if (propName == "hole")
                {
                    holeCount++;
                }
                else if (propName == "title")
                {
                    title = prop.getValue<cro::String>();
                }
                //TODO we could validate the hole files exist
                //but that's done when loading the game anyway
                //here we just want some lobby info and an
                //early indication if a client is missing the
                //data the host has
            }

            if (holeCount > 0)
            {
                auto& data = m_courseData.emplace_back();
                if (!title.empty())
                {
                    data.title = std::to_string(courseNumber) + ". " + title;
                }
                if (!description.empty())
                {
                    data.description = description;
                }
                data.directory = dir;
                data.isUser = isUser;
                data.holeCount[0] = "All " + std::to_string(std::min(holeCount, 18)) + " holes";
                data.holeCount[1] = "Front " + std::to_string(std::max(holeCount / 2, 1));
                data.holeCount[2] = "Back " + std::to_string(std::min(holeCount - (holeCount / 2), 9));

                courseNumber++;
                m_courseIndices[m_currentRange].count++;
            }
        }


        //check for thumbnail
        courseFile = rootDir + dir + "/preview.png";
        testPath = courseFile;
        if (!isUser)
        {
            testPath = cro::FileSystem::getResourcePath() + testPath;
        }

        std::unique_ptr<cro::Texture> t = std::make_unique<cro::Texture>();
        if (cro::FileSystem::fileExists(testPath) &&
            t->loadFromFile(courseFile))
        {
            m_courseThumbs.insert(std::make_pair(dir, std::move(t)));
        }

        //and video thumbnail
        courseFile = rootDir + dir + "/preview.mpg";
        testPath = courseFile;
        if (!isUser)
        {
            testPath = cro::FileSystem::getResourcePath() + testPath;
        }

        if (cro::FileSystem::fileExists(testPath))
        {
            m_videoPaths.insert(std::make_pair(dir, courseFile));
        }
    }

    //moved to createUI() because this func gets called multiple times
    /*if (!m_courseData.empty())
    {
        m_sharedData.courseIndex = std::min(m_sharedData.courseIndex, m_courseData.size() - 1);
    }*/
}

void MenuState::createToolTip()
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font);// .setString("buns");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(cro::Colour::Black);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        auto position = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        position.x = std::floor(position.x);
        position.y = std::floor(position.y);
        position.z = 2.f;

        static constexpr glm::vec3 Offset(8.f, -4.f, 0.f);
        position += (Offset * m_viewScale.x);

        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Transform>().setScale(m_viewScale);
    };
    m_toolTip = entity;
}

void MenuState::showToolTip(const std::string& label)
{
    if (label != m_toolTip.getComponent<cro::Text>().getString())
    {
        m_toolTip.getComponent<cro::Text>().setString(label);
    }

    m_toolTip.getComponent<cro::Callback>().active = true;
}

void MenuState::hideToolTip()
{
    m_toolTip.getComponent<cro::Callback>().active = false;
    m_toolTip.getComponent<cro::Transform>().setPosition(glm::vec3(10000.f));
}

void MenuState::createUI()
{
    m_currentRange = Range::Official;
    parseCourseDirectory(ConstVal::MapPath, false);

    m_currentRange = Range::Custom;
    parseCourseDirectory(cro::App::getPreferencePath() + ConstVal::UserMapPath, true);

    //TODO workshop path


    m_currentRange = Range::Official; //make this default

    if (!m_courseData.empty())
    {
        m_sharedData.courseIndex = std::min(m_sharedData.courseIndex, m_courseData.size() - 1);
    }

    parseAvatarDirectory();
    createToolTip();

    auto mouseEnterCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto mouseExitCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({0.f, 0.f, -0.5f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_backgroundTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;

        /*auto& shader = m_resources.shaders.get(ShaderID::FXAA);
        auto handle = shader.getGLHandle();
        auto uID = shader.getUniformID("u_resolution");
        glCheck(glUseProgram(handle));
        glCheck(glUniform2f(uID, texSize.x, texSize.y));*/
    };
    auto courseEnt = entity;
    /*m_resources.shaders.loadFromString(ShaderID::FXAA, FXAAVertex, FXAAFrag);
    auto& shader = m_resources.shaders.get(ShaderID::FXAA);
    courseEnt.getComponent<cro::Drawable2D>().setShader(&shader);*/

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::RootNode;
    auto rootNode = entity;

    //consumes input during menu animation.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    createMainMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createAvatarMenu(rootNode);
#ifdef USE_GNS
    createBrowserMenu(rootNode, mouseEnterCallback, mouseExitCallback);
#else
    createJoinMenu(rootNode, mouseEnterCallback, mouseExitCallback);
#endif
    createLobbyMenu(rootNode, mouseEnterCallback, mouseExitCallback);

    //diplays version number
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 2.f, 10.f, 1.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Version: " + StringVer);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto versionEnt = entity;

    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt, versionEnt](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(m_menuPositions[m_currentMenu] * m_viewScale);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));

        static constexpr glm::vec2 versionPos(2.f, 10.f);
        versionEnt.getComponent<cro::Transform>().setScale(m_viewScale);
        versionEnt.getComponent<cro::Transform>().setPosition(versionPos * m_viewScale);

        refreshUI();
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void MenuState::createMainMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().direction = MenuData::Out;
    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().currentTime = 1.f;
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Main] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/main_menu.spt", m_resources.textures);

    //just store these here to save reloading the same sprite sheet
    m_sprites[SpriteID::ButtonBanner] = spriteSheet.getSprite("banner_small");
    m_sprites[SpriteID::Cursor] = spriteSheet.getSprite("cursor");
    m_sprites[SpriteID::Flag] = spriteSheet.getSprite("flag");
    m_sprites[SpriteID::AddPlayer] = spriteSheet.getSprite("add_player");
    m_sprites[SpriteID::RemovePlayer] = spriteSheet.getSprite("remove_player");
    m_sprites[SpriteID::PrevMenu] = spriteSheet.getSprite("exit");
    m_sprites[SpriteID::NextMenu] = spriteSheet.getSprite("continue");
    m_sprites[SpriteID::ReadyUp] = spriteSheet.getSprite("ready_up");
    m_sprites[SpriteID::StartGame] = spriteSheet.getSprite("start_game");
    m_sprites[SpriteID::Connect] = spriteSheet.getSprite("connect");


    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.72f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;

    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto titleEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 1.f, bounds.height });
    entity.getComponent<cro::Transform>().move({ 57.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    titleEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), bounds.height, 0.1f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea({0.f, 0.f, 0.f, 0.f});
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("super");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height * 0.7f) });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [bounds, titleEnt](cro::Entity e, float dt)
    {
        if (!titleEnt.getComponent<cro::Callback>().active)
        {
            auto crop = e.getComponent<cro::Drawable2D>().getCroppingArea();
            crop.height = bounds.height;
            crop.width = std::min(bounds.width, crop.width + (bounds.width * dt));
            e.getComponent<cro::Drawable2D>().setCroppingArea(crop);

            if (crop.width == bounds.width)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
    };
    titleEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //menu text background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 26.f, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("banner");
    auto textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 0);
    entity.getComponent<cro::Callback>().function =
        [&, textureRect](cro::Entity e, float dt)
    {
        auto& [currTime, state] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (state == 0)
        {
            //intro anim
            currTime = std::min(1.f, currTime + dt);
            float scale = cro::Util::Easing::easeOutQuint(currTime);
            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f, scale));

            auto rect = textureRect;
            rect.width = currTime * (static_cast<float>(GolfGame::getActiveTarget()->getSize().x) / m_viewScale.x);
            e.getComponent<cro::Sprite>().setTextureRect(rect);

            if (currTime == 1)
            {
                state = 1;
                e.getComponent<cro::Callback>().active = false;

                //only set this if we're not already connected
                //else we'll be going straight to the lobby
                if (!m_sharedData.clientConnection.connected)
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                }
            }
        }
        else
        {
            //fit to size
            auto rect = textureRect;
            rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) / m_viewScale.x;
            e.getComponent<cro::Sprite>().setTextureRect(rect);
            e.getComponent<cro::Callback>().active = false;
        }
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto bannerEnt = entity;

    //golf cart
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 4.f, 0.01f });
    entity.getComponent<cro::Transform>().setScale({ -1.f, 1.f });
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cart");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
    entity.getComponent<cro::Callback>().function =
        [bannerEnt](cro::Entity e, float dt)
    {
        auto& dir = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        switch (dir)
        {
        case 0:
        {
            auto width = e.getComponent<cro::Sprite>().getTextureRect().width;
            auto position = e.getComponent<cro::Transform>().getPosition();
            position.x = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width + width;
            e.getComponent<cro::Transform>().setPosition(position);

            if (!bannerEnt.getComponent<cro::Callback>().active)
            {
                //animation stopped
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                dir = 2;
            }
        }
        break;
        case 1:

            break;
        case 2:
        {
            float width = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width;
            width *= 0.7f;

            auto position = e.getComponent<cro::Transform>().getPosition();
            float diff = width - position.x;
            e.getComponent<cro::Transform>().move({ diff * dt, 0.f });
        }
        break;
        }
    };
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    //auto cartEnt = entity;

    //banner header
    bounds = spriteSheet.getSprite("header").getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ textureRect.width / 2.f, textureRect.height - bounds.height, -0.1f });
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("header");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [bannerEnt, textureRect](cro::Entity e, float dt)
    {
        float width = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width;
        auto position = e.getComponent<cro::Transform>().getPosition();
        position.x = width / 2.f;

        if (!bannerEnt.getComponent<cro::Callback>().active)
        {
            float diff = textureRect.height - position.y;
            position.y += diff * (dt * 2.f);
        }

        e.getComponent<cro::Transform>().setPosition(position);
    };
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //banner footer
    bounds = spriteSheet.getSprite("footer").getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ textureRect.width / 2.f, 0.f, -0.3f });
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("footer");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [bannerEnt, bounds](cro::Entity e, float dt)
    {
        float width = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width;
        auto position = e.getComponent<cro::Transform>().getPosition();
        position.x = width / 2.f;

        if (!bannerEnt.getComponent<cro::Callback>().active)
        {
            float diff = -bounds.height - position.y;
            position.y += diff * (dt * 2.f);
        }

        e.getComponent<cro::Transform>().setPosition(position);
    };
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -100.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cursor");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + glm::vec3(-20.f, -7.f, 0.f));
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        });

    static constexpr float TextOffset = 26.f;
    static constexpr float LineSpacing = 10.f;
    glm::vec3 textPos = { TextOffset, 54.f, 0.1f };

    auto createButton = [&](const std::string& label)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(textPos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(label);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;

        bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        textPos.y -= LineSpacing;

        return entity;
    };


    if (!m_courseData.empty()
        && !m_sharedData.ballInfo.empty()
        && ! m_sharedData.avatarInfo.empty())
    {
        //host
        entity = createButton("Create Game");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        m_sharedData.hosting = true;

                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                        menuEntity.getComponent<cro::Callback>().active = true;

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });


        //join
        entity = createButton("Join Game");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        m_sharedData.hosting = false;

                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                        menuEntity.getComponent<cro::Callback>().active = true;

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });

        //facilities menu
        entity = createButton("19th Hole");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        requestStackPush(StateID::Practice);
                        //m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });
    }
    else
    {
        std::string str = "Error:\n";
        if (m_courseData.empty())
        {
            str += "No course data found\n";
        }
        if (m_sharedData.ballInfo.empty())
        {
            str += "Missing Ball Data\n";
        }
        if (m_sharedData.avatarInfo.empty())
        {
            str += "Missing Avatar Data";
        }

        //display error
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(textPos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(str);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);

        bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        textPos.y -= LineSpacing * 3.f;
    }

    //options
    entity = createButton("Options");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackPush(StateID::Options);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });


    //quit
    entity = createButton("Quit");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::App::quit();
                }
            });

}

void MenuState::createJoinMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Join] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Join]);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/connect_menu.spt", m_resources.textures);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //ip text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 200.f, -16.f });
    entity.addComponent<cro::Text>(font).setString(m_sharedData.targetIP);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //add a cursor to the end of the string when active
        cro::String str = m_sharedData.targetIP;
        if (str.size() < ConstVal::MaxIPChars)
        {
            str += "_";
        }
        e.getComponent<cro::Text>().setString(str);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    auto highlight = m_uiScene.createEntity();
    highlight.addComponent<cro::Transform>().setPosition({ 11.f, 16.f, 0.1f });
    highlight.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    highlight.addComponent<cro::Drawable2D>();
    highlight.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight");

    auto balls = m_uiScene.createEntity();
    balls.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    balls.addComponent<cro::Drawable2D>();
    balls.addComponent<cro::Sprite>() = spriteSheet.getSprite("bounce");
    balls.addComponent<cro::SpriteAnimation>().play(0);
    bounds = balls.getComponent<cro::Sprite>().getTextureBounds();
    balls.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });


    //box background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = -0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([highlight](cro::Entity) mutable { highlight.getComponent<cro::Sprite>().setColour(cro::Colour::White); highlight.getComponent<cro::AudioEmitter>().play(); });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([highlight](cro::Entity) mutable { highlight.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        beginTextEdit(textEnt, &m_sharedData.targetIP, ConstVal::MaxIPChars);
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                        if (evt.type == SDL_CONTROLLERBUTTONUP)
                        {
                            requestStackPush(StateID::Keyboard);
                        }
                    }
                    else
                    {
                        applyTextEdit();
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    textEnt.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
    textEnt.getComponent<cro::Transform>().move({ -60.f, -12.f, 0.1f });
    balls.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
    balls.getComponent<cro::Transform>().move({ 0.f, 0.f, 0.1f });
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(balls.getComponent<cro::Transform>());
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //banner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ButtonBanner];
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteRect](cro::Entity e, float)
    {
        auto rect = spriteRect;
        rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
        e.getComponent<cro::Sprite>().setTextureRect(rect);
        e.getComponent<cro::Callback>().active = false;
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto mouseEnterHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left -= bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -40.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Connect];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit(); //finish any pending changes
                    refreshUI();

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    if (!m_sharedData.targetIP.empty() &&
                        !m_sharedData.clientConnection.connected)
                    {
                        m_matchMaking.joinGame(0);
                    }

                    auto defaultCallback = e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown];
                    e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = 0;

                    auto tempEnt = m_uiScene.createEntity();
                    tempEnt.addComponent<cro::Callback>().active = true;
                    tempEnt.getComponent<cro::Callback>().setUserData<std::pair<std::uint32_t, float>>(defaultCallback, 0.f);
                    tempEnt.getComponent<cro::Callback>().function =
                        [&, e](cro::Entity t, float dt) mutable
                    {
                        auto& [cb, currTime] = t.getComponent<cro::Callback>().getUserData<std::pair<std::uint32_t, float>>();
                        currTime += dt;
                        if (currTime > 1)
                        {
                            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = cb;
                            t.getComponent<cro::Callback>().active = false;
                            m_uiScene.destroyEntity(t);
                        }
                    };
                }
            });
    
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createBrowserMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Join] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Join]);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_browser.spt", m_resources.textures);


    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.55f };
    entity.getComponent<UIElement>().depth = -0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.rootNode = entity;


    //banner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ButtonBanner];
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteRect](cro::Entity e, float)
    {
        auto rect = spriteRect;
        rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
        e.getComponent<cro::Sprite>().setTextureRect(rect);
        e.getComponent<cro::Callback>().active = false;
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto mouseEnterHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left -= bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });

    auto arrowSelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&,entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto arrowUnselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto lobbyActivated = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                std::size_t idx = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
                idx += (LobbyPager::ItemsPerPage * m_lobbyPager.currentPage);

                if (idx < m_lobbyPager.lobbyIDs.size())
                {
                    //this will be reset next time the page is scrolled, and prevents double presses
                    e.getComponent<cro::UIInput>().enabled = false;

                    m_audioEnts[AudioID::Start].getComponent<cro::AudioEmitter>().play();

                    m_matchMaking.joinGame(m_lobbyPager.lobbyIDs[idx]);
                    m_sharedData.lobbyID = m_lobbyPager.lobbyIDs[idx];
                }

                refreshUI();
            }
        });

    //entry highlights
    glm::vec2 highlightPos(6.f, 161.f);
    for (auto i = 0u; i < LobbyPager::ItemsPerPage; ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(highlightPos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lobby_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());

        entity.addComponent<cro::Callback>().setUserData<std::uint32_t>(i); //used by button activated callback
        entity.getComponent<cro::Callback>().function = HighlightAnimationCallback();

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = lobbyActivated;

        m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_lobbyPager.slots.push_back(entity);
        highlightPos.y -= entity.getComponent<cro::Sprite>().getTextureBounds().height;
    }

    auto updateActiveSlots = [&]()
    {
        auto start = m_lobbyPager.currentPage * LobbyPager::ItemsPerPage;
        auto end = start + LobbyPager::ItemsPerPage;

        for (auto i = start; i < end; ++i)
        {
            m_lobbyPager.slots[i % LobbyPager::ItemsPerPage].getComponent<cro::UIInput>().enabled = (i < m_lobbyPager.lobbyIDs.size());
        }
    };

    //button left
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 13.f, 5.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_left");
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonLeft[0] = entity;

    //highlight left
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 11.f, 3.f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight_left");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, updateActiveSlots](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    if (m_lobbyPager.pages.size() > 1)
                    {
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        m_lobbyPager.currentPage = (m_lobbyPager.currentPage + (m_lobbyPager.pages.size() - 1)) % m_lobbyPager.pages.size();
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                        //only enable item slots as available on the new page
                        updateActiveSlots();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonLeft[1] = entity;


    //friends overlay
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 200.f, 10.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("friends_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f)});
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    Social::findFriends();
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //button right
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 382.f, 5.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_right");
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonRight[0] = entity;

    //highlight right
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 380.f, 3.f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight_right");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, updateActiveSlots](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    if (m_lobbyPager.pages.size() > 1)
                    {
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        m_lobbyPager.currentPage = (m_lobbyPager.currentPage + 1) % m_lobbyPager.pages.size();
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                        updateActiveSlots();

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonRight[1] = entity;

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //refresh
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 0.f, -2.f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -20.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("refresh");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_matchMaking.refreshLobbyList(Server::GameMode::Golf);
                    updateLobbyList(); //clears existing display until message comes in
                }
            });

    menuTransform.addChild(entity.getComponent<cro::Transform>());


    updateLobbyList();
}

void MenuState::createLobbyMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Lobby] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Lobby]);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_resources.textures);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    auto bgSprite = spriteSheet.getSprite("background");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    entity.getComponent<UIElement>().depth = -0.2f;
    entity.getComponent<UIElement>().resizeCallback =
        [&, bgSprite](cro::Entity e)
    {
        auto baseSize = glm::vec2(cro::App::getWindow().getSize()) / m_viewScale;
        constexpr float EdgeOffset = 50.f; //this much from outside before splitting
        
        e.getComponent<cro::Drawable2D>().setTexture(bgSprite.getTexture());
        auto bounds = bgSprite.getTextureBounds();
        auto rect = bgSprite.getTextureRectNormalised();

        //how much bigger to get either side in wider views
        float expansion = std::min(100.f, std::floor((baseSize.x - bounds.width) / 2.f));
        //only needs > 0 really but this gives a little leeway
        expansion = (baseSize.x - bounds.width > 10) ? expansion : 0.f;
        float edgeOffsetNorm = (EdgeOffset / bgSprite.getTexture()->getSize().x);

        bounds.width += expansion * 2.f;

        e.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f, bounds.height), glm::vec2(rect.left, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(rect.left, rect.bottom)),

                cro::Vertex2D(glm::vec2(EdgeOffset, bounds.height), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(EdgeOffset, 0.f), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom)),
                cro::Vertex2D(glm::vec2(EdgeOffset + expansion, bounds.height), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(EdgeOffset + expansion, 0.f), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom)),


                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset - expansion, bounds.height), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset - expansion, 0.f), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom)),
                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset, bounds.height), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset, 0.f), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom)),


                cro::Vertex2D(glm::vec2(bounds.width, bounds.height), glm::vec2(rect.left + rect.width, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(bounds.width, 0.f), glm::vec2(rect.left + rect.width, rect.bottom))
            });
    
        e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        m_lobbyExpansion = expansion;

        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::LobbyText;
        cmd.action = [expansion](cro::Entity e, float)
        {
            //set cropping area
            if (e.hasComponent<cro::Text>())
            {
                auto bounds = cro::Text::getLocalBounds(e);
                bounds.width = std::min(bounds.width, MinLobbyCropWidth + expansion);
                e.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
            }


            //set offset if on right
            if (auto pos = e.getComponent<cro::Transform>().getPosition(); pos.x > LobbyTextSpacing)
            {
                pos.x = LobbyTextSpacing + expansion + 1.f;
                e.getComponent<cro::Transform>().setPosition(pos);
            }
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };
    entity.getComponent<UIElement>().resizeCallback(entity);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    bounds = entity.getComponent<cro::Drawable2D>().getLocalBounds();
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

#ifdef USE_GNS
    //scrolls info about the selected course
    auto& labelFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 0.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont);// .setString(scoreStr);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (m_currentMenu == MenuID::Lobby)
        {
            auto scrollBounds = cro::Text::getLocalBounds(e);

            auto pos = e.getComponent<cro::Transform>().getPosition();

            pos.x -= 20.f * dt;
            pos.y = 17.f;
            pos.z = 0.3f;

            static constexpr float Offset = 150.f;
            const auto bgWidth = bounds.width;
            if (pos.x < -scrollBounds.width + Offset)
            {
                pos.x = bgWidth;
                pos.x -= Offset;
            }

            e.getComponent<cro::Transform>().setPosition(pos);

            cro::FloatRect cropping = { -pos.x + Offset, -16.f, (bgWidth) - (Offset * 2.f), 18.f };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        }
    };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::CourseTicker] = entity;
#endif

    auto textResizeCallback = 
        [&,bgEnt](cro::Entity e)
    {
        e.getComponent<cro::Transform>().setPosition({ bgEnt.getComponent<cro::Transform>().getOrigin().x , e.getComponent<UIElement>().absolutePosition.y, e.getComponent<UIElement>().depth });
    };

    //display the score type
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 156.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { bounds.width / 2.f, 156.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().resizeCallback = textResizeCallback;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(ScoreTypes[m_sharedData.scoreType]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ScoreType | CommandID::Menu::UIElement;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //and score description
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 74.f, 0.f });
    entity.getComponent<cro::Transform>().setPosition({(bounds.width / 2.f), 140.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { bounds.width / 2.f, 140.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().resizeCallback = textResizeCallback;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(RuleDescriptions[m_sharedData.scoreType]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ScoreDesc | CommandID::Menu::UIElement;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //gimme radius
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 82.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString("Gimme Radius");
    entity.addComponent<UIElement>().absolutePosition = { bounds.width / 2.f, 82.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().resizeCallback = textResizeCallback;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), 66.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
    entity.addComponent<UIElement>().absolutePosition = { bounds.width / 2.f, 66.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().resizeCallback = textResizeCallback;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::GimmeDesc | CommandID::Menu::UIElement;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 74.f, 0.f });
    entity.getComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), 50.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { bounds.width / 2.f, 50.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().resizeCallback = textResizeCallback;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString("The ball is considered holed\nwhen it's within this radius");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //display lobby members - updateLobbyAvatars() adds the text ents to this.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, bounds.height - 25.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::LobbyList;
    entity.addComponent<cro::Callback>().setUserData<std::vector<cro::Entity>>(); //abuse this component to store handles to the text children.
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //displays the thumbnails for the selected course
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("thumbnail_window");
    entity.addComponent<UIElement>().absolutePosition = { bounds.width / 2.f, bounds.height / 2.f };
    entity.getComponent<UIElement>().depth = 1.2f;
    entity.getComponent<UIElement>().resizeCallback = textResizeCallback;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto thumbBgEnt = entity;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection] = thumbBgEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 6.f, 34.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::HoleThumb] = entity;


    //hole count
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseHoles;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //checkbox to show reverse status
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 4.f, 3.f };
    entity.getComponent<UIElement>().depth = 0.01f;

    bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float)
    {
        auto b = bounds;
        if (m_sharedData.reverseCourse)
        {
            b.bottom -= bounds.height;
        }
        e.getComponent<cro::Sprite>().setTextureRect(b);
    };
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //background for course info
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("course_desc");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().relativePosition = CourseDescPosition;
    entity.getComponent<UIElement>().depth = -0.2f;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor( bounds.width / 2.f), std::floor(bounds.height / 2.f) + 3.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    bgEnt = entity;

   

    //course title
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 68.f, 01.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseTitle;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //course description
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 01.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseDesc;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //current rules
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 42.f, 01.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseRules;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString("IMPLEMENT ME");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);

    //banner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ButtonBanner];
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteRect](cro::Entity e, float)
    {
        auto rect = spriteRect;
        rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
        e.getComponent<cro::Sprite>().setTextureRect(rect);
        e.getComponent<cro::Callback>().active = false;
    };
    auto bannerEnt = entity;
    menuTransform.addChild(entity.getComponent<cro::Transform>());



    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -10000.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto enter = mouseEnter;
    auto exit = mouseExit;

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f });

            m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto mouseEnterHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left -= bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });

    //quit confirmation
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);

    struct ConfirmationData final
    {
        float progress = 0.f;
        enum
        {
            In, Out
        }dir = In;
        bool quitWhenDone = false;
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("message_board");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
        float scale = 0.f;
        if (data.dir == ConfirmationData::In)
        {
            data.progress = std::min(1.f, data.progress + (dt * 2.f));
            scale = cro::Util::Easing::easeOutBack(data.progress);

            if (data.progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::ConfirmQuit);
                m_currentMenu = MenuID::ConfirmQuit;
            }
        }
        else
        {
            data.progress = std::max(0.f, data.progress - (dt * 4.f));
            scale = cro::Util::Easing::easeOutQuint(data.progress);
            if (data.progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                m_currentMenu = MenuID::Lobby;

                if (data.quitWhenDone)
                {
                    quitLobby();
                }
                else
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Lobby);
                }
                refreshUI();
            }
        }

        
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;

    //quad to darken the screen
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().function =
        [&, confirmEnt](cro::Entity e, float)
    {
        auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale);

        if (scale > 0)
        {
            auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
            e.getComponent<cro::Transform>().setScale(size / scale);
        }

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
        }

        e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
        m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = confirmEnt.getComponent<cro::Callback>().active;
    };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto shadeEnt = entity;

    //confirmation text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Are You Sure?");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 44.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("This will kick all players.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Magenta);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto messageEnt = entity;



    //stash this so we can access it from the event handler (escape to ignore etc)
    quitConfirmCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;
        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 20.f, 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("No");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitConfirmCallback();
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 20.f, 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("Yes");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, confirmEnt, shadeEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = true;
                    confirmEnt.getComponent<cro::Callback>().active = true;
                    shadeEnt.getComponent<cro::Callback>().active = true;
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                    //restore the rules tab if necessary
                    float scale = m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y;
                    if (scale == 0)
                    {
                        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //back
    enterConfirmCallback = [&, confirmEnt, shadeEnt, messageEnt]() mutable
    {
        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        messageEnt.getComponent<cro::Text>().setFillColour(m_sharedData.hosting ? TextNormalColour : cro::Colour::Transparent);

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    enterConfirmCallback();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -16.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ReadyButton;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ReadyUp]; //which sprite is set by sending a message to this ent when we know if we're hosting or joining
    entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::ReadyUp].getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hosting)
                    {
                        //check all members ready
                        bool ready = true;
                        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                        {
                            if (m_sharedData.connectionData[i].playerCount != 0
                                && !m_readyState[i])
                            {
                                ready = false;
                                break;
                            }
                        }

                        if (ready && m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            //by drawing a black quad over the screen immediately
                            //gives the impression of responsiveness
                            glClear(GL_COLOR_BUFFER_BIT);

                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                        }
                    }
                    else
                    {
                        //toggle readyness but only if the selected course is locally available
                        if (!m_sharedData.mapDirectory.empty())
                        {
                            std::uint8_t ready = m_readyState[m_sharedData.clientConnection.connectionID] ? 0 : 1;
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady,
                                std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | ready),
                                net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                            if (ready)
                            {
                                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                            }
                            else
                            {
                                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                            }
                        }
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //server info message
#ifndef USE_GNS
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.98f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ServerInfo;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Connected to");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setShadowColour(cro::Colour(std::uint8_t(110), 179, 157));
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
#endif


    //running scores
    struct ScoreInfo final
    {
        std::uint8_t clientID = 0;
        std::uint8_t playerID = 0;
        std::int8_t score = 0;
    };

    std::vector<ScoreInfo> scoreInfo;

    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {

            if (!m_sharedData.connectionData[i].playerData[j].name.empty())
            {
                auto& info = scoreInfo.emplace_back();
                info.clientID = i;
                info.playerID = j;
                switch (m_sharedData.scoreType)
                {
                default:
                case ScoreType::Stroke:
                    info.score = m_sharedData.connectionData[i].playerData[j].parScore;
                    break;
                case ScoreType::Match:
                    info.score = m_sharedData.connectionData[i].playerData[j].matchScore;
                    break;
                case ScoreType::Skins:
                    info.score = m_sharedData.connectionData[i].playerData[j].skinScore;
                    break;
                }
            }
        }
    }

    std::sort(scoreInfo.begin(), scoreInfo.end(), 
        [&](const ScoreInfo& a, const ScoreInfo& b)
        {
            if (m_sharedData.scoreType == ScoreType::Stroke)
            {
                return a.score < b.score;
            }
            else
            {
                return a.score > b.score;
            }
        });

    std::vector<cro::String> names;
    for (const auto& score : scoreInfo)
    {
        names.push_back(m_sharedData.connectionData[score.clientID].playerData[score.playerID].name);
        names.back() += ": (" + std::to_string(score.score) + ")";
        switch (m_sharedData.scoreType)
        {
        default:
        case ScoreType::Stroke:
            if (score.score < 0)
            {
                names.back() += " Under PAR";
            }
            else if (score.score > 0)
            {
                names.back() += " Over PAR";
            }
            break;
        case ScoreType::Skins:
            names.back() += " Skins";
            break;
        case ScoreType::Match:
            names.back() += " Match Points";
            break;
        }
    }

    cro::String str;
    if (!names.empty())
    {
        str = "Last Round's Top Scorers: " + names[0];
        for (auto i = 1u; i < names.size() && i < 4u; ++i)
        {
            str += " - " + names[i];
        }
    }
    else
    {
        str = "Welcome To Super Video Golf!";
    }

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (m_currentMenu == MenuID::Lobby)
        {
            auto pos = e.getComponent<cro::Transform>().getPosition();

            pos.x -= 20.f * dt;
            pos.y = 15.f;
            pos.z = 0.3f;

            static constexpr float Offset = 50.f;
            if (pos.x < -bounds.width + Offset)
            {
                pos.x = cro::App::getWindow().getSize().x / m_viewScale.x;
                pos.x -= Offset;
            }

            e.getComponent<cro::Transform>().setPosition(pos);

                
            cro::FloatRect cropping = { -pos.x + Offset, -16.f, (cro::App::getWindow().getSize().x / m_viewScale.x) - (Offset * 2.f), 18.f };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        }
    };

    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void MenuState::updateLobbyData(const net::NetEvent& evt)
{
    ConnectionData cd;
    if (cd.deserialise(evt.packet))
    {
        //hum. this overwrite level values as they're maintained independently
        //would it be better to resend our level data when rx'ing this?
        auto lvl = m_sharedData.connectionData[cd.connectionID].level;
        m_sharedData.connectionData[cd.connectionID] = cd;
        m_sharedData.connectionData[cd.connectionID].level = lvl;

#ifdef USE_GNS
        //check the new player data for UGC
        for (auto i = 0u; i < cd.playerCount; ++i)
        {
            if (indexFromBallID(cd.playerData[i].ballID) == -1)
            {
                //no local ball for this player
                Social::fetchRemoteContent(cd.peerID, cd.playerData[i].ballID, Social::UserContent::Ball);
            }

            auto id = cd.playerData[i].hairID;
            auto hair = std::find_if(m_sharedData.hairInfo.begin(), m_sharedData.hairInfo.end(),
                [id](const SharedStateData::HairInfo& h)
                {
                    return h.uid == id;
                });
            if (hair == m_sharedData.hairInfo.end())
            {
                //no local hair model
                Social::fetchRemoteContent(cd.peerID, id, Social::UserContent::Hair);
            }
        }
#endif
    }

    if (m_sharedData.hosting)
    {
        std::int32_t playerCount = 0;
        for (const auto& c : m_sharedData.connectionData)
        {
            playerCount += c.playerCount;
        }

        m_matchMaking.setGamePlayerCount(playerCount);
    }

    //new players won't have other levels
    std::uint16_t xp = (Social::getLevel() << 8) | m_sharedData.clientConnection.connectionID;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::PlayerXP, xp, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    updateLobbyAvatars();
}

void MenuState::updateLobbyAvatars()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::LobbyList;
    cmd.action = [&](cro::Entity e, float)
    {
        auto& children = e.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
        for (auto c : children)
        {
            m_uiScene.destroyEntity(c);
        }
        children.clear();

        const auto applyTexture = [&](std::size_t idx, cro::Texture& targetTexture, const std::array<uint8_t, 4u>& flags)
        {
            for (auto j = 0u; j < flags.size(); ++j)
            {
                m_playerAvatars[idx].setColour(pc::ColourKey::Index(j), flags[j]);
            }
            m_playerAvatars[idx].apply(&targetTexture);
        };
        const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Label);
        cro::SimpleText simpleText(font);
        simpleText.setCharacterSize(LabelTextSize);
        simpleText.setFillColour(TextNormalColour);
        simpleText.setShadowOffset({ 1.f, -1.f });
        simpleText.setShadowColour(LeaderboardTextDark);
        //simpleText.setBold(true);

        cro::Image img;
        img.create(1, 1, cro::Colour::White);
        cro::Texture quadTexture;
        quadTexture.loadFromImage(img);
        cro::SimpleQuad simpleQuad(quadTexture);
        simpleQuad.setBlendMode(cro::Material::BlendMode::None);
        simpleQuad.setColour(cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha / 2.f));

        cro::Texture iconTexture;
        cro::Image iconImage;

        std::int32_t h = 0;
        std::int32_t clientCount = 0;
        std::int32_t playerCount = 0;
        glm::vec2 textureSize(LabelTextureSize);
        textureSize.y -= LabelIconSize.y;
        
        auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

        std::array<cro::String, 2u> stringCols = {};

        const std::array<glm::vec3, 4u> IconPositions =
        {
            glm::vec3(1.f, -119.f, 0.1f),
            glm::vec3(1.f, -132.f, 0.1f),
            glm::vec3(1.f + LobbyTextSpacing, -119.f, 0.1f),
            glm::vec3(1.f + LobbyTextSpacing, -132.f, 0.1f)
        };

        for (const auto& c : m_sharedData.connectionData)
        {
            //update the name label texture
            if (c.connectionID < m_sharedData.nameTextures.size())
            {
                m_sharedData.nameTextures[c.connectionID].clear(cro::Colour::Transparent);

                simpleQuad.setTexture(quadTexture);
                simpleQuad.setColour(cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha / 2.f));

                for (auto i = 0u; i < c.playerCount; ++i)
                {
                    simpleText.setString(c.playerData[i].name);
                    auto bounds = simpleText.getLocalBounds();
                    simpleText.setPosition({ std::round((textureSize.x - bounds.width) / 2.f), (i * (textureSize.y / ConstVal::MaxPlayers)) + 4.f });

                    simpleQuad.setPosition({ simpleText.getPosition().x - 2.f,(i * (textureSize.y / ConstVal::MaxPlayers))});
                    simpleQuad.setScale({ (bounds.width + 5.f), 14.f });
                    simpleQuad.draw();
                    simpleText.draw();
                }

                iconImage = Social::getUserIcon(c.peerID);
                if (iconImage.getPixelData())
                {
                    iconTexture.loadFromImage(iconImage);
                    simpleQuad.setTexture(iconTexture);
                    simpleQuad.setScale({ 1.f, 1.f });
                    simpleQuad.setPosition({ 0.f, textureSize.y });
                    simpleQuad.setColour(cro::Colour::White);
                    simpleQuad.draw();
                }

                m_sharedData.nameTextures[c.connectionID].display();
            }

            glm::vec2 iconPos(1.f, 0.f);
            const std::int32_t col = (playerCount / ConstVal::MaxPlayers);
            const std::int32_t row = (playerCount % ConstVal::MaxPlayers);
            iconPos.x += (LobbyTextSpacing + m_lobbyExpansion) * col;
            iconPos.y = row * -14.f; //TODO constify row spacing

            //add list of names on the connected client
            for (auto i = 0u; i < c.playerCount; ++i)
            {
                //stringCols[playerCount / ConstVal::MaxPlayers] += "buns\n";

                auto avatarIndex = indexFromAvatarID(c.playerData[i].skinID);
                applyTexture(avatarIndex, m_sharedData.avatarTextures[c.connectionID][i], c.playerData[i].avatarFlags);
                stringCols[playerCount / ConstVal::MaxPlayers] += c.playerData[i].name.substr(0, ConstVal::MaxStringChars) + "\n";

                playerCount++;
            }

            if (c.playerCount != 0)
            {
                clientCount++;
            }

            //client icons are attached to this
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(IconPositions[h]);

            //used to update spacing by resize callback from lobby background ent.
            entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::LobbyText;
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);
            auto iconEnt = entity;

            //add a ready status for that client
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, h](cro::Entity e2, float)
            {
                cro::Colour colour = m_readyState[h] ? TextGreenColour : LeaderboardTextDark;

                auto& verts = e2.getComponent<cro::Drawable2D>().getVertexData();
                for (auto& v : verts)
                {
                    v.colour = colour;
                }
            };
            iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);

            auto& verts = entity.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(5.f, 0.f)),
                cro::Vertex2D(glm::vec2(0.f, 5.f)),
                cro::Vertex2D(glm::vec2(5.f))
            };
            entity.getComponent<cro::Drawable2D>().updateLocalBounds();

            //status text
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 8.f, 6.f, 0.1f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(smallFont).setString(c.playerCount ? "Ready" : "Not Connected");
            entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);



            //rank text
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(smallFont).setString("Level");
            entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, h](cro::Entity ent, float)
            {
                if (m_sharedData.connectionData[h].playerCount == 0)
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                else
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    ent.getComponent<cro::Text>().setString("Level " + std::to_string(m_sharedData.connectionData[h].level));
                    
                    float offset = ent.getComponent<cro::Callback>().getUserData<float>();
                    ent.getComponent<cro::Transform>().setPosition({ std::floor((56.f + (m_lobbyExpansion / 2.f)) - offset), 6.f, 0.1f});
                }
            };
            iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);
            auto rankEnt = entity;

            //add a rank badge
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({-16.f, -12.f, 0.2f});
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LevelBadge];
            entity.addComponent<cro::SpriteAnimation>();

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, h](cro::Entity ent, float)
            {
                if (m_sharedData.connectionData[h].playerCount == 0
                    ||  m_sharedData.connectionData[h].level == 0)
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                else
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    auto index = std::min(5, m_sharedData.connectionData[h].level / 10);
                    ent.getComponent<cro::SpriteAnimation>().play(index);
                }
            };
            rankEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);

            //if this is our local client then add the current xp level
            if (h == m_sharedData.clientConnection.connectionID)
            {
                constexpr float BarWidth = 80.f;
                constexpr float BarHeight = 10.f;

                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({(BarWidth / 2.f) - 2.f, -4.f, -0.15f});

                const auto CornerColour = cro::Colour(std::uint8_t(152), 122, 104);

                const auto progress = Social::getLevelProgress();
                entity.addComponent<cro::Drawable2D>().setVertexData(
                    {
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), TextHighlightColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), TextHighlightColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), LeaderboardTextDark),

                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), LeaderboardTextDark),

                        //corners
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, BarHeight / 2.f), CornerColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (BarHeight / 2.f) - 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, -BarHeight / 2.f), CornerColour),


                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), CornerColour),

                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                    });
                entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
                rankEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);

                rankEnt.getComponent<cro::Callback>().setUserData<float>((BarWidth / 2.f) - 7.f);
            }
            else
            {
                //text width
                auto bounds = cro::Text::getLocalBounds(rankEnt);
                rankEnt.getComponent<cro::Callback>().setUserData<float>((bounds.width / 2.f) + 8.f);
            }



            //add a network status icon (not attached to icons)
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(iconPos, 0.1f));
            entity.getComponent<cro::Transform>().move({ -17.f, -12.f });
            if (iconPos.x > LobbyTextSpacing)
            {
                //Ican't even remember why I have t odo this any more...
                entity.getComponent<cro::Transform>().setOrigin({ 17.f, 0.f });
            }
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::NetStrength];
            entity.addComponent<cro::SpriteAnimation>();

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, h](cro::Entity ent, float)
            {
                if (m_sharedData.connectionData[h].playerCount == 0)
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                else
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    auto index = std::min(4u, m_sharedData.connectionData[h].pingTime / 30);
                    ent.getComponent<cro::SpriteAnimation>().play(index);
                }
            };
            entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::LobbyText;
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);

            h++;
        }



        //create tex columns for name lists
        glm::vec2 textPos(1.f, 0.f);
        for (const auto& str : stringCols)
        {
            if (!str.empty())
            {
                auto entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(textPos, 0.2f));
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Text>(largeFont).setString(str);
                entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
                entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
                entity.getComponent<cro::Text>().setVerticalSpacing(6.f);

                auto crop = cro::Text::getLocalBounds(entity);
                crop.width = std::min(crop.width, MinLobbyCropWidth + m_lobbyExpansion);
                entity.getComponent<cro::Drawable2D>().setCroppingArea(crop);
                if (textPos.x > LobbyTextSpacing)
                {
                    entity.getComponent<cro::Transform>().move({ m_lobbyExpansion, 0.f });
                }

                //used to update spacing by resize callback from lobby background ent.
                entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::LobbyText;
                e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);
            }
            textPos.x += LobbyTextSpacing;
        }


        auto strClientCount = std::to_string(clientCount);
        Social::setStatus(Social::InfoID::Lobby, { "Golf", strClientCount.c_str(), std::to_string(ConstVal::MaxClients).c_str() });
        Social::setGroup(m_sharedData.lobbyID, playerCount);

        auto temp = m_uiScene.createEntity();
        temp.addComponent<cro::Callback>().active = true;
        temp.getComponent<cro::Callback>().function = [&](cro::Entity e, float)
        {
            refreshUI();
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);

            auto f = m_uiScene.createEntity();
            f.addComponent<cro::Callback>().active = true;
            f.getComponent<cro::Callback>().function =
                [&](cro::Entity g, float)
            {
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
                g.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(g);
            };
        };
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void MenuState::updateLobbyList()
{
#ifdef USE_GNS
    for (auto& e : m_lobbyPager.pages)
    {
        m_uiScene.destroyEntity(e);
    }

    m_lobbyPager.pages.clear();
    m_lobbyPager.lobbyIDs.clear();

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& lobbyData = m_matchMaking.getLobbies();
    if (lobbyData.empty())
    {
        //no lobbies found :(
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(LobbyTextRootPosition);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(" No Games Found.");
        entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);

        m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_lobbyPager.pages.push_back(entity);

        for (auto e : m_lobbyPager.slots)
        {
            e.getComponent<cro::UIInput>().enabled = false;
        }
    }
    else
    {
        auto pageCount = (lobbyData.size() / LobbyPager::ItemsPerPage) + 1;
        for (auto i = 0u; i < pageCount; ++i)
        {
            cro::String pageString;

            const auto startIndex = i * LobbyPager::ItemsPerPage;
            const auto endIndex = std::min(lobbyData.size(), startIndex + LobbyPager::ItemsPerPage);
            for (auto j = startIndex; j < endIndex; ++j)
            {
                std::stringstream ss;
                ss << " " << lobbyData[j].clientCount << "  " << std::setw(2) << std::setfill('0') << lobbyData[j].playerCount << " - ";
                pageString += ss.str();
                pageString += lobbyData[j].title + "\n";

                m_lobbyPager.lobbyIDs.push_back(lobbyData[j].ID);
            }

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(LobbyTextRootPosition);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setString(pageString);
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);

            m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_lobbyPager.pages.push_back(entity);
        }
        m_lobbyPager.pages[0].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_lobbyPager.currentPage = std::min(m_lobbyPager.currentPage, m_lobbyPager.pages.size() - 1);

        //enable slot highlights for current page
        auto start = m_lobbyPager.currentPage * LobbyPager::ItemsPerPage;
        auto end = start + LobbyPager::ItemsPerPage;

        for (auto i = start; i < end; ++i)
        {
            m_lobbyPager.slots[i % LobbyPager::ItemsPerPage].getComponent<cro::UIInput>().enabled = (i < m_lobbyPager.lobbyIDs.size());
        }
    }

    //hide or show buttons
    if (m_lobbyPager.pages.size() > 1)
    {
        m_lobbyPager.buttonLeft[0].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::UIInput>().enabled = true;

        m_lobbyPager.buttonRight[0].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::UIInput>().enabled = true;

        for (auto page : m_lobbyPager.pages)
        {
            page.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }
    else
    {
        m_lobbyPager.buttonLeft[0].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::UIInput>().enabled = false;

        m_lobbyPager.buttonRight[0].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::UIInput>().enabled = false;
    }
#endif
}

void MenuState::quitLobby()
{
    m_sharedData.clientConnection.connected = false;
    m_sharedData.clientConnection.connectionID = ConstVal::NullValue;
    m_sharedData.clientConnection.ready = false;
    m_sharedData.clientConnection.netClient.disconnect();

    m_matchMaking.leaveGame();

    if (m_sharedData.hosting)
    {
        m_sharedData.serverInstance.stop();
        m_sharedData.hosting = false;
    }

    for (auto& cd : m_sharedData.connectionData)
    {
        cd.playerCount = 0;
    }
    

    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;


    //delete the course selection entities as they'll be re-created as needed
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::CourseSelect;
    cmd.action =
        [&](cro::Entity b, float)
    {
        m_uiScene.destroyEntity(b);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    Social::setStatus(Social::InfoID::Menu, { "Main Menu" });
    Social::setGroup(0);
}

void MenuState::addCourseSelectButtons()
{
    //choose scoring type
    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { -62.f, 59.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    auto bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevRules;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y == 0;
    };
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 63.f, 59.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextRules;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y == 0;
    };
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //choose gimme radius
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { -62.f, -15.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevRadius;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y == 0;
    };
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 63.f, -15.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextRadius;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y == 0;
    };
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    //hole count buttons.
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 32.f, 21.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevHoleCount;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y != 0;
    };
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 117.f, 21.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextHoleCount;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y != 0;
    };
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    //toggle course reverse
    auto checkboxEnt = m_uiScene.createEntity();
    checkboxEnt.addComponent<cro::Transform>();
    checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    checkboxEnt.addComponent<cro::Drawable2D>();
    checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
    checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    checkboxEnt.addComponent<UIElement>().absolutePosition = { 3.f, 2.f };
    checkboxEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 78.f; //wild stab at the width of the text (it's not here to measure...)
    checkboxEnt.addComponent<cro::UIInput>().area = bounds;
    checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.toggleReverseCourse;
    checkboxEnt.addComponent<cro::Callback>().active = true;
    checkboxEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y != 0;
    };
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());



    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    if (Social::isAvailable())
    {
        //friends only lobby
        auto resizeCallback =
            [&](cro::Entity e)
        {
            e.getComponent<cro::Transform>().move({ -m_lobbyExpansion, 0.f });
        };

        auto checkboxEnt = m_uiScene.createEntity();
        checkboxEnt.addComponent<cro::Transform>();
        checkboxEnt.addComponent<cro::Drawable2D>();
        checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
        checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        checkboxEnt.addComponent<UIElement>().absolutePosition = { -189.f, -86.f };
        checkboxEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
        checkboxEnt.getComponent<UIElement>().depth = 0.01f;
        checkboxEnt.getComponent<UIElement>().resizeCallback = resizeCallback;

        bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
        checkboxEnt.addComponent<cro::Callback>().active = true;
        checkboxEnt.getComponent<cro::Callback>().function =
            [&, bounds](cro::Entity e, float)
        {
            auto b = bounds;
            if (m_matchMaking.getFriendsOnly())
            {
                b.bottom -= bounds.height;
            }
            e.getComponent<cro::Sprite>().setTextureRect(b);
        };

        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


        checkboxEnt = m_uiScene.createEntity();
        checkboxEnt.addComponent<cro::Transform>();
        checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        checkboxEnt.addComponent<cro::Drawable2D>();
        checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
        checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        checkboxEnt.addComponent<UIElement>().absolutePosition = { -190.f, -87.f };
        checkboxEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
        checkboxEnt.getComponent<UIElement>().depth = 0.01f;
        checkboxEnt.getComponent<UIElement>().resizeCallback = resizeCallback;
        bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
        checkboxEnt.addComponent<cro::UIInput>().area = bounds;
        checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.toggleFriendsOnly;
        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());

        auto labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString("Friends Only");
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { -176.f, -79.f };
        labelEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
        labelEnt.getComponent<UIElement>().depth = 0.01f;
        labelEnt.getComponent<UIElement>().resizeCallback = resizeCallback;
        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
        checkboxEnt.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(labelEnt).width;

        auto resizeCallbackRight =
            [&](cro::Entity e)
        {
            e.getComponent<cro::Transform>().move({ m_lobbyExpansion, 0.f });
        };

        labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString("Invite");
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { 156.f, -79.f };
        labelEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
        labelEnt.getComponent<UIElement>().depth = 0.01f;
        labelEnt.getComponent<UIElement>().resizeCallback = resizeCallbackRight;
        bounds = cro::Text::getLocalBounds(labelEnt);
        labelEnt.addComponent<cro::UIInput>().area = bounds;
        labelEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        labelEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectText;
        labelEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectText;
        labelEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.inviteFriends;
        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());


        labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Envelope];
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { 138.f, -86.f };
        labelEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
        labelEnt.getComponent<UIElement>().depth = 0.01f;
        labelEnt.getComponent<UIElement>().resizeCallback = resizeCallbackRight;
        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

        //tedious method of delaying this by one frame. Ensures everything is properly placed
        auto timerEnt = m_uiScene.createEntity();
        timerEnt.addComponent<cro::Callback>().active = true;
        timerEnt.getComponent<cro::Callback>().function =
            [&](cro::Entity o, float)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Menu::CourseSelect;
            cmd.action = [](cro::Entity e, float)
            {
                if (e.hasComponent<UIElement>())
                {
                    if (e.getComponent<UIElement>().resizeCallback)
                    {
                        e.getComponent<UIElement>().resizeCallback(e);
                    }
                }
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            o.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(o);
        };
    }





    //choose course
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { -108.f, -22.f };
    buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevCourse;
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    //rules button
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyRuleButton];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 0.f, -19.f };
    buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //hax.
        e.getComponent<cro::SpriteAnimation>().play(static_cast<std::int32_t>(
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y));
    };
    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyRuleButtonHighlight];
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 0.f, -19.f };
    buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.025f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    auto area = bounds;
    area.width /= 2.f;
    area.left += area.width;
    buttonEnt.addComponent<cro::UIInput>().area = area;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.toggleGameRules;
    buttonEnt.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 108.f, -22.f };
    buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextCourse;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    if (m_courseData.size() > m_courseIndices[Range::Official].count)
    {
        //add arrows to scroll through course list sub indices.
        buttonEnt = m_uiScene.createEntity();
        buttonEnt.addComponent<cro::Transform>();
        buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
        buttonEnt.addComponent<cro::SpriteAnimation>();
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        buttonEnt.addComponent<UIElement>().absolutePosition = { -68.f, -34.f };
        buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
        buttonEnt.getComponent<UIElement>().depth = 0.01f;
        bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
        buttonEnt.addComponent<cro::UIInput>().area = bounds;
        buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevHoleType;
        buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
        



        buttonEnt = m_uiScene.createEntity();
        buttonEnt.addComponent<cro::Transform>();
        buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
        buttonEnt.addComponent<cro::SpriteAnimation>();
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        buttonEnt.addComponent<UIElement>().absolutePosition = { 68.f, -34.f };
        buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
        buttonEnt.getComponent<UIElement>().depth = 0.01f;
        bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
        buttonEnt.addComponent<cro::UIInput>().area = bounds;
        buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextHoleType;

        buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


        auto labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString(CourseTypes[m_currentRange]);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect | CommandID::Menu::CourseType;;
        labelEnt.addComponent<UIElement>().absolutePosition = { 0.f, -30.f };
        labelEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
        labelEnt.getComponent<UIElement>().depth = 0.1f;

        centreText(labelEnt);
        m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    }
}

void MenuState::prevHoleCount()
{
    m_sharedData.holeCount = (m_sharedData.holeCount + 2) % 3;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::HoleCount, m_sharedData.holeCount, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void MenuState::nextHoleCount()
{
    m_sharedData.holeCount = (m_sharedData.holeCount + 1) % 3;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::HoleCount, m_sharedData.holeCount, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void MenuState::prevCourse()
{
    auto idx = m_sharedData.courseIndex - m_courseIndices[m_currentRange].start;
    idx = (idx + (m_courseIndices[m_currentRange].count - 1)) % m_courseIndices[m_currentRange].count;

    m_sharedData.courseIndex = m_courseIndices[m_currentRange].start + idx;

    m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
    auto data = serialiseString(m_sharedData.mapDirectory);
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void MenuState::nextCourse()
{
    auto idx = m_sharedData.courseIndex - m_courseIndices[m_currentRange].start;
    idx = (idx + 1) % m_courseIndices[m_currentRange].count;

    m_sharedData.courseIndex = m_courseIndices[m_currentRange].start + idx;

    m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
    auto data = serialiseString(m_sharedData.mapDirectory);
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void MenuState::refreshUI()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::UIElement;
    cmd.action =
        [&](cro::Entity e, float)
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        const auto& element = e.getComponent<UIElement>();
        auto pos = element.absolutePosition;
        pos += element.relativePosition * size / m_viewScale;

        pos.x = std::floor(pos.x);
        pos.y = std::floor(pos.y);

        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));

        if (element.resizeCallback)
        {
            element.resizeCallback(e);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //and resizes banners horizontally
    cmd.targetFlags = CommandID::Menu::UIBanner;
    cmd.action =
        [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //makes sure to refresh for at least one frame if cam currently static
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
}

void MenuState::updateCourseRuleString()
{
    const auto data = std::find_if(m_courseData.cbegin(), m_courseData.cend(),
        [&](const CourseData& cd)
        {
            return cd.directory == m_sharedData.mapDirectory;
        });

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::CourseRules;
    cmd.action = [&, data](cro::Entity e, float)
    {
        auto str = ScoreTypes[m_sharedData.scoreType];
        str += ", " + GimmeString[m_sharedData.gimmeRadius];

        if (data != m_courseData.end())
        {
            str += ", " + data->holeCount[m_sharedData.holeCount];
        }

        if (m_sharedData.reverseCourse)
        {
            str += ", Reversed";
        }
        
        e.getComponent<cro::Text>().setString(str);
        centreText(e);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

#ifdef USE_GNS
    //update ticker
    if (m_lobbyWindowEntities[LobbyEntityID::CourseTicker].isValid())
    {
        if (!data->isUser)
        {
            std::stringstream ss;
            float playTime = Achievements::getAvgStat(m_sharedData.mapDirectory);
            if (playTime > 0)
            {
                float minutes = playTime / 60.f;
                float seconds = playTime - (std::round(minutes) * 60.f);
                ss.precision(2);
                ss << "Avg. Play Duration: ";
                ss << minutes << "m ";
                ss << seconds << "s - ";
            }

            ss << "Top Players: ";

            cro::String scoreStr(ss.str());
            scoreStr += Social::getTopFive(m_sharedData.mapDirectory, m_sharedData.holeCount);
            m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Text>().setString(scoreStr);
            m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    }
#endif
}

void MenuState::updateUnlockedItems()
{
    //current day streak
    auto streak = Social::updateStreak();
    CRO_ASSERT(streak < 8, "");
    switch (streak)
    {
    default:
        m_sharedData.unlockedItems.push_back(ul::UnlockID::Streak01 + (streak - 1));
        break;
    case 0: //do nothing
        break;
    }

    if (streak != 0)
    {
        Social::awardXP(StreakXP[streak-1]);
    }



    //clubs
    auto clubFlags = Social::getUnlockStatus(Social::UnlockType::Club);
    if (clubFlags == 0)
    {
        clubFlags = ClubID::DefaultSet;
    }
    auto level = Social::getLevel();
    auto clubCount = std::min(ClubID::LockedSet.size(), static_cast<std::size_t>(level / 5));
    for (auto i = 0u; i < clubCount; ++i)
    {
        auto clubID = ClubID::LockedSet[i];
        if ((clubFlags & ClubID::Flags[clubID]) == 0)
        {
            clubFlags |= ClubID::Flags[clubID];
            m_sharedData.unlockedItems.push_back(ul::UnlockID::FiveWood + i);
        }
    }

    m_sharedData.inputBinding.clubset = clubFlags;
    Social::setUnlockStatus(Social::UnlockType::Club, clubFlags);

    if (m_sharedData.inputBinding.clubset == ClubID::FullSet)
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::FullHouse]);
    }


    //ball flags (balls are unlocked every 10 levels)
    auto ballFlags = Social::getUnlockStatus(Social::UnlockType::Ball);
    auto ballCount = std::min(5, level / 10);
    for (auto i = 0; i < ballCount; ++i)
    {
        auto flag = (1 << i);
        if ((ballFlags & flag) == 0)
        {
            ballFlags |= flag;
            m_sharedData.unlockedItems.push_back(ul::UnlockID::BronzeBall + i);
        }
    }

    Social::setUnlockStatus(Social::UnlockType::Ball, ballFlags);


    //level up
    if (level > 0)
    {
        //levels are same interval as balls + 1st level
        auto levelCount = ballCount + 1;
        auto levelFlags = Social::getUnlockStatus(Social::UnlockType::Level);

        for (auto i = 0; i < levelCount; ++i)
        {
            auto flag = (1 << i);
            if ((levelFlags & flag) == 0)
            {
                levelFlags |= flag;
                m_sharedData.unlockedItems.push_back(ul::UnlockID::Level1 + i);
            }
        }
        Social::setUnlockStatus(Social::UnlockType::Level, levelFlags);
    }


    //generic unlocks from achievements etc
    auto genericFlags = Social::getUnlockStatus(Social::UnlockType::Generic);
    constexpr std::int32_t genericBase = ul::UnlockID::RangeExtend01;

    if (level > 14)
    {
        //club range is extended at level 15 and 30
        auto flag = (1 << genericBase);
        if ((genericFlags & flag) == 0)
        {
            genericFlags |= flag;
            m_sharedData.unlockedItems.push_back(ul::UnlockID::RangeExtend01);
        }
        else if (level > 29)
        {
            flag = (1 << (ul::UnlockID::RangeExtend02 - genericBase));
            if ((genericFlags & flag) == 0)
            {
                genericFlags |= flag;
                m_sharedData.unlockedItems.push_back(ul::UnlockID::RangeExtend02);
            }
        }
    }

    auto flag = (1 << (ul::UnlockID::Clubhouse - genericBase));
    if ((genericFlags & flag) == 0 &&
        Achievements::getAchievement(AchievementStrings[AchievementID::JoinTheClub])->achieved)
    {
        genericFlags |= flag;
        m_sharedData.unlockedItems.push_back(ul::UnlockID::Clubhouse);
    }

    flag = (1 << (ul::UnlockID::CourseEditor - genericBase));
    if ((genericFlags & flag) == 0 &&
        Achievements::getAchievement(AchievementStrings[AchievementID::GrandTour])->achieved)
    {
        genericFlags |= flag;
        m_sharedData.unlockedItems.push_back(ul::UnlockID::CourseEditor);
    }

    Social::setUnlockStatus(Social::UnlockType::Generic, genericFlags);
}