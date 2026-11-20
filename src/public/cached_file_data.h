#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

import ks.core.basetypes;
import ks.core.dbg;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.convar;
import ks.tier1.filename_table;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.vector_ops;
#include "filesystem.h" // FileNameHandle_t
import std;

// If you change to serialization protocols, this must be bumped...
inline constexpr auto UTL_CACHE_SYSTEM_VERSION = 2;

inline constexpr auto CACHED_FILE_DATA_UNDEFINED_DISKINFO = (int)-2;

// Cacheable types must derive from this and implement the appropriate methods...
class IBaseCacheInfo
{
public:
	virtual void Save( ks::byte_writer &buf ) = 0;
	virtual void Restore( ks::byte_reader &buf ) = 0;

	virtual void Rebuild( char const *filename ) = 0;
};

typedef unsigned int (*PFNCOMPUTECACHEMETACHECKSUM)( void );

typedef enum
{
	CACHED_FILE_USE_TIMESTAMP = 0,
	CACHED_FILE_USE_FILESIZE,
} CachedFileDataType_t;

template <class T>
class CCachedFileData
{
public:
	CCachedFileData
	( 
		char const *repositoryFileName, 
		int version, 
		PFNCOMPUTECACHEMETACHECKSUM checksumfunc = nullptr, 
		CachedFileDataType_t fileCheckType = CACHED_FILE_USE_TIMESTAMP,
		bool nevercheckdisk = false,
		bool readonly = false,
		bool savemanifest = false
	)
		: m_sRepositoryFileName( ks::nz( repositoryFileName ) ),
		m_nVersion( version ),
		m_pfnMetaChecksum( checksumfunc ),
		m_bDirty( false ),
		m_bInitialized( false ),
		m_uCurrentMetaChecksum( 0u ),
		m_fileCheckType( fileCheckType ),
		m_bNeverCheckDisk( nevercheckdisk ),
		m_bReadOnly( readonly ),
		m_bSaveManifest( savemanifest )
	{
		contract_assert( !m_sRepositoryFileName.empty() );
	}

	virtual ~CCachedFileData()
	{
		m_Elements.clear();
		m_ElementIndex.clear();
		int c = std::ssize( m_Data );
		for ( int i = 0; i < c ; ++i )
		{
			delete m_Data[ i ];
		}
		m_Data.clear();
	}

	T* Get( char const *filename );
	const T* Get( char const *filename ) const;

	T* operator[]( int i );
	const T* operator[]( int i ) const;

	int Count() const;

	void GetElementName( int i, char *buf, int buflen )
	{
		buf[ 0 ] = 0;
		if ( !ks::is_valid_index( m_Elements, i ) )
			return;

		g_pFullFileSystem->String( m_Elements[ i ].handle, buf, buflen );
	}

	bool EntryExists( char const *filename ) const
	{
		return m_ElementIndex.contains( g_pFullFileSystem->FindOrAddFileName( filename ) );
	}

	void SetElement( char const *name, int fileinfo, T* src )
	{
		SetDirty( true );

		int idx = GetIndex( name );

		ElementType_t& e = m_Elements[ idx ];

		ks::byte_writer buf;

		contract_assert( e.dataIndex != -1 );

		T *dest = m_Data[ e.dataIndex ];

		contract_assert( dest );

		// I suppose we could do an assignment operator, but this should save/restore the data element just fine for
		//  tool purposes
		((IBaseCacheInfo *)src)->Save( buf );
		ks::byte_reader reader = buf.reader();
		((IBaseCacheInfo *)dest)->Restore( reader );

		e.fileinfo = fileinfo;
		if ( ( e.fileinfo == -1 ) &&
			( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) )
		{
			e.fileinfo = 0;
		}
		// Force recheck
		e.diskfileinfo = CACHED_FILE_DATA_UNDEFINED_DISKINFO;
	}

