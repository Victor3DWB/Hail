#include "ReflectionCodeGenerator_PCH.h"
#include "CppParser.h"
#include "Reflection/ReflectionDefines.h"
#include <iostream>

#include "Utility\FileSystem.h"
#include "DebugMacros.h"

#include <fstream>
#include "Utility\StringUtility.h"
#include "Utility\InOutStream.h"

using namespace Hail;

namespace
{
	StaticArray<const char*, 11> BASIC_TYPE_NAMES{
	"bool",
	"uint8",
	"uint16",
	"uint32",
	"uint64",
	"int8",
	"int16",
	"int32",
	"int64",
	"float32",
	"float64",
	};

	constexpr const char* REFLECTABLE_CLASS_TO_LOOK_FOR = "REFLECTABLE_CLASS()";
	constexpr const char* REFLECTABLE_MEMBER_TO_LOOK_FOR = "REFLECTABLE_MEMBER()";
	constexpr const char* REFLECTION_COMMENT = "//Reflection code inserted below, do not add or modify code in the namespace Reflection";

	enum class HPPTokens
	{
		OPENING_BRACKET = '{',
		CLOSING_BRACKET = '}'
	};

	constexpr char CHAR_SPACE = ' ';
	constexpr char CHAR_FORWARD_SLASH = '/';
	constexpr char CHAR_COMMENT_STAR = '*';
	constexpr char CHAR_BRACKET_START = '{';
	constexpr char CHAR_BRACKET_END = '}';

	[[nodiscard]] bool IsCharacterASpecialCPlusPlusChar(char charToCheck)
	{
		switch (charToCheck)
		{
		case '/' :
			return true;
			break;
		case '{':
			return true;
			break;
		case '}':
			return true;
			break;
		case '*':
			return true;
			break;
		case '(':
			return true;
			break;
		case ')':
			return true;
			break;
		case ' ':
			return true;
			break;
		case '=':
			return true;
			break;
		case '-':
			return true;
			break;
		case '+':
			return true;
			break;
		case '\r':
			return true;
			break;
		case '\t':
			return true;
			break;
		case ';':
			return true;
			break;
		case ':':
			return true;
			break;
		default:
			break;
		}
		return false;
	}


	[[nodiscard]] int ParseWhiteLines(const char* lineToParse, size_t startToken)
	{
		const size_t stringSize = Hail::StringLength(lineToParse);
		if (startToken >= stringSize)
		{
			return Hail::INVALID_STRING_RESULT;
		}
		bool inComments = false;
		size_t currentCharacterIndex = startToken;
		char currentCharacter = lineToParse[currentCharacterIndex];
		while (currentCharacterIndex < stringSize)
		{
			if (inComments)
			{
				currentCharacter = lineToParse[++currentCharacterIndex];
				if (currentCharacter == CHAR_COMMENT_STAR && lineToParse[currentCharacterIndex + 1] == CHAR_FORWARD_SLASH)
				{
					inComments = false;
					currentCharacterIndex += 2;
					currentCharacter = lineToParse[currentCharacterIndex];
					continue;
				}
				continue;
			}

			if (currentCharacter == CHAR_SPACE)
			{
				currentCharacter = lineToParse[++currentCharacterIndex];
				continue;
			}
			
			if(currentCharacter == CHAR_FORWARD_SLASH && lineToParse[currentCharacterIndex + 1] == CHAR_COMMENT_STAR)
			{
				inComments = true;
				currentCharacterIndex += 2;
				currentCharacter = lineToParse[currentCharacterIndex];
				continue;
			}
			return currentCharacterIndex;
		}
		return Hail::INVALID_STRING_RESULT;
	}
	[[nodiscard]] int GetCPlusPlusSymbolLengthInLine(const char* lineToParse, size_t startToken)
	{
		const size_t stringSize = Hail::StringLength(lineToParse);
		if (startToken >= stringSize)
		{
			return Hail::INVALID_STRING_RESULT;
		}
		size_t currentCharacterIndex = startToken;
		char currentCharacter = lineToParse[currentCharacterIndex];
		while (currentCharacterIndex < stringSize)
		{
			if (IsCharacterASpecialCPlusPlusChar(currentCharacter))
			{
				return currentCharacterIndex - startToken;
			}
			currentCharacter = lineToParse[++currentCharacterIndex];
		}
		return Hail::END_OF_STRING;
	}
	GrowingArray<const char*> CPLUSPLUS_TOKENS{
		"const",
		"explicit",
		"inline",
		"volatile"
	};

