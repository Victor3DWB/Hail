#pragma once
#include <initializer_list>
#include "InternalMessageHandling\InternalMessageHandling.h"
		
namespace Hail
{
	template<typename T, typename CountType = size_t>
	class GrowingArray
	{
	public:
		GrowingArray();

		GrowingArray(const std::initializer_list<T>& initList);

		GrowingArray(CountType nrOfRecommendedItems);
		GrowingArray(CountType nrOfRecommendedItems, const T& objectToFillWith);
		GrowingArray(const GrowingArray& growingArray);
		GrowingArray(GrowingArray&& growingArray);

		~GrowingArray();

		GrowingArray& operator=(const GrowingArray& growingArray);
		GrowingArray& operator=(GrowingArray&& growingArray);

		//Increments the counter to match the capacity of the internal array.
		void Fill();

		// Creates the memory on the heap for X amount of items, if it already has that capacity nothing happens.
		void Prepare(CountType numberOfItemsToInit);
		void PrepareAndFill(CountType numberOfItemsToInit);

		inline T& operator[](const CountType& index);
		inline const T& operator[](const CountType& index) const;

		inline T& Add(const T& object);
		inline T& Add();
		inline void AddN(uint32 numberOfItemsToAdd);
		inline void AddN(const T& object, uint32 numberOfItemsToAdd);
		inline void Insert(CountType index, const T& object);

		inline bool RemoveCyclic(const T& object);
		inline bool RemoveCyclicAtIndex(CountType itemNumber);
		inline bool RemoveAtIndex(CountType itemNumber);
		inline bool RemoveLast();
		inline int Find(const T& object) const;
		// Search condition have to be a lambda with input argument (const T& a), return state [ return bool }
		template<class SearchCondition>
		int FindCustomCondition(SearchCondition searchCondition);

		// Search condition have to be a lambda with input argument (const T& a, const T&b), return a < b;
		template<class Comparer>
		void Sort(Comparer compareFunc);

		inline T& GetLast();
		inline const T& GetLast() const;

		// Resets the counter but does not free the memory.
		inline void RemoveAll();
		// Deletes the memory and resets the GrowingArray.
		inline void DeleteAll();

		__forceinline CountType Size() const;
		__forceinline CountType Capacity() const { return m_capacity; };
		inline bool Empty() const { return m_elementCount == 0; }
		inline void Resize(CountType newSize);

		inline T* Data() { return m_arrayPointer; }


	private:
		void DumpAll();
		void GrowArray();
		void GrowArray(const CountType growSize);

