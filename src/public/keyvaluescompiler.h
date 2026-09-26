//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef KEYVALUESCOMPILER_H
#define KEYVALUESCOMPILER_H

#include "tier0/platform.h"
#include "tier1/byte_reader.h"
#include "tier1/byte_writer.h"
#include "tier1/filename_table.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class KeyValues;

#define COMPILED_KEYVALUES_ID	MAKEID( 'V', 'K', 'V', 'F' )

#define COMPILED_KEYVALUES_VERSION	1

struct KVHeader_t
{
	int			fileid;
	int			version;
	int			numStrings;
};

#pragma pack(1)
struct KVFile_t
{
	KVFile_t() :
		filename( 0 ),
		firstElement( 0 ),
		numElements( 0 )
	{
	}
	short			filename;
	short			firstElement;
	short			numElements;
};

struct KVInfo_t
{
	KVInfo_t() :
		key( 0 ),
		value( 0 ),
		parentIndex( -1 ),
		issubtree( false )
	{
	}

	inline void SetParent( int index )
	{
		Assert( index <= 32768 );
		parentIndex = index;
	}

	inline short GetParent() const
	{
		return parentIndex;
	}

	inline void SetSubTree( bool state )
	{
		issubtree = state;
	}

	inline bool IsSubTree() const
	{
		return issubtree;
	}

	short		key;
	short		value;

private:

	short		parentIndex;
	bool		issubtree;
};
#pragma pack()

//-----------------------------------------------------------------------------
// Purpose: stringtable is a session global string table.
//-----------------------------------------------------------------------------
class CCompiledKeyValuesWriter
{
public:

	CCompiledKeyValuesWriter()
	{
		m_StringTable.AddString( "" );
	}

	void AppendKeyValuesFile( char const *filename );
	void WriteFile( char const *outfile );

private:

	void Describe( const KVFile_t& file );

	void BuildKVData_R( KeyValues *kv, int parent );

	void WriteStringTable( ks::byte_writer &buf );
	void WriteData( ks::byte_writer &buf );
	void WriteFiles( ks::byte_writer &buf );

	std::vector< KVFile_t >		m_Files;
	std::vector< KVInfo_t >		m_Data;

	// Dense ids in insertion order: they are written to the compiled file.
	struct StringTable_t
	{
		int AddString( const char *pString )
		{
			auto [it, bNew] = m_Index.try_emplace( pString, (int)m_Strings.size() );
			if ( bNew )
				m_Strings.emplace_back( pString );
			return it->second;
		}
		const char *String( int i ) const	{ return m_Strings[i].c_str(); }
		int GetNumStrings() const			{ return (int)m_Strings.size(); }

		std::vector< std::string >					m_Strings;
		std::map< std::string, int, std::less<> >	m_Index;
	};
	StringTable_t				m_StringTable;
};


class CRunTimeKeyValuesStringTable
{
public:

	bool ReadStringTable( int numStrings, ks::byte_reader &buf );
	
	inline int Count() const
	{
		return std::ssize( m_Strings );
	}

	inline char const *Lookup( short index )
	{
		return m_Strings[ index ];
	}

private:
	std::vector< const char * >	m_Strings; 
};

class CCompiledKeyValuesReader
{
public:

	CCompiledKeyValuesReader();
	
	bool		LoadFile( char const *filename );

	KeyValues	*Instance( char const *kvfilename );
	bool		InstanceInPlace( KeyValues& head, char const *kvfilename );
	bool		LookupKeyValuesRootKeyName( char const *kvfilename, char *outbuf, size_t bufsize );

	// Indices are in-order positions.
	int			First() const;
	int			Next( int i ) const;
	int			InvalidIndex() const;

	void		GetFileName( int index, char *buf, size_t bufsize );

private:

	struct FileInfo_t
	{
		FileInfo_t() : 
			hFile( 0 ),
			nFirstIndex( 0 ), 
			nCount( 0 ) 
		{
		}
		FileNameHandle_t	hFile;
		short				nFirstIndex;
		short				nCount;

		struct Less
		{
			bool operator()( const FileInfo_t& lhs, const FileInfo_t& rhs ) const
			{
				return lhs.hFile < rhs.hFile;
			}
		};
	};

	KeyValues *CreateFromData( const FileInfo_t& info );
	bool CreateInPlaceFromData( KeyValues& head, const FileInfo_t& info );

	// Now get the actual files
	std::multiset< FileInfo_t, FileInfo_t::Less >	m_Dict;
	std::vector< KVInfo_t >		m_Data;

	CRunTimeKeyValuesStringTable		m_StringTable;

	std::vector< std::byte >			m_LoadBuffer;
};

#endif // KEYVALUESCOMPILER_H
