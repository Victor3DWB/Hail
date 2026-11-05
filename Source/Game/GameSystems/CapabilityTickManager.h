#pragma once
#include "CapabilityBaseClass.h"
#include "Containers/GrowingArray/GrowingArray.h"
#include "Containers/StaticArray/StaticArray.h"

namespace Hail
{
    struct CapabilityTickSortable
    {
        ECapabilityTickGroup m_group = ECapabilityTickGroup::Gameplay;
        ECapabilityTickSubGroup m_subGroup = ECapabilityTickSubGroup::Normal;

        bool operator <(const CapabilityTickSortable& Right) const
        {
            if (m_group != Right.m_group)
                return m_group < Right.m_group;

            return m_subGroup < Right.m_subGroup;
        }

        bool operator >(const CapabilityTickSortable& Right) const
        {
            return Right < (*this);
        }
    };

    class CapabilityTickWorld;

    // ---

    class CapabilityTickLinkedElement
    {
    public:
        CapabilityTickLinkedElement() = default;

        void Tick(float DeltaTime);

        //void Register(CapabilityTickWorld* World);
        void Unregister();

        void SetSorting(CapabilityTickSortable NewSorting)
        {
            m_sortable = NewSorting;
        }

        bool GetIsRegistered() const
        {
            return m_pWorld != nullptr;
        }

        CapabilityTickSortable GetSorting() const
        {
            return m_sortable;
        }

        CapabilityTickSortable m_sortable;
        Capability* m_pCapability = nullptr;
        CapabilityTickWorld* m_pWorld = nullptr;
        CapabilityTickLinkedElement* m_pPrevious = nullptr;
        CapabilityTickLinkedElement* m_pNext = nullptr;
    };

    // ---

    class CapabilityTickLinkedList
    {
    public:
        CapabilityTickLinkedList();

        void TickAll(float DeltaTime);
        void Add(CapabilityTickLinkedElement& pTicker);

        CapabilityTickLinkedElement* m_pFirstElement = nullptr;
        CapabilityTickLinkedElement* m_pLastElement = nullptr;

        GrowingArray<CapabilityTickLinkedElement*> m_elementsToAdd;
    };

    // ---

    class CapabilityTickWorld
    {
    public:
        CapabilityTickWorld();

        void Init();
        void Tick(float DeltaTime);
        void RegisterCapability(Capability& capability);
        void UnRegisterCapability(Capability* pCapability);

    private:
        void RegisterTicker(CapabilityTickLinkedElement& Ticker);

        StaticArray<CapabilityTickLinkedList, (uint8_t)ECapabilityTickGroup::Max> m_tickLists;
        GrowingArray<CapabilityTickLinkedElement> m_capabilityElements;
        GrowingArray<uint32> m_freeCapabilityElementSlots;
    };
}