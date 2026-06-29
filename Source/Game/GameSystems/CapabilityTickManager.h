#pragma once
#include "CapabilityBaseClass.h"
#include "Containers/GrowingArray/GrowingArray.h"
#include "Containers/StaticArray/StaticArray.h"


namespace Hail
{

    struct CapabilityTickSortable
    {
        ECapabilityTickGroup Group = ECapabilityTickGroup::Gameplay;
        ECapabilityTickSubGroup SubGroup = ECapabilityTickSubGroup::Normal;

        bool operator <(const CapabilityTickSortable& Right) const
        {
            if ((int)Group < (int)Right.Group)
                return true;
            if ((int)Group > (int)Right.Group)
                return false;
            if ((int)SubGroup < (int)Right.SubGroup)
                return true;
            /*if ((int)SubGroup > (int)Right.SubGroup)
                return false;*/
            return false;
        }

        bool operator >(const CapabilityTickSortable& Right) const
        {
            return !(*this < Right);
        }
    };

    class CapabilityTickWorld;

    // ---

    class CapabilityTickLinkedElement
    {
    public:
        CapabilityTickLinkedElement() {};

        void Tick(float DeltaTime);

        //void Register(CapabilityTickWorld* World);
        void Unregister();

        void SetSorting(CapabilityTickSortable NewSorting)
        {
            Sortable = NewSorting;
        }

        bool GetIsRegistered() const
        {
            return pWorld != nullptr;
        }

        CapabilityTickSortable GetSorting() const
        {
            return Sortable;
        }

        CapabilityTickSortable Sortable;
        Capability* pCapability = nullptr;
        CapabilityTickWorld* pWorld = nullptr;
        CapabilityTickLinkedElement* pPrevious = nullptr;
        CapabilityTickLinkedElement* pNext = nullptr;
    };

    // ---

    class CapabilityTickLinkedList
    {
    public:
        CapabilityTickLinkedList();

        void TickAll(float DeltaTime);
        void Add(CapabilityTickLinkedElement& pTicker);

        CapabilityTickLinkedElement* pFirstElement = nullptr;
        CapabilityTickLinkedElement* pLastElement = nullptr;

        GrowingArray<CapabilityTickLinkedElement*> ElementsToAdd;
    };

    // ---

    class CapabilityTickWorld
    {
    public:
        CapabilityTickWorld();

        void Init();
        void Tick(float DeltaTime);
        void RegisterCapability(Capability& pCapability);

    private:
        void RegisterTicker(CapabilityTickLinkedElement& Ticker);

        StaticArray<CapabilityTickLinkedList, (uint8_t)ECapabilityTickGroup::Max> TickLists;
        GrowingArray<CapabilityTickLinkedElement> CapabilityElements;
    };
}