	// If you create a cache and don't call init/shutdown, you can call this to do a quick check to see if the checksum/version
	//  will cause a rebuild...
	bool	IsUpToDate();

	void	Shutdown();
	bool	Init();

	void	Save();

	void	Reload();

	void	ForceRecheckDiskInfo();
	// Iterates all entries and gets filesystem info and optionally causes rebuild on any existing items which are out of date
	void	CheckDiskInfo( bool force_rebuild, int cacheFileTime = 0L );

	void	SaveManifest();
	bool	ManifestExists();

	int	GetFileInfo( char const *filename )
	{
		auto it = m_ElementIndex.find( g_pFullFileSystem->FindOrAddFileName( filename ) );
		if ( it == m_ElementIndex.end() )
		{
			return 0L;
		}

		return m_Elements[ it->second ].fileinfo;
	}
	int GetFileInfo( int idx )
	{
		if ( !ks::is_valid_index( m_Elements, idx ) )
			return 0L;
		return m_Elements[ idx ].fileinfo;
	}

	int		GetNumElements()
	{
		return (int)m_Elements.size();
	}

	bool		IsDirty() const
	{
		return m_bDirty;
	}

	T *RebuildItem( const char *filename );

	void SetNeverCheckDisk( bool bNeverCheckDisk );

	void RecheckItem( char const *filename );

private:

	void		InitSmallBuffer( FileHandle_t& fh, int fileSize, bool& deleteFile );
	void		InitLargeBuffer( FileHandle_t& fh, bool& deleteFile );

	int			GetIndex( const char *filename )
	{
		FileNameHandle_t handle = g_pFullFileSystem->FindOrAddFileName( filename );
		auto [it, bNew] = m_ElementIndex.try_emplace( handle, (int)m_Elements.size() );
		if ( bNew )
		{
			T *data = new T();

			ElementType_t &element = m_Elements.emplace_back();
			element.handle = handle;
			element.dataIndex = std::ssize( m_Data ); m_Data.push_back( data );
		}

		return it->second;
	}

	void		CheckInit();

	void		SetDirty( bool dirty )
	{
		m_bDirty = dirty;
	}

	void		RebuildCache( char const *filename, T *data );

	struct ElementType_t
	{
		ElementType_t() :
			handle( 0 ),
			fileinfo( 0 ),
			diskfileinfo( CACHED_FILE_DATA_UNDEFINED_DISKINFO ),
			dataIndex( -1 )
		{
		}

		FileNameHandle_t	handle;
		int				fileinfo;
		int				diskfileinfo;
		int					dataIndex;
	};

	// Insertion order: the indices of operator[], GetElementName and GetFileInfo.
	std::vector< ElementType_t >	m_Elements;
	std::map< FileNameHandle_t, int >	m_ElementIndex;	// handle -> m_Elements index, in handle order
	std::vector< T * >				m_Data;
	std::string						m_sRepositoryFileName;
	int								m_nVersion;
	PFNCOMPUTECACHEMETACHECKSUM		m_pfnMetaChecksum;
	unsigned int					m_uCurrentMetaChecksum;
	CachedFileDataType_t			m_fileCheckType;
	bool							m_bNeverCheckDisk : 1;
	bool							m_bReadOnly		  : 1;
	bool							m_bSaveManifest   : 1;
	bool							m_bDirty          : 1;
	bool							m_bInitialized    : 1;

};


