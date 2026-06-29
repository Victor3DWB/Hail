#include "ResourceCompiler_PCH.h"
#include "TextureCompiler.h"

#include "DebugMacros.h"

#include "Utility\FileSystem.h"
#include "Utility\StringUtility.h"
#include "Utility\InOutStream.h"

#include "MetaResource.h"

using namespace Hail;
using namespace TextureCompiler;

bool IsPowerOfTwo(uint32_t x)
{
	return (x != 0) && ((x & (x - 1)) == 0);
}

struct TGAHEADER
{
	// Length of id string
	char	idLength;

	// Image storage info
	char	colourMapType;
	char	imageType;

	// Colour Map
	short	firstEntry;
	short	numEntries;
	char	bitsPerEntry;

	// Image description
	short	xOrigin;
	short	yOrigin;
	short	width;
	short	height;
	char	bitsPerPixel;
	char	descriptor;
};

enum
{
	TGA_UNSUPPORTED = 1,
	TGA_NO_IMAGE = 2,
	TGA_MAP = 4,
	TGA_RGB = 8,
	TGA_BW = 16,
	TGA_RLE = 32
};

typedef union PixelInfo
{
    uint32_t Colour;
    struct
    {
        uint8_t R, G, B, A;
    };
} *PPixelInfo;

FilePath ExportCompiled8BitTexture(const FilePath& originalTexturePath, uint8_t* compiledTextureData, TextureProperties shaderHeader, uint32_t numberOfColors, GUID guid);
FilePath ExportCompiled16BitTexture(const FilePath& originalTexturePath, uint16_t* compiledTextureData, TextureProperties shaderHeader, uint32_t numberOfColors, GUID guid) { return FilePath(); };
FilePath ExportCompiled32BitTexture(const FilePath& originalTexturePath, uint32_t* compiledTextureData, TextureProperties shaderHeader, uint32_t numberOfColors, GUID guid){ return FilePath(); };

