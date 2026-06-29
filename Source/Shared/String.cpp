#include "Shared_PCH.h"
#include <string.h>
#include <string>
#include <assert.h>
#include <stdarg.h>

#include "MathUtils.h"
#include "Utility/StringUtility.h"
#include "String.hpp"
#include "Types.h"
#include "InternalMessageHandling\InternalMessageHandling.h"
#include "StringMemoryAllocator.h"

using namespace Hail;

Hail::StringL::StringL()
	: m_length(0)
	, m_allocatedLength(0)
{
	m_memory.m_p = (nullptr);
}

Hail::StringL::StringL(const char* const string)
{
	m_memory.m_p = nullptr;
	m_length = StringLength(string);
	if (m_length > 15)
	{
		m_allocatedLength = m_length;
		StringMemoryAllocator::GetInstance().AllocateString(string, m_length, &m_memory.m_p);
	}
	else
	{
		m_allocatedLength = 0;
		strcpy_s(m_memory.m_shortString, string);
	}
}

Hail::StringL::StringL(const StringL& anotherString)
{
	m_memory.m_p = nullptr;
	if (anotherString.m_allocatedLength > 15)
	{
		StringMemoryAllocator::GetInstance().AllocateString(anotherString.m_memory.m_p, anotherString.m_allocatedLength, &m_memory.m_p);
		m_allocatedLength = anotherString.m_allocatedLength;
	}
	else
	{
		strcpy_s(m_memory.m_shortString, anotherString.m_memory.m_shortString);
		m_allocatedLength = 0u;
	}
	m_length = anotherString.Length();
}

Hail::StringL::StringL(const String64& string64)
{
	m_memory.m_p = nullptr;
	m_length = string64.Length();
	if (m_length > 15)
	{
		StringMemoryAllocator::GetInstance().AllocateString(string64.Data(), m_length, &m_memory.m_p);
		m_allocatedLength = m_length;
	}
	else
	{
		strcpy_s(m_memory.m_shortString, string64.Data());
		m_allocatedLength = 0u;
	}
}

Hail::StringL::StringL(StringL&& moveableString)
{
	m_memory.m_p = nullptr;
	m_length = moveableString.m_length;
	m_allocatedLength = moveableString.m_allocatedLength;
	if (moveableString.m_length > 0)
	{
		if (moveableString.m_allocatedLength > 15)
		{
			m_memory.m_p = moveableString.m_memory.m_p;
			moveableString.m_memory.m_p = nullptr;
		}
		else
		{
			strcpy_s(m_memory.m_shortString, moveableString.m_memory.m_shortString);
		}
	}
	moveableString.m_length = 0;
	moveableString.m_allocatedLength = 0;
}

Hail::StringL::StringL(const std::string& stlString)
{
	H_ERROR("Use of stl string.");
	m_length = stlString.size();
	if (stlString.size() > 15)
	{
		StringMemoryAllocator::GetInstance().AllocateString(stlString.c_str(), m_length, &m_memory.m_p);
		m_allocatedLength = m_length;
	}
	else
	{
		strcpy_s(m_memory.m_shortString, stlString.c_str());
		m_allocatedLength = 0u;
	}
}

Hail::StringL::~StringL()
{
	Clear();
}

