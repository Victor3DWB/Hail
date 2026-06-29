#pragma once
#include <string.h>
#include <string>
#include <assert.h>
#include <stdarg.h>

#include "Utility/StringUtility.h"

//TODO: namespace Hail this file

class String64
{
public:
	String64()
	{
		strcpy_s(m_data, "");
	}
	String64(const char* const string)
	{
		strcpy_s(m_data, string);
	}
	String64(const String64& anotherString)
	{
		strcpy_s(m_data, anotherString);
	}
	String64(String64&& moveableString) noexcept
	{
		strcpy_s(m_data, moveableString);
	}
	String64(const std::string& stlString)
	{
		assert(stlString.size() < 64);
		if (stlString.size() < 64)
		{
			strcpy_s(m_data, stlString.data());
		}
		else
		{
			strcpy_s(m_data, "errors n stuff");
		}
	}
	static String64 Format(const char* const format, ...)
	{
		String64 str;
		va_list vl;
		va_start(vl, format);
		vsprintf_s(str.m_data, format, vl);
		va_end(vl);
		return str;
	}

	operator const char* () const
	{
		return m_data;
	}
	operator char* ()
	{
		return m_data;
	}
	String64& operator=(const String64& anotherString)
	{
		strcpy_s(m_data, anotherString);

		return *this;
	}
	String64& operator=(String64&& moveableString) noexcept
	{
		strcpy_s(m_data, moveableString);

		return *this;
	}
	String64& operator=(const char* const pString)
	{
		strcpy_s(m_data, pString);

		return *this;
	}
	char* Data()
	{
		return m_data;
	}
	const char* const Data() const
	{
		return m_data;
	}

	bool Empty() const
	{
		return m_data[0] == '\0';
	}
	const uint32_t Length() const { return strlen(m_data); }

private:
	char m_data[64];
};

static bool operator<(const String64& string1, const String64& string2)
{
	return strcmp(string1, string2) < -0;
}
static bool operator>(const String64& string1, const String64& string2)
{
	return strcmp(string1, string2) > 0;
}
static bool operator==(const String64& string1, const String64& string2)
{
	return strcmp(string1, string2) == 0;
}
static bool operator!=(const String64& string1, const String64& string2)
{
	return strcmp(string1, string2) != 0;
}
static String64 operator+(const String64& string1, const String64& string2)
{
	String64 str = string1;
	strcat_s(str, 64 - strlen(string1), string2);
	return str;
}

namespace Hail
{
	// Long string, used for debug messageing and tools
	class StringL
	{
	public:
		StringL();
		StringL(const char* const string);
		StringL(const StringL& anotherString);
		StringL(const String64& string64);
		StringL(StringL&& moveableString);
		StringL(const std::string& stlString);
		~StringL();

		static StringL Format(const char* const format, ...);

		operator const char* () const;
		operator char* ();
		StringL& operator=(const StringL& anotherString);
		StringL& operator=(StringL&& moveableString);
		StringL& operator=(const char* const pString);
		StringL& operator+=(StringL& anotherString);
		StringL& operator+=(const char* pString);
		StringL& operator+=(const char character);
		StringL operator+(const StringL& string1);

		char* Data();
		const char* const Data() const;
		void Reserve(uint32 numOfChars);
		void RemoveCharsFromBack(uint32 numOfChars);
		// DeAllocates memory if this string uses a memory pool, and sets length to 0.
		void Clear();

		const uint32 Length() const { return m_length; }

	private:

		union stringMemory
		{
			char* m_p;
			char m_shortString[16];
		}m_memory;
		uint32 m_length;
		uint32 m_allocatedLength;
	};
}

class WString64
{
public:
	WString64()
	{
		wcscpy_s(m_data, L"");
	}
	WString64(const wchar_t* const string)
	{
		wcscpy_s(m_data, string);
	}
	WString64(const WString64& anotherString)
	{
		wcscpy_s(m_data, anotherString);
	}
	WString64(WString64&& moveableString) noexcept
	{
		wcscpy_s(m_data, moveableString);
	}
	WString64(const std::wstring& stlString)
	{
		assert(stlString.size() < 64);
		if (stlString.size() < 64)
		{
			wcscpy_s(m_data, stlString.data());
		}
		else
		{
			wcscpy_s(m_data, L"errors n stuff");
		}
	}
	static WString64 Format(const wchar_t* const format, ...)
	{
		WString64 str;
		va_list vl;
		va_start(vl, format);
		vswprintf_s(str.m_data, format, vl);
		va_end(vl);
		return str;
	}

	operator const wchar_t* () const 
	{
		return m_data;
	}

	operator wchar_t* ()
	{
		return m_data;
	}
	WString64& operator=(const WString64& anotherString)
	{
		wcscpy_s(m_data, anotherString);

		return *this;
	}
	WString64& operator=(WString64&& moveableString) noexcept
	{
		wcscpy_s(m_data, moveableString);

		return *this;
	}
	WString64& operator=(const wchar_t* const pString)
	{
		wcscpy_s(m_data, pString);

		return *this;
	}
	wchar_t* Data()
	{
		return m_data;
	}
	const uint32_t Length() const { return wcslen(m_data); }