		CountType m_elementCount;
		CountType m_capacity;
		T* m_arrayPointer;
	};

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::GrowingArray()
	{
		m_arrayPointer = nullptr;
		m_capacity = 0;
		m_elementCount = 0;
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::~GrowingArray()
	{
		DumpAll();
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::GrowingArray(const std::initializer_list<T>& initList) :
		m_capacity(0),
		m_elementCount(0),
		m_arrayPointer(nullptr)
	{
		Prepare((CountType)initList.size());

		for (T object : initList)
			m_arrayPointer[m_elementCount++] = object;
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::GrowingArray(CountType nrOfRecommendedItems) :
		m_capacity(nrOfRecommendedItems),
		m_elementCount(0),
		m_arrayPointer(nullptr)
	{
		Prepare(m_capacity);
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::GrowingArray(CountType nrOfRecommendedItems, const T& objectToFillWith) :
		m_capacity(nrOfRecommendedItems),
		m_elementCount(0),
		m_arrayPointer(nullptr)
	{
		Prepare(m_capacity);
		for (m_elementCount = 0; m_elementCount < m_capacity; m_elementCount++)
		{
			m_arrayPointer[m_elementCount] = objectToFillWith;
		}
	}


	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::GrowingArray(const GrowingArray& growingArray)
	{
		m_arrayPointer = nullptr;
		m_capacity = 0;
		m_elementCount = 0;
		(*this) = growingArray;
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>::GrowingArray(GrowingArray&& growingArray)
	{
		m_capacity = growingArray.m_capacity;
		m_arrayPointer = growingArray.m_arrayPointer;
		m_elementCount = growingArray.m_elementCount;
		m_arrayPointer = growingArray.m_arrayPointer;

		growingArray.m_arrayPointer = nullptr;
		growingArray.m_elementCount = 0;
		growingArray.m_capacity = 0;
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::Prepare(CountType numberOfItemsToInit)
	{
		if (m_arrayPointer && numberOfItemsToInit <= m_capacity)
			return;

		if (!m_arrayPointer)
		{
			m_elementCount = 0;
			m_capacity = numberOfItemsToInit;
			m_arrayPointer = new T[m_capacity];
			return;
		}

		T* oldPointer = m_arrayPointer;

		m_capacity = numberOfItemsToInit;
		m_arrayPointer = new T[m_capacity];

		for (CountType iSlot = 0; iSlot < m_elementCount; ++iSlot)
		{
			m_arrayPointer[iSlot] = oldPointer[iSlot];
		}

		delete[] oldPointer;
	}

	template<typename T, typename CountType>
	inline void GrowingArray<T, CountType>::PrepareAndFill(CountType numberOfItemsToInit)
	{
		Prepare(numberOfItemsToInit);
		m_elementCount = numberOfItemsToInit;
	}

	template<typename T, typename CountType>
	inline void GrowingArray<T, CountType>::Fill()
	{
		m_elementCount = m_capacity;
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>& GrowingArray<typename T, typename CountType>::operator=(const GrowingArray& growingArray)
	{
		DumpAll();
		if (growingArray.m_capacity)
			Prepare(growingArray.m_capacity);

		m_elementCount = growingArray.m_elementCount;
		for (CountType iSlot = 0; iSlot < m_elementCount; ++iSlot)
		{
			m_arrayPointer[iSlot] = growingArray.m_arrayPointer[iSlot];
		}

		return (*this);
	}

	template <typename T, typename CountType>
	GrowingArray<typename T, typename CountType>& GrowingArray<typename T, typename CountType>::operator=(GrowingArray&& growingArray)
	{
		m_capacity = growingArray.m_capacity;
		m_elementCount = growingArray.m_elementCount;
		m_arrayPointer = growingArray.m_arrayPointer;
		growingArray.m_arrayPointer = nullptr;
		growingArray.m_elementCount = 0;
		growingArray.m_capacity = 0;

		return (*this);
	}

	template <typename T, typename CountType>
	T& GrowingArray<typename T, typename CountType>::operator[](const CountType& index)
	{
		H_ASSERT(index < m_elementCount, "Index is out of range.");
		return m_arrayPointer[index];
	}

	template <typename T, typename CountType>
	const T& GrowingArray<typename T, typename CountType>::operator[](const CountType& index) const
	{
		H_ASSERT(index < m_elementCount, "Index is out of range.");
		return m_arrayPointer[index];
	}

	template <typename T, typename CountType>
	T& GrowingArray<typename T, typename CountType>::Add(const T& object)
	{
		if (!m_arrayPointer)
			Prepare(m_capacity != 0 ? m_capacity : 8);

		if (m_elementCount + 1 > (m_capacity))
			GrowArray();

		m_arrayPointer[m_elementCount] = object;
		++m_elementCount;
		return GetLast();
	}

	template <typename T, typename CountType>
	T& GrowingArray<typename T, typename CountType>::Add()
	{
		Add(T());
		return GetLast();
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::AddN(uint32 numberOfItemsToAdd)
	{
		if (!m_arrayPointer)
			Prepare(m_capacity != 0 ? m_capacity : 8);

		if (m_elementCount + numberOfItemsToAdd > (m_capacity))
			GrowArray(m_elementCount + numberOfItemsToAdd * 2);

		for (size_t i = m_elementCount; i < m_capacity; i++)
			m_arrayPointer[i] = {};

		m_elementCount += numberOfItemsToAdd;
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::AddN(const T& object, uint32 numberOfItemsToAdd)
	{
		if (!m_arrayPointer)
			Prepare(m_capacity != 0 ? m_capacity : 8);

		if (m_elementCount + numberOfItemsToAdd > (m_capacity))
			GrowArray(m_elementCount + numberOfItemsToAdd * 2);

		for (size_t i = m_elementCount; i < m_capacity; i++)
			m_arrayPointer[i] = object;

		m_elementCount += numberOfItemsToAdd;
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::Insert(CountType index, const T& object)
	{
		if (!m_arrayPointer)
			Prepare(8);

		T tempObject = m_arrayPointer[m_elementCount - 1];

		if (index != m_elementCount)
		{
			for (CountType iData = m_elementCount - 1; iData > index; --iData)
			{
				m_arrayPointer[iData] = m_arrayPointer[iData - 1];
			}
		}
		m_arrayPointer[index] = object;
		Add(tempObject);
	}


	template <typename T, typename CountType>
	bool GrowingArray<typename T, typename CountType>::RemoveCyclic(const T& object)
	{
		if (!m_arrayPointer)
			return false;

		const CountType ItemSlot = Find(object);
		if (ItemSlot != -1)
		{
			RemoveCyclicAtIndex(ItemSlot);
		}
		return true;
	}

	template <typename T, typename CountType>
	bool GrowingArray<typename T, typename CountType>::RemoveCyclicAtIndex(CountType itemNumber)
	{
		H_ASSERT(m_arrayPointer, "Uninitialized GrowingArray.");
		if (!m_arrayPointer)
			return false;

		H_ASSERT(itemNumber < m_elementCount, "Index is out of range.");
		if (m_elementCount != 1)
		{
			m_arrayPointer[itemNumber] = m_arrayPointer[m_elementCount - 1];
			--m_elementCount;
		}
		else
		{
			RemoveAll();
		}
		return true;
	}

	template <typename T, typename CountType>
	bool GrowingArray<T, CountType>::RemoveAtIndex(CountType itemNumber)
	{
		if (!m_arrayPointer)
			return false;
		H_ASSERT(itemNumber < m_elementCount, "Index is out of range.");
		--m_elementCount;
		for (unsigned int i = itemNumber; i < m_elementCount; ++i)
		{
			m_arrayPointer[i] = m_arrayPointer[i + 1];
		}
		return true;
	}

	template<typename T, typename CountType>
	inline bool GrowingArray<T, CountType>::RemoveLast()
	{
		if (!m_arrayPointer)
			return false;
		if (m_elementCount > 0)
		{
			m_elementCount--;
		}
		return true;
	}

	template <typename T, typename CountType>
	int GrowingArray<typename T, typename CountType>::Find(const T& object) const
	{
		if (!m_arrayPointer)
			return -1;

		for (CountType iSlot = 0; iSlot < m_elementCount; ++iSlot)
		{
			if (m_arrayPointer[iSlot] == object)
			{
				return iSlot;
			}
		}

		return -1;
	}

	template <typename T, typename CountType>
	T& GrowingArray<typename T, typename CountType>::GetLast()
	{
		H_ASSERT(!Empty(), "GrowingArray is empty.");
		return m_arrayPointer[m_elementCount - 1];
	}

	template <typename T, typename CountType>
	const T& GrowingArray<typename T, typename CountType>::GetLast() const
	{
		H_ASSERT(!Empty(), "GrowingArray is empty.");
		return m_arrayPointer[m_elementCount - 1];
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::RemoveAll()
	{
		m_elementCount = 0;
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::DeleteAll()
	{
		DumpAll();
	}

	template <typename T, typename CountType>
	CountType GrowingArray<typename T, typename CountType>::Size() const
	{
		return m_elementCount;
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::Resize(CountType aNewSize)
	{
		if (!m_arrayPointer)
			Prepare(aNewSize);
		GrowArray(aNewSize);
	}


	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::GrowArray()
	{
		if (!m_arrayPointer)
			return;
		GrowArray(m_capacity * 2);
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::GrowArray(const CountType newSize)
	{
		if (!m_arrayPointer || newSize <= m_capacity)
			return;
		T* tempPointer = m_arrayPointer;
		const CountType tempCount = m_elementCount;
		m_arrayPointer = nullptr;
		m_elementCount = 0;
		Prepare(newSize);

		for (; m_elementCount < tempCount; m_elementCount++)
		{
			m_arrayPointer[m_elementCount] = tempPointer[m_elementCount];
		}

		delete[] tempPointer;
		tempPointer = nullptr;
	}

	template <typename T, typename CountType>
	void GrowingArray<typename T, typename CountType>::DumpAll()
	{
		if (m_arrayPointer)
		{
			delete[] m_arrayPointer;
			m_arrayPointer = nullptr;
		}
		m_capacity = 0;
		RemoveAll();
	}

	template<typename T, typename CountType>
	template<class SearchCondition>
	inline int GrowingArray<T, CountType>::FindCustomCondition(SearchCondition searchCondition)
	{
		if (!m_elementCount)
			return -1;

		for (int i = 0; i < m_elementCount; i++)
		{
			if (searchCondition(m_arrayPointer[i]))
				return i;
		}

		return -1;
	}

	template<typename T, typename CountType>
	template<class Comparer>
	inline void GrowingArray<T, CountType>::Sort(Comparer compareFunc)
	{
		// currently implemented with a Bubble sort from online, I will update this later with a linear sort if below 128 entries, otherwise it should be a quicksort
		CountType n = m_elementCount;

		// Outer loop that corresponds to the number of elements to be sorted
		for (int i = 0; i < n - 1; i++) 
		{
			// Last i elements are already in place
			for (int j = 0; j < n - i - 1; j++) 
			{
				// Comparing adjacent elements
				if (compareFunc(m_arrayPointer[j], m_arrayPointer[j + 1]))
				{
					// Should maybe be done with stl::swap especially if T is an expensive object to create a copy of and should be moved, but ehh, fuck stl amiright? 
					const T jVal = m_arrayPointer[j];
					m_arrayPointer[j] = m_arrayPointer[j + 1];
					m_arrayPointer[j + 1] = jVal;
				}
			}
		}
	}

}