StringL Hail::StringL::Format(const char* const format, ...)
{
	uint32 length = StringLength(format);

	if (format == NULL)
		return StringL();

	uint32 numberOfArguments = 0;
	char currentCharacter;
	const char* types_ptr = format;
	bool nextSymbolIsAnArg = false;

	char* stringArgument;
	char numberCharString[64];
	types_ptr = format;
	va_list argp{};
	va_start(argp, format);

	while (*types_ptr != '\0') 
	{
		currentCharacter = *types_ptr;
		if (nextSymbolIsAnArg && currentCharacter != '%')
		{
			nextSymbolIsAnArg = false;
			if (currentCharacter == 'i')
			{
				int intArgument = va_arg(argp, int);
				sprintf(numberCharString, "%i", intArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == 's')
			{
				stringArgument = va_arg(argp, char*);
				length += StringLength(stringArgument);
			}
			else if (currentCharacter == 'c')
			{
				va_arg(argp, char); // move arg pointer forward
				length += 1;
			}
			else if (currentCharacter == 'f')
			{
				float floatArgument = (float)va_arg(argp, double);
				sprintf(numberCharString, "%f", floatArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == 'u')
			{
				uint32 uintArgument = va_arg(argp, uint32);
				sprintf(numberCharString, "%u", uintArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == 'd')
			{
				double doubleArgument = va_arg(argp, double);
				sprintf(numberCharString, "%d", doubleArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == 'p')
			{
				void* voidArgument = va_arg(argp, void*);
				sprintf(numberCharString, "%p", voidArgument);
				length += StringLength(numberCharString);
			}
			else
			{
				H_ASSERT(false, StringL::Format("Unsupported type, %c", currentCharacter));
			}
			memset(numberCharString, 0, 64);
		}
		if (currentCharacter == '%' && nextSymbolIsAnArg != true)
		{
			numberOfArguments++;
			nextSymbolIsAnArg = true;
		}
		++types_ptr;
	}

	// if we got an argument, add a length of 1 to get room for the end sign \0
	if (numberOfArguments)
		length++;

	va_end(argp);

	StringL str;
	va_list vl;
	va_start(vl, format);
	// allocate from length
	int result = 0;
	if (length - (numberOfArguments * 2) > 15)
	{
		str.m_allocatedLength = length - (numberOfArguments * 2);
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, str.m_allocatedLength, &str.m_memory.m_p);
		memset(str.m_memory.m_p, 0, str.m_allocatedLength);
		result = vsprintf_s(str.m_memory.m_p, str.m_allocatedLength, format, vl);
		H_ASSERT(result > 0, "Invalid formatting.");
	}
	else
	{
		str.m_allocatedLength = 0;
		result = vsprintf_s(str.m_memory.m_shortString, length, format, vl);
		H_ASSERT(result > 0, "Invalid formatting.");
	}
	str.m_length = result;
	va_end(vl);
	return str;
}

Hail::StringL::operator const char* () const
{
	return m_length > 15 ? m_memory.m_p : m_memory.m_shortString;
}

Hail::StringL::operator char* ()
{
	return m_length > 15 ? m_memory.m_p : m_memory.m_shortString;
}

StringL& Hail::StringL::operator=(const StringL& anotherString)
{
	if (anotherString.m_length == 0)
	{
		if (m_allocatedLength)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}
		m_length = 0;
		return *this;
	}


	if (m_allocatedLength >= anotherString.m_allocatedLength && m_allocatedLength != 0)
	{
		// If the other string is a shortstring we need to deallocate ouur own memory.
		if (anotherString.m_allocatedLength < 16)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}

		if (m_allocatedLength > 15)
		{
			memcpy(m_memory.m_p, anotherString.m_memory.m_p, anotherString.m_length);
			m_memory.m_p[anotherString.m_length] = 0;
		}
		else
			strcpy_s(m_memory.m_shortString, anotherString.m_memory.m_shortString);

		m_length = anotherString.Length();
		return *this;
	}
	if (m_allocatedLength > 15)
	{
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
		m_allocatedLength = 0;
	}
	m_length = anotherString.Length();
	if (anotherString.Length() > 15)
	{
		StringMemoryAllocator::GetInstance().AllocateString(anotherString.m_memory.m_p, m_length, &m_memory.m_p);
		memcpy(m_memory.m_p, anotherString.m_memory.m_p, anotherString.m_length);
		m_memory.m_p[anotherString.m_length] = 0;
		m_allocatedLength = m_length;
	}
	else
	{
		strcpy_s(m_memory.m_shortString, anotherString.m_memory.m_shortString);
		m_allocatedLength = 0;
	}
	return *this;
}

StringL& Hail::StringL::operator=(StringL&& moveableString)
{
	if (m_allocatedLength > 15)
	{
		m_allocatedLength = 0;
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
	}

	m_length = moveableString.Length();
	if (moveableString.m_allocatedLength > 15)
	{
		m_memory.m_p = moveableString.m_memory.m_p;
		m_allocatedLength = moveableString.m_allocatedLength;
	}
	else
	{
		m_allocatedLength = 0;
		strcpy_s(m_memory.m_shortString, moveableString.m_memory.m_shortString);
	}
	moveableString.m_memory.m_p = nullptr;
	moveableString.m_length = 0;
	moveableString.m_allocatedLength = 0;
	return *this;
}

StringL& Hail::StringL::operator=(const char* const pString)
{
	m_length = StringLength(pString);

	if (m_allocatedLength > 15 && m_length > m_allocatedLength)
	{
		m_allocatedLength = 0;
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
	}

	if (m_length > 15)
	{
		StringMemoryAllocator::GetInstance().AllocateString(pString, m_length, &m_memory.m_p);
		m_allocatedLength = m_length;
	}
	else
	{
		strcpy_s(m_memory.m_shortString, pString);
		m_allocatedLength = 0;
	}
	return *this;
}

StringL& Hail::StringL::operator+=(StringL& anotherString)
{
	if (anotherString.Length() == 0)
		return *this;

	const uint32 previousLength = Length();
	const uint32 newLength = previousLength + anotherString.Length();
	if (m_allocatedLength > 15)
	{
		if (newLength > m_allocatedLength)
		{
			char* previousString = m_memory.m_p;
			StringMemoryAllocator::GetInstance().AllocateString(previousString, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			StringMemoryAllocator::GetInstance().DeallocateString(&previousString);
		}

		memcpy(m_memory.m_p + (previousLength), anotherString.Data(), anotherString.m_length);
		m_memory.m_p[newLength] = 0;
	}
	else
	{
		if (newLength > 15)
		{
			char previousString[16];
			memcpy(previousString, m_memory.m_shortString, previousLength);
			StringMemoryAllocator::GetInstance().AllocateString(nullptr, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			memcpy(m_memory.m_p, previousString, previousLength);
			memcpy(m_memory.m_p + (previousLength), anotherString.Data(), anotherString.m_length);
			m_memory.m_p[newLength] = 0;
		}
		else
		{
			memcpy(&m_memory.m_shortString[previousLength], anotherString.m_memory.m_shortString, anotherString.Length());
			m_memory.m_shortString[newLength] = 0;
		}
	}
	m_length = newLength;

	return *this;
}

StringL& Hail::StringL::operator+=(const char* pString)
{
	const uint32 previousLength = Length();
	const uint32 newStringLength = StringLength(pString);
	const uint32 newLength = previousLength + newStringLength;
	const uint32 oldAllocatedLength = m_allocatedLength;
	if (newLength > 15)
	{
		const bool bAllocatesNewMemory = newLength > oldAllocatedLength;
		if (bAllocatesNewMemory)
		{
			const StringL previousString = std::move(*this);
			StringMemoryAllocator::GetInstance().AllocateString(nullptr, newLength + 32u, &m_memory.m_p);

			memcpy(m_memory.m_p, previousString.Data(), previousLength);
			memcpy(m_memory.m_p + (previousLength), pString, newStringLength);

			m_allocatedLength = newLength + 32u;
		}
		else
		{
			memcpy(m_memory.m_p + (previousLength), pString, newStringLength);
		}
		m_memory.m_p[newLength] = 0;
	}
	else
	{
		if (oldAllocatedLength > 15)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}

		memcpy(&m_memory.m_shortString[previousLength], pString, newStringLength);
		m_memory.m_shortString[newLength] = 0;
	}
	m_length = newLength;

	return *this;
}

StringL& Hail::StringL::operator+=(const char character)
{
	const uint32 previousLength = Length();
	const uint32 newStringLength = previousLength + 1u;
	if (newStringLength > 15)
	{
		char oldShortString[16];
		memcpy(oldShortString, m_memory.m_shortString, 16u);
		char* pPreviousString = m_allocatedLength > 15 ? m_memory.m_p : nullptr;

		bool bAllocatedNewMemory = false;
		if (m_length > m_allocatedLength)
		{
			StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length + 32u, &m_memory.m_p);
			m_allocatedLength = m_length + 32u;
			bAllocatedNewMemory = true;
		}
		if (bAllocatedNewMemory)
		{
			if (pPreviousString)
			{
				memcpy(m_memory.m_p, pPreviousString, previousLength);
				StringMemoryAllocator::GetInstance().DeallocateString(&pPreviousString);
			}
			else
			{
				memcpy(m_memory.m_p, oldShortString, previousLength);
			}
			memcpy(m_memory.m_p + (previousLength), &character, 1u);
		}
		else
		{
			m_memory.m_p[m_length - 1u] = character;
		}
		m_memory.m_p[m_length] = 0;
	}
	else
	{
		if (m_allocatedLength > 15)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}

		m_memory.m_shortString[m_length - 1u] = character;
		m_memory.m_shortString[m_length] = 0;
	}
	m_length = previousLength + newStringLength;

	return *this;
}