	String64 ToCharString() const 
	{ 
		String64 returnString;
		Hail::FromWCharToConstChar(m_data, returnString.Data(), 64u);
		return returnString;
	}

private:
	wchar_t m_data[64];
};

static bool operator<(const WString64& string1, const WString64& string2)
{
	return wcscmp(string1, string2) < -0;
}
static bool operator>(const WString64& string1, const WString64& string2)
{
	return wcscmp(string1, string2) > 0;
}
static bool operator==(const WString64& string1, const WString64& string2)
{
	return wcscmp(string1, string2) == 0;
}
static bool operator!=(const WString64& string1, const WString64& string2)
{
	return wcscmp(string1, string2) != 0;
}
static WString64 operator+(const WString64& string1, const WString64& string2)
{
	WString64 str = string1;
	wcscat_s(str, 64 - wcslen(string1), string2);
	return str;
}

class WString256
{
public:
	WString256()
	{
		wcscpy_s(m_data, L"");
	}
	WString256(const wchar_t* const string)
	{
		wcscpy_s(m_data, string);
	}
	WString256(const WString256& anotherString)
	{
		wcscpy_s(m_data, anotherString);
	}
	WString256(WString256&& moveableString) noexcept
	{
		wcscpy_s(m_data, moveableString);
	}
	WString256(const std::wstring& stlString)
	{
		assert(stlString.size() < 256);
		if (stlString.size() < 256)
		{
			wcscpy_s(m_data, stlString.data());
		}
		else
		{
			wcscpy_s(m_data, L"errors n stuff");
		}
	}
	static WString256 Format(const wchar_t* const format, ...)
	{
		WString256 str;
		va_list vl;
		va_start(vl, format);
		vswprintf_s(str.m_data, format, vl);
		va_end(vl);
		return str;
	}

	operator const wchar_t* () const
	{
		return m_data;
	}

	operator wchar_t* ()
	{
		return m_data;
	}
	WString256& operator=(const WString256& anotherString)
	{
		wcscpy_s(m_data, anotherString);

		return *this;
	}
	WString256& operator=(WString256&& moveableString) noexcept
	{
		if (moveableString)
		{
			wcscpy_s(m_data, moveableString);
		}
		else
		{
			m_data[0] = 0;
		}
		return *this;
	}
	WString256& operator=(const wchar_t* const pString)
	{
		if (pString)
		{
			wcscpy_s(m_data, pString);
		}
		else
		{
			m_data[0] = 0;
		}

		return *this;
	}
	wchar_t* Data()
	{
		return m_data;
	}
	const uint32_t Length() const { return wcslen(m_data); }

	Hail::StringL ToCharString() const
	{
		Hail::StringL returnString;
		returnString.Reserve(Length());
		Hail::FromWCharToConstChar(m_data, returnString.Data(), 256);
		return returnString;
	}

private:
	wchar_t m_data[256];
};

static bool operator<(const WString256& string1, const WString256& string2)
{
	return wcscmp(string1, string2) < -0;
}
static bool operator>(const WString256& string1, const WString256& string2)
{
	return wcscmp(string1, string2) > 0;
}
static bool operator==(const WString256& string1, const WString256& string2)
{
	return wcscmp(string1, string2) == 0;
}
static bool operator!=(const WString256& string1, const WString256& string2)
{
	return wcscmp(string1, string2) != 0;
}
static WString256 operator+(const WString256& string1, const WString256& string2)
{
	WString256 str = string1;
	wcscat_s(str, 256 - wcslen(string1), string2);
	return str;
}

namespace Hail
{


	// Long wide string, used for all UI. 
	// TODO, add support for translation and serialization
	class StringLW
	{
	public:
		StringLW();
		StringLW(const char* const string);
		StringLW(const wchar_t* const string);
		StringLW(const StringL& anotherString);
		StringLW(const StringLW& anotherWString);
		StringLW(const String64& string64);
		StringLW(const WString64& wString64);
		StringLW(StringLW&& moveableString);
		~StringLW();

		static StringLW Format(const wchar_t* const format, ...);
		operator const wchar_t* () const;
		operator wchar_t* ();
		StringLW& operator=(const StringLW& anotherString);
		StringLW& operator=(StringLW&& moveableString);
		StringLW& operator=(const char* const pString);
		StringLW& operator=(const wchar_t* const pWString);
		StringLW& operator+=(StringLW& anotherString);
		StringLW& operator+=(const char* pString);
		StringLW& operator+=(const wchar_t* pWString);
		StringLW operator+(const StringLW& string1);

		wchar_t* Data();
		const wchar_t* const Data() const;

		StringL ToCharString() const;

		// Returns length in number of characters, not byte length.
		const uint32 Length() const { return m_length; }

		// DeAllocates memory if this string uses a memory pool, and sets length to 0.
		void Clear();
		void Reserve(uint32 numOfChars);
	private:

		union stringMemory
		{
			wchar_t* m_p;
			wchar_t m_shortString[8];
		}m_memory;
		uint32 m_length;
		uint32 m_allocatedLength;
	};
}