template <class T>
T* CCachedFileData<T>::Get( char const *filename )
{
	int idx = GetIndex( filename );

	ElementType_t& e = m_Elements[ idx ];

	if ( e.fileinfo == -1 &&
		m_fileCheckType == CACHED_FILE_USE_FILESIZE )
	{
		e.fileinfo = 0;
	}
	int cachefileinfo = e.fileinfo;
	// Set the disk fileinfo the first time we encounter the filename
	if ( e.diskfileinfo == CACHED_FILE_DATA_UNDEFINED_DISKINFO )
	{
		if ( m_bNeverCheckDisk )
		{
			e.diskfileinfo = cachefileinfo;
		}
		else
		{
			if ( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) 
			{
				e.diskfileinfo = g_pFullFileSystem->Size( filename, "GAME" );
				// Missing files get a disk file size of 0
				if ( e.diskfileinfo == -1 )
				{
					e.diskfileinfo = 0;
				}
			}
			else
			{
				e.diskfileinfo = g_pFullFileSystem->GetFileTime( filename, "GAME" );
			}
		}
	}

	contract_assert( e.dataIndex != -1 );

	T *data = m_Data[ e.dataIndex ];

	contract_assert( data );

	// Compare fileinfo to disk fileinfo and rebuild cache if out of date or not correct...
	if ( cachefileinfo != e.diskfileinfo )
	{
		if ( !m_bReadOnly )
		{
			RebuildCache( filename, data );
		}
		e.fileinfo = e.diskfileinfo;
	}

	return data;
}

template <class T>
const T* CCachedFileData<T>::Get( char const *filename ) const
{
	return const_cast< CCachedFileData<T> * >(this)->Get( filename );
}

template <class T>
T* CCachedFileData<T>::operator[]( int i )
{
	return m_Data[ m_Elements[ i ].dataIndex ];
}

template <class T>
const T* CCachedFileData<T>::operator[]( int i ) const
{
	return m_Data[ m_Elements[ i ].dataIndex ];
}

template <class T>
int CCachedFileData<T>::Count() const
{
	return (int)m_Elements.size();
}

template <class T>
void CCachedFileData<T>::Reload()
{
	Shutdown();
	Init();
}