StringL Hail::StringL::operator+(const StringL& string1)
{
	const uint32 previousLength = m_length;
	m_length = string1.m_length + m_length;
	if (m_length > 15)
	{
		char previousShortString[16];
		if (previousLength < 16)
			memcpy(previousShortString, m_memory.m_shortString, previousLength);

		const char* previousString = m_allocatedLength > 15 ? m_memory.m_p : previousShortString;
		if (m_length > m_allocatedLength)
		{
			if (m_allocatedLength > 15)
				StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);

			StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
			m_allocatedLength = m_length;
		}

		memcpy(m_memory.m_p, previousString, previousLength);
		memcpy(&m_memory.m_p[previousLength], string1.m_allocatedLength > 15 ? string1.m_memory.m_p : string1.m_memory.m_shortString, string1.m_length);
		m_memory.m_p[m_length] = 0;
	}
	else
	{
		char previousShortString[16];
		memcpy(previousShortString, m_memory.m_shortString, previousLength);

		const char* previousString = previousShortString;
		memcpy(m_memory.m_shortString, previousString, previousLength);
		memcpy(m_memory.m_p + (previousLength), string1.m_memory.m_shortString, string1.m_length);
		m_memory.m_p[m_length] = 0;
		m_allocatedLength = 0;
	}
	return *this;
}

