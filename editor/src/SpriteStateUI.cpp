/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "SpriteState.hpp"
#include "SharedStateData.hpp"
#include "UIConsts.hpp"
#include "Messages.hpp"
#include "GLCheck.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/gui/Gui.hpp>

void SpriteState::openSprite(const std::string& path)
{
    if (m_spriteSheet.loadFromFile(path, m_textures, m_sharedData.workingDirectory + "/"))
    {
        auto* texture = m_spriteSheet.getTexture();
        m_entities[EntityID::Root].getComponent<cro::Sprite>().setTexture(*texture);

        auto size = texture->getSize();
        auto position = glm::vec2(cro::App::getWindow().getSize() - size);
        position /= 2.f;
        m_entities[EntityID::Root].getComponent<cro::Transform>().setPosition(position);


        //update the border (transparent textures won't easliy stand out from the background)
        m_entities[EntityID::Border].getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(0.f, size.y), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(size), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(size.x, 0.f), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(0.f), cro::Colour::Cyan),
            }
        );

        m_currentFilePath = path;
        m_activeSprite = nullptr;
    }
}

void SpriteState::updateBoundsEntity()
{
    if (m_activeSprite)
    {
        m_entities[EntityID::Bounds].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

        auto bounds = m_activeSprite->second.getTextureRect();
        glm::vec2 position(bounds.left, bounds.bottom);
        glm::vec2 size(bounds.width, bounds.height);

        m_entities[EntityID::Bounds].getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(position, cro::Colour::Red),
                cro::Vertex2D(glm::vec2(position.x, position.y + size.y), cro::Colour::Red),
                cro::Vertex2D(position + size, cro::Colour::Red),
                cro::Vertex2D(glm::vec2(position.x + size.x, position.y), cro::Colour::Red),
                cro::Vertex2D(position, cro::Colour::Red)
            }
        );
    }
    else
    {
        m_entities[EntityID::Bounds].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
}

void SpriteState::createUI()
{
	registerWindow([this]() 
		{
			drawMenuBar();
            drawInspector();
            drawSpriteWindow();
            drawPreferences();
		});
    
    getContext().mainWindow.setTitle("Sprite Editor");
}

