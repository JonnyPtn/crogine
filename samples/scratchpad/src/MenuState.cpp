/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

#include "MenuState.hpp"
#include "MyApp.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Easings.hpp>

#include <fstream>
#include <iomanip>

using namespace sp;

namespace
{
    constexpr glm::vec2 ViewSize(1920.f, 1080.f);
    constexpr float MenuSpacing = 40.f;

    bool activated(const cro::ButtonEvent& evt)
    {
        switch (evt.type)
        {
        default: return false;
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
            return evt.button.button == SDL_BUTTON_LEFT;
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERBUTTONDOWN:
            return evt.cbutton.button == SDL_CONTROLLER_BUTTON_A;
        case SDL_FINGERUP:
        case SDL_FINGERDOWN:
            return true;
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            return (evt.key.keysym.sym == SDLK_SPACE || evt.key.keysym.sym == SDLK_RETURN);
        }
    }

    bool showVideoPlayer = false;
    bool showBoilerplate = false;
}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context, MyApp& app)
    : cro::State    (stack, context),
    m_gameInstance  (app),
    m_scene         (context.appInstance.getMessageBus())
{
    app.unloadPlugin();

    //launches a loading screen (registered in MyApp.cpp)
    context.mainWindow.loadResources([this]() {
        //add systems to scene
        addSystems();
        //load assets (textures, shaders, models etc)
        loadAssets();
        //create some entities
        createScene();
        //create ImGui stuff
        createUI();
    });
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);

    m_scene.forwardEvent(evt);
    return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MenuState::simulate(float dt)
{
    m_video.update(dt);

    m_scene.simulate(dt);
    return true;
}

void MenuState::render()
{
    //draw any renderable systems
    m_scene.render();
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
}

void MenuState::loadAssets()
{
    m_font.loadFromFile("assets/fonts/VeraMono.ttf");
}

void MenuState::createScene()
{
    //background
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -1.f));
    entity.addComponent<cro::Drawable2D>().getVertexData() = 
    {
        cro::Vertex2D(glm::vec2(0.f, ViewSize.y), cro::Colour::CornflowerBlue),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::CornflowerBlue),
        cro::Vertex2D(glm::vec2(ViewSize), cro::Colour::CornflowerBlue),
        cro::Vertex2D(glm::vec2(ViewSize.x, 0.f), cro::Colour::CornflowerBlue)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
 


    //menu
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(200.f, 960.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Scratchpad");
    entity.getComponent<cro::Text>().setCharacterSize(80);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
    entity.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
    entity.getComponent<cro::Text>().setOutlineThickness(1.5f);

    auto* uiSystem = m_scene.getSystem<cro::UISystem>();
    auto selected = uiSystem->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Red);
        });
    auto unselected = uiSystem->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
        });


    auto createButton = [&](const std::string label, glm::vec2 position)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(position);
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(m_font).setString(label);
        e.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
        e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
        e.getComponent<cro::Text>().setOutlineThickness(1.f);
        e.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(e);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;

        return e;
    };

    //batcat button
    glm::vec2 textPos(200.f, 800.f);
    entity = createButton("Batcat", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::BatCat);
                }
            });

    //mesh collision button
    textPos.y -= MenuSpacing;
    entity = createButton("Mesh Collision", textPos);    
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::MeshCollision);
                }
            });

    //BSP button
    textPos.y -= MenuSpacing;
    entity = createButton("BSP", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::BSP);
                }
            });

    //voxels button
    textPos.y -= MenuSpacing;
    entity = createButton("Voxels", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Voxels);
                }
            });

    //billiards button
    textPos.y -= MenuSpacing;
    entity = createButton("Billiards", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Billiards);
                }
            });


    //VATSs button
    textPos.y -= MenuSpacing;
    entity = createButton("Vertex Animation Textures", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::VATs);
                }
            });

    //bush button
    textPos.y -= MenuSpacing;
    entity = createButton("Booshes", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Bush);
                }
            });

    //retro button
    textPos.y -= MenuSpacing;
    entity = createButton("Retro Shading", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Retro);
                }
            });

    //frustum button
    textPos.y -= MenuSpacing;
    entity = createButton("Frustum Visualisation", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Frustum);
                }
            });

    //frustum button
    textPos.y -= MenuSpacing;
    entity = createButton("Rolling Balls", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Rolling);
                }
            });

    //swing test button
    textPos.y -= MenuSpacing;
    entity = createButton("Swingput", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Swing);
                }
            });

    //animation blending
    textPos.y -= MenuSpacing;
    entity = createButton("Anim Blending", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::AnimBlend);
                }
            });

    //SSAO
    textPos.y -= MenuSpacing;
    entity = createButton("SSAO", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::SSAO);
                }
            });

    //Log rolling
    textPos.y -= MenuSpacing;
    entity = createButton("Log Roll", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Log);
                }
            });

    //load plugin
    textPos.y -= MenuSpacing;
    entity = createButton("Load Plugin", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //TODO directory browsing crashes xfce
                    //TODO font doesn't load without trailing /

                    //requestStackClear();
                    //auto path = cro::FileSystem::openFolderDialogue();
                    //if (!path.empty())
                    {
                        //m_gameInstance.loadPlugin("plugin/");
                    }
                    cro::FileSystem::showMessageBox("Fix Me", "Fix Me", cro::FileSystem::OK);
                }
            });

    //quit button
    textPos.y -= MenuSpacing * 2.f;
    entity = createButton("Quit", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::App::quit();
                }
            });




    //camera
    auto updateCam = [&](cro::Camera& cam)
    {
        static constexpr float viewRatio = ViewSize.x / ViewSize.y;

        cam.setOrthographic(0.f, ViewSize.x, 0.f, ViewSize.y, -0.1f, 2.f);

        glm::vec2 size(cro::App::getWindow().getSize());
        auto sizeRatio = size.x / size.y;

        if (sizeRatio > viewRatio)
        {
            cam.viewport.width = viewRatio / sizeRatio;
            cam.viewport.left = (1.f - cam.viewport.width) / 2.f;

            cam.viewport.height = 1.f;
            cam.viewport.bottom = 0.f;
        }
        else
        {
            cam.viewport.width = 1.f;
            cam.viewport.left = 0.f;

            cam.viewport.height = sizeRatio / viewRatio;
            cam.viewport.bottom = (1.f - cam.viewport.height) / 2.f;
        }
    };

    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateCam;
    updateCam(cam);
}