char* Hail::StringL::Data()
{
	return m_allocatedLength > 0u ? m_memory.m_p : m_memory.m_shortString;
}

const char* const Hail::StringL::Data() const
{
	return m_allocatedLength > 0u ? m_memory.m_p : m_memory.m_shortString;
}

void Hail::StringL::Reserve(uint32 numOfChars)
{
	m_length = numOfChars;
	if (numOfChars > 15)
	{
		if (m_allocatedLength > 0)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
		}
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
		m_allocatedLength = m_length;
	}
}

void Hail::StringL::RemoveCharsFromBack(uint32 numOfChars)
{
	if (m_length == 0u)
		return;

	int32 newLength = m_length - numOfChars;
	newLength = Math::Max(0, newLength);

	if (newLength > 15)
	{
		H_ASSERT(m_allocatedLength > 15);
		memset(&m_memory.m_p[newLength], 0, m_length);
	}
	else
	{
		if (m_allocatedLength > 15)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}

		memset(&m_memory.m_shortString[newLength], 0, m_length);
	}
	m_length = newLength;
}

void Hail::StringL::Clear()
{
	if (m_allocatedLength > 15)
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);

	m_memory.m_p = nullptr;
	m_length = 0;
	m_allocatedLength = 0u;
}

Hail::StringLW::StringLW() : m_length(0), m_allocatedLength(0)
{
	m_memory.m_p = nullptr;
}

Hail::StringLW::StringLW(const char* const string)
{
	m_memory.m_p = nullptr;
	m_length = StringLength(string);
	if (m_length > 7)
	{
		m_allocatedLength = m_length;
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
		FromConstCharToWChar(string, m_memory.m_p, m_length);
	}
	else
	{
		m_allocatedLength = 0;
		FromConstCharToWChar(string, m_memory.m_shortString, 8);
	}
}

Hail::StringLW::StringLW(const wchar_t* const string)
{
	m_memory.m_p = nullptr;
	m_length = StringLength(string);
	if (m_length > 7)
	{
		m_allocatedLength = m_length;
		StringMemoryAllocator::GetInstance().AllocateString(string, m_length, &m_memory.m_p);
	}
	else
	{
		m_allocatedLength = 0;
		wcscpy_s(m_memory.m_shortString, string);
	}
}