void SpriteState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        //file menu
        if (ImGui::BeginMenu("File##sprite"))
        {
            if (ImGui::MenuItem("Open##sprite", nullptr, nullptr))
            {
                auto path = cro::FileSystem::openFileDialogue("", "spt");
                if (!path.empty())
                {
                    std::replace(path.begin(), path.end(), '\\', '/');
                    if (path.find(m_sharedData.workingDirectory) == std::string::npos)
                    {
                        cro::FileSystem::showMessageBox("Error", "Sprite Sheet not opened from working directory");
                    }
                    else
                    {
                        openSprite(path);
                    }
                }
            }
            if (ImGui::MenuItem("Save##sprite", nullptr, nullptr))
            {
                if (!m_currentFilePath.empty())
                {
                    m_spriteSheet.saveToFile(m_currentFilePath);
                }
            }

            if (ImGui::MenuItem("Save As...##sprite", nullptr, nullptr))
            {
                auto path = cro::FileSystem::saveFileDialogue(m_currentFilePath, "spt");
                if (!path.empty())
                {
                    m_spriteSheet.saveToFile(path);
                    m_currentFilePath = path;
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

            if (ImGui::MenuItem("Quit##sprite", nullptr, nullptr))
            {
                cro::App::quit();
            }
            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View##sprite"))
        {
            ImGui::MenuItem("Options", nullptr, &m_showPreferences);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void SpriteState::drawInspector()
{
    ImGui::SetNextWindowSize({ 480.f, 700.f });
    ImGui::Begin("Inspector##sprite", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button("Add##sprite"))
    {
        m_activeSprite = nullptr;
        updateBoundsEntity();
    }
    if (m_activeSprite)
    {
        ImGui::SameLine();
        if (ImGui::Button("Remove##sprite"))
        {
            m_activeSprite = nullptr;
            updateBoundsEntity();
        }
    }

    const auto* texture = m_spriteSheet.getTexture();
    glm::vec2 texSize(1.f);
    if (texture)
    {
        texSize = { texture->getSize() };

        ImGui::Text("Texture Path: %s", m_spriteSheet.getTexturePath().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse##texPath"))
        {

        }
        ImGui::Text("Texture Size: %d, %d", texture->getSize().x, texture->getSize().y);
        ImGui::NewLine();
    }


    auto i = 0;
    for (auto& spritePair : m_spriteSheet.getSprites())
    {
        auto& [name, sprite] = spritePair;

        std::string label = "Sprite - " + name + "##" + std::to_string(i);
        if (ImGui::TreeNode(label.c_str()))
        {
            if (ImGui::IsItemClicked())
            {
                m_activeSprite = &spritePair;
                updateBoundsEntity();
            }

            ImGui::Indent(uiConst::IndentSize);

            auto bounds = sprite.getTextureRect();

            if (texture)
            {
                ImVec2 uv0(bounds.left / texSize.x, ((bounds.bottom + bounds.height) / texSize.y));
                ImVec2 uv1((bounds.left + bounds.width) / texSize.x, (bounds.bottom / texSize.y));

                float ratio = bounds.height / bounds.width;
                ImVec2 size(std::min(bounds.width, 240.f), 0.f);
                size.y = size.x * ratio;

                ImGui::ImageButton(*texture, size, uv0, uv1);
            }

            ImGui::Text("Bounds: %3.3f, %3.3f, %3.3f, %3.3f", bounds.left, bounds.bottom, bounds.width, bounds.height);
            ImGui::NewLine();
            ImGui::Indent(uiConst::IndentSize);

            auto j = 0u;
            for (const auto& anim : sprite.getAnimations())
            {
                label = "Frame Rate: " + std::to_string(anim.framerate);
                ImGui::Text("%s", label.c_str());

                if (anim.looped)
                {
                    label = "Looped: true";
                }
                else
                {
                    label = "Looped: false";
                }
                ImGui::Text("%s", label.c_str());

                label = "Loop Start: " + std::to_string(anim.loopStart);
                ImGui::Text("%s", label.c_str());

                label = "Frame Count: " + std::to_string(anim.frames.size());
                ImGui::Text("%s", label.c_str());
                ImGui::NewLine();

                j++;
            }
            ImGui::Unindent(uiConst::IndentSize);
            ImGui::Unindent(uiConst::IndentSize);
            ImGui::TreePop();
        }
        if (ImGui::IsItemClicked())
        {
            m_activeSprite = &spritePair;
            updateBoundsEntity();
        }


        i++;
    }

    ImGui::End();
}

void SpriteState::drawSpriteWindow()
{
    if (m_activeSprite)
    {
        ImGui::SetNextWindowSize({ 480.f, 480.f });
        ImGui::Begin("Edit Sprite");
        auto& [name, sprite] = *m_activeSprite;
        ImGui::Text("%s", name.c_str());

        auto* texture = sprite.getTexture();
        glm::vec2 size(texture->getSize());

        if (texture)
        {
            auto bounds = sprite.getTextureRect();
            if (ImGui::SliderFloat4("Bounds", reinterpret_cast<float*>(&bounds), 0.f, size.x > size.y ? size.x : size.y, "%.2f"))
            {
                bounds.left = std::max(0.f, bounds.left);
                bounds.bottom = std::max(0.f, bounds.bottom);
                bounds.width = std::max(1.f, bounds.width);
                bounds.height = std::max(1.f, bounds.height);

                bounds.left = std::min(bounds.left, size.x - bounds.width);
                bounds.bottom = std::min(bounds.bottom, size.y - bounds.height);

                sprite.setTextureRect(bounds);

                updateBoundsEntity();
            }
        }

        ImGui::End();
    }
}

void SpriteState::drawPreferences()
{
    if (m_showPreferences)
    {
        ImGui::SetNextWindowSize({ 400.f, 260.f });
        if (ImGui::Begin("Preferences##sprite", &m_showPreferences))
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
                uiConst::showTipMessage(m_sharedData.workingDirectory.c_str());
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

            if (!m_showPreferences ||
                ImGui::Button("Close"))
            {
                //notify so the global prefs are also written
                auto* msg = getContext().appInstance.getMessageBus().post<UIEvent>(MessageID::UIMessage);
                msg->type = UIEvent::WrotePreferences;

                m_showPreferences = false;
            }
        }
        ImGui::End();
    }
}