FilePath TextureCompiler::CompileSpecificTGATexture(const FilePath& filePath, GUID guid)
{
	String64 extension = filePath.Object().Extension().ToCharString();
	H_ASSERT(StringCompare(extension,"tga") || StringCompare(extension, "tga"));
	TGAHEADER tgaHeader;
	InOutStream tgaFile;
	if (!tgaFile.OpenFile(filePath, FILE_OPEN_TYPE::READ, true))
	{
		return FilePath();
	}

	// Go to end of file to check TGA version
	// 
	tgaFile.SeekToEnd();

	// We need to store the file size for a worst case scenario
	// RLE compression can increase the amount of data
	// depending on the image. (This scenario will only arise, in
	// an image with very few same pixel runs).

	// Seek to version identifier (Always specified as being 18
	// characters from the end of the file)

	tgaFile.Seek(-18, 1);

	// Read version identifier
	char versionCheck[17] = "";
	tgaFile.Read(versionCheck, 16);

	// Check version
	int version = 1;
	if (strcmp(versionCheck, "TRUEVISION-XFILE") == 0)
	{
		version = 2;
	}

	// Back to the beginning of the file
	tgaFile.SeekToStart();

	// Need to read each field in one at a time since the structure padding likes
	// to eat the 4th and 10th bytes
	// 
	tgaFile.Read(&tgaHeader.idLength, sizeof(tgaHeader.idLength));
	tgaFile.Read(&tgaHeader.colourMapType, sizeof(tgaHeader.colourMapType));
	tgaFile.Read(&tgaHeader.imageType, sizeof(tgaHeader.imageType));

	// If colourMapType is 0 and these 3 fields below are not 0, something may have went wrong
	tgaFile.Read((char*)(&tgaHeader.firstEntry), sizeof(tgaHeader.firstEntry));
	tgaFile.Read((char*)(&tgaHeader.numEntries), sizeof(tgaHeader.numEntries));
	tgaFile.Read(&tgaHeader.bitsPerEntry, sizeof(tgaHeader.bitsPerEntry));


	tgaFile.Read((char*)(&tgaHeader.xOrigin), sizeof(tgaHeader.xOrigin));
	tgaFile.Read((char*)(&tgaHeader.yOrigin), sizeof(tgaHeader.yOrigin));
	tgaFile.Read((char*)(&tgaHeader.width), sizeof(tgaHeader.width));
	tgaFile.Read((char*)(&tgaHeader.height), sizeof(tgaHeader.height));
	tgaFile.Read(&tgaHeader.bitsPerPixel, sizeof(tgaHeader.bitsPerPixel));
	tgaFile.Read(&tgaHeader.descriptor, sizeof(tgaHeader.descriptor));

	// Holds bitwise flags for TGA file
	int tgaDesc = 0;

	switch (tgaHeader.imageType)
	{
	case 0:
		tgaDesc |= TGA_NO_IMAGE;
		break;
	case 2:
		tgaDesc |= TGA_RGB;
		break;
	case 10:
		tgaDesc |= (TGA_RGB | TGA_RLE);
		break;
	default:
		tgaDesc |= TGA_UNSUPPORTED;
		break;
	}

	if ((tgaDesc & TGA_UNSUPPORTED) == 0) 
	{
		//Debug_PrintConsoleConstChar("TGA Format Supported");
	}
	else
	{
		String64 textureName;
		FromWCharToConstChar(filePath.Object().Name(), textureName, 64);
		Debug_PrintConsoleConstChar("TGA Format UnSupported in file: ");
		Debug_PrintConsoleConstChar(textureName);
		return FilePath();
	}
	char* skip = "";
	tgaFile.Read(skip, tgaHeader.idLength);

	// Skip the colour map if it doesn't exist
	if (!(tgaDesc & TGA_MAP))
	{
		int colourMapSize = tgaHeader.colourMapType * tgaHeader.numEntries;
		tgaFile.Read(skip, colourMapSize);
	}

	//if (!IsPowerOfTwo(tgaHeader.height) || !IsPowerOfTwo(tgaHeader.width))
	//{
	//	Debug_PrintConsoleConstChar("Texture is not in Power of 2");
	//	return false;
	//}

	// imageDataSize is the total number of bytes taken by the image
	// after being loaded and decompressed if necessary
	int imageDataSize = tgaHeader.width * tgaHeader.height * (tgaHeader.bitsPerPixel / 8);
	
	unsigned char* pixelData = new unsigned char[imageDataSize];

	// Read the image data
	int originalPosition = (int)tgaFile.GetFileSeekPosition();

	// This read operation may read past the end of the file
	// so could break something (hasn't happened yet)
	tgaFile.Read((char*)pixelData, imageDataSize);

	// RLE decoding
	if (tgaDesc & TGA_RLE)
	{
		Debug_PrintConsoleConstChar("Decoding RLE");
		// Used to decode RLE
		char* tempPixelData;
		tempPixelData = new char[tgaFile.GetFileSize()];
		// Copy data over for decoding
		memcpy(tempPixelData, pixelData, tgaFile.GetFileSize());
		// Holds the current pixel index for the j loop below
		int indexAccum = 0;
		int bytesPerPixel = (tgaHeader.bitsPerPixel / 8);
		int bytesPerPixelRLE = bytesPerPixel + 1;

		// Increments of i are controlled in the for loop because depending
		// on whether or not the packet being checked is run-length encoded 
		// the increment may have to be between bytesPerPixel and 128 (Max size of either packet)
		for (int i = 0; indexAccum < imageDataSize; )
		{
			// runCount holds the length of the packet taken from the packet info byte
			// runCount can be a maximum of 127.
			int runCount = (127 & tempPixelData[i]) + 1;

			// Check the packet info byte for RLE
			// Run-length encoded packet
			if (128 & tempPixelData[i])
			{
				// In an encoded packet, runCount specifies
				// the repititions of the pixel data (up to 127)
				for (int j = 0; j < runCount; j++)
				{
					for (int k = 1; k < bytesPerPixelRLE; k++)
						pixelData[indexAccum++] = tempPixelData[i + k];
				}

				i += bytesPerPixelRLE;
			}

			// Raw data packet
			else if (!(128 & tempPixelData[i]))
			{
				// Skip past the packet info byte
				i++;

				// In a raw packet, runCount specifies
				// the number of pixels that are to follow (up to 127)
				for (int j = 0; j < runCount; j++)
				{
					for (int k = 0; k < bytesPerPixel; k++)
						pixelData[indexAccum++] = tempPixelData[i + k];

					i += bytesPerPixel;
				}
			}
		}

		delete[] tempPixelData;
	}
	tgaFile.CloseFile();

	TextureProperties compileHeader;
	uint32_t numberOfColors = 0;
	if (tgaHeader.bitsPerPixel / 8 == 3)
	{
		compileHeader.textureType = (uint16)(Hail::eTextureSerializeableType::R8G8B8);
		numberOfColors = 3;
	}
	else if (tgaHeader.bitsPerPixel / 8 == 4)
	{
		compileHeader.textureType = (uint16)(Hail::eTextureSerializeableType::R8G8B8A8);
		numberOfColors = 4;
	}
	compileHeader.height = tgaHeader.height;
	compileHeader.width = tgaHeader.width;

	return ExportCompiled8BitTexture(filePath, pixelData, compileHeader, numberOfColors, guid);
}