Hail::StringLW::StringLW(const StringL& anotherString)
{
	m_memory.m_p = nullptr;
	if (anotherString.Length() > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, anotherString.Length(), &m_memory.m_p);
 		FromConstCharToWChar(anotherString.Data(), m_memory.m_p, anotherString.Length());
		m_allocatedLength = anotherString.Length();
	}
	else
	{
		FromConstCharToWChar(anotherString.Data(), m_memory.m_shortString, 8);
		m_allocatedLength = 0u;
	}
	m_length = anotherString.Length();
}

Hail::StringLW::StringLW(const StringLW& anotherWString)
{
	m_length = 0u;
	m_memory.m_p = nullptr;
	if (anotherWString.m_length > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(anotherWString.Data(), anotherWString.m_length, &m_memory.m_p);
		m_allocatedLength = anotherWString.m_allocatedLength;
	}
	else
	{
		wcscpy_s(m_memory.m_shortString, anotherWString.m_memory.m_shortString);
		m_allocatedLength = 0u;
	}
	m_length = anotherWString.Length();
}

Hail::StringLW::StringLW(const String64& string64)
{
	m_memory.m_p = nullptr;
	m_length = string64.Length();
	if (m_length > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
		FromConstCharToWChar(string64.Data(), m_memory.m_p, m_length);
		m_allocatedLength = m_length;
	}
	else
	{
		FromConstCharToWChar(string64.Data(), m_memory.m_shortString, 8);
		m_allocatedLength = 0u;
	}
}

Hail::StringLW::StringLW(const WString64& wString64)
{
	m_memory.m_p = nullptr;
	m_length = wString64.Length();
	if (m_length > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(wString64, m_length, &m_memory.m_p);
		m_allocatedLength = m_length;
	}
	else
	{
		wcscpy_s(m_memory.m_shortString, wString64);
		m_allocatedLength = 0u;
	}
}

Hail::StringLW::StringLW(StringLW&& moveableString)
{
	m_memory.m_p = nullptr;
	m_length = moveableString.m_length;
	m_allocatedLength = moveableString.m_allocatedLength;
	if (moveableString.m_length > 7)
	{
		m_memory.m_p = moveableString.m_memory.m_p;
		moveableString.m_memory.m_p = nullptr;
	}
	else
	{
		wcscpy_s(m_memory.m_shortString, moveableString.m_memory.m_shortString);
	}

	moveableString.m_length = 0;
	moveableString.m_allocatedLength = 0;
}

Hail::StringLW::~StringLW()
{
	Clear();
}

