#include "ParticleState.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/util/Matrix.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    enum WindowID
    {
        Inspector,
        Browser,
        ViewGizmo,
        Info,

        Count
    };

    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(), glm::vec2())
    };

    std::string lastSavePath;
}

void ParticleState::initUI()
{
    loadPrefs();

    registerWindow(
        [&]()
        {
            //ImGui::ShowDemoWindow();
            drawMenuBar();
            drawInspector();
            drawBrowser();
            drawInfo();
            drawGizmo();
        });

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void ParticleState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        //file menu
        if (ImGui::BeginMenu("File##Particle"))
        {
            if (ImGui::MenuItem("Open##Particle", nullptr, nullptr))
            {
                auto path = cro::FileSystem::openFileDialogue(lastSavePath, "xyp,cps");
                if (!path.empty())
                {
                    (m_particleSettings->loadFromFile(path, m_resources.textures));
                    {
                        lastSavePath = path;

                        m_selectedBlendMode = m_particleSettings->blendmode;
                        m_particleSettings->textureID = 0;

                        if (!m_particleSettings->texturePath.empty())
                        {
                            auto texPath = m_sharedData.workingDirectory + "/" + m_particleSettings->texturePath;
                            if (cro::FileSystem::fileExists(texPath)
                                && m_texture.loadFromFile(texPath))
                            {
                                m_particleSettings->textureID = m_texture.getGLHandle();
                                m_particleSettings->textureSize = m_texture.getSize();
                                m_texture.setSmooth(true);
                                m_texture.setRepeated(true);
                            }
                        }
                    }
                }
            }
            if (ImGui::MenuItem("Save##Particle", nullptr, nullptr))
            {
                if (!lastSavePath.empty())
                {
                    m_particleSettings->saveToFile(lastSavePath);
                }
                else
                {
                    auto path = cro::FileSystem::saveFileDialogue(lastSavePath, "cps");
                    if (!path.empty())
                    {
                        m_particleSettings->saveToFile(path);
                        lastSavePath = path;
                    }
                }
            }

            if (ImGui::MenuItem("Save As...##Particle", nullptr, nullptr))
            {
                auto path = cro::FileSystem::saveFileDialogue(lastSavePath, "cps");
                if (!path.empty())
                {
                    m_particleSettings->saveToFile(path);
                    lastSavePath = path;
                }
            }

            if (getStateCount() > 1)
            {
                if (ImGui::MenuItem("Return To World Editor"))
                {
                    getContext().mainWindow.setTitle("Crogine Editor");
                    requestStackPop();
                }
            }

            if (ImGui::MenuItem("Quit##Particle", nullptr, nullptr))
            {
                cro::App::quit();
            }
            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View##Particle"))
        {
            if (ImGui::MenuItem("Options", nullptr, nullptr))
            {
                m_showPreferences = !m_showPreferences;
            }

            if (ImGui::MenuItem("Load Preview Model", nullptr, nullptr))
            {
                if (m_sharedData.workingDirectory.empty())
                {
                    cro::FileSystem::showMessageBox("Warning", "Working Directory not set.\nModel may not load correctly.");
                }

                //TODO track last used path
                auto path = cro::FileSystem::openFileDialogue("", "cmt");
                if (!path.empty())
                {
                    openModel(path);
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (m_showPreferences)
    {
        drawOptions();
    }
}

void ParticleState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector##Particle", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("Properties");
        if (ImGui::BeginTabItem("Appearance"))
        {
            //lifetime
            ImGui::SliderFloat("Lifetime", &m_particleSettings->lifetime, 0.1f, 10.f);

            //spread
            ImGui::SliderFloat("Spread", &m_particleSettings->spread, 0.f, 360.f);

            //scale modifier
            ImGui::SliderFloat("Scale Modifier", &m_particleSettings->scaleModifier, -5.f, 5.f);

            //acceleration
            ImGui::SliderFloat("Acceleration", &m_particleSettings->acceleration, 0.f, 10.f);
            
            //size
            ImGui::SliderFloat("Size", &m_particleSettings->size, 0.01f, 1.f);
            
            //emit rate
            ImGui::SliderFloat("Emit Rate", &m_particleSettings->emitRate, 0.1f, 50.f);
            
            //rotation speed
            ImGui::SliderFloat("Rotation Speed", &m_particleSettings->rotationSpeed, -180.f, 180.f);
            
            //spawn radius
            ImGui::SliderFloat("Spawn Radius", &m_particleSettings->spawnRadius, 0.f, 4.f);

            //----Animation----
            ImGui::Separator();


            //frame count
            std::int32_t count = static_cast<std::int32_t>(m_particleSettings->frameCount);
            if (ImGui::InputInt("Frame Count", &count))
            {
                count = std::max(1, count);
                m_particleSettings->frameCount = count;
            }
            
            //frame rate
            ImGui::InputFloat("Frame Rate", &m_particleSettings->framerate, 0.1f, 50.f);
            
            //animate
            ImGui::Checkbox("Animate", &m_particleSettings->animate);
            
            //use random frame
            ImGui::Checkbox("Use Random Frame", &m_particleSettings->useRandomFrame);


            ImGui::Separator();

            //colour
            ImGui::ColorEdit4("Colour", m_particleSettings->colour.asArray());

            //blend mode
            if (ImGui::Combo("Blend Mode", &m_selectedBlendMode, "Alpha\0Add\0Multiply\0\0"))
            {
                m_particleSettings->blendmode = static_cast<cro::EmitterSettings::BlendMode>(m_selectedBlendMode);
            }
            
            //open tetxure
            if (m_particleSettings->texturePath.empty())
            {
                ImGui::Text("No Texture Loaded.");
            }
            else
            {
                ImGui::Text("%s", m_particleSettings->texturePath.c_str());
            }


            if (ImGui::Button("Set Texture"))
            {
                auto path = cro::FileSystem::openFileDialogue(m_sharedData.workingDirectory, "png,jpg,bmp");
                if (!path.empty()
                    && m_texture.loadFromFile(path))
                {
                    m_texture.setSmooth(true);
                    m_texture.setRepeated(true);

                    std::replace(path.begin(), path.end(), '\\', '/');

                    //try correcting with current working directory
                    if (!m_sharedData.workingDirectory.empty())
                    {
                        if (path.find(m_sharedData.workingDirectory) != std::string::npos)
                        {
                            path = path.substr(m_sharedData.workingDirectory.size());
                        }
                    }
                    m_particleSettings->textureID = m_texture.getGLHandle();
                    m_particleSettings->texturePath = path;
                    m_particleSettings->textureSize = m_texture.getSize();
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Behaviour"))
        {
            //initial velocity
            ImGui::SliderFloat3("Initial Velocity", &m_particleSettings->initialVelocity[0], -5.f, 5.f);
            
            //gravity
            ImGui::SliderFloat3("Gravity", &m_particleSettings->gravity[0], -5.f, 5.f);
            
            //spawn offset
            ImGui::SliderFloat3("Spawn Offset", &m_particleSettings->spawnOffset[0], -5.f, 5.f);
            
            //forces
            
            
            //emit count
            std::int32_t count = static_cast<std::int32_t>(m_particleSettings->emitCount);
            if (ImGui::InputInt("Emit Count", &count))
            {
                count = std::max(1, count);
                m_particleSettings->emitCount = static_cast<std::uint32_t>(count);
            }
            ImGui::SameLine();
            uiConst::showToolTip("Number of particles released each tick");
            
            //release count
            count = m_particleSettings->releaseCount;
            if (ImGui::InputInt("Release Count", &count))
            {
                count = std::max(0, count);
                m_particleSettings->releaseCount = count;
            }
            ImGui::SameLine();
            uiConst::showToolTip("Total number of particles to release before the system stops. 0 spawns particles infinitely");

            //random initial rotation
            ImGui::Checkbox("Random Initial Rotation", &m_particleSettings->randomInitialRotation);

            //inherit rotation
            ImGui::Checkbox("Inherit Rotation", &m_particleSettings->inheritRotation);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();

        if (m_entities[EntityID::Emitter].getComponent<cro::ParticleEmitter>().stopped())
        {
            if (ImGui::Button("Start"))
            {
                m_entities[EntityID::Emitter].getComponent<cro::ParticleEmitter>().start();
            }
        }
        else
        {
            if (ImGui::Button("Stop"))
            {
                m_entities[EntityID::Emitter].getComponent<cro::ParticleEmitter>().stop();
            }
        }
    }
    ImGui::End();
}

void ParticleState::drawBrowser()
{
    auto [pos, size] = WindowLayouts[WindowID::Browser];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser##Particle", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        
    }

    ImGui::End();
}

void ParticleState::drawInfo()
{
    auto [pos, size] = WindowLayouts[WindowID::Info];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("InfoBar##Particle", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, m_messageColour);
        ImGui::Text("%s", cro::Console::getLastOutput().c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        auto cursor = ImGui::GetCursorPos();
        cursor.y -= 4.f;
        ImGui::SetCursorPos(cursor);

        if (ImGui::Button("More...", ImVec2(56.f, 20.f)))
        {
            cro::Console::show();
        }
    }
    ImGui::End();
}

void ParticleState::drawGizmo()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(io.DisplaySize.x * uiConst::InspectorWidth, 0, io.DisplaySize.x - (uiConst::InspectorWidth * io.DisplaySize.x),
        io.DisplaySize.y - (uiConst::BrowserHeight * io.DisplaySize.y));

    auto [pos, size] = WindowLayouts[WindowID::ViewGizmo];

    //view cube doohickey
    auto tx = glm::inverse(m_entities[EntityID::ArcBall].getComponent<cro::Transform>().getLocalTransform());
    ImGuizmo::ViewManipulate(&tx[0][0], 10.f, ImVec2(pos.x, pos.y), ImVec2(size.x, size.y), 0/*x10101010*/);
    m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setRotation(glm::inverse(tx));

    //tooltip - TODO change this as it's not accurate
    if (io.MousePos.x > pos.x && io.MousePos.x < pos.x + size.x
        && io.MousePos.y > pos.y && io.MousePos.y < pos.y + size.y)
    {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, 0);
        ImGui::PushStyleColor(ImGuiCol_Border, 0);
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

        auto p = ImGui::GetCursorPos();
        ImGui::PushStyleColor(ImGuiCol_Text, 0xff000000);
        ImGui::TextUnformatted("Left Click Rotate\nMiddle Mouse Pan\nScroll Zoom");
        ImGui::PopStyleColor();

        p.x -= 1.f;
        p.y -= 1.f;
        ImGui::SetCursorPos(p);
        ImGui::TextUnformatted("Left Click Rotate\nMiddle Mouse Pan\nScroll Zoom");

        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        ImGui::PopStyleColor(2);
    }

    //draw gizmo if selected ent is valid
    if (m_selectedEntity.isValid())
    {
        const auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
        tx = m_selectedEntity.getComponent<cro::Transform>().getLocalTransform();
        ImGuizmo::Manipulate(&cam.getActivePass().viewMatrix[0][0], &cam.getProjectionMatrix()[0][0], static_cast<ImGuizmo::OPERATION>(m_gizmoMode), ImGuizmo::MODE::LOCAL, &tx[0][0]);
        m_selectedEntity.getComponent<cro::Transform>().setLocalTransform(tx);
    }
}

void ParticleState::drawOptions()
{
    if (m_showPreferences)
    {
        ImGui::SetNextWindowSize({ 400.f, 260.f });
        if (ImGui::Begin("Preferences##world", &m_showPreferences))
        {
            ImGui::Text("%s", "Working Directory:");
            if (m_sharedData.workingDirectory.empty())
            {
                ImGui::Text("%s", "Not Set");
            }
            else
            {
                auto dir = m_sharedData.workingDirectory.substr(0, 30) + "...";
                ImGui::Text("%s", dir.c_str());
                ImGui::SameLine();
                uiConst::showToolTip(m_sharedData.workingDirectory.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                auto path = cro::FileSystem::openFolderDialogue(m_sharedData.workingDirectory);
                if (!path.empty())
                {
                    m_sharedData.workingDirectory = path;
                    std::replace(m_sharedData.workingDirectory.begin(), m_sharedData.workingDirectory.end(), '\\', '/');
                }
            }

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            auto skyColour = getContext().appInstance.getClearColour();
            if (ImGui::ColorEdit3("Sky Colour", skyColour.asArray()))
            {
                getContext().appInstance.setClearColour(skyColour);
            }

            ImGui::NewLine();
            ImGui::NewLine();
            if (!m_showPreferences ||
                ImGui::Button("Close"))
            {
                savePrefs();
                m_showPreferences = false;
            }
        }
        ImGui::End();
    }
}

void ParticleState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);
    WindowLayouts[WindowID::Inspector] = 
        std::make_pair(glm::vec2(0.f, uiConst::TitleHeight),
            glm::vec2(width * uiConst::InspectorWidth, height - (uiConst::TitleHeight + uiConst::InfoBarHeight)));

    WindowLayouts[WindowID::Browser] =
        std::make_pair(glm::vec2(width * uiConst::InspectorWidth, height - (height * uiConst::BrowserHeight) - uiConst::InfoBarHeight),
            glm::vec2(width - (width * uiConst::InspectorWidth), height * uiConst::BrowserHeight));

    WindowLayouts[WindowID::Info] =
        std::make_pair(glm::vec2(0.f, height - uiConst::InfoBarHeight), glm::vec2(width, uiConst::InfoBarHeight));

    WindowLayouts[WindowID::ViewGizmo] =
        std::make_pair(glm::vec2(width - uiConst::ViewManipSize, uiConst::TitleHeight), glm::vec2(uiConst::ViewManipSize, uiConst::ViewManipSize));
}

void ParticleState::updateMouseInput(const cro::Event& evt)
{
    //TODO this isn't really consistent with the controls for model viewer...

    const float moveScale = 0.004f;
    if (evt.motion.state & SDL_BUTTON_RMASK)
    {
        float pitchMove = static_cast<float>(evt.motion.yrel) * moveScale * m_viewportRatio;
        float yawMove = static_cast<float>(evt.motion.xrel) * moveScale;

        auto& tx = m_scene.getActiveCamera().getComponent<cro::Transform>();
        tx.rotate(cro::Transform::X_AXIS, -pitchMove);
        tx.rotate(cro::Transform::Y_AXIS, -yawMove);
    }
    //else if (evt.motion.state & SDL_BUTTON_RMASK)
    //{
    //    //do roll
    //    float rollMove = static_cast<float>(-evt.motion.xrel) * moveScale;

    //    auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
    //    tx.rotate(cro::Transform::Z_AXIS, -rollMove);
    //}
    else if (evt.motion.state & SDL_BUTTON_MMASK)
    {
        auto camTx = m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldTransform();

        auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
        tx.move(cro::Util::Matrix::getRightVector(camTx) * -static_cast<float>(evt.motion.xrel) / 160.f);
        tx.move(cro::Util::Matrix::getUpVector(camTx) * static_cast<float>(evt.motion.yrel) / 160.f);
    }
}

void ParticleState::openModel(const std::string& path)
{
    cro::ModelDefinition modelDef(m_sharedData.workingDirectory);
    if (modelDef.loadFromFile(path, m_resources, &m_environmentMap))
    {
        if (m_entities[EntityID::Model].isValid())
        {
            m_scene.destroyEntity(m_entities[EntityID::Model]);
        }

        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        modelDef.createModel(entity, m_resources);

        m_entities[EntityID::Emitter].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_entities[EntityID::Model] = entity;
    }
}