FilePath ExportCompiled8BitTexture(const FilePath& originalTexturePath, uint8_t* compiledTextureData, TextureProperties TextureProperties, uint32_t numberOfColors, GUID guid)
{
	FileObject textureName = originalTexturePath.Object();
	InOutStream textureExporter;
	textureName.SetExtension(L"txr");
	FilePath finalPath = FilePath::GetTextureCompiledDirectory() + textureName;
	
	if (!textureExporter.OpenFile(finalPath, FILE_OPEN_TYPE::WRITE, true))
	{
		return FilePath();
	}
	String64 nameCString;
	FromWCharToConstChar(textureName.Name(), nameCString, 64);

	textureExporter.Write((char*)&TextureProperties, TextureHeaderSize, 1);

	for (uint32_t i = 0; i < TextureProperties.width * TextureProperties.height * numberOfColors; i+= numberOfColors)
	{
		switch (numberOfColors)
		{
		case 1:
			textureExporter.Write((char*)&compiledTextureData[i], sizeof(char), 1);
			break;
		case 2:
			textureExporter.Write((char*)&compiledTextureData[i], sizeof(char), 1);
			textureExporter.Write((char*)&compiledTextureData[i + 1], sizeof(char), 1);
			break;
		case 3:
			textureExporter.Write((char*)&compiledTextureData[i + 2], sizeof(char), 1); //flipping to rgb from bgr
			textureExporter.Write((char*)&compiledTextureData[i + 1], sizeof(char), 1);
			textureExporter.Write((char*)&compiledTextureData[i], sizeof(char), 1);
			break;
		case 4:
			textureExporter.Write((char*)&compiledTextureData[i + 2], sizeof(char), 1); //flipping to rgb from bgr
			textureExporter.Write((char*)&compiledTextureData[i + 1], sizeof(char), 1);
			textureExporter.Write((char*)&compiledTextureData[i], sizeof(char), 1);
			textureExporter.Write((char*)&compiledTextureData[i + 3], sizeof(char), 1);
			break;
		default:
			break;
		}
	}
	//outStream.write((char*)&compiledTextureData, TextureProperties.width * TextureProperties.height * sizeOfColor * numberOfColors);

	// TODO: check if resource exist if we overwrite it, and get its GUID
	MetaResource textureMetaResource;
	textureMetaResource.ConstructResourceAndID(originalTexturePath, finalPath, guid);
	textureMetaResource.Serialize(textureExporter);
	textureExporter.CloseFile();
	return finalPath;
}