void MenuState::createUI()
{
    registerWindow([&]()
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("Utility"))
                {
                    if (ImGui::MenuItem("Video Player"))
                    {
                        showVideoPlayer = true;
                    }

                    if (ImGui::MenuItem("Generate Boilerplate"))
                    {
                        showBoilerplate = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            if (showVideoPlayer)
            {
                if (ImGui::Begin("MPG1 Playback", &showVideoPlayer))
                {
                    static std::string label("No file open");
                    ImGui::Text("%s", label.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Open video"))
                    {
                        auto path = cro::FileSystem::openFileDialogue("", "mpg");
                        if (!path.empty())
                        {
                            if (!m_video.loadFromFile(path))
                            {
                                cro::FileSystem::showMessageBox("Error", "Could not open file");
                                label = "No file open";
                            }
                            else
                            {
                                label = cro::FileSystem::getFileName(path);
                            }
                        }
                    }

                    ImGui::Image(m_video.getTexture(), { 352.f, 288.f }, { 0.f, 1.f }, { 1.f, 0.f });

                    if (ImGui::Button("Play"))
                    {
                        m_video.play();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Pause"))
                    {
                        m_video.pause();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop"))
                    {
                        m_video.stop();
                    }
                    ImGui::SameLine();
                    auto looped = m_video.getLooped();
                    if (ImGui::Checkbox("Loop", &looped))
                    {
                        m_video.setLooped(looped);
                    }

                    ImGui::Text("%3.3f / %3.3f", m_video.getPosition(), m_video.getDuration());

                    ImGui::SameLine();
                    if (ImGui::Button("Jump"))
                    {
                        m_video.seek(100.f);
                    }
                }
                ImGui::End();
            }

            if (showBoilerplate)
            {
                if (ImGui::Begin("Create Boilerplate", &showBoilerplate))
                {
                    static std::string textBuffer;
                    ImGui::InputText("Name:", &textBuffer);
                    ImGui::SameLine();

                    if (ImGui::Button("Create"))
                    {
                        if (!textBuffer.empty())
                        {
                            if (!createStub(textBuffer))
                            {
                                cro::FileSystem::showMessageBox("Error", "Stub not created (already exists?)");
                            }
                            else
                            {
                                std::stringstream ss;
                                ss << "Created stub files for " << textBuffer << "State in src/" << textBuffer << "\n\n";
                                ss << "Add the files to the project, register the state in MyApp\n";
                                ss << "and add " << textBuffer << " to the StateIDs enum\n";

                                cro::FileSystem::showMessageBox("Success", ss.str());
                            }
                        }

                        textBuffer.clear();
                    }
                }
                ImGui::End();
            }
        });
}

bool MenuState::createStub(const std::string& name)
{
    auto className = cro::Util::String::toLower(name);

    std::array badChar =
    {
        ' ', '\t', '\\', '/', ';',
        '\r', '\n'
    };

    for (auto c : badChar)
    {
        std::replace(className.begin(), className.end(), c, '_');
    }
    
    std::string path = "src/" + className;
    if (cro::FileSystem::directoryExists(path))
    {
        LogE << name << ": path already exists" << std::endl;
        return false;
    }

    if (!cro::FileSystem::createDirectory(path))
    {
        LogE << "Failed creating " << path << std::endl;
        return false;
    }

    auto frontStr = cro::Util::String::toUpper(className.substr(0, 1));
    className[0] = frontStr[0];

    std::ofstream cmakeFile(path + "/CMakeLists.txt");
    if (cmakeFile.is_open() && cmakeFile.good())
    {
        cmakeFile << "set(" << cro::Util::String::toUpper(className) << "_SRC\n";
        cmakeFile << "  ${PROJECT_DIR}/" << cro::Util::String::toLower(className) << "/" << className << "State.cpp)";

        cmakeFile.close();
    }

    auto headerPath = path + "/" + className + "State.hpp";
    std::ofstream headerFile(headerPath);
    if (headerFile.is_open() && headerFile.good())
    {
        headerFile << "//Auto-generated header file for Scratchpad Stub " << cro::SysTime::dateString() << ", " << cro::SysTime::timeString() << "\n\n";
        headerFile << "#pragma once\n\n";
        headerFile << "#include \"../StateIDs.hpp\"\n\n";
        headerFile << "#include <crogine/core/State.hpp>\n#include <crogine/ecs/Scene.hpp>\n#include <crogine/gui/GuiClient.hpp>\n#include <crogine/graphics/ModelDefinition.hpp>\n\n";

        headerFile << "class " << className << "State final : public cro::State, public cro::GuiClient\n{\n";
        headerFile << "public:\n    " << className << "State(cro::StateStack&, cro::State::Context);\n\n";
        headerFile << "    cro::StateID getStateID() const override { return States::ScratchPad::" << name << "; }\n\n";
        headerFile << "    bool handleEvent(const cro::Event&) override;\n";
        headerFile << "    void handleMessage(const cro::Message&) override;\n";
        headerFile << "    bool simulate(float) override;\n";
        headerFile << "    void render() override;\n\n";

        headerFile << "private:\n\n";
        headerFile << "    cro::Scene m_gameScene;\n";
        headerFile << "    cro::Scene m_uiScene;\n";
        headerFile << "    cro::ResourceCollection m_resources;\n\n";
        headerFile << "    void addSystems();\n";
        headerFile << "    void loadAssets();\n";
        headerFile << "    void createScene();\n";
        headerFile << "    void createUI();\n\n";

        headerFile << "};";
        headerFile.close();
    }
    else
    {
        LogE << "Failed creating " << headerPath << std::endl;
        return false;
    }

    auto cppPath = path + "/" + className + "State.cpp";
    std::ofstream cppFile(cppPath);
    if (cppFile.is_open() && cppFile.good())
    {
        cppFile << "//Auto-generated source file for Scratchpad Stub " << cro::SysTime::dateString() << ", " << cro::SysTime::timeString() << "\n\n";

        cppFile << "#include \"" << className << "State.hpp\"\n";

        cppFile << R"(
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>

)";

        cppFile << className << "State::" << className << "State(cro::StateStack& stack, cro::State::Context context)";
        cppFile << R"(
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
)";

        cppFile << "bool " << className << "State::handleEvent(const cro::Event& evt)";
        cppFile << R"(
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

)";
        cppFile << "void " << className << "State::handleMessage(const cro::Message& msg)";
        cppFile << R"(
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

)";

        cppFile << "bool " << className << "State::simulate(float dt)";
        cppFile << R"(
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

)";
        cppFile << "void " << className << "State::render()";
        cppFile << R"(
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
)";

        cppFile << "void " << className << "State::addSystems()";
        cppFile << R"(
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

)";

        cppFile << "void " << className << "State::loadAssets()\n{\n}\n\n";
        cppFile << "void " << className << "State::createScene()";
        cppFile << R"(
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 2.f });
}

)";

        cppFile << "void " << className << "State::createUI()";
        cppFile << R"(
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
})";

        cppFile.close();
    }
    else
    {
        LogE << "Failed creating " << cppPath << std::endl;
        return false;
    }


    return true;
}