template <class T>
bool CCachedFileData<T>::IsUpToDate()
{
	// Don't call Init/Shutdown if using this method!!!
	contract_assert( !m_bInitialized );

	if ( !std::ssize( m_sRepositoryFileName ) )
	{
		Error( "CCachedFileData:  Can't IsUpToDate, no repository file specified." );
		return false;
	}

	// Always compute meta checksum
	m_uCurrentMetaChecksum = m_pfnMetaChecksum ? (*m_pfnMetaChecksum)() : 0;

	FileHandle_t fh;

	fh = g_pFullFileSystem->Open( m_sRepositoryFileName.c_str(), "rb", "MOD" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
	{
		return false;
	}

	// Version data is in first 12 bytes of file
	byte	header[ 12 ];
	g_pFullFileSystem->Read( header, sizeof( header ), fh );
	g_pFullFileSystem->Close( fh );

	int cacheversion = *( int *)&header[ 0 ];

	if ( UTL_CACHE_SYSTEM_VERSION != cacheversion )
	{
		DevMsg( "Discarding repository '%s' due to cache system version change\n", m_sRepositoryFileName.c_str() );
		contract_assert( !m_bReadOnly );
		if ( !m_bReadOnly )
		{
			g_pFullFileSystem->RemoveFile( m_sRepositoryFileName.c_str(), "MOD" );
		}
		return false;
	}

	// Now parse data from the buffer
	int version = *( int *)&header[ 4 ];
	if ( version != m_nVersion )
	{
		DevMsg( "Discarding repository '%s' due to version change\n", m_sRepositoryFileName.c_str() );
		contract_assert( !m_bReadOnly );
		if ( !m_bReadOnly )
		{
			g_pFullFileSystem->RemoveFile( m_sRepositoryFileName.c_str(), "MOD" );
		}
		return false;
	}

	// This is a checksum based on any meta data files which the cache depends on (supplied by a passed in
	//  meta data function
	if ( m_pfnMetaChecksum )
	{
		unsigned int cache_meta_checksum = *( unsigned int *)&header[ 8 ];
		if ( cache_meta_checksum != m_uCurrentMetaChecksum )
		{
			DevMsg( "Discarding repository '%s' due to meta checksum change\n", m_sRepositoryFileName.c_str() );
			contract_assert( !m_bReadOnly );
			if ( !m_bReadOnly )
			{
				g_pFullFileSystem->RemoveFile( m_sRepositoryFileName.c_str(), "MOD" );
			}
			return false;
		}
	}

	// Looks valid
	return true;
}

template <class T>
void CCachedFileData<T>::InitSmallBuffer( FileHandle_t& fh, int fileSize, bool& deleteFile )
{
	deleteFile = false;

	const auto loadData = g_pFullFileSystem->ReadToVector( fh );
	g_pFullFileSystem->Close( fh );

	ks::byte_reader loadBuf;
	if ( loadData )
	{
		loadBuf = ks::byte_reader( *loadData );
	}

	int cacheversion = loadBuf.read< int >();

	if ( UTL_CACHE_SYSTEM_VERSION == cacheversion )
	{
		// Now parse data from the buffer
		int version = loadBuf.read< int >();
		
		if ( version == m_nVersion )
		{
			// This is a checksum based on any meta data files which the cache depends on (supplied by a passed in
			//  meta data function
			unsigned int cache_meta_checksum = loadBuf.read< int >();
			
			if ( !m_pfnMetaChecksum ||
				( cache_meta_checksum == m_uCurrentMetaChecksum ) )
			{
				int count = loadBuf.read< int >();
				
				contract_assert( count < 2000000 );

				for ( int i = 0 ; i < count; ++i )
				{
					int bufsize = loadBuf.read< int >();
					contract_assert( bufsize < 1000000 );

					ks::byte_reader buf( loadBuf.read_span( std::max( bufsize, 0 ) ) );

					// Read the element name
					char elementFileName[ 512 ];
					buf.read_cstring( elementFileName, sizeof( elementFileName ) );

					// Now read the element
					int slot = GetIndex( elementFileName );

					contract_assert( slot >= 0 );

					ElementType_t& element = m_Elements[ slot ];

					element.fileinfo = buf.read< int >();
					if ( ( element.fileinfo == -1 ) &&
						( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) )
					{
						element.fileinfo = 0;
					}

					contract_assert( element.dataIndex != -1 );

					T *data = m_Data[ element.dataIndex ];

					contract_assert( data );

					((IBaseCacheInfo *)data)->Restore( buf );
				}
			}
			else
			{
				Msg( "Discarding repository '%s' due to meta checksum change\n", m_sRepositoryFileName.c_str() );
				deleteFile = true;
			}
		}
		else
		{
			Msg( "Discarding repository '%s' due to version change\n", m_sRepositoryFileName.c_str() );
			deleteFile = true;
		}
	}
	else
	{
		DevMsg( "Discarding repository '%s' due to cache system version change\n", m_sRepositoryFileName.c_str() );
		deleteFile = true;
	}
}

template <class T>
void CCachedFileData<T>::InitLargeBuffer( FileHandle_t& fh, bool& deleteFile )
{
	deleteFile = false;

	int cacheversion = 0;
	g_pFullFileSystem->Read( &cacheversion, sizeof( cacheversion ), fh );

	if ( UTL_CACHE_SYSTEM_VERSION == cacheversion )
	{
		// Now parse data from the buffer
		int version = 0;
		g_pFullFileSystem->Read( &version, sizeof( version ), fh );
		
		if ( version == m_nVersion )
		{
			// This is a checksum based on any meta data files which the cache depends on (supplied by a passed in
			//  meta data function
			unsigned int cache_meta_checksum = 0;
			
			g_pFullFileSystem->Read( &cache_meta_checksum, sizeof( cache_meta_checksum ), fh );

			if ( !m_pfnMetaChecksum ||
				( cache_meta_checksum == m_uCurrentMetaChecksum ) )
			{
				int count = 0;
				
				g_pFullFileSystem->Read( &count, sizeof( count ), fh );

				contract_assert( count < 2000000 );

				std::vector< std::byte > bufData;

				for ( int i = 0 ; i < count; ++i )
				{
					int bufsize = 0;
					g_pFullFileSystem->Read( &bufsize, sizeof( bufsize ), fh );

					contract_assert( bufsize < 1000000 );
					if ( bufsize > 1000000 )
					{
						Msg( "Discarding repository '%s' due to corruption\n", m_sRepositoryFileName.c_str() );
						deleteFile = true;
						break;
					}


					bufData.resize( std::max( bufsize, 0 ) );
					int nBytesRead = g_pFullFileSystem->Read( bufData.data(), bufsize, fh );

					ks::byte_reader buf( bufData.data(), std::max( nBytesRead, 0 ) );

					// Read the element name
					char elementFileName[ 512 ];
					buf.read_cstring( elementFileName, sizeof( elementFileName ) );

					// Now read the element
					int slot = GetIndex( elementFileName );

					contract_assert( slot >= 0 );

					ElementType_t& element = m_Elements[ slot ];

					element.fileinfo = buf.read< int >();
					if ( ( element.fileinfo == -1 ) &&
						( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) )
					{
						element.fileinfo = 0;
					}

					contract_assert( element.dataIndex != -1 );

					T *data = m_Data[ element.dataIndex ];

					contract_assert( data );

					((IBaseCacheInfo *)data)->Restore( buf );
				}
			}
			else
			{
				Msg( "Discarding repository '%s' due to meta checksum change\n", m_sRepositoryFileName.c_str() );
				deleteFile = true;
			}
		}
		else
		{
			Msg( "Discarding repository '%s' due to version change\n", m_sRepositoryFileName.c_str() );
			deleteFile = true;
		}
	}
	else
	{
		DevMsg( "Discarding repository '%s' due to cache system version change\n", m_sRepositoryFileName.c_str() );
		deleteFile = true;
	}

	g_pFullFileSystem->Close( fh );
}

template <class T>
bool CCachedFileData<T>::Init()
{
	if ( m_bInitialized )
	{
		return true;
	}

	m_bInitialized = true;

	if ( !std::ssize( m_sRepositoryFileName ) )
	{
		Error( "CCachedFileData:  Can't Init, no repository file specified." );
		return false;
	}

	// Always compute meta checksum
	m_uCurrentMetaChecksum = m_pfnMetaChecksum ? (*m_pfnMetaChecksum)() : 0;

	FileHandle_t fh;

	fh = g_pFullFileSystem->Open( m_sRepositoryFileName.c_str(), "rb", "MOD" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
	{
		// Nothing on disk, we'll recreate everything from scratch...
		if ( !m_bReadOnly )
		{
			SetDirty( true );
		}
		return true;
	}
	int fileTime = g_pFullFileSystem->GetFileTime( m_sRepositoryFileName.c_str(), "MOD" );
	int size = g_pFullFileSystem->Size( fh );

	bool deletefile = false;

	if ( size > 1024 * 1024 )
	{
		InitLargeBuffer( fh, deletefile );
	}
	else
	{
		InitSmallBuffer( fh, size, deletefile );
	}

	if ( deletefile )
	{
		contract_assert( !m_bReadOnly );
		if ( !m_bReadOnly )
		{
			g_pFullFileSystem->RemoveFile( m_sRepositoryFileName.c_str(), "MOD" );
			SetDirty( true );
		}
	}
	CheckDiskInfo( false, fileTime );
	return true;
}

class CSortedCacheFile
{
public:
	FileNameHandle_t	handle;
	int					index;

	static bool Less( const CSortedCacheFile &file0, const CSortedCacheFile &file1 )
	{
		char name0[ 512 ];
		char name1[ 512 ];
		g_pFullFileSystem->String( file0.handle, name0, sizeof( name0 ) );
		g_pFullFileSystem->String( file1.handle, name1, sizeof( name1 ) );
		return V_stricmp( name0, name1 ) < 0 ? true : false;
	}
};

template <class T>
void CCachedFileData<T>::Save()
{
	char path[ 512 ];
	V_strncpy( path, m_sRepositoryFileName.c_str(), sizeof( path ) );
	V_StripFilename( path );

	g_pFullFileSystem->CreateDirHierarchy( path, "MOD" );

	if ( g_pFullFileSystem->FileExists( m_sRepositoryFileName.c_str(), "MOD" ) && 
		!g_pFullFileSystem->IsFileWritable( m_sRepositoryFileName.c_str(), "MOD" ) )
	{
		g_pFullFileSystem->SetFileWritable( m_sRepositoryFileName.c_str(), true, "MOD" );
	}

	// Now write to file
	FileHandle_t fh;
	fh = g_pFullFileSystem->Open( m_sRepositoryFileName.c_str(), "wb" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
	{
		Warning( "Unable to persist cache '%s', check file permissions\n", m_sRepositoryFileName.c_str() );
	}
	else
	{
		SetDirty( false );

		int v = UTL_CACHE_SYSTEM_VERSION;
		g_pFullFileSystem->Write( &v, sizeof( v ), fh );
		v = m_nVersion;
		g_pFullFileSystem->Write( &v, sizeof( v ), fh );
		v = (int)m_uCurrentMetaChecksum;
		g_pFullFileSystem->Write( &v, sizeof( v ), fh );

		// Element count
		int c = Count();

		g_pFullFileSystem->Write( &c, sizeof( c ), fh );

		// Save repository back out to disk...
		ks::byte_writer buf;

		// Sort file alphabetically
		std::vector< CSortedCacheFile > list;
		for ( auto [handle, i] : m_ElementIndex )
		{
			list.push_back( { handle, i } );
		}
		std::ranges::sort( list, CSortedCacheFile::Less );

		for ( int i = 0; i < std::ssize( list ); ++i )
		{
			ElementType_t &element = m_Elements[ list[ i ].index ];

			buf.clear();

			char fn[ 512 ];
			g_pFullFileSystem->String( element.handle, fn, sizeof( fn ) );
			buf.write_cstring( fn );
			buf.write< int >( element.fileinfo );

			contract_assert( element.dataIndex != -1 );

			T *data = m_Data[ element.dataIndex ];

			contract_assert( data );

			((IBaseCacheInfo *)data)->Save( buf );

			int bufsize = (int)buf.size();
			g_pFullFileSystem->Write( &bufsize, sizeof( bufsize ), fh );
			g_pFullFileSystem->Write( buf.data(), bufsize, fh );
		}

		g_pFullFileSystem->Close( fh );
	}

	if ( m_bSaveManifest )
	{
		SaveManifest();
	}
}

template <class T>
void CCachedFileData<T>::Shutdown()
{
	if ( !m_bInitialized )
		return;

	m_bInitialized = false;

	if ( IsDirty() )
	{
		Save();
	}
	// No matter what, create the manifest if it doesn't exist on the HD yet
	else if ( m_bSaveManifest && !ManifestExists() )
	{
		SaveManifest();
	}

	m_Elements.clear();
	m_ElementIndex.clear();
}

template <class T>
bool CCachedFileData<T>::ManifestExists()
{
	char manifest_name[ 512 ];
	V_strncpy( manifest_name, m_sRepositoryFileName.c_str(), sizeof( manifest_name ) );
	V_SetExtension( manifest_name, ".manifest", sizeof( manifest_name ) );
	return g_pFullFileSystem->FileExists( manifest_name, "MOD" );
}

template <class T>
void CCachedFileData<T>::SaveManifest()
{
	// Save manifest out to disk...
	ks::text_writer buf;

	// Sort file alphabetically
	std::vector< CSortedCacheFile > list;
	for ( auto [handle, i] : m_ElementIndex )
	{
		list.push_back( { handle, i } );
	}
	std::ranges::sort( list, CSortedCacheFile::Less );

	for ( int i = 0; i < std::ssize( list ); ++i )
	{
		ElementType_t &element = m_Elements[ list[ i ].index ];

		char fn[ 512 ];
		g_pFullFileSystem->String( element.handle, fn, sizeof( fn ) );

		buf.printf( "\"%s\"\r\n", fn );
	}

	char path[ 512 ];
	V_strncpy( path, m_sRepositoryFileName.c_str(), sizeof( path ) );
	V_StripFilename( path );

	g_pFullFileSystem->CreateDirHierarchy( path, "MOD" );

	char manifest_name[ 512 ];
	V_strncpy( manifest_name, m_sRepositoryFileName.c_str(), sizeof( manifest_name ) );

	V_SetExtension( manifest_name, ".manifest", sizeof( manifest_name ) );

	if ( g_pFullFileSystem->FileExists( manifest_name, "MOD" ) && 
		!g_pFullFileSystem->IsFileWritable( manifest_name, "MOD" ) )
	{
		g_pFullFileSystem->SetFileWritable( manifest_name, true, "MOD" );
	}

	// Now write to file
	FileHandle_t fh;
	fh = g_pFullFileSystem->Open( manifest_name, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		g_pFullFileSystem->Write( buf.c_str(), (int)buf.size(), fh );
		g_pFullFileSystem->Close( fh );

		// DevMsg( "Persisting cache manifest '%s' (%d entries)\n", manifest_name, c );
	}
	else
	{
		Warning( "Unable to persist cache manifest '%s', check file permissions\n", manifest_name );
	}
}

template <class T>
void CCachedFileData<T>::RecheckItem( char const *filename )
{
	int idx = GetIndex( filename );
	ElementType_t& e = m_Elements[ idx ];

	e.diskfileinfo = CACHED_FILE_DATA_UNDEFINED_DISKINFO;

	int cachefileinfo = e.fileinfo;
	// Set the disk fileinfo the first time we encounter the filename
	if ( e.diskfileinfo == CACHED_FILE_DATA_UNDEFINED_DISKINFO )
	{
		if ( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) 
		{
			e.diskfileinfo = g_pFullFileSystem->Size( filename, "GAME" );
			// Missing files get a disk file size of 0
			if ( e.diskfileinfo == -1 )
			{
				e.diskfileinfo = 0;
			}
		}
		else
		{
			e.diskfileinfo = g_pFullFileSystem->GetFileTime( filename, "GAME" );
		}
	}

	contract_assert( e.dataIndex != -1 );

	T *data = m_Data[ e.dataIndex ];

	contract_assert( data );

	// Compare fileinfo to disk fileinfo and rebuild cache if out of date or not correct...
	if ( cachefileinfo != e.diskfileinfo )
	{
		if ( !m_bReadOnly )
		{
			RebuildCache( filename, data );
		}
	}
	e.fileinfo = e.diskfileinfo;
}

template <class T>
T *CCachedFileData<T>::RebuildItem( const char *filename )
{
	int idx = GetIndex( filename );
	ElementType_t& e = m_Elements[ idx ];

	e.diskfileinfo = CACHED_FILE_DATA_UNDEFINED_DISKINFO;

	int cachefileinfo = e.fileinfo;
	// Set the disk fileinfo the first time we encounter the filename
	if ( e.diskfileinfo == CACHED_FILE_DATA_UNDEFINED_DISKINFO )
	{
		if ( m_bNeverCheckDisk )
		{
			e.diskfileinfo = cachefileinfo;
		}
		else
		{
			if ( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) 
			{
				e.diskfileinfo = g_pFullFileSystem->Size( filename, "GAME" );
				// Missing files get a disk file size of 0
				if ( e.diskfileinfo == -1 )
				{
					e.diskfileinfo = 0;
				}
			}
			else
			{
				e.diskfileinfo = g_pFullFileSystem->GetFileTime( filename, "GAME" );
			}
		}
	}

	contract_assert( e.dataIndex != -1 );

	T *data = m_Data[ e.dataIndex ];

	contract_assert( data );

	// Compare fileinfo to disk fileinfo and rebuild cache if out of date or not correct...
	if ( !m_bReadOnly )
	{
		RebuildCache( filename, data );
	}
	e.fileinfo = e.diskfileinfo;

	return data;
}

template <class T>
void CCachedFileData<T>::RebuildCache( char const *filename, T *data )
{
	contract_assert( !m_bReadOnly );

	// Recache item, mark self as dirty
	SetDirty( true );

	((IBaseCacheInfo *)data)->Rebuild( filename );
}

template <class T>
void CCachedFileData<T>::ForceRecheckDiskInfo()
{
	for ( ElementType_t& element : m_Elements )
	{
		element.diskfileinfo = CACHED_FILE_DATA_UNDEFINED_DISKINFO;
	}
}

// Iterates all entries and causes rebuild on any existing items which are out of date
template <class T>
void	CCachedFileData<T>::CheckDiskInfo( bool forcerebuild, int cacheFileTime )
{
	char fn[ 512 ];
	int i;
	if ( forcerebuild )
	{
		for ( auto [handle, idx] : m_ElementIndex )
		{
			g_pFullFileSystem->String( handle, fn, sizeof( fn ) );
			Get(fn);
		}
		return;
	}

	// Handle order, which groups files by path.
	std::vector< CSortedCacheFile > list;
	for ( auto [handle, idx] : m_ElementIndex )
	{
		ElementType_t& element = m_Elements[ idx ];
		list.push_back( { handle, idx } );
		if ( m_bNeverCheckDisk && 
			element.diskfileinfo == CACHED_FILE_DATA_UNDEFINED_DISKINFO )
		{
			element.diskfileinfo = element.fileinfo;
		}
	}

	if ( list.empty() || m_bNeverCheckDisk )
		return;

	// Actually sorting by filename here doesn't appear to be a win since FileNameHandle_t is basically sorted by Path anyway (path is stored in the high part of the DWORD)

	for ( int listStart = 0, listEnd = 0; listStart < std::ssize( list ); listStart = listEnd+1 )
	{
		int pathIndex = g_pFullFileSystem->GetPathIndex( m_Elements[list[listStart].index].handle );
		for ( listEnd = listStart; listEnd < std::ssize( list ); listEnd++ )
		{
			ElementType_t& element = m_Elements[ list[listEnd].index ];

			int pathTest = g_pFullFileSystem->GetPathIndex( element.handle );
			if ( pathTest != pathIndex )
				break;
		}
		g_pFullFileSystem->String( m_Elements[list[listStart].index].handle, fn, sizeof( fn ) );
		V_StripFilename( fn );

		int pathTime = g_pFullFileSystem->GetPathTime( fn, "GAME" );
		bool bCheck = (pathTime > cacheFileTime) ? true : false;

		for ( i = listStart; i < listEnd; i++ )
		{
			ElementType_t& element = m_Elements[ list[i].index ];

			if ( element.diskfileinfo == CACHED_FILE_DATA_UNDEFINED_DISKINFO )
			{
				if ( !bCheck )
				{
					element.diskfileinfo = element.fileinfo;
				}
				else
				{
					g_pFullFileSystem->String( element.handle, fn, sizeof( fn ) );
					if ( m_fileCheckType == CACHED_FILE_USE_FILESIZE ) 
					{
						element.diskfileinfo = g_pFullFileSystem->Size( fn, "GAME" );

						// Missing files get a disk file size of 0
						if ( element.diskfileinfo == -1 )
						{
							element.diskfileinfo = 0;
						}
					}
					else
					{
						element.diskfileinfo = g_pFullFileSystem->GetFileTime( fn, "GAME" );
					}
				}
			}
		}
	}
}

template <class T>
void CCachedFileData<T>::SetNeverCheckDisk( bool bNeverCheckDisk )
{
	m_bNeverCheckDisk = bNeverCheckDisk;
}