	[[nodiscard]] bool TokenIsASpecialMemberOrCPlusPlusToken(const char* tokenToCheck)
	{
		size_t length = Hail::StringLength(tokenToCheck);
		if (length == 0)
		{
			return true;
		}
		if (length == 1)
		{
			return IsCharacterASpecialCPlusPlusChar(tokenToCheck[0]);
		}
		for (size_t i = 0; i < CPLUSPLUS_TOKENS.Size(); i++)
		{
			if (Hail::StringCompare(CPLUSPLUS_TOKENS[i], tokenToCheck))
			{
				return true;
			}
		}
		return false;
	}
	struct ReflectableMember
	{
		String64 name;
		String64 type;
		bool isANewDefinedType = true;
	};

	[[nodiscard]] ReflectableMember FindMemberInLine(StringL lineToCheck)
	{
		int currentCharacterIndex = 0;
		ReflectableMember returnMember;
		while (currentCharacterIndex != Hail::END_OF_STRING)
		{
			int previousCharacterIndex = currentCharacterIndex;
			int tokenLength = GetCPlusPlusSymbolLengthInLine(lineToCheck.Data(), currentCharacterIndex);
			if (tokenLength == Hail::INVALID_STRING_RESULT)
			{
				return returnMember;
			}
			if (tokenLength != 0)
			{
				if (tokenLength == Hail::END_OF_STRING)
				{
					tokenLength = lineToCheck.Length() - currentCharacterIndex;
				}

				StringL token;
				memcpy(token.Data(), lineToCheck.Data() + previousCharacterIndex, tokenLength);
				token[tokenLength] = '\0';

				if (!TokenIsASpecialMemberOrCPlusPlusToken(token))
				{
					assert(token.Length() < 64);
					if (returnMember.type.Empty())
					{
						returnMember.type = token;
						for (uint32_t i = 0; i < static_cast<uint32_t>(BASIC_TYPE_NAMES.Getsize()); i++)
						{
							if (Hail::StringCompare(BASIC_TYPE_NAMES[i], token))
							{
								returnMember.isANewDefinedType = false;
								break;
							}
						}
					}
					else
					{
						returnMember.name = token;
						return returnMember;
					}
				}
			}

			if (tokenLength == 0)
			{
				currentCharacterIndex = previousCharacterIndex + 1;
			}
			else
			{
				currentCharacterIndex += tokenLength;
			}
		}
		return returnMember;
	}



	struct ReflectableStructure
	{
		Hail::FilePath file;
		unsigned int bracketLevel = 0;
		String64 structName;
		Stack<String64> parentNames;
		GrowingArray<ReflectableMember> members;
	};

	struct FileReflectableStructures
	{
		Hail::FilePath file;
		GrowingArray<ReflectableStructure> structuresInFile;
	};

