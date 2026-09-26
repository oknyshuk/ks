//====== Copyright (c) 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxelement.h"
#include "tier1/byte_reader.h"
#include "tier1/byte_writer.h"
#include "tier1/text_reader.h"
#include "tier1/text_writer.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "datamodel/dmxheader.h"
#include "dmxserializationdictionary.h"
#include "tier1/memstack.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>


#define DMX_BINARY_VER_STRINGTABLE 2
#define DMX_BINARY_VER_GLOBAL_STRINGTABLE 4
#define DMX_BINARY_VER_STRINGTABLE_LARGESYMBOLS 5

#define CURRENT_BINARY_ENCODING		5


//-----------------------------------------------------------------------------
// DMX elements/attributes can only be accessed inside a dmx context
//-----------------------------------------------------------------------------
static int s_bInDMXContext;
CMemoryStack s_DMXAllocator;
static bool s_bAllocatorInitialized;

void BeginDMXContext( )
{
	Assert( !s_bInDMXContext );

	if ( !s_bAllocatorInitialized )
	{
		const int k_nStackSize = 16 * 1024 * 1024;
		s_DMXAllocator.Init( "DMXAlloc", k_nStackSize, 0, 0, 16 );
		s_bAllocatorInitialized = true;
	}

	s_bInDMXContext = true;
}

void EndDMXContext( bool bDecommitMemory )
{
	Assert( s_bInDMXContext );
	s_bInDMXContext = false;
	s_DMXAllocator.FreeAll( bDecommitMemory );
}

void DecommitDMXMemory()
{
	s_DMXAllocator.FreeAll( true );
}


//-----------------------------------------------------------------------------
// Used for allocation. All will be freed when we leave the DMX context
//-----------------------------------------------------------------------------
void* DMXAlloc( size_t size )
{
	Assert( s_bInDMXContext );
	if ( !s_bInDMXContext )
		return 0;
	MEM_ALLOC_CREDIT_( "DMXAlloc" );
	return s_DMXAllocator.Alloc( size, false );
}


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
bool UnserializeTextDMX( const char *pFileName, ks::text_reader &buf, CDmxElement **ppRoot );
bool SerializeTextDMX( const char *pFileName, ks::text_writer &buf, CDmxElement *pRoot );


//-----------------------------------------------------------------------------
// special element indices
//-----------------------------------------------------------------------------
enum
{
	ELEMENT_INDEX_NULL = -1,
	ELEMENT_INDEX_EXTERNAL = -2,
};


//-----------------------------------------------------------------------------
// Serialization class for Binary output
//-----------------------------------------------------------------------------
class CDmxSerializer
{
public:
	bool Unserialize( ks::byte_reader &buf, int nEncodingVersion, CDmxElement **ppRoot );
	bool Serialize( ks::byte_writer &buf, CDmxElement *pRoot, const char *pFileName );

private:
	// For serialize
	typedef std::map< std::string, int, std::less<> > mapSymbolToIndex_t;	// case sensitive
	static int SymbolIndex( const mapSymbolToIndex_t *pSymbols, const char *pStr );

	// Methods related to serialization
	bool ShouldWriteAttribute( const char *pAttributeName, CDmxAttribute *pAttribute );
	void SerializeElementIndex( ks::byte_writer &buf, CDmxSerializationDictionary& list, CDmxElement *pElement );
	void SerializeElementAttribute( ks::byte_writer &buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute );
	void SerializeElementArrayAttribute( ks::byte_writer &buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute );
	bool SaveElementDict( ks::byte_writer &buf, mapSymbolToIndex_t *pStringValueSymbols, CDmxElement *pElement );
	bool SaveElement( ks::byte_writer &buf, CDmxSerializationDictionary& dict, mapSymbolToIndex_t *pStringValueSymbols, CDmxElement *pElement);
	void GatherSymbols( mapSymbolToIndex_t *pStringValueSymbols, std::vector< std::string_view > *pSymbolOrder, CDmxElement *pElement );