StringLW Hail::StringLW::Format(const wchar_t* const format, ...)
{
	uint32 length = StringLength(format);

	if (format == NULL)
		return StringL();

	uint32 numberOfArguments = 0;
	wchar_t currentCharacter;
	const wchar_t* types_ptr = format;
	bool nextSymbolIsAnArg = false;
	bool bIsANumber = false;

	wchar_t* stringArgument;
	wchar_t numberCharString[64];
	types_ptr = format;
	va_list argp{};
	va_start(argp, format);

	while (*types_ptr != '\0')
	{
		currentCharacter = *types_ptr;
		if (nextSymbolIsAnArg && currentCharacter != '%')
		{
			bIsANumber = false;
			if (StringUtility::IntFromChar(currentCharacter) != -1)
			{
				bIsANumber = true;
			}
			nextSymbolIsAnArg = bIsANumber;
			if (currentCharacter == L'i' || currentCharacter == L'I')
			{
				int intArgument = va_arg(argp, int);
				swprintf(numberCharString, L"%i", intArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == L's' || currentCharacter == L'S')
			{
				stringArgument = va_arg(argp, wchar_t*);
				length += StringLength(stringArgument);
			}
			else if (currentCharacter == 'c' || currentCharacter == L'C')
			{
				va_arg(argp, char); // move arg pointer forward
				length += 1;
			}
			else if (currentCharacter == 'f' || currentCharacter == L'F')
			{
				float floatArgument = (float)va_arg(argp, double);
				swprintf(numberCharString, L"%f", floatArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == 'u' || currentCharacter == L'U')
			{
				uint32 uintArgument = va_arg(argp, uint32);
				swprintf(numberCharString, L"%u", uintArgument);
				length += StringLength(numberCharString);
			}
			else if (currentCharacter == 'd' || currentCharacter == L'D')
			{
				double doubleArgument = va_arg(argp, double);
				swprintf(numberCharString, L"%d", doubleArgument);
				length += StringLength(numberCharString);
			}
			else if (!bIsANumber)
			{
				H_ASSERT(false, StringL::Format("Unsupported type, %c", currentCharacter));
			}
			memset(numberCharString, 0, 64);
		}
		if (currentCharacter == '%' && nextSymbolIsAnArg != true)
		{
			numberOfArguments++;
			nextSymbolIsAnArg = true;
		}
		++types_ptr;
	}

	// if we got an argument, add a length of 1 to get room for the end sign \0
	if (numberOfArguments)
		length++;

	va_end(argp);

	StringLW str;
	va_list vl;
	va_start(vl, format);
	// allocate from length
	int result = 0;
	if (length > 15)
	{
		str.m_allocatedLength = length - (numberOfArguments * 2);
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, length, &str.m_memory.m_p);
		result = vswprintf_s(str.m_memory.m_p, str.m_allocatedLength, format, vl);
		H_ASSERT(result > 0, "Invalid formatting.");
	}
	else
	{
		str.m_allocatedLength = 0;
		result = vswprintf_s(str.m_memory.m_shortString, length, format, vl);
		H_ASSERT(result > 0, "Invalid formatting.");
	}
	str.m_length = result;
	va_end(vl);
	return str;
}

Hail::StringLW::operator const wchar_t* () const
{
	return m_length > 7 ? m_memory.m_p : m_memory.m_shortString;
}

Hail::StringLW::operator wchar_t* ()
{
	return m_length > 7 ? m_memory.m_p : m_memory.m_shortString;
}

StringLW& Hail::StringLW::operator=(const StringLW& anotherString)
{
	if (anotherString.m_length == 0)
	{
		if (m_allocatedLength)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}
		m_length = 0;
		return *this;
	}

	if (m_allocatedLength >= anotherString.m_allocatedLength && m_allocatedLength != 0)
	{
		// If the other string is a shortstring we need to deallocate ouur own memory.
		if (anotherString.m_allocatedLength < 8)
		{
			StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
			m_allocatedLength = 0;
		}

		if (m_allocatedLength > 7)
		{
			memcpy(m_memory.m_p, anotherString.m_memory.m_p, anotherString.m_length * sizeof(wchar_t));
			m_memory.m_p[anotherString.m_length] = 0;
		}
		else
			wcscpy_s(m_memory.m_shortString, anotherString.m_memory.m_shortString);

		m_length = anotherString.Length();
		return *this;
	}
	if (m_allocatedLength > 7)
	{
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
		m_allocatedLength = 0;
	}
	m_length = anotherString.Length();

	if (anotherString.Length() > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(anotherString.m_memory.m_p, m_length, &m_memory.m_p);
		memcpy(m_memory.m_p, anotherString.m_memory.m_p, anotherString.m_length * sizeof(wchar_t));
		m_memory.m_p[anotherString.m_length] = 0;
		m_allocatedLength = m_length;
	}
	else
	{
		wcscpy_s(m_memory.m_shortString, anotherString.m_memory.m_shortString);
		m_allocatedLength = 0;
	}
	return *this;
}

StringLW& Hail::StringLW::operator=(StringLW&& moveableString)
{
	if (m_allocatedLength > 7)
	{
		m_allocatedLength = 0;
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
	}

	m_length = moveableString.Length();

	if (moveableString.m_allocatedLength > 7)
	{
		m_memory.m_p = moveableString.m_memory.m_p;
		m_allocatedLength = moveableString.m_allocatedLength;
	}
	else
	{
		m_allocatedLength = 0;
		wcscpy_s(m_memory.m_shortString, moveableString.m_memory.m_shortString);
	}
	moveableString.m_memory.m_p = nullptr;
	moveableString.m_length = 0;
	moveableString.m_allocatedLength = 0;
	return *this;
}

