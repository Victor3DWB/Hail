#pragma once
#include "Containers\GrowingArray\GrowingArray.h"
#include "Containers\StaticArray\StaticArray.h"
namespace Hail
{
	struct InternalMessage;
	enum class eMessageLogType : uint8
	{
		DrawUniqueueMessages,
		DrawAllMessages
	};
	class ImGuiMessageLogger
	{
	public:

		ImGuiMessageLogger();

		void RenderImGuiCommands();
		void FillAndSortMessageList(bool bUpdateMessageList);

	private:
		enum class eMessageSortingType : uint8
		{
			MessageType,
			Message,
			NumberOfOccurences,
			Time,
			File,
			Count
		};


		void DrawMessageLog();


		eMessageLogType m_logType;
		eMessageSortingType m_currentSortingType;
		StaticArray<bool, (uint8)eMessageType::Count> m_visibleTypes;
		GrowingArray<const InternalMessage*> m_sortedMessages;
	};
}


