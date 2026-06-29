#include "Game_PCH.h"
#include "CapabilityTickManager.h"
#include "CapabilityBaseClass.h"

namespace Hail
{
	void CapabilityTickLinkedElement::Tick(float DeltaTime)
	{
		if (!pCapability->m_bDidLateSetup)
		{
			pCapability->m_bDidLateSetup = true;
			pCapability->LateSetup();
		}

		if (!pCapability->m_bActive && pCapability->ShouldActivate())
		{
			pCapability->m_bActive = true;
			pCapability->m_activeDuration = 0.0;
			pCapability->m_inactiveDuration = 0.0;
			pCapability->OnActivated();
		}
		else if (pCapability->m_bActive && pCapability->ShouldDeactivate())
		{
			pCapability->m_bActive = false;
			pCapability->m_activeDuration = 0.0;
			pCapability->m_inactiveDuration = 0.0;
			pCapability->OnDeactivated();
		}

		if (pCapability->m_bActive)
		{
			pCapability->TickActive(DeltaTime * pCapability->GetTimeDilation());
			pCapability->m_activeDuration += DeltaTime * pCapability->GetTimeDilation();
		}
		else
		{
			pCapability->TickInactive(DeltaTime * pCapability->GetTimeDilation());
			pCapability->m_inactiveDuration += DeltaTime * pCapability->GetTimeDilation();
		}
	}

	/*void CapabilityTickLinkedElement::Register(CapabilityTickWorld* World)
	{
		assert(pWorld == nullptr);
		pWorld = World;
		pWorld->RegisterTicker(*this);
	}*/

	void CapabilityTickLinkedElement::Unregister()
	{
		assert(pWorld != nullptr);

	}

	// ---

	void SortWithQuick(GrowingArray<CapabilityTickLinkedElement*>* aArray, uint8_t left, uint8_t right)
	{
		uint8_t i = left, j = right;
		CapabilityTickLinkedElement* temp;
		const CapabilityTickLinkedElement* pivot = (*aArray)[(left + right) / 2];

		/* partition */
		while (i <= j)
		{
			while ((*aArray)[i]->Sortable < pivot->Sortable)
				i++;
			while ((*aArray)[j]->Sortable > pivot->Sortable)
				j--;

			if (i <= j) {
				temp = (*aArray)[i];
				(*aArray)[i] = (*aArray)[j];
				(*aArray)[j] = temp;
				i++;
				j--;
			}
		}

		/* recursion */
		if (left < j)
			SortWithQuick(aArray, left, j);
		if (i < right)
			SortWithQuick(aArray, i, right);
	}

	CapabilityTickLinkedList::CapabilityTickLinkedList()
	{
	}

	void CapabilityTickLinkedList::TickAll(float DeltaTime)
	{
		if (ElementsToAdd.Size() > 0)
		{
			// insert new elements while ticking
			if (ElementsToAdd.Size() > 1)
				SortWithQuick(&ElementsToAdd, 0, (uint8_t)ElementsToAdd.Size() - 1);
			size_t AddNextIndex = 0;
			size_t NumThingsToAdd = ElementsToAdd.Size();

			CapabilityTickLinkedElement* pCurrentElement = pFirstElement;
			do
			{
				// we're first
				if (pCurrentElement == nullptr)
				{
					pFirstElement = ElementsToAdd[AddNextIndex++];
					pCurrentElement = pFirstElement;
				}

				// we're last
				const bool bAnythingLeftToAdd = AddNextIndex < NumThingsToAdd;
				if (bAnythingLeftToAdd && pCurrentElement->pNext == nullptr)
				{
					CapabilityTickLinkedElement* ToInsert = ElementsToAdd[AddNextIndex++];
					pCurrentElement->pNext = ToInsert;
					ToInsert->pPrevious = pCurrentElement;
					pLastElement = ToInsert;
				}
				else if (bAnythingLeftToAdd && ElementsToAdd[AddNextIndex]->Sortable < pCurrentElement->Sortable)
				{
					CapabilityTickLinkedElement* ToInsert = ElementsToAdd[AddNextIndex++];
					CapabilityTickLinkedElement* pPrevious = pCurrentElement->pPrevious;
					if (pPrevious != nullptr)
					{
						pPrevious->pNext = ToInsert;
						ToInsert->pPrevious = pPrevious;
					}
					pCurrentElement->pPrevious = ToInsert;
					ToInsert->pNext = pCurrentElement;
					pCurrentElement = ToInsert;
				}

				pCurrentElement->Tick(DeltaTime);
				pCurrentElement = pCurrentElement->pNext;
			} while (pCurrentElement != nullptr);

			ElementsToAdd.RemoveAll();
		}
		else
		{
			// tick normally
			CapabilityTickLinkedElement* pCurrentElement = pFirstElement;
			while (pCurrentElement != nullptr)
			{
				pCurrentElement->Tick(DeltaTime);
				pCurrentElement = pCurrentElement->pNext;
			}
		}
	}

	void CapabilityTickLinkedList::Add(CapabilityTickLinkedElement& pTicker)
	{
		ElementsToAdd.Add(&pTicker);
	}

	// ---

	CapabilityTickWorld::CapabilityTickWorld()
	{
	}

	void CapabilityTickWorld::Init()
	{
		CapabilityElements.Prepare(1024);
		for (int i = 0; i < TickLists.Getsize(); ++i)
		{
			TickLists[i].ElementsToAdd.Prepare(256);
		}
	}

	void CapabilityTickWorld::RegisterCapability(Capability& pCapability)
	{
		CapabilityTickLinkedElement NewElement;
		NewElement.pCapability = &pCapability;
		NewElement.pWorld = this;
		NewElement.Sortable.Group = pCapability.m_group;
		NewElement.Sortable.SubGroup = pCapability.m_subGroup;
		CapabilityElements.Add(NewElement);
		RegisterTicker(CapabilityElements.GetLast());
		pCapability.Setup();
	}

	void CapabilityTickWorld::RegisterTicker(CapabilityTickLinkedElement& Ticker)
	{
		CapabilityTickSortable Sorting = Ticker.GetSorting();
		TickLists[(uint8_t)Sorting.Group].Add(Ticker);
	}

	void CapabilityTickWorld::Tick(float DeltaTime)
	{
		for (int i = 0; i < TickLists.Getsize(); ++i)
		{
			TickLists[i].TickAll(DeltaTime);
		}
	}
}
