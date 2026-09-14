#include "Game_PCH.h"
#include "CapabilityTickManager.h"
#include "CapabilityBaseClass.h"

namespace Hail
{
	void CapabilityTickLinkedElement::Tick(float DeltaTime)
	{ 
		if (!m_pCapability->m_bDidLateSetup)
		{
			m_pCapability->m_bDidLateSetup = true;
			m_pCapability->LateSetup();
		}

		if (!m_pCapability->m_bActive && m_pCapability->ShouldActivate())
		{
			m_pCapability->m_bActive = true;
			m_pCapability->m_activeDuration = 0.0;
			m_pCapability->m_inactiveDuration = 0.0;
			m_pCapability->OnActivated();
		}
		else if (m_pCapability->m_bActive && m_pCapability->ShouldDeactivate())
		{
			m_pCapability->m_bActive = false;
			m_pCapability->m_activeDuration = 0.0;
			m_pCapability->m_inactiveDuration = 0.0;
			m_pCapability->OnDeactivated();
		}

		if (m_pCapability->m_bActive)
		{
			m_pCapability->TickActive(DeltaTime * m_pCapability->GetTimeDilation());
			m_pCapability->m_activeDuration += DeltaTime * m_pCapability->GetTimeDilation();
		}
		else
		{
			m_pCapability->TickInactive(DeltaTime * m_pCapability->GetTimeDilation());
			m_pCapability->m_inactiveDuration += DeltaTime * m_pCapability->GetTimeDilation();
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
		H_ASSERT(m_pWorld != nullptr);
		*this = CapabilityTickLinkedElement();
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
			while ((*aArray)[i]->m_sortable < pivot->m_sortable)
				i++;
			while ((*aArray)[j]->m_sortable > pivot->m_sortable)
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
		if (m_elementsToAdd.Size() > 0)
		{
			// insert new elements while ticking
			if (m_elementsToAdd.Size() > 1)
				SortWithQuick(&m_elementsToAdd, 0, (uint8_t)m_elementsToAdd.Size() - 1);
			size_t AddNextIndex = 0;
			size_t NumThingsToAdd = m_elementsToAdd.Size();

			CapabilityTickLinkedElement* pCurrentElement = m_pFirstElement;
			do
			{
				// we're first
				if (pCurrentElement == nullptr)
				{
					m_pFirstElement = m_elementsToAdd[AddNextIndex++];
					pCurrentElement = m_pFirstElement;
				}

				// we're last
				const bool bAnythingLeftToAdd = AddNextIndex < NumThingsToAdd;
				if (bAnythingLeftToAdd && pCurrentElement->m_pNext == nullptr)
				{
					CapabilityTickLinkedElement* ToInsert = m_elementsToAdd[AddNextIndex++];
					pCurrentElement->m_pNext = ToInsert;
					ToInsert->m_pPrevious = pCurrentElement;
					m_pLastElement = ToInsert;
				}
				else if (bAnythingLeftToAdd && m_elementsToAdd[AddNextIndex]->m_sortable < pCurrentElement->m_sortable)
				{
					CapabilityTickLinkedElement* ToInsert = m_elementsToAdd[AddNextIndex++];
					CapabilityTickLinkedElement* pPrevious = pCurrentElement->m_pPrevious;
					if (pPrevious != nullptr)
					{
						pPrevious->m_pNext = ToInsert;
						ToInsert->m_pPrevious = pPrevious;
					}
					pCurrentElement->m_pPrevious = ToInsert;
					ToInsert->m_pNext = pCurrentElement;
					pCurrentElement = ToInsert;
				}

				pCurrentElement->Tick(DeltaTime);
				pCurrentElement = pCurrentElement->m_pNext;
			} while (pCurrentElement != nullptr);

			m_elementsToAdd.RemoveAll();
		}
		else
		{
			// tick normally
			CapabilityTickLinkedElement* pCurrentElement = m_pFirstElement;
			while (pCurrentElement != nullptr)
			{
				pCurrentElement->Tick(DeltaTime);
				pCurrentElement = pCurrentElement->m_pNext;
			}
		}
	}

	void CapabilityTickLinkedList::Add(CapabilityTickLinkedElement& pTicker)
	{
		m_elementsToAdd.Add(&pTicker);
	}

	// ---

	CapabilityTickWorld::CapabilityTickWorld()
	{
	}

	void CapabilityTickWorld::Init()
	{
		m_capabilityElements.Prepare(1024);
		for (int i = 0; i < m_tickLists.Getsize(); ++i)
		{
			m_tickLists[i].m_elementsToAdd.Prepare(256);
		}
	}

	void CapabilityTickWorld::RegisterCapability(Capability& pCapability)
	{
		CapabilityTickLinkedElement newElement;
		newElement.m_pCapability = &pCapability;
		newElement.m_pWorld = this;
		newElement.m_sortable.m_group = pCapability.m_group;
		newElement.m_sortable.m_subGroup = pCapability.m_subGroup;

		if (m_freeCapabilityElementSlots.Size() > 0)
		{
			uint32 nextElementIndex = m_freeCapabilityElementSlots[0];
			m_freeCapabilityElementSlots.RemoveCyclicAtIndex(0);
			m_capabilityElements[nextElementIndex] = newElement;
			RegisterTicker(m_capabilityElements[nextElementIndex]);
		}
		else
		{
			m_capabilityElements.Add(newElement);
			RegisterTicker(m_capabilityElements.GetLast());
		}
		pCapability.Setup();
	}

	void CapabilityTickWorld::UnRegisterCapability(Capability* pCapability)
	{
		for ( uint32 iCapabilityTicker = 0; iCapabilityTicker < m_capabilityElements.Size(); ++iCapabilityTicker)
		{
			CapabilityTickLinkedElement& capabilityLinkedElement = m_capabilityElements[iCapabilityTicker];
			if ( capabilityLinkedElement.m_pCapability == pCapability )
			{
				CapabilityTickSortable sorting = capabilityLinkedElement.GetSorting();
				CapabilityTickLinkedList& currentTickLinkedList = m_tickLists[(uint8_t)sorting.m_group];
				CapabilityTickLinkedElement* pCurrentElement = currentTickLinkedList.m_pFirstElement;
				while (pCurrentElement)
				{
					if (pCurrentElement->m_pCapability == pCapability)
					{
						H_ASSERT(&capabilityLinkedElement == pCurrentElement);
						CapabilityTickLinkedElement* pPrevious = pCurrentElement->m_pPrevious;
						CapabilityTickLinkedElement* pNext = pCurrentElement->m_pNext;

						const bool bReplaceTickListFirst = currentTickLinkedList.m_pFirstElement == pCurrentElement;
						const bool bReplaceTickListLast = currentTickLinkedList.m_pLastElement == pCurrentElement;

						if (pPrevious)
						{
							pPrevious->m_pNext = pNext;
						}
						if (pNext)
						{
							pNext->m_pPrevious = pPrevious;
						}

						if (bReplaceTickListFirst)
						{
							currentTickLinkedList.m_pFirstElement = pNext;
						}
						if (bReplaceTickListLast)
						{
							currentTickLinkedList.m_pLastElement = pPrevious;
						}

						pCurrentElement = nullptr;
					}
					else
					{
						pCurrentElement = pCurrentElement->m_pNext;
					}
				}

				capabilityLinkedElement.Unregister();
				m_freeCapabilityElementSlots.Add(iCapabilityTicker);
				break;
			}
		}
	}

	void CapabilityTickWorld::RegisterTicker(CapabilityTickLinkedElement& Ticker)
	{
		CapabilityTickSortable sorting = Ticker.GetSorting();
		m_tickLists[(uint8_t)sorting.m_group].Add(Ticker);
	}

	void CapabilityTickWorld::Tick(float DeltaTime)
	{
		for (int i = 0; i < m_tickLists.Getsize(); ++i)
		{
			m_tickLists[i].TickAll(DeltaTime);
		}
	}
}
