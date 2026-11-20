// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.stacktools;
import ks.core.types;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;

// First include standard libraries
import std;


CCallStackStorage::CCallStackStorage( const CCallStackStorage &copyFrom )
{
	iValidEntries = copyFrom.iValidEntries;
	std::memcpy( pStack, copyFrom.pStack, sizeof( void * ) * copyFrom.iValidEntries );
}


int GetCallStack( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	return 0;
}

int GetCallStack_Fast( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	return 0;
}

//where we'll find our PDB's for win32. Translation will not work until this has been called once (even if with NULL)
void SetStackTranslationSymbolSearchPath( const char *szSemicolonSeparatedList )
{
}

void StackToolsNotify_LoadedLibrary( const char *szLibName )
{
}

int TranslateStackInfo( const void * const *pCallStack, int iCallStackCount, tchar *szOutput, int iOutBufferSize, const tchar *szEntrySeparator, TranslateStackInfo_StyleFlags_t style )
{
	if( iOutBufferSize > 0 )
		*szOutput = '\0';

	return 0;
}

void PreloadStackInformation( const void **pAddresses, int iAddressCount )
{
}

bool GetFileAndLineFromAddress( const void *pAddress, tchar *pFileNameOut, int iMaxFileNameLength, uint32 &iLineNumberOut, uint32 *pDisplacementOut )
{
	if( iMaxFileNameLength > 0 )
		*pFileNameOut = '\0';

	return false;
}

bool GetSymbolNameFromAddress( const void *pAddress, tchar *pSymbolNameOut, int iMaxSymbolNameLength, uint64 *pDisplacementOut )
{
	if( iMaxSymbolNameLength > 0 )
		*pSymbolNameOut = '\0';

	return false;
}

bool GetModuleNameFromAddress( const void *pAddress, tchar *pModuleNameOut, int iMaxModuleNameLength )
{
	if( iMaxModuleNameLength > 0 )
		*pModuleNameOut = '\0';

	return false;
}


CCallStackStorage::CCallStackStorage( FN_GetCallStack GetStackFunction, uint32 iSkipCalls )
{
	iValidEntries = GetStackFunction( pStack, std::size( pStack ), iSkipCalls + 1 );
}


//Encodes data so that every byte's most significant bit is a 1. Ensuring no null terminators.
//This puts the encoded data in the 128-255 value range. Leaving all standard ascii characters for control.
//Returns string length (not including the written null terminator as is standard). 
//Or if the buffer is too small. Returns negative of necessary buffer size (including room needed for null terminator)
int EncodeBinaryToString( const void *pToEncode, int iDataLength, char *pEncodeOut, int iEncodeBufferSize )
{
	int iEncodedSize = iDataLength;
	iEncodedSize += (iEncodedSize + 6) / 7; //Have 1 control byte for every 7 actual bytes
	iEncodedSize += sizeof( uint32 ) + 1; //data size at the beginning of the blob and null terminator at the end

	if( (iEncodedSize > iEncodeBufferSize) || (pEncodeOut == nullptr) || (pToEncode == nullptr) )
		return -iEncodedSize; //not enough room

	uint8 *pEncodeWrite = (uint8 *)pEncodeOut;	

	//first encode the data size. Encodes lowest 28 bits and discards the high 4
	pEncodeWrite[0] = ((iDataLength >> 21) & 0xFF) | 0x80;
	pEncodeWrite[1] = ((iDataLength >> 14) & 0xFF) | 0x80;
	pEncodeWrite[2] = ((iDataLength >> 7) & 0xFF) | 0x80;
	pEncodeWrite[3] = ((iDataLength >> 0) & 0xFF) | 0x80;
	pEncodeWrite += 4;

	const uint8 *pEncodeRead = (const uint8 *)pToEncode;
	const uint8 *pEncodeStop = pEncodeRead + iDataLength;
	uint8 *pEncodeWriteLastControlByte = pEncodeWrite;
	int iControl = 0;

	//Encode the data
	while( pEncodeRead < pEncodeStop )
	{
		if( iControl == 0 )
		{
			pEncodeWriteLastControlByte = pEncodeWrite;
			*pEncodeWriteLastControlByte = 0x80;
		}
		else
		{
			*pEncodeWrite = *pEncodeRead | 0x80; //encoded data always has the MSB bit set (cheap avoidance of null terminators)
			*pEncodeWriteLastControlByte |= (((*pEncodeRead) & 0x80) ^ 0x80) >> iControl; //We use the control byte to XOR the MSB back to original values on decode
			++pEncodeRead;
		}

		++pEncodeWrite;
		++iControl;
		iControl &= 7; //8->0
	}
	*pEncodeWrite = '\0';

	return iEncodedSize - 1;
}

//Decodes a string produced by EncodeBinaryToString(). Safe to decode in place if you don't mind trashing your string, binary byte count always less than string byte count.
//Returns:
//	>= 0 is the decoded data size
//	INT_MIN (most negative value possible) indicates an improperly formatted string (not our data)
//	all other negative values are the negative of how much dest buffer size is necessary.
int DecodeBinaryFromString( const char *pString, void *pDestBuffer, int iDestBufferSize, char **ppParseFinishOut )
{
	const uint8 *pDecodeRead = (const uint8 *)pString;

	if( (pDecodeRead[0] < 0x80) || (pDecodeRead[1] < 0x80) || (pDecodeRead[2] < 0x80) || (pDecodeRead[3] < 0x80) )
	{
		if( ppParseFinishOut != nullptr )
			*ppParseFinishOut = (char *)pString;

		return std::numeric_limits<int>::min(); //Don't know what the string is, but it's not our format
	}

	int iDecodedSize = 0;
	iDecodedSize |= (pDecodeRead[0] & 0x7F) << 21;
	iDecodedSize |= (pDecodeRead[1] & 0x7F) << 14;
	iDecodedSize |= (pDecodeRead[2] & 0x7F) << 7;
	iDecodedSize |= (pDecodeRead[3] & 0x7F) << 0;
	pDecodeRead += 4;

	int iTextLength = iDecodedSize;
	iTextLength += (iTextLength + 6) / 7; //Have 1 control byte for every 7 actual bytes

	//make sure it's formatted properly
	for( int i = 0; i != iTextLength; ++i )
	{
		if( pDecodeRead[i] < 0x80 ) //encoded data always has MSB set
		{
			if( ppParseFinishOut != nullptr )
				*ppParseFinishOut = (char *)pString;

			return std::numeric_limits<int>::min(); //either not our data, or part of the string is missing
		}
	}

	if( iDestBufferSize < iDecodedSize )
	{
		if( ppParseFinishOut != nullptr )
			*ppParseFinishOut = (char *)pDecodeRead;

		return -iDecodedSize; //dest buffer not big enough to hold the data
	}

	const uint8 *pStopDecoding = pDecodeRead + iTextLength;		
	uint8 *pDecodeWrite = (uint8 *)pDestBuffer;
	int iControl = 0;
	int iLSBXOR = 0;

	while( pDecodeRead < pStopDecoding )
	{
		if( iControl == 0 )
		{
			iLSBXOR = *pDecodeRead;
		}
		else
		{
			*pDecodeWrite = *pDecodeRead ^ ((iLSBXOR << iControl) & 0x80);
			++pDecodeWrite;
		}

		++pDecodeRead;
		++iControl;
		iControl &= 7; //8->0
	}

	if( ppParseFinishOut != nullptr )
		*ppParseFinishOut = (char *)pDecodeRead;

	return iDecodedSize;	
}
