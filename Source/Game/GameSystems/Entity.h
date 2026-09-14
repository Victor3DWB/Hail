#pragma once
#include "Types.h"
#include "Transforms.h"

#include "Containers/GrowingArray/GrowingArray.h"

namespace Hail
{
	using EntityHandle = uint32;

	class Entity
	{
	public:
		Transform2D m_transform;
		Transform2D* GetTransform() { return &m_transform; }
		uint32 GetId() const { return m_id; }
		bool IsValid() const { return m_id != MAX_UINT; }
	private:
		friend class EntityManager;
		EntityHandle m_id;
		EntityHandle m_next;
		EntityHandle m_parentId = MAX_UINT; // TODO:
	};

}