StringLW& Hail::StringLW::operator=(const char* const pString)
{
	m_length = StringLength(pString);
	if (m_allocatedLength > 7 && m_length > m_allocatedLength)
	{
		m_allocatedLength = 0;
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
	}
	if (m_length > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
		FromConstCharToWChar(pString, m_memory.m_p, m_length);
		m_memory.m_p[m_length] = 0;
		m_allocatedLength = m_length;
	}
	else
	{
		FromConstCharToWChar(pString, m_memory.m_shortString, 7);
		m_memory.m_shortString[m_length] = 0;
		m_allocatedLength = 0;
	}
	return *this;
}

StringLW& Hail::StringLW::operator=(const wchar_t* const pWString)
{
	m_length = StringLength(pWString);

	if (m_allocatedLength > 7 && m_length > m_allocatedLength)
	{
		m_allocatedLength = 0;
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);
	}

	if (m_length > 7)
	{
		StringMemoryAllocator::GetInstance().AllocateString(pWString, m_length, &m_memory.m_p);
		m_allocatedLength = m_length;
	}
	else
	{
		wcscpy_s(m_memory.m_shortString, pWString);
		m_allocatedLength = 0;
	}
	return *this;
}

StringLW& Hail::StringLW::operator+=(StringLW& anotherString)
{
	const uint32 previousLength = Length();
	const uint32 newLength = previousLength + anotherString.Length();

	if (m_allocatedLength > 0)
	{
		if (newLength > m_allocatedLength)
		{
			wchar_t* previousString = m_memory.m_p;
			StringMemoryAllocator::GetInstance().AllocateString(previousString, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			StringMemoryAllocator::GetInstance().DeallocateString(&previousString);
		}

		memcpy(m_memory.m_p + (previousLength), anotherString.Data(), anotherString.m_length * sizeof(wchar_t));
		m_memory.m_p[newLength] = 0;
	}
	else
	{
		if (newLength > 7)
		{
			wchar_t previousString[8];
			memcpy(previousString, m_memory.m_shortString, previousLength * sizeof(wchar_t));
			StringMemoryAllocator::GetInstance().AllocateString(nullptr, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			memcpy(m_memory.m_p, previousString, previousLength * sizeof(wchar_t));
			memcpy(m_memory.m_p + (previousLength), anotherString.Data(), anotherString.m_length * sizeof(wchar_t));
			m_memory.m_p[newLength] = 0;
		}
		else
		{
			memcpy(&m_memory.m_shortString[previousLength], anotherString.m_memory.m_shortString, anotherString.Length() * sizeof(wchar_t));
			m_memory.m_shortString[newLength] = 0;
		}
	}
	m_length = newLength;

	return *this;
}

StringLW& Hail::StringLW::operator+=(const char* pString)
{
	const uint32 previousLength = m_length;
	const uint32 newStringLength = StringLength(pString);
	const uint32 newLength = newStringLength + previousLength;
	if (m_allocatedLength > 0)
	{
		if (newLength > m_allocatedLength)
		{
			wchar_t* previousString = m_memory.m_p;
			StringMemoryAllocator::GetInstance().AllocateString(previousString, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			StringMemoryAllocator::GetInstance().DeallocateString(&previousString);
		}

		FromConstCharToWChar(pString, m_memory.m_p + (previousLength), newStringLength);
		m_memory.m_p[newLength] = 0;
	}
	else
	{
		if (newLength > 7)
		{
			wchar_t previousString[8];
			memcpy(previousString, m_memory.m_shortString, previousLength * sizeof(wchar_t));
			StringMemoryAllocator::GetInstance().AllocateString(nullptr, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			memcpy(m_memory.m_p, previousString, previousLength * sizeof(wchar_t));
			FromConstCharToWChar(pString, m_memory.m_shortString + (previousLength), newStringLength);
			m_memory.m_p[newLength] = 0;
		}
		else
		{
			FromConstCharToWChar(pString, m_memory.m_shortString + (previousLength), newStringLength);
			m_memory.m_shortString[newLength] = 0;
		}

		m_memory.m_shortString[m_length] = 0;
	}
	m_length = newLength;

	return *this;
}

StringLW& Hail::StringLW::operator+=(const wchar_t* pWString)
{
	const uint32 previousLength = m_length;
	const uint32 newStringLength = StringLength(pWString);
	const uint32 newLength = newStringLength + previousLength;
	if (m_allocatedLength > 0)
	{
		if (newLength > m_allocatedLength)
		{
			wchar_t* previousString = m_memory.m_p;
			StringMemoryAllocator::GetInstance().AllocateString(previousString, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			StringMemoryAllocator::GetInstance().DeallocateString(&previousString);
		}

		memcpy(m_memory.m_p + (previousLength), pWString, newStringLength * sizeof(wchar_t));
		m_memory.m_p[newLength] = 0;
	}
	else
	{
		if (newLength > 7)
		{
			wchar_t previousString[8];
			memcpy(previousString, m_memory.m_shortString, previousLength * sizeof(wchar_t));
			StringMemoryAllocator::GetInstance().AllocateString(nullptr, newLength, &m_memory.m_p);
			m_allocatedLength = newLength;
			memcpy(m_memory.m_p, previousString, previousLength * sizeof(wchar_t));
			memcpy(m_memory.m_p + (previousLength), pWString, newStringLength * sizeof(wchar_t));
			m_memory.m_p[newLength] = 0;
		}
		else
		{
			memcpy(&m_memory.m_shortString[previousLength], pWString, newStringLength * sizeof(wchar_t));
			m_memory.m_shortString[newLength] = 0;
		}
	}
	m_length = newLength;

	return *this;
}

StringLW Hail::StringLW::operator+(const StringLW& string1)
{
	const uint32 previousLength = m_length;
	m_length = string1.m_length + m_length;
	if (m_length > 7)
	{
		wchar_t previousShortString[8];
		if (previousLength < 8)
			memcpy(previousShortString, m_memory.m_shortString, previousLength);

		const wchar_t* previousString = m_allocatedLength > 7 ? m_memory.m_p : previousShortString;
		if (m_length > m_allocatedLength)
		{
			if (m_allocatedLength > 7)
				StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);

			StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
			m_allocatedLength = m_length;
		}
		memcpy(m_memory.m_p, previousString, previousLength * sizeof(wchar_t));
		memcpy(&m_memory.m_p[previousLength], string1.m_allocatedLength > 7 ? string1.m_memory.m_p : string1.m_memory.m_shortString, string1.m_length * sizeof(wchar_t));
		m_memory.m_p[m_length] = 0;
	}
	else
	{
		wchar_t previousShortString[8];
		memcpy(previousShortString, m_memory.m_shortString, previousLength * sizeof(wchar_t));

		memcpy(m_memory.m_shortString, previousShortString, previousLength * sizeof(wchar_t));
		memcpy(m_memory.m_p + (previousLength), string1.m_memory.m_shortString, string1.m_length * sizeof(wchar_t));
		m_memory.m_p[m_length] = 0;
		m_allocatedLength = 0;
	}
	return *this;
}

wchar_t* Hail::StringLW::Data()
{
	return m_length > 7u ? m_memory.m_p : m_memory.m_shortString;
}

const wchar_t* const Hail::StringLW::Data() const
{
	return m_length > 7u ? m_memory.m_p : m_memory.m_shortString;
}

Hail::StringL Hail::StringLW::ToCharString() const
{
	StringL returnString;
	returnString.Reserve(m_length);
	Hail::FromWCharToConstChar(Data(), returnString.Data(), m_length);
	return returnString;
}

void Hail::StringLW::Clear()
{
	if (m_allocatedLength > 7)
		StringMemoryAllocator::GetInstance().DeallocateString(&m_memory.m_p);

	m_memory.m_p = nullptr;
	m_length = 0;
	m_allocatedLength = 0u;
}

void Hail::StringLW::Reserve(uint32 numOfChars)
{
	m_length = numOfChars;
	StringMemoryAllocator::GetInstance().AllocateString(nullptr, m_length, &m_memory.m_p);
	m_allocatedLength = m_length;
}