	[[nodiscard]] FileReflectableStructures GetStructuresFromFile(const GrowingArray<StringL>& lines, const Hail::FilePath& currentPath)
	{
		String64 fileName;
		wcstombs(fileName, currentPath.Object().Name(), currentPath.Object().Name().Length() + 1);

		FileReflectableStructures reflectableStructuresInFile;
		reflectableStructuresInFile.file = currentPath;

		size_t lineWhereClassTokenWillBeFound = 0;
		size_t lineWhereMemberTokenWillBeFound = 0;
		unsigned int bracketLevel = 0;
		Stack<ReflectableStructure> currentStructures;

		size_t lineToInsertNamespaceIn = lines.Size();

		for (size_t i = 0; i < lines.Size(); i++)
		{
			const StringL& line = lines[i];
			if (Hail::StringContains(line.Data(), REFLECTABLE_CLASS_TO_LOOK_FOR))
			{
				lineWhereClassTokenWillBeFound = i + 1;

				Debug_PrintConsoleConstChar("Reflectable class in:");
				Debug_PrintConsoleConstChar(fileName);

				ReflectableStructure currentStructure;
				currentStructure.bracketLevel = bracketLevel;

				if (currentStructures.Size() > 0)
				{
					//if the current structure is not a child
					if (currentStructures.Top().bracketLevel >= bracketLevel)
					{
						reflectableStructuresInFile.structuresInFile.Add(currentStructures.Pop());
					}
					else
					{
						ReflectableStructure parentStructure = currentStructures.Top();
						while (parentStructure.parentNames.Size() > 0)
						{
							currentStructure.parentNames.Push(parentStructure.parentNames.Pop());
						}
						currentStructure.parentNames.Push(parentStructure.structName);
					}
				}
				currentStructures.Push(currentStructure);
			}

			if (currentStructures.Size() > 0)
			{
				ReflectableStructure& structureToManipulate = currentStructures.Top();
				if (lineWhereClassTokenWillBeFound == i)
				{
					//Gather class name and search for opening bracket
					int startOfClassName = Hail::StringContainsAndEndsAtIndex(line.Data(), "class");
					if (startOfClassName != Hail::INVALID_STRING_RESULT)
					{
						startOfClassName += 5;//Jump over class token = 5 characters
						startOfClassName += ParseWhiteLines(line.Data(), startOfClassName) - startOfClassName;
						int classNameLength = GetCPlusPlusSymbolLengthInLine(line.Data(), startOfClassName);
						if (classNameLength != Hail::INVALID_STRING_RESULT)
						{
							classNameLength = classNameLength != Hail::END_OF_STRING ? classNameLength : Hail::StringLength(line.Data()) - startOfClassName;
							String64 className;
							memcpy(className.Data(), line.Data() + startOfClassName, classNameLength);
							className[classNameLength] = '\0';

							Debug_PrintConsoleConstChar("class name:");
							Debug_PrintConsoleConstChar(className.Data());

							structureToManipulate.structName = className;
						}
					}
					else
					{
						int startOfStructName = Hail::StringContainsAndEndsAtIndex(line.Data(), "struct");
						if (startOfStructName != Hail::INVALID_STRING_RESULT)
						{
							startOfStructName += 6;//Jump over struct token = 6 characters
							startOfStructName += ParseWhiteLines(line.Data(), startOfStructName) - startOfStructName;
							int structNameLength = GetCPlusPlusSymbolLengthInLine(line.Data(), startOfStructName);
							if (structNameLength != Hail::INVALID_STRING_RESULT)
							{
								structNameLength = structNameLength != Hail::END_OF_STRING ? structNameLength : Hail::StringLength(line.Data()) - startOfStructName;
								String64 structName;
								memcpy(structName.Data(), line.Data() + startOfStructName, structNameLength);
								structName[structNameLength] = '\0';

								Debug_PrintConsoleConstChar("struct name:");
								Debug_PrintConsoleConstChar(structName.Data());

								structureToManipulate.structName = structName;
							}
						}
						else
						{
							//error
						}
					}

				}

				if (Hail::StringContains(line.Data(), REFLECTABLE_MEMBER_TO_LOOK_FOR))
				{
					Debug_PrintConsoleConstChar("Reflectable member in:");
					Debug_PrintConsoleConstChar(structureToManipulate.structName);
					lineWhereMemberTokenWillBeFound = i + 1;
				}
				if (i == lineWhereMemberTokenWillBeFound)
				{
					ReflectableMember member = FindMemberInLine(line);
					if (!member.name.Empty() && !member.type.Empty())
					{
						Debug_PrintConsoleConstChar("Member name:");

						Debug_PrintConsoleString64(member.name);
						structureToManipulate.members.Add(member);
					}
					else
					{
						assert(false, "Define must be right above the member variable that will be reflected");
					}
				}

			}
			if (Hail::StringContains(line.Data(), Hail::REFLECTION_NAMESPACE))
			{
				lineToInsertNamespaceIn = i;
				break;
			}

			if (Hail::StringContains(line.Data(), CHAR_BRACKET_START))
			{
				bracketLevel++;
			}
			if (Hail::StringContains(line.Data(), CHAR_BRACKET_END))
			{
				if (bracketLevel > 0)
				{
					bracketLevel--;
				}
			}
		}

		while (currentStructures.Size() > 0)
		{
			reflectableStructuresInFile.structuresInFile.Add(currentStructures.Pop());
		}
		return reflectableStructuresInFile;
	}

