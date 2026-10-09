// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Tools for grabbing/dumping the stack at runtime

export module ks.core:stacktools;

import :platform;

export extern "C++" {

int GetCallStack( void **pReturnAddressesOut, int iArrayCount, int iSkipCount );

//ONLY WORKS IF THE CRAWLED PORTION OF THE STACK DISABLES FRAME POINTER OMISSION (/Oy-)  "vpc /nofpo"
int GetCallStack_Fast( void **pReturnAddressesOut, int iArrayCount, int iSkipCount );

typedef int (*FN_GetCallStack)( void **pReturnAddressesOut, int iArrayCount, int iSkipCount );

//where we'll find our PDB's for win32.
void SetStackTranslationSymbolSearchPath( const char *szSemicolonSeparatedList = nullptr );
void StackToolsNotify_LoadedLibrary( const char *szLibName );

//maximum output sample "tier0.dll!TranslateStackInfo - u:\Dev\L4D\src\tier0\stacktools.cpp(162) + 4 bytes"
enum TranslateStackInfo_StyleFlags_t
{
	TSISTYLEFLAG_NONE = 0,
	TSISTYLEFLAG_MODULENAME = (1<<0), //start with module			Sample: "tier0.dll!"
	TSISTYLEFLAG_SYMBOLNAME = (1<<1), //include the symbol name		Sample: "TranslateStackInfo"
	TSISTYLEFLAG_FULLPATH = (1<<2), //include full path				Sample: "u:\Dev\L4D\src\tier0\stacktools.cpp"
	TSISTYLEFLAG_SHORTPATH = (1<<3), //only include 2 directories	Sample: "\src\tier0\stacktools.cpp"
	TSISTYLEFLAG_LINE = (1<<4), //file line number					Sample: "(162)"
	TSISTYLEFLAG_LINEANDOFFSET = (1<<5), //file line + offset		Sample: "(162) + 4 bytes"
	TSISTYLEFLAG_LAST = TSISTYLEFLAG_LINEANDOFFSET,
	TSISTYLEFLAG_DEFAULT = (TSISTYLEFLAG_MODULENAME | TSISTYLEFLAG_SYMBOLNAME | TSISTYLEFLAG_FULLPATH | TSISTYLEFLAG_LINEANDOFFSET), //produces sample above
};

//Generates a formatted list of function information, returns number of translated entries
//On 360 this generates a string that can be decoded by VXConsole in print functions. Optimal path for translation because it's one way. Other paths require multiple transactions.
int TranslateStackInfo( const void * const *pCallStack, int iCallStackCount, tchar *szOutput, int iOutBufferSize, const tchar *szEntrySeparator, TranslateStackInfo_StyleFlags_t style = TSISTYLEFLAG_DEFAULT );

void PreloadStackInformation( void * const *pAddresses, int iAddressCount ); //caches data and reduces communication with VXConsole to speed up 360 decoding when using any of the Get***FromAddress() functions. Nop on PC.
bool GetFileAndLineFromAddress( const void *pAddress, tchar *pFileNameOut, int iMaxFileNameLength, uint32 &iLineNumberOut, uint32 *pDisplacementOut = nullptr );
bool GetSymbolNameFromAddress( const void *pAddress, tchar *pSymbolNameOut, int iMaxSymbolNameLength, uint64 *pDisplacementOut = nullptr );
bool GetModuleNameFromAddress( const void *pAddress, tchar *pModuleNameOut, int iMaxModuleNameLength );

class CCallStackStorage //a helper class to grab a stack trace as close to the leaf code surface as possible, then pass it on to deeper functions intact with less unpredictable inlining pollution
{
public:
	CCallStackStorage( FN_GetCallStack GetStackFunction = GetCallStack, uint32 iSkipCalls = 0 );
	CCallStackStorage( const CCallStackStorage &copyFrom );

	void *pStack[128]; //probably too big, possibly too small for some applications. Don't want to spend the time figuring out how to generalize this without templatizing pollution or mallocs
	uint32 iValidEntries;
};

//Encodes data so that every byte's most significant bit is a 1. Ensuring no null terminators.
//This puts the encoded data in the 128-255 value range. Leaving all standard ascii characters for control.
//Returns string length (not including the written null terminator as is standard). 
//Or if the buffer is too small. Returns negative of necessary buffer size (including room needed for null terminator)
int EncodeBinaryToString( const void *pToEncode, int iDataLength, char *pEncodeOut, int iEncodeBufferSize );

//Decodes a string produced by EncodeBinaryToString(). Safe to decode in place if you don't mind trashing your string, binary byte count always less than string byte count.
//Returns:
//	>= 0 is the decoded data size
//	INT_MIN (most negative value possible) indicates an improperly formatted string (not our data)
//	all other negative values are the negative of how much dest buffer size is necessary.
int DecodeBinaryFromString( const char *pString, void *pDestBuffer, int iDestBufferSize, char **ppParseFinishOut = nullptr );

// 360<->VXConsole specific communication definitions

enum StackTranslation_BinaryHandler_Command_t
{
	ST_BHC_LOADEDLIBARY,
	ST_BHC_GETTRANSLATIONINFO,
};

#pragma pack(push)
#pragma pack(1)
struct FullStackInfo_t
{
	const void *pAddress;
	char szModuleName[24];
	char szFileName[MAX_PATH/2];
	char szSymbol[64];
	uint32 iLine;
	uint32 iSymbolOffset;

};
#pragma pack(pop)

}