	// Methods related to unserialization
	CDmxElement* UnserializeElementIndex( ks::byte_reader &buf, std::vector< CDmxElement* > &elementList );
	void UnserializeElementAttribute( ks::byte_reader &buf, CDmxAttribute *pAttribute, std::vector< CDmxElement* > &elementList );
	void UnserializeElementArrayAttribute( ks::byte_reader &buf, CDmxAttribute *pAttribute, std::vector< CDmxElement* > &elementList );
	bool UnserializeAttributes( ks::byte_reader &buf, CDmxElement *pElement, std::vector< CDmxElement* > &elementList, int nStrings, int *offsetTable, char *stringTable, int nEncodingVersion );
	int GetStringOffsetTable( ks::byte_reader &buf, int *offsetTable, int nStrings );

	inline char const *Dme_GetStringFromBuffer( ks::byte_reader &buf, bool bUseLargeSymbols, int nStrings, int *offsetTable, char *stringTable )
	{
		int uSym = ( bUseLargeSymbols ) ? buf.read< int >() : buf.read< short >();
		if ( uSym >= nStrings )
			return nullptr;
		return stringTable + offsetTable[ uSym ];
	}
};



//-----------------------------------------------------------------------------
// Should we write out the attribute?
//-----------------------------------------------------------------------------
bool CDmxSerializer::ShouldWriteAttribute( const char *pAttributeName, CDmxAttribute *pAttribute )
{
	if ( !pAttribute )
		return false;

	// These are already written in the initial element dictionary
	if ( !Q_stricmp( pAttributeName, "name" ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Write out the index of the element to avoid looks at read time
//-----------------------------------------------------------------------------
void CDmxSerializer::SerializeElementIndex( ks::byte_writer &buf, CDmxSerializationDictionary& list, CDmxElement *pElement )
{
	if ( !pElement )
	{
		buf.write< int >( ELEMENT_INDEX_NULL ); // invalid handle
		return;
	}

	buf.write< int >( list.Find( pElement ) );
}


//-----------------------------------------------------------------------------
// Writes out element attributes
//-----------------------------------------------------------------------------
void CDmxSerializer::SerializeElementAttribute( ks::byte_writer &buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute )
{
	SerializeElementIndex( buf, list, pAttribute->GetValue<CDmxElement*>() );
}


//-----------------------------------------------------------------------------
// Writes out element array attributes
//-----------------------------------------------------------------------------
void CDmxSerializer::SerializeElementArrayAttribute( ks::byte_writer &buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute )
{
	const std::vector< CDmxElement* > &vec = pAttribute->GetArray<CDmxElement*>();

	int nCount = std::ssize( vec );
	buf.write< int >( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		SerializeElementIndex( buf, list, vec[ i ] );
	}
}


//-----------------------------------------------------------------------------
// Writes out all attributes
//-----------------------------------------------------------------------------
// Index of pStr in the written symbol table, or -1.
int CDmxSerializer::SymbolIndex( const mapSymbolToIndex_t *pSymbols, const char *pStr )
{
	if ( !pStr )
		return -1;
	auto it = pSymbols->find( std::string_view( pStr ) );
	return it != pSymbols->end() ? it->second : -1;
}

bool CDmxSerializer::SaveElement( ks::byte_writer &buf, CDmxSerializationDictionary& list, mapSymbolToIndex_t *pStringValueSymbols, CDmxElement *pElement )
{
	int nAttributesToSave = 0;

	// Count the attributes...
	int nCount = pElement->AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = pElement->GetAttribute( i );
		const char *pName = pAttribute->GetName( );
		if ( !ShouldWriteAttribute( pName, pAttribute ) )
			continue;

		++nAttributesToSave;
	}

	// Now write them all out.
	buf.write< int >( nAttributesToSave );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = pElement->GetAttribute( i );
		const char *pName = pAttribute->GetName();
		if ( !ShouldWriteAttribute( pName, pAttribute ) )
			continue;

		buf.write< int >( SymbolIndex( pStringValueSymbols, pName ) );
		buf.write< char >( static_cast< char >( pAttribute->GetType() ) );
		switch( pAttribute->GetType() )
		{
		default:
			pAttribute->Serialize( buf );
			break;

		case AT_ELEMENT:
			SerializeElementAttribute( buf, list, pAttribute );
			break;

		case AT_ELEMENT_ARRAY:
			SerializeElementArrayAttribute( buf, list, pAttribute );
			break;

		case AT_STRING:
			{
				buf.write< int >( SymbolIndex( pStringValueSymbols, pAttribute->GetValueString() ) );
			}
			break;
		}
	}

	return buf.ok();
}

bool CDmxSerializer::SaveElementDict( ks::byte_writer &buf, mapSymbolToIndex_t *pStringValueSymbols, CDmxElement *pElement )
{
	buf.write< int >( SymbolIndex( pStringValueSymbols, pElement->GetTypeString() ) );
	buf.write< int >( SymbolIndex( pStringValueSymbols, pElement->GetName() ) );
	buf.write_bytes( &pElement->GetId(), sizeof(DmObjectId_t) );
	return buf.ok();
}

//-----------------------------------------------------------------------------
// Main entry point for serialization
//-----------------------------------------------------------------------------

// Adds each distinct string once; pSymbolOrder keeps first-seen order (the file's string table order).
void CDmxSerializer::GatherSymbols( mapSymbolToIndex_t *pStringValueSymbols, std::vector< std::string_view > *pSymbolOrder, CDmxElement *pElement )
{
	auto AddString = [&]( const char *pStr )
	{
		if ( !pStr )
			return;
		auto [ it, bInserted ] = pStringValueSymbols->try_emplace( pStr, (int)pSymbolOrder->size() );
		if ( bInserted )
			pSymbolOrder->push_back( it->first );
	};

	AddString( pElement->GetTypeString() );
	AddString( pElement->GetName() );

	int nAttributes = pElement->AttributeCount();
	for ( int ai = 0; ai < nAttributes; ++ai )
	{
		CDmxAttribute *pAttr = pElement->GetAttribute( ai );
		if ( !pAttr )
			continue;

		AddString( pAttr->GetName() );

		if ( pAttr->GetType() == AT_STRING )
		{
			AddString( pAttr->GetValueString() );
		}
	}
}

bool CDmxSerializer::Serialize( ks::byte_writer &buf, CDmxElement *pRoot, const char *pFileName )
{
	DmxSerializationHandle_t i;

	// Save elements, attribute links
	CDmxSerializationDictionary dict;
	dict.BuildElementList( pRoot, true );

	// collect string table (case sensitive)
	mapSymbolToIndex_t symbolToIndexMap;
	std::vector< std::string_view > symbols;

	for ( i = dict.FirstRootElement(); i != DMX_SERIALIZATION_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		CDmxElement *pElement = dict.GetRootElement( i );
		if ( !pElement )
			return false;

		GatherSymbols( &symbolToIndexMap, &symbols, pElement );
	}

	// write out the symbol table for this file (may be significantly smaller than datamodel's full symbol table)
	int nSymbols = std::ssize( symbols );

	buf.write< int >( nSymbols );

	for ( std::string_view sym : symbols )
	{
		buf.write_cstring( sym );
	}

	// First write out the dictionary of all elements (to avoid later stitching up in unserialize)
	buf.write< int >( dict.RootElementCount() );
	for ( i = dict.FirstRootElement(); i != DMX_SERIALIZATION_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		if ( !SaveElementDict( buf, &symbolToIndexMap, dict.GetRootElement( i ) ) )
			return false;
	}

	// Now write out the attributes of each of those elements
	for ( i = dict.FirstRootElement(); i != DMX_SERIALIZATION_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		if ( !SaveElement( buf, dict, &symbolToIndexMap, dict.GetRootElement( i ) ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads an element index and converts it to a handle (local or external)
//-----------------------------------------------------------------------------
CDmxElement* CDmxSerializer::UnserializeElementIndex( ks::byte_reader &buf, std::vector< CDmxElement* > &elementList )
{
	int nElementIndex = buf.read< int >();
	if ( nElementIndex == ELEMENT_INDEX_EXTERNAL )
	{
		Warning( "Reading externally referenced elements is not supported!\n" );

		char idstr[ 40 ];
		buf.read_cstring( idstr, sizeof( idstr ) );
//		DmObjectId_t id;
//		UniqueIdFromString( &id, idstr, sizeof( idstr ) );

		return nullptr;
	}

	Assert( nElementIndex < std::ssize( elementList ) );
	Assert( nElementIndex >= 0 || nElementIndex == ELEMENT_INDEX_NULL );
	if ( nElementIndex < 0 || !elementList[ nElementIndex ] )
		return nullptr;

	return elementList[ nElementIndex ];
}


//-----------------------------------------------------------------------------
// Reads an element attribute
//-----------------------------------------------------------------------------
void CDmxSerializer::UnserializeElementAttribute( ks::byte_reader &buf, CDmxAttribute *pAttribute, std::vector< CDmxElement* > &elementList )
{
	CDmxElement *pElement = UnserializeElementIndex( buf, elementList );
	pAttribute->SetValue( pElement );
}


//-----------------------------------------------------------------------------
// Reads an element array attribute
//-----------------------------------------------------------------------------
void CDmxSerializer::UnserializeElementArrayAttribute( ks::byte_reader &buf, CDmxAttribute *pAttribute, std::vector< CDmxElement* > &elementList )
{
	int nElementCount = buf.read< int >();
	std::vector< CDmxElement* >& elementArray = pAttribute->GetArrayForEdit< CDmxElement* >();
	elementArray.reserve( nElementCount );

	for ( int i = 0; i < nElementCount; ++i )
	{
		CDmxElement *pElement = UnserializeElementIndex( buf, elementList );
		elementArray.push_back( pElement );
	}
}


//-----------------------------------------------------------------------------
// Reads a single element
//-----------------------------------------------------------------------------
bool CDmxSerializer::UnserializeAttributes( ks::byte_reader &buf, CDmxElement *pElement, std::vector< CDmxElement* > &elementList, int nStrings, int *offsetTable, char *stringTable, int nEncodingVersion )
{
	CDmxElementModifyScope modify( pElement );

	bool bUseLargeSymbols = nEncodingVersion >= DMX_BINARY_VER_STRINGTABLE_LARGESYMBOLS;


	char nameBuf[ 1024 ];
	int nAttributeCount = buf.read< int >();
	for ( int i = 0; i < nAttributeCount; ++i )
	{
		const char *pName = nullptr;
		{
			if ( stringTable )
			{
				pName = Dme_GetStringFromBuffer( buf, bUseLargeSymbols, nStrings, offsetTable, stringTable );
				if ( !pName )
					return false;
			}
			else
			{
				buf.read_cstring( nameBuf, sizeof( nameBuf ) );
				pName = nameBuf;
			}
		}

		DmAttributeType_t nAttributeType = (DmAttributeType_t)buf.read< char >();
		Assert( nAttributeType >= AT_FIRST_VALUE_TYPE && nAttributeType < AT_TYPE_COUNT );

		CDmxAttribute *pAttribute = pElement->AddAttribute( pName );
		if ( !pAttribute )
		{
			Warning( "WARNING: Failed to create attribute '%s' - likely out of memory in s_DMXAllocator!\n", pName );
			return false;
		}

		switch( nAttributeType )
		{
		default:
			pAttribute->Unserialize( nAttributeType, buf );
			break;

		case AT_ELEMENT:
			UnserializeElementAttribute( buf, pAttribute, elementList );
			break;

		case AT_ELEMENT_ARRAY:
			UnserializeElementArrayAttribute( buf, pAttribute, elementList );
			break;

		case AT_STRING:
			if ( stringTable && nEncodingVersion >= DMX_BINARY_VER_GLOBAL_STRINGTABLE )
			{
				const char *pValue = Dme_GetStringFromBuffer( buf, bUseLargeSymbols, nStrings, offsetTable, stringTable );
				if ( !pValue )
					return false;
				pAttribute->SetValue( pValue );
			}
			else
			{
				pAttribute->Unserialize( nAttributeType, buf );
			}
			break;
		}
	}

	return buf.ok();
}


int CDmxSerializer::GetStringOffsetTable( ks::byte_reader &buf, int *offsetTable, int nStrings )
{
	const std::string_view table = ks::text_view( buf.rest() );
	size_t nPos = 0;
	for ( int i = 0; i < nStrings; ++i )
	{
		offsetTable[ i ] = nPos;

		const size_t nEnd = table.find( '\0', nPos );
		if ( nEnd == std::string_view::npos )
			return -1;
		nPos = nEnd + 1;
	}

	return nPos;
}

//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmxSerializer::Unserialize( ks::byte_reader &buf, int nEncodingVersion, CDmxElement **ppRoot )
{
	if ( nEncodingVersion < 0 || nEncodingVersion > CURRENT_BINARY_ENCODING )
		return false;

	bool bReadStringTable = nEncodingVersion >= DMX_BINARY_VER_STRINGTABLE;
	bool bUseLargeSymbols = nEncodingVersion >= DMX_BINARY_VER_STRINGTABLE_LARGESYMBOLS;

	// Keep reading until we read a NULL terminator
	while( buf.read< char >() != 0 )
	{
		if ( !buf.ok() )
			return false;
	}

	// Read string table
	int nStrings = 0;
	int *offsetTable = nullptr;
	char *stringTable = nullptr;
	if ( bReadStringTable )
	{
		if ( nEncodingVersion >= DMX_BINARY_VER_GLOBAL_STRINGTABLE )
		{
			nStrings = buf.read< int >();
		}
		else
		{
			nStrings = buf.read< short >();
		}


		if ( nStrings > 0 )
		{
			offsetTable = ( int* )stackalloc( nStrings * sizeof( int ) );

			// this causes entire string table to be mapped in memory at once
			int nStringMemoryUsage = GetStringOffsetTable( buf, offsetTable, nStrings );
			if ( nStringMemoryUsage < 0 )
				return false;
			stringTable = ( char* )stackalloc( nStringMemoryUsage * sizeof( char ) );
			buf.read_bytes( stringTable, nStringMemoryUsage );
		}
	}

	// Read in the element count.
	int nElementCount = buf.read< int >();
	if ( !nElementCount )
	{
		// Empty (but valid) file
		return true;
	}

	if ( nElementCount < 0 || ( bReadStringTable && !stringTable ) )
	{
		// Invalid file. Non-empty files with a string table need at least one to associate with elements.
		return false;
	}

	char pTypeBuf[256];
	char pNameBuf[2048];
	DmObjectId_t id;

	// Read + create all elements
	std::vector< CDmxElement* > elementList; elementList.reserve( nElementCount );
	for ( int i = 0; i < nElementCount; ++i )
	{
		const char *pType = nullptr;
		if ( stringTable )
		{
			pType = Dme_GetStringFromBuffer( buf, bUseLargeSymbols, nStrings, offsetTable, stringTable );
			if ( !pType )
				return false;
		}
		else
		{
			buf.read_cstring( pTypeBuf, sizeof( pTypeBuf ) );
			pType = pTypeBuf;
		}

		const char *pName = nullptr;
		if ( bReadStringTable && nEncodingVersion >= DMX_BINARY_VER_GLOBAL_STRINGTABLE )
		{
			pName = Dme_GetStringFromBuffer( buf, bUseLargeSymbols, nStrings, offsetTable, stringTable );
			if ( !pName )
				return false;
		}
		else
		{
			buf.read_cstring( pNameBuf, sizeof( pNameBuf ) );
			pName = pNameBuf;
		}

		buf.read_bytes( &id, sizeof(DmObjectId_t) );

		CDmxElement *pElement = new CDmxElement( pType );
		{
			CDmxElementModifyScope modify( pElement );
			CDmxAttribute *pAttribute = pElement->AddAttribute( "name" );
			pAttribute->SetValue( pName );
			pElement->SetId( id );
		}
		elementList.push_back( pElement );
	}

	// The root is the 0th element
	*ppRoot = elementList[ 0 ];

	// Now read all attributes
	for ( int i = 0; i < nElementCount; ++i )
	{
		if ( !UnserializeAttributes( buf, elementList[ i ], elementList, nStrings, offsetTable, stringTable, nEncodingVersion ) )
		{
			return false;
		}
	}

	return buf.ok();
}


//-----------------------------------------------------------------------------
// Serialization main entry point
//-----------------------------------------------------------------------------
// Write the format name into the file using XML format so that 
// 3rd-party XML readers can read the file without fail
static std::string MakeDMXHeader( const char *pEncodingName, int nEncodingVersion )
{
	const char *pFormatName = GENERIC_DMX_FORMAT;
	int nFormatVersion = 1; // HACK - we should have some way of automatically updating this when the encoding version changes!
	return std::format( "{} encoding {} {} format {} {} {}\n", DMX_VERSION_STARTING_TOKEN, pEncodingName, nEncodingVersion, pFormatName, nFormatVersion, DMX_VERSION_ENDING_TOKEN );
}

bool SerializeDMX( ks::byte_writer &buf, CDmxElement *pRoot, const char *pFileName )
{
	buf.write_cstring( MakeDMXHeader( "binary", CURRENT_BINARY_ENCODING ) );

	CDmxSerializer dmxSerializer;
	return dmxSerializer.Serialize( buf, pRoot, pFileName );
}

bool SerializeDMX( ks::text_writer &buf, CDmxElement *pRoot, const char *pFileName )
{
	buf.put( MakeDMXHeader( "keyvalues2", 1 ) );

	return SerializeTextDMX( pFileName ? pFileName : "<no file>", buf, pRoot );
}

bool SerializeDMX( const char *pFileName, const char *pPathID, bool bTextMode, CDmxElement *pRoot )
{
	// NOTE: This guarantees full path names for pathids
	char pBuf[MAX_PATH];
	const char *pFullPath = pFileName;
	if ( !Q_IsAbsolutePath( pFullPath ) && !pPathID )
	{
		char pDir[MAX_PATH];
		if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
		{
			Q_ComposeFileName( pDir, pFileName, pBuf, sizeof(pBuf) );
			Q_RemoveDotSlashes( pBuf );
			pFullPath = pBuf;
		}
	}

	std::span< const std::byte > data;
	ks::byte_writer binary;
	ks::text_writer text;
	if ( !bTextMode )
	{
		if ( !SerializeDMX( binary, pRoot, pFullPath ) )
			return false;
		data = binary.bytes();
	}
	else
	{
		if ( !SerializeDMX( text, pRoot, pFullPath ) )
			return false;
		data = text.bytes();
	}

	if ( !g_pFullFileSystem->WriteFile( pFullPath, pPathID, data ) )
	{
		Warning( "SerializeDMX: Unable to open file \"%s\"\n", pFullPath );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Read the header, return the version (or false if it's not a DMX file)
//-----------------------------------------------------------------------------
static void CopyToBuffer( char *pDest, int nDestLen, std::string_view src )
{
	const size_t nCopy = std::min( src.size(), (size_t)nDestLen - 1 );
	memcpy( pDest, src.data(), nCopy );
	pDest[ nCopy ] = '\0';
}

static bool ParseDMXHeader( ks::text_reader &buf, bool bBufIsText, char *pEncodingName, int nEncodingNameLen, int &nEncodingVersion, char *pFormatName, int nFormatNameLen, int &nFormatVersion )
{
	char header[ DMX_MAX_HEADER_LENGTH ] = { 0 };
	std::optional< std::string_view > headerToken = buf.read_between( DMX_VERSION_STARTING_TOKEN, DMX_VERSION_ENDING_TOKEN );
	bool bOk = headerToken.has_value();
	if ( bOk )
	{
		CopyToBuffer( header, sizeof( header ), *headerToken );

		// sscanf considered harmful. We don't have POSIX 2008 support on OS X and "C11 Annex K" is optional... (optional specs considered useful)
		char pTmpEncodingName[ sizeof( header ) ] = { 0 };
		char pTmpFormatName  [ sizeof( header ) ] = { 0 };
		int nAssigned = sscanf( header, "encoding %s %d format %s %d\n", pTmpEncodingName, &nEncodingVersion, pTmpFormatName, &nFormatVersion );
		bOk = ( V_strlen( pTmpEncodingName ) < nEncodingNameLen ) && ( V_strlen( pTmpFormatName ) < nFormatNameLen );
		V_strncpy( pEncodingName, pTmpEncodingName, nEncodingNameLen );
		V_strncpy( pFormatName, pTmpFormatName, nFormatNameLen );
		bOk = bOk && ( nAssigned == 4 );
		if ( bOk )
		{
			bOk = !V_stricmp( pEncodingName, bBufIsText ? "keyvalues2" : "binary" );
		}
	}

	// TODO - retire legacy format version reading
	if ( !bOk )
	{
		buf.seek( 0 );
		std::optional< std::string_view > legacyToken = buf.read_between( DMX_LEGACY_VERSION_STARTING_TOKEN, DMX_LEGACY_VERSION_ENDING_TOKEN );
		bOk = legacyToken.has_value();
		if ( bOk )
		{
			CopyToBuffer( pFormatName, nFormatNameLen, *legacyToken );
			nEncodingVersion = 0;
			nFormatVersion = 0; // format version is encoded in the format name string

			if ( !V_stricmp( pFormatName, "binary_v1" ) || !V_stricmp( pFormatName, "binary_v2" ) )
			{
				bOk = !bBufIsText;
				V_strncpy( pEncodingName, "binary", nEncodingNameLen );
			}
			else if ( !V_stricmp( pFormatName, "keyvalues2_v1" ) || !V_stricmp( pFormatName, "keyvalues2_flat_v1" ) )
			{
				bOk = bBufIsText;
				V_strncpy( pEncodingName, "keyvalues2", nEncodingNameLen );
			}
			else
			{
				bOk = false;
			}
		}
	}

	return bOk && buf.ok();
}

static bool ReadDMXHeader( ks::byte_reader &buf, char *pEncodingName, int nEncodingNameLen, int &nEncodingVersion, char *pFormatName, int nFormatNameLen, int &nFormatVersion )
{
	ks::text_reader text( ks::text_view( buf.bytes() ) );
	text.seek( buf.tell() );
	if ( !ParseDMXHeader( text, false, pEncodingName, nEncodingNameLen, nEncodingVersion, pFormatName, nFormatNameLen, nFormatVersion ) )
		return false;

	buf.seek( text.tell() );
	return buf.ok();
}

static bool ReadDMXHeader( ks::text_reader &buf, char *pEncodingName, int nEncodingNameLen, int &nEncodingVersion, char *pFormatName, int nFormatNameLen, int &nFormatVersion )
{
	return ParseDMXHeader( buf, true, pEncodingName, nEncodingNameLen, nEncodingVersion, pFormatName, nFormatNameLen, nFormatVersion );
}


//-----------------------------------------------------------------------------
// Unserialization main entry point
//-----------------------------------------------------------------------------
template < class Reader >
static bool ReadDMXHeaderAndWarn( Reader &buf, const char *pFileName, int &nEncodingVersion )
{
	// NOTE: Checking the format name string for a version check here is how you'd do it
	int nFormatVersion;
	char pEncodingName[ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	char pFormatName  [ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	if ( !ReadDMXHeader( buf,
		pEncodingName, sizeof( pEncodingName ), nEncodingVersion,
		pFormatName, sizeof( pFormatName ), nFormatVersion ) )
		return false;

	// TODO - retire legacy format version reading
	if ( nFormatVersion == 0 ) // legacy formats store format version in their format name string
	{
		Warning( "reading file '%s' of legacy format '%s' - dmxconvert this file to a newer format!\n", pFileName ? pFileName : "<no file>", pFormatName );
	}

	return true;
}

bool UnserializeDMX( ks::byte_reader &buf, CDmxElement **ppRoot, const char *pFileName )
{
	*ppRoot = nullptr;

	int nEncodingVersion;
	if ( !ReadDMXHeaderAndWarn( buf, pFileName, nEncodingVersion ) )
		return false;

	CDmxSerializer dmxUnserializer;
	return dmxUnserializer.Unserialize( buf, nEncodingVersion, ppRoot );
}

bool UnserializeDMX( ks::text_reader &buf, CDmxElement **ppRoot, const char *pFileName )
{
	*ppRoot = nullptr;

	int nEncodingVersion;
	if ( !ReadDMXHeaderAndWarn( buf, pFileName, nEncodingVersion ) )
		return false;

	return UnserializeTextDMX( pFileName ? pFileName : "<no file>", buf, ppRoot );
}

bool UnserializeDMX( const char *pFileName, const char *pPathID, bool bTextMode, CDmxElement **ppRoot )
{
	// NOTE: This guarantees full path names for pathids
	char pBuf[MAX_PATH];
	const char *pFullPath = pFileName;
	if ( !Q_IsAbsolutePath( pFullPath ) && !pPathID )
	{
		char pDir[MAX_PATH];
		if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
		{
			Q_ComposeFileName( pDir, pFileName, pBuf, sizeof(pBuf) );
			Q_RemoveDotSlashes( pBuf );
			pFullPath = pBuf;
		}
	}

	std::optional< std::vector< std::byte > > data = g_pFullFileSystem->ReadFile( pFullPath, pPathID );
	if ( !data )
	{
		Warning( "UnserializeDMX: Unable to open file \"%s\"\n", pFullPath );
		return false;
	}

	if ( !bTextMode )
	{
		ks::byte_reader buf( *data );
		return UnserializeDMX( buf, ppRoot, pFullPath );
	}

	ks::text_reader buf( ks::text_view( *data ) );
	return UnserializeDMX( buf, ppRoot, pFullPath );
}


//-----------------------------------------------------------------------------
// Cleans up read-in elements
//-----------------------------------------------------------------------------
void CleanupDMX( CDmxElement *pRoot )
{
	if ( pRoot )
	{
		pRoot->RemoveAllElementsRecursive();
	}
}