	void WriteToCPPFile(Hail::FilePath pathToParseToo, unsigned int lastIncludeLine, const GrowingArray<StringL>& lines, FileReflectableStructures& fileStructures)
	{
		Hail::InOutStream outStream;
		if (!outStream.OpenFile(pathToParseToo, Hail::FILE_OPEN_TYPE::WRITE, false))
		{
			return;
		}

		for (size_t i = 0; i < lastIncludeLine; i++)
		{
			const auto& line = lines[i];
			outStream.Write(line.Data(), sizeof(char), line.Length());
			outStream.Write("\n", sizeof(char), 1);
		}

		//if (lineToInsertNamespaceIn == lines.Size())
		{
			outStream.Write(REFLECTION_COMMENT, sizeof(char), Hail::StringLength(REFLECTION_COMMENT));
			outStream.Write("\n", sizeof(char), 1);
		}
		outStream.Write(Hail::REFLECTION_NAMESPACE, sizeof(char), Hail::StringLength(Hail::REFLECTION_NAMESPACE));
		outStream.Write("\n{\n", sizeof(char), 3);
		for (size_t structureIndex = 0; structureIndex < fileStructures.structuresInFile.Size(); structureIndex++)
		{
			ReflectableStructure& currentStructure = fileStructures.structuresInFile[structureIndex];
			{
				StringL structureName;
				while (currentStructure.parentNames.Size() > 0)
				{
					String64 parentName = currentStructure.parentNames.Pop();
					Hail::AddToString(structureName.Data(), parentName, 256);
					Hail::AddToString(structureName.Data(), "::", 256);
				}
				Hail::AddToString(structureName.Data(), currentStructure.structName, 256);
				StringL beginAttributesLine = StringL::Format("\tBEGIN_ATTRIBUTES_FOR(%s)\n", structureName.Data());
				outStream.Write(beginAttributesLine.Data(), sizeof(char), beginAttributesLine.Length());
			}
			for (size_t memberVariableIndex = 0; memberVariableIndex < currentStructure.members.Size(); memberVariableIndex++)
			{
				ReflectableMember currentMemberVariable = currentStructure.members[memberVariableIndex];
				StringL reflectableMemberString = StringL::Format("\tDEFINE_MEMBER(%s, %s)\n",
					currentMemberVariable.name.Data(),
					currentMemberVariable.type.Data());
				outStream.Write(reflectableMemberString.Data(), sizeof(char), reflectableMemberString.Length());
			}
			{
				String64 endAttributesLine = "\tEND_ATTRIBUTES\n";
				outStream.Write(endAttributesLine.Data(), sizeof(char), endAttributesLine.Length());
			}
		}
		outStream.Write("\n}\n", sizeof(char), 3);
		size_t i = lastIncludeLine;
		for ( ; i < lines.Size(); i++)
		{
			const auto& line = lines[i];
			outStream.Write(line.Data(), sizeof(char), line.Length());
			if (i + 1 != lines.Size())
			{
				outStream.Write("\n", sizeof(char), 1);
			}
		}
		outStream.CloseFile();
	}
}

