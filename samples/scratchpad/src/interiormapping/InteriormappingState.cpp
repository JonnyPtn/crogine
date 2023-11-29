//Auto-generated source file for Scratchpad Stub 28/11/2023, 13:22:27

#include "InteriorMappingState.hpp"

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

#include <crogine/graphics/BinaryMeshBuilder.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>

namespace
{
#include "IMShader.inl"

    struct ShaderID final
    {
        enum
        {
            IntMapping,

            Count
        };
    };
    std::array<std::uint32_t, ShaderID::Count> shaderIDs = {};

    struct MaterialID final
    {
        enum
        {
            InMapping = 1
        };
    };
}

InteriorMappingState::InteriorMappingState(cro::StateStack& stack, cro::State::Context context)
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
bool InteriorMappingState::handleEvent(const cro::Event& evt)
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

void InteriorMappingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool InteriorMappingState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void InteriorMappingState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void InteriorMappingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void InteriorMappingState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::IntMapping, IMVertex, IMFragment);
    m_resources.materials.add(MaterialID::InMapping, m_resources.shaders.get(ShaderID::IntMapping));

    m_resources.materials.get(MaterialID::InMapping).setProperty("u_roomTexture", m_resources.textures.get("assets/images/interior.png"));
}

void InteriorMappingState::createScene()
{
    cro::ModelDefinition md(m_resources);
    //if (md.loadFromFile("assets/models/cylinder_tangents.cmt"))
    //{
    //    auto entity = m_gameScene.createEntity();
    //    entity.addComponent<cro::Transform>().setPosition({ -1.5, 0.f, 0.f });
    //    md.createModel(entity);

    //    entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(MaterialID::InMapping));

    //    //TODO refactor this to a sngle func that take the entity as a param
    //    registerWindow([&, entity]() mutable
    //        {
    //            if (ImGui::Begin("Cylinder"))
    //            {
    //                auto pos = entity.getComponent<cro::Transform>().getPosition();

    //                if (ImGui::SliderFloat("X", &pos.x, -3.f, 3.f))
    //                {
    //                    pos.x = std::clamp(pos.x, -3.f, 3.f);
    //                    entity.getComponent<cro::Transform>().setPosition(pos);
    //                }

    //                if (ImGui::SliderFloat("Y", &pos.y, -2.f, 2.f))
    //                {
    //                    pos.y = std::clamp(pos.y, -2.f, 2.f);
    //                    entity.getComponent<cro::Transform>().setPosition(pos);
    //                }

    //                static glm::vec3 rotation = glm::vec3(0.f);
    //                if (ImGui::SliderFloat("Rotation X", &rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
    //                {
    //                    rotation.x = std::clamp(rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI);
    //                    entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
    //                }
    //            }
    //            ImGui::End();
    //        });
    //}

    if (md.loadFromFile("assets/models/plane_tangents.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(MaterialID::InMapping));

        registerWindow([&, entity]() mutable
            {
                if (ImGui::Begin("Plane"))
                {
                    auto pos = entity.getComponent<cro::Transform>().getPosition();
                    
                    if (ImGui::SliderFloat("X", &pos.x, -3.f, 3.f))
                    {
                        pos.x = std::clamp(pos.x, -3.f, 3.f);
                        entity.getComponent<cro::Transform>().setPosition(pos);
                    }
                    
                    if (ImGui::SliderFloat("Y", &pos.y, -2.f, 2.f))
                    {
                        pos.y = std::clamp(pos.y, -2.f, 2.f);
                        entity.getComponent<cro::Transform>().setPosition(pos);
                    }

                    static glm::vec3 rotation = glm::vec3(0.f);
                    if (ImGui::SliderFloat("Rotation X", &rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
                    {
                        rotation.x = std::clamp(rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }
                    if (ImGui::SliderFloat("Rotation Y", &rotation[2], -cro::Util::Const::PI, cro::Util::Const::PI))
                    {
                        rotation.z = std::clamp(rotation[2], -cro::Util::Const::PI, cro::Util::Const::PI);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }
                    if (ImGui::SliderFloat("Rotation Z", &rotation[1], -cro::Util::Const::PI, cro::Util::Const::PI))
                    {
                        rotation.y = std::clamp(rotation[1], -cro::Util::Const::PI, cro::Util::Const::PI);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }
                    if (ImGui::Button("Reset"))
                    {
                        rotation = glm::vec3(0.f);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }

                    static glm::vec3 scale(1.f);
                    if (ImGui::SliderFloat("Scale X", &scale.x, 0.5f, 2.f))
                    {
                        scale.x = std::clamp(scale.x, 0.5f, 2.f);
                        entity.getComponent<cro::Transform>().setScale(scale);
                    }
                }
                ImGui::End();
            });
    }

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 1.f, 3.5f });
    //m_gameScene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.4f);
}

void InteriorMappingState::createUI()
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
}