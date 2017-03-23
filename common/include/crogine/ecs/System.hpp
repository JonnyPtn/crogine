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

#ifndef CRO_SYSTEM_HPP_
#define CRO_SYSTEM_HPP_

#include <crogine/Config.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/Component.hpp>

#include <vector>
#include <typeindex>

namespace cro
{
    class Time;

    using UniqueType = std::type_index;

    /*!
    \brief Base class for systems.
    Systems should all derive from this base class, and instanciated before any entities
    are created. Concrete system types should declare a list component types via requireComponent()
    on construction, so that only entities with the relevant components are added to the system.
    */
    class CRO_EXPORT_API System
    {
    public:

        using Ptr = std::unique_ptr<System>;

        /*!
        \brief Constructor.
        Pass in a reference to the concrete implementation to generate
        a unique type ID for this system.
        */
        template <typename T>
        explicit System(T* c) : m_type(typeid(*c)){}

        virtual ~System() = default;

        /*!
        \brief Returns the unique type ID of the system
        */
        UniqueType getType() const { return m_type; }

        /*!
        \brief Adds a component type to the list of components required by the 
        system for it to be interested in a particular entity.
        */
        template <typename T>
        void requireComponent();

        /*!
        \brief Returns a list of entities that thisd system is currently interested in
        */
        std::vector<Entity> getEntities() const;

        /*!
        \brief Adds an entity to the list to process
        */
        void addEntity(Entity);

        /*!
        \brief Removes an entity from the list to process
        */
        void removeEntity(Entity);

        /*!
        \brief Returns the component mask used to mask entities with corresponding
        components for this system to process
        */
        const ComponentMask& getComponentMask() const;

        /*!
        \brief Implement this for system specific processing to entities.
        */
        virtual void process(cro::Time);

    protected:
        std::vector<Entity>& getEntities() { return m_entities; }

    private:

        UniqueType m_type;

        ComponentMask m_componentMask;
        std::vector<Entity> m_entities;

        friend class SystemManager;
    };

    class CRO_EXPORT_API SystemManager final
    {
    public:
        SystemManager();

        ~SystemManager() = default;
        SystemManager(const SystemManager&) = delete;
        SystemManager(const SystemManager&&) = delete;
        SystemManager& operator = (const SystemManager&) = delete;
        SystemManager& operator = (const SystemManager&&) = delete;

        /*!
        \brief Adds a system of a given type to the manager.
        If the system already exists nothing is changed.
        \returns Reference to the system, fo instance a rendering
        system maybe required elsewhere so a reference to it can be kept.
        */
        template <typename T, typename... Args>
        T& addSystem(Args&&... args);

        /*!
        \brief Removes the system of this type, if it exists
        */
        template <typename T>
        void removeSystem();

        /*!
        \brief Returns a reference to this system type, if it exists
        */
        template <typename T>
        T& getSystem();

        /*!
        \brief Returns true if a system of this type exists within the manager
        */
        template <typename T>
        bool hasSystem() const;

        /*!
        \brief Submits an entity to all available systems
        */
        void addToSystems(Entity);

        /*!
        \brief Removes the given Entity from any systems to which it may belong
        */
        void removeFromSystems(Entity);

        /*!
        \brief Runs a simulation step by calling process() on each system
        */
        void process(Time);
    private:

        std::vector<std::unique_ptr<System>> m_systems;
    };

#include "System.inl"
#include "SystemManager.inl"
}

#endif //CRO_SYSTEM_HPP_