void Hail::ParseAndGenerateCodeForProjects(const char* projectToParse)
{
	GrowingArray<FileReflectableStructures> filesWithStructures(6);
	Debug_PrintConsoleConstChar("Searching for reflection defines in header files");
	{
		StringL projectDirectory = SOURCE_DIR;
		projectDirectory = projectDirectory + ("/") + projectToParse + ("/");
		RecursiveFileIterator* iterator = new RecursiveFileIterator(projectDirectory.Data());
		while (iterator->IterateOverFolderRecursively())
		{
			const FilePath currentPath = iterator->GetCurrentPath();
			const FileObject currentObject = currentPath.Object();

			String64 extension = currentObject.Extension().ToCharString();
			if (!StringContains("h", extension.Data()) || !StringContains("hpp", extension.Data()))
			{
				continue;
			}
			InOutStream stream;
			if (!stream.OpenFile(currentPath, FILE_OPEN_TYPE::READ, false))
			{
				Debug_PrintConsoleConstChar("Failed to open cpp file for reading.");
				continue;
			}
			GrowingArray<char> readData(stream.GetFileSize(), 0);
			char character = 1;
			uint32_t sizeofchar = sizeof(char);
			stream.Read(readData.Data(), stream.GetFileSize(), 1);
			GrowingArray<StringL> lines(128);
			StringL currentLine;
			size_t newLineStart = 0;
			for (size_t i = 0; i < stream.GetFileSize(); i++)
			{
				if (readData[i] != '\n')
				{
					const int characterIndex = i - newLineStart;
					if (characterIndex > 254)
					{
						break;
					}
					//TODO: Assert if larger than 254
					currentLine[characterIndex] = readData[i];
				}
				else
				{
					const int characterIndex = i - newLineStart;
					//TODO: Add string support longer than 256 and implement it in this file
					if (characterIndex > 254)
					{
						break;
					}
					currentLine[characterIndex] = '\0';
					lines.Add(currentLine);
					newLineStart = i + 1;
					currentLine[0] = '\0';
				}
			}
			FileReflectableStructures headerStructure = GetStructuresFromFile(lines, currentPath);
			if (!headerStructure.structuresInFile.Empty())
			{
				filesWithStructures.Add(headerStructure);
			}
		}
		delete iterator;
	}


	Debug_PrintConsoleConstChar("Inserting reflection code in cpp files");
	for (size_t i = 0; i < filesWithStructures.Size(); i++)
	{
		//Move to new function
		if (filesWithStructures[i].structuresInFile.Size() > 0)
		{
			GrowingArray<StringL> lines(64);
			const FileObject fileObjectsHeader = filesWithStructures[i].file.Object(); 
			FileIterator iterator = FileIterator(filesWithStructures[i].file.Parent());
			unsigned int lineWhereIncludesStop = 0;
			FilePath currentPath;
			while (iterator.IterateOverFolder())
			{
				currentPath = iterator.GetCurrentPath();
				const FileObject currentObject = currentPath.Object();
				bool correctImplementationFile = false;
				if (fileObjectsHeader.Name() == currentObject.Name()
					&& WString64(L"cpp") == currentObject.Extension())
				{
					correctImplementationFile = true;
				}

				if (!correctImplementationFile)
				{
					continue;
				}

				InOutStream stream;
				if (!stream.OpenFile(currentPath, FILE_OPEN_TYPE::READ, false))
				{
					Debug_PrintConsoleConstChar("Failed to open cpp file for reading.");
				}

				GrowingArray<char> readData(stream.GetFileSize(), 0);

				stream.Read(readData.Data(), stream.GetFileSize(), 1);
				StringL currentLine;
				size_t newLineStart = 0;
				uint32 lineWhereReflectionNamespaceWasFound = 0xffffffff;
				bool foundNamespaceOpeningBracket = false;
				bool namespaceHaveBeenSkipped = false;
				for (size_t i = 0; i < stream.GetFileSize(); i++)
				{
					if (readData[i] != '\n')
					{
						currentLine[i - newLineStart] = readData[i];
						if (i + 1 == stream.GetFileSize())
						{
							currentLine[(i + 1) - newLineStart] = '\0';
							lines.Add(currentLine);
							break;
						}
						continue;
					}

					currentLine[i - newLineStart] = '\0';

					if (StringContains(currentLine.Data(), "#include"))
					{
						lineWhereIncludesStop = lines.Size() + 2; //+2 is just to give some space from the includes
					}

					if (lineWhereReflectionNamespaceWasFound == 0xffffffff)
					{
						if (StringContains(currentLine.Data(), REFLECTION_COMMENT))
						{
							lineWhereReflectionNamespaceWasFound = lines.Size();
							newLineStart = i + 1;
							currentLine[0] = '\0';
							continue;
						}
						lines.Add(currentLine);
						newLineStart = i + 1;
						currentLine[0] = '\0';
						continue;
					}
					if (!namespaceHaveBeenSkipped)
					{
						if (!foundNamespaceOpeningBracket && StringContains(currentLine.Data(), '{'))
						{
							foundNamespaceOpeningBracket = true;
						}
						if (foundNamespaceOpeningBracket && StringContains(currentLine.Data(), '}'))
						{
							namespaceHaveBeenSkipped = true;
						}
						newLineStart = i + 1;
						currentLine[0] = '\0';
						continue;
					}
					lines.Add(currentLine);
					newLineStart = i + 1;
					currentLine[0] = '\0';
				}
				break;
			}

			if (lines.Empty())
			{
				continue;
			}
			WriteToCPPFile(currentPath, lineWhereIncludesStop, lines, filesWithStructures[i]);

		}
	}

}
