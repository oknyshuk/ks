//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <tier0/platform.h>
#define INVALID_HANDLE_VALUE (void *)0
#define FILE_BEGIN SEEK_SET
#define FILE_END SEEK_END
#include "tier1/byte_reader.h"
#include "tier1/byte_writer.h"
#include "utlvector.h"
#include "zip_utils.h"
#include "zip_uncompressed.h"
#include "checksum_crc.h"
#include "utlstring.h"
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "tier0/memalloc.h"

// Data descriptions for byte swapping - only needed
// for structures that are written to file for use by the game.
class CWin32File
{
public:
	static HANDLE CreateTempFile( std::string &WritePath, std::string &FileName )
	{
		char tempFileName[MAX_PATH];
		if ( WritePath.empty() )
		{
			// use a safe name in the cwd
			char *pBuffer = tmpnam( nullptr );
			if ( !pBuffer )
			{
				return INVALID_HANDLE_VALUE;
			}
			if ( pBuffer[0] == '\\' )
			{
				pBuffer++;
			}
			if ( pBuffer[strlen( pBuffer )-1] == '.' )
			{
				pBuffer[strlen( pBuffer )-1] = '\0';
			}
			V_snprintf( tempFileName, sizeof( tempFileName ), "_%s.tmp", pBuffer );
		}
		else
		{
			char uniqueFilename[MAX_PATH];
			static int counter = 0;
			time_t now = time( nullptr );
			struct tm *tm = localtime( &now );
			V_sprintf_safe( uniqueFilename, "%d_%d_%d_%d_%d.tmp", tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec, ++counter );                                                \
			V_ComposeFileName( WritePath.c_str(), uniqueFilename, tempFileName, sizeof( tempFileName ) );
		}

		FileName = tempFileName;
		FILE *hFile = fopen( tempFileName, "rw+" );
		
		return (HANDLE)hFile;
	}

	static unsigned int FileSeek( HANDLE hFile, unsigned int distance, DWORD MoveMethod )
	{
		return fseeko( (FILE *)hFile, distance, MoveMethod );
	}

	static unsigned int FileTell( HANDLE hFile )
	{
		return ftello( (FILE *)hFile );
	}

	static bool FileRead( HANDLE hFile, void *pBuffer, unsigned int size )
	{
		size_t bytesRead = fread( pBuffer, 1, size, (FILE *)hFile );
		return bytesRead == size;
	}

	static bool FileWrite( HANDLE hFile, void *pBuffer, unsigned int size )
	{
		size_t bytesWrtitten = fwrite( pBuffer, 1, size, (FILE *)hFile );
		return bytesWrtitten == size;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Interface to allow abstraction of zip file output methods, and
// avoid duplication of code. Files may be written to a byte_writer or a filestream
//-----------------------------------------------------------------------------
abstract_class IWriteStream
{
public:
	virtual void Put( const void* pMem, int size ) = 0;
	virtual unsigned int Tell( void ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Wrapper for byte_writer methods
//-----------------------------------------------------------------------------
class CBufferStream : public IWriteStream
{
public:
	CBufferStream( ks::byte_writer &buff ) : IWriteStream(), m_buff( &buff ) {}

	// Implementing IWriteStream method
	virtual void Put( const void* pMem, int size ) { m_buff->write_bytes( pMem, size ); }

	// Implementing IWriteStream method
	virtual unsigned int Tell( void ) { return m_buff->tell(); }

private:
	ks::byte_writer *m_buff;
};

//-----------------------------------------------------------------------------
// Purpose: Wrapper for file I/O methods
//-----------------------------------------------------------------------------
class CFileStream : public IWriteStream
{
public:
	CFileStream( FILE *fout ) : IWriteStream(), m_file( fout ), m_hFile( INVALID_HANDLE_VALUE ) {}
	CFileStream( HANDLE hOutFile ) : IWriteStream(), m_file( nullptr ), m_hFile( hOutFile ) {}

	// Implementing IWriteStream method
	virtual void Put( const void* pMem, int size ) 
	{ 
		if ( m_file )
		{
			fwrite( pMem, size, 1, m_file ); 
		}
	}

	// Implementing IWriteStream method
	virtual unsigned int Tell( void ) 
	{ 
		if ( m_file )
		{
			return ftell( m_file );
		}
		else
		{
			return 0;
		} 
	}

private:
	FILE	*m_file;
	HANDLE	m_hFile;
};

//-----------------------------------------------------------------------------
// Purpose: Container for modifiable pak file which is embedded inside the .bsp file
//  itself.  It's used to allow one-off files to be stored local to the map and it is
//  hooked into the file system as an override for searching for named files.
//-----------------------------------------------------------------------------
class CZipFile
{
public:
	// Construction
					CZipFile( const char *pDiskCacheWritePath, bool bSortByName );
					~CZipFile( void );

	// Public API
	// Clear all existing data
	void			Reset( void );

	// Add file to zip under relative name
	void			AddFileToZip( const char *relativename, const char *fullpath );

	// Delete file from zip
	void			RemoveFileFromZip( const char *relativename );

	// Add buffer to zip as a file with given name
	void			AddBufferToZip( const char *relativename, void *data, int length, bool bTextMode );

	// Check if a file already exists in the zip.
	bool			FileExistsInZip( const char *relativename );

	// Reads a file from a zip file
	std::optional< std::vector< std::byte > >	ReadFileFromZip( const char *relativename, bool bTextMode );
	std::optional< std::vector< std::byte > >	ReadFileFromZip( HANDLE hZipFile, const char *relativename, bool bTextMode );

	// Initialize the zip file from a buffer
	void			ParseFromBuffer( void *buffer, int bufferlength );
	HANDLE			ParseFromDisk( const char *pFilename );

	// Estimate the size of the zip file (including header, padding, etc.)
	unsigned int	EstimateSize();

	// Print out a directory of files in the zip.
	void			PrintDirectory( void );

	// Use to iterate directory, pass 0 for first element
	// returns nonzero element id with filled buffer, or -1 at list conclusion
	int				GetNextFilename( int id, char *pBuffer, int bufferSize, int &fileSize );

	// Write the zip to a buffer
	void			SaveToBuffer( ks::byte_writer &buffer );
	// Write the zip to a filestream
	void			SaveToDisk( FILE *fout );
	void			SaveToDisk( HANDLE hOutFile );

	unsigned int	CalculateSize( void );

	void			ForceAlignment( bool aligned, bool bCompatibleFormat, unsigned int alignmentSize );

	unsigned int	GetAlignment();

private:
	enum
	{
		MAX_FILES_IN_ZIP = 32768,
	};

	// Entry name, pooled in m_Names. m_nOrder is first-seen order, which unsorted zips are written in.
	struct ZipName_t
	{
		const char			*m_pName = "";
		unsigned int		m_nOrder = 0;
	};

	typedef struct
	{
		ZipName_t			m_Name;
		unsigned int		filepos;
		int					filelen;
	} TmpFileInfo_t;

	unsigned int	m_AlignmentSize;
	bool			m_bForceAlignment;
	bool			m_bCompatibleFormat;

	unsigned short	CalculatePadding( unsigned int filenameLen, unsigned int pos );
	void			SaveDirectory( IWriteStream& stream );
	int				MakeXZipCommentString( char *pComment );
	void			ParseXZipCommentString( const char *pComment );
	
	// Internal entry for faster searching, etc.
	class CZipEntry
	{
	public:
					CZipEntry( void );
					~CZipEntry( void );

					CZipEntry( const CZipEntry& src );

		// Name of entry
		ZipName_t		m_Name;

		// Lenth of data element
		int				m_Length;
		// Raw data, could be null and data may be in disk write cache
		void			*m_pData;

		// Offset in Zip ( set and valid during final write )
		unsigned int	m_ZipOffset;
		// CRC of blob ( set and valid during final write )
		CRC32_t			m_ZipCRC;

		// Location of data in disk cache
		unsigned int	m_DiskCacheOffset;
		unsigned int	m_SourceDiskOffset;
	};

	// Orders by first-seen order, or caselessly by name when sorting by name.
	struct CZipNameLess
	{
		bool m_bCaseless;
		bool operator()( const ZipName_t &a, const ZipName_t &b ) const
		{
			return m_bCaseless ? V_stricmp( a.m_pName, b.m_pName ) < 0 : a.m_nOrder < b.m_nOrder;
		}
	};

	struct NameHash
	{
		using is_transparent = void;
		size_t operator()( std::string_view s ) const { return std::hash< std::string_view >()( s ); }
	};

	// Name -> first-seen order. Never shrinks, so pooled names outlive the entries using them.
	std::unordered_map< std::string, unsigned int, NameHash, std::equal_to<> > m_Names;

	// For fast name lookup and sorting; keyed by CZipEntry::m_Name. Parsed zips may repeat names.
	typedef std::multimap< ZipName_t, CZipEntry, CZipNameLess > ZipFiles_t;
	ZipFiles_t m_Files;

	ZipName_t				InternName( const char *pName );
	ZipFiles_t::iterator	FindFile( const char *pName );

	// Used to buffer zip data, instead of ram
	bool				m_bUseDiskCacheForWrites;
	HANDLE				m_hDiskCacheWriteFile;
	std::string			m_DiskCacheName;
	std::string			m_DiskCacheWritePath;

	bool				m_bIsUpdateFormat;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CZipFile::CZipEntry::CZipEntry( void )
{
	m_Length = 0;
	m_pData = nullptr;
	m_ZipOffset = 0;
	m_ZipCRC = 0;
	m_DiskCacheOffset = 0;
	m_SourceDiskOffset = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
CZipFile::CZipEntry::CZipEntry( const CZipFile::CZipEntry& src )
{
	m_Name = src.m_Name;
	m_Length = src.m_Length;

	if ( src.m_Length > 0 && src.m_pData )
	{
		m_pData = MemAlloc_Alloc( src.m_Length );
		memcpy( m_pData, src.m_pData, src.m_Length );
	}
	else
	{
		m_pData = nullptr;
	}

	m_ZipOffset = src.m_ZipOffset;
	m_ZipCRC = src.m_ZipCRC;
	m_DiskCacheOffset = src.m_DiskCacheOffset;
	m_SourceDiskOffset = src.m_SourceDiskOffset;
}

//-----------------------------------------------------------------------------
// Purpose: Clear any leftover data
//-----------------------------------------------------------------------------
CZipFile::CZipEntry::~CZipEntry( void )
{
	if ( m_pData )
	{
		g_pMemAlloc->Free( m_pData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
CZipFile::CZipFile( const char *pDiskCacheWritePath, bool bSortByName )
: m_Files( CZipNameLess{ bSortByName } )
{
	m_AlignmentSize = 0;
	m_bForceAlignment = false;
	m_bCompatibleFormat = true;
	m_bIsUpdateFormat = false;

	m_bUseDiskCacheForWrites = ( pDiskCacheWritePath != nullptr );
	m_DiskCacheWritePath = ks::nz( pDiskCacheWritePath );
	m_hDiskCacheWriteFile = INVALID_HANDLE_VALUE;
}

//-----------------------------------------------------------------------------
// Purpose: Destroy zip data
//-----------------------------------------------------------------------------
CZipFile::~CZipFile( void )
{
	m_bUseDiskCacheForWrites = false;
	Reset();
}

CZipFile::ZipName_t CZipFile::InternName( const char *pName )
{
	auto it = m_Names.find( std::string_view( pName ) );
	if ( it == m_Names.end() )
		it = m_Names.emplace( pName, (unsigned int)m_Names.size() ).first;
	return { it->first.c_str(), it->second };
}

CZipFile::ZipFiles_t::iterator CZipFile::FindFile( const char *pName )
{
	ZipName_t key{ pName, 0 };
	if ( !m_Files.key_comp().m_bCaseless )
	{
		auto it = m_Names.find( std::string_view( pName ) );
		if ( it == m_Names.end() )
			return m_Files.end();
		key.m_nOrder = it->second;
	}
	return m_Files.find( key );
}

//-----------------------------------------------------------------------------
// Purpose: Delete all current data
//-----------------------------------------------------------------------------
void CZipFile::Reset( void )
{
	m_Files.clear();

	if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
	{
		fclose( (FILE *)m_hDiskCacheWriteFile );
		unlink( m_DiskCacheName.c_str() );
		m_hDiskCacheWriteFile = INVALID_HANDLE_VALUE;
	}

	if ( m_bUseDiskCacheForWrites )
	{
		m_hDiskCacheWriteFile = CWin32File::CreateTempFile( m_DiskCacheWritePath, m_DiskCacheName );
	}
}

void CZipFile::ForceAlignment( bool bAligned, bool bCompatibleFormat, unsigned int alignment )
{
	// special update format, force the args as desired
	m_bIsUpdateFormat = false;
	if ( alignment == 0xFFFFFFFF )
	{
		bAligned = false;
		alignment = 0;
		m_bIsUpdateFormat = true;
	}

	m_bForceAlignment = bAligned;
	m_AlignmentSize = alignment;
	m_bCompatibleFormat = bCompatibleFormat;

	if ( !bAligned )
	{
		m_AlignmentSize = 0;
	}
	else if ( !IsPowerOfTwo( m_AlignmentSize ) )
	{
		m_AlignmentSize = 0;
	}
}

unsigned int CZipFile::GetAlignment()
{
	if ( !m_bForceAlignment || !m_AlignmentSize )
	{
		return 0;
	}

	return m_AlignmentSize;
}

//-----------------------------------------------------------------------------
// Purpose: Load pak file from raw buffer
// Input  : *buffer - 
//			bufferlength - 
//-----------------------------------------------------------------------------
void CZipFile::ParseFromBuffer( void *buffer, int bufferlength )
{
	// Throw away old data
	Reset();

	ks::byte_reader buf( buffer, bufferlength );
	unsigned int fileLen = bufferlength;

	ZIP_EndOfCentralDirRecord rec = { 0 };

	bool bFoundEndOfCentralDirRecord = false;
	unsigned int offset = fileLen - sizeof( ZIP_EndOfCentralDirRecord );
	// If offset is ever greater than startOffset then it means that it has
	// wrapped. This used to be a tautological >= 0 test.
	for ( unsigned int startOffset = offset; offset <= startOffset; offset-- )
	{
		buf.seek( offset );
		rec = buf.read< ZIP_EndOfCentralDirRecord >();
		if ( buf.ok() && rec.signature == PKID( 5, 6 ) )
		{
			bFoundEndOfCentralDirRecord = true;

			// Set any xzip configuration
			if ( rec.commentLength )
			{
				char commentString[128];
				int commentLength = std::min<std::common_type_t<short unsigned int, long unsigned int>>( rec.commentLength, sizeof( commentString ) );
				buf.read_bytes( commentString, commentLength );
				commentString[commentLength] = '\0';
				ParseXZipCommentString( commentString );
			}
			break;
		}
		else
		{
			// wrong record
			rec.nCentralDirectoryEntries_Total = 0;
		}
	}
	Assert( bFoundEndOfCentralDirRecord );
	
	// Make sure there are some files to parse
	int numzipfiles = rec.nCentralDirectoryEntries_Total;
	if ( numzipfiles <= 0 )
	{
		// No files
		return;
	}

	buf.seek( rec.startOfCentralDirOffset );

	// Allocate space for directory
	TmpFileInfo_t *newfiles = new TmpFileInfo_t[numzipfiles];
	Assert( newfiles );

	// build directory
	int i;
	for ( i = 0; i < rec.nCentralDirectoryEntries_Total; i++ )
	{
		ZIP_FileHeader zipFileHeader;
		zipFileHeader = buf.read< ZIP_FileHeader >();
		Assert( zipFileHeader.signature == PKID( 1, 2 ) );
		Assert( zipFileHeader.compressionMethod == 0 );
		
		char tmpString[1024];
		buf.read_bytes( tmpString, zipFileHeader.fileNameLength );
		tmpString[zipFileHeader.fileNameLength] = '\0';
		Q_strlower( tmpString );

		// can determine actual filepos, assuming a well formed zip
		newfiles[i].m_Name = InternName( tmpString );
		newfiles[i].filelen = zipFileHeader.compressedSize;
		newfiles[i].filepos = zipFileHeader.relativeOffsetOfLocalHeader +
								sizeof( ZIP_LocalFileHeader ) + 
								zipFileHeader.fileNameLength + 
								zipFileHeader.extraFieldLength;

		int nextOffset;
		if ( m_bCompatibleFormat )
		{
			nextOffset = zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
		}
		else
		{
			nextOffset = 0;
		}
		buf.skip( nextOffset );
	}

	// Insert current data into rb tree
	for ( i=0; i<numzipfiles; i++ )
	{
		CZipEntry e;
		e.m_Name = newfiles[i].m_Name;
		e.m_Length = newfiles[i].filelen;
		
		// Make sure length is reasonable
		if ( e.m_Length > 0 )
		{
			e.m_pData = MemAlloc_Alloc( e.m_Length );

			// Copy in data
			buf.seek( newfiles[i].filepos );
			buf.read_bytes( e.m_pData, e.m_Length );
		}
		else
		{
			e.m_pData = nullptr;
		}

		// Add to tree
		m_Files.emplace( e.m_Name, e );
	}

	// Through away directory
	delete[] newfiles;
}

//-----------------------------------------------------------------------------
// Purpose: Mount pak file from disk
//-----------------------------------------------------------------------------
HANDLE CZipFile::ParseFromDisk( const char *pFilename )
{
	HANDLE hFile = fopen( pFilename, "rw+" );
	if ( !hFile )
	{
		// not found
		return nullptr;
	}	

	unsigned int fileLen = CWin32File::FileSeek( hFile, 0, FILE_END );
	CWin32File::FileSeek( hFile, 0, FILE_BEGIN );
	if ( fileLen < sizeof( ZIP_EndOfCentralDirRecord ) )
	{
		// bad format
		fclose( (FILE *)hFile );
		return nullptr;
	}

	// need to get the central dir
	ZIP_EndOfCentralDirRecord rec = { 0 };
	unsigned int offset = fileLen - sizeof( ZIP_EndOfCentralDirRecord );
	// If offset is ever greater than startOffset then it means that it has
	// wrapped. This used to be a tautological >= 0 test.
	for ( unsigned int startOffset = offset; offset <= startOffset; offset-- )
	{
		CWin32File::FileSeek( hFile, offset, FILE_BEGIN );
		
		CWin32File::FileRead( hFile, &rec, sizeof( rec ) );

		if ( rec.signature == PKID( 5, 6 ) )
		{
			// Set any xzip configuration
			if ( rec.commentLength )
			{
				char commentString[128];
				int commentLength = std::min<std::common_type_t<short unsigned int, long unsigned int>>( rec.commentLength, sizeof( commentString ) );
				CWin32File::FileRead( hFile, commentString, commentLength );
				commentString[commentLength] = '\0';
				ParseXZipCommentString( commentString );
			}
			break;
		}
		else
		{
			// wrong record
			rec.nCentralDirectoryEntries_Total = 0;
		}
	}

	// Make sure there are some files to parse
	int numZipFiles = rec.nCentralDirectoryEntries_Total;
	if ( numZipFiles <= 0 )
	{
		// No files
		fclose( (FILE *)hFile );
		return nullptr;
	}

	CWin32File::FileSeek( hFile, rec.startOfCentralDirOffset, FILE_BEGIN );

	// read entire central dir into memory
	std::vector< std::byte > zipDirData( rec.centralDirectorySize );
	CWin32File::FileRead( hFile, zipDirData.data(), rec.centralDirectorySize );
	ks::byte_reader zipDirBuff( zipDirData );

	// build directory
	for ( int i = 0; i < numZipFiles; i++ )
	{
		ZIP_FileHeader zipFileHeader;
		zipFileHeader = zipDirBuff.read< ZIP_FileHeader >();

		if ( zipFileHeader.signature != PKID( 1, 2 ) ||  zipFileHeader.compressionMethod != 0 )
		{
			// bad contents
			fclose( (FILE *)hFile );
			return nullptr;
		}
		
		char fileName[1024];
		zipDirBuff.read_bytes( fileName, zipFileHeader.fileNameLength );
		fileName[zipFileHeader.fileNameLength] = '\0';
		Q_strlower( fileName );

		// can determine actual filepos, assuming a well formed zip
		CZipEntry e;
		e.m_Name = InternName( fileName );
		e.m_Length = zipFileHeader.compressedSize;
		e.m_SourceDiskOffset = zipFileHeader.relativeOffsetOfLocalHeader +
								sizeof( ZIP_LocalFileHeader ) + 
								zipFileHeader.fileNameLength + 
								zipFileHeader.extraFieldLength;
		// Add to tree
		m_Files.emplace( e.m_Name, e );

		int nextOffset;
		if ( m_bCompatibleFormat )
		{
			nextOffset = zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
		}
		else
		{
			nextOffset = 0;
		}

		zipDirBuff.skip( nextOffset );
	}

	return hFile;
}

static int GetLengthOfBinStringAsText( const char *pSrc, int srcSize )
{
	const char *pSrcScan = pSrc;
	const char *pSrcEnd = pSrc + srcSize;
	int numChars = 0;
	for( ; pSrcScan < pSrcEnd; pSrcScan++ )
	{
		if( *pSrcScan == '\n' )
		{
			numChars += 2;
		}
		else
		{
			numChars++;
		}
	}
	return numChars;
}


//-----------------------------------------------------------------------------
// Copies text data from a form appropriate for disk to a normal string
//-----------------------------------------------------------------------------
static std::vector< std::byte > ReadTextData( const char *pSrc, int nSrcSize )
{
	std::vector< std::byte > out;
	out.reserve( nSrcSize );
	const char *pSrcEnd = pSrc + nSrcSize;
	for ( const char *pSrcScan = pSrc; pSrcScan < pSrcEnd; ++pSrcScan )
	{
		if ( *pSrcScan == '\r' )
		{
			if ( pSrcScan[1] == '\n' )
			{
				out.push_back( std::byte{ '\n' } );
				++pSrcScan;
				continue;
			}
		}

		out.push_back( static_cast< std::byte >( *pSrcScan ) );
	}

	return out;
}


//-----------------------------------------------------------------------------
// Copies text data into a form appropriate for disk
//-----------------------------------------------------------------------------
static void CopyTextData( char *pDst, const char *pSrc, int dstSize, int srcSize )
{
	const char *pSrcScan = pSrc;
	const char *pSrcEnd = pSrc + srcSize;
	char *pDstScan = pDst;

#ifdef DBGFLAG_ASSERT
	char *pDstEnd = pDst + dstSize;
#endif

	for ( ; pSrcScan < pSrcEnd; pSrcScan++ )
	{
		if ( *pSrcScan == '\n' )
		{
			*pDstScan = '\r';
			pDstScan++;
			*pDstScan = '\n';
			pDstScan++;
		}
		else
		{
			*pDstScan = *pSrcScan;
			pDstScan++;
		}
	}
	Assert( pSrcScan == pSrcEnd );
	Assert( pDstScan == pDstEnd );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new lump, or overwrites existing one
// Input  : *relativename - 
//			*data - 
//			length - 
//-----------------------------------------------------------------------------
void CZipFile::AddBufferToZip( const char *relativename, void *data, int length, bool bTextMode )
{
	// Lower case only
	char name[512];
	Q_strcpy( name, relativename );
	Q_strlower( name );

	int dstLength = length;
	if ( bTextMode )
	{
		dstLength = GetLengthOfBinStringAsText( ( const char * )data, length );
	}
	
	// See if entry is in list already
	CZipEntry e;
	e.m_Name = InternName( name );
	auto it = m_Files.find( e.m_Name );

	// If already existing, throw away old data and update data and length
	if ( it != m_Files.end() )
	{
		CZipEntry *update = &it->second;
		if ( update->m_pData )
		{
			g_pMemAlloc->Free( update->m_pData );
		}

		if ( bTextMode )
		{
			update->m_pData = MemAlloc_Alloc( dstLength );
			CopyTextData( ( char * )update->m_pData, ( char * )data, dstLength, length );
			update->m_Length = dstLength;
		}
		else
		{
			update->m_pData = MemAlloc_Alloc( length );
			memcpy( update->m_pData, data, length );
			update->m_Length = length;
		}

		if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
		{
			update->m_DiskCacheOffset = CWin32File::FileTell( m_hDiskCacheWriteFile );
			CWin32File::FileWrite( m_hDiskCacheWriteFile, update->m_pData, update->m_Length );
			g_pMemAlloc->Free( update->m_pData );
			update->m_pData = nullptr;
		}
	}
	else
	{
		// Create a new entry
		e.m_Length = dstLength;
		if ( dstLength > 0 )
		{
			if ( bTextMode )
			{
				e.m_pData = MemAlloc_Alloc( dstLength );
				CopyTextData( (char *)e.m_pData, ( char * )data, dstLength, length );
			}
			else
			{
				e.m_pData = MemAlloc_Alloc( length );
				memcpy( e.m_pData, data, length );
			}
	
			if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
			{
				e.m_DiskCacheOffset = CWin32File::FileTell( m_hDiskCacheWriteFile );
				CWin32File::FileWrite( m_hDiskCacheWriteFile, e.m_pData, e.m_Length );
				g_pMemAlloc->Free( e.m_pData );
				e.m_pData = nullptr;
			}
		}
		else
		{
			e.m_pData = nullptr;
		}

		m_Files.emplace( e.m_Name, e );
	}
}


//-----------------------------------------------------------------------------
// Reads a file from the zip
//-----------------------------------------------------------------------------
std::optional< std::vector< std::byte > > CZipFile::ReadFileFromZip( const char *pRelativeName, bool bTextMode )
{
	// Lower case only
	char pName[512];
	Q_strncpy( pName, pRelativeName, 512 );
	Q_strlower( pName );

	// See if entry is in list already
	auto it = FindFile( pName );
	if ( it == m_Files.end() )
	{
		// not found
		return std::nullopt;
	}

	CZipEntry *pEntry = &it->second;
	if ( bTextMode )
	{
		return ReadTextData( (char*)pEntry->m_pData, pEntry->m_Length );
	}

	const auto *pBytes = static_cast< const std::byte * >( pEntry->m_pData );
	return std::vector< std::byte >( pBytes, pBytes + pEntry->m_Length );
}

//-----------------------------------------------------------------------------
// Reads a file from the zip
//-----------------------------------------------------------------------------
std::optional< std::vector< std::byte > > CZipFile::ReadFileFromZip( HANDLE hZipFile, const char *pRelativeName, bool bTextMode )
{
	// Lower case only
	char pName[512];
	Q_strncpy( pName, pRelativeName, 512 );
	Q_strlower( pName );

	// See if entry is in list already
	auto it = FindFile( pName );
	if ( it == m_Files.end() )
	{
		// not found
		return std::nullopt;
	}

	CZipEntry *pEntry = &it->second;

	void *pData = MemAlloc_Alloc( pEntry->m_Length );
	CWin32File::FileSeek( hZipFile, pEntry->m_SourceDiskOffset, FILE_BEGIN );
	if ( !CWin32File::FileRead( hZipFile, pData, pEntry->m_Length ) )
	{
		g_pMemAlloc->Free( pData );
		return std::nullopt;
	}

	std::vector< std::byte > out;
	if ( bTextMode )
	{
		out = ReadTextData( (const char *)pData, pEntry->m_Length );
	}
	else
	{
		const auto *pBytes = static_cast< const std::byte * >( pData );
		out.assign( pBytes, pBytes + pEntry->m_Length );
	}

	g_pMemAlloc->Free( pData );

	return out;
}

//-----------------------------------------------------------------------------
// Purpose: Check if a file already exists in the zip.
// Input  : *relativename - 
//-----------------------------------------------------------------------------
bool CZipFile::FileExistsInZip( const char *pRelativeName )
{
	// Lower case only
	char pName[512];
	Q_strncpy( pName, pRelativeName, 512 );
	Q_strlower( pName );

	// See if entry is in list already
	auto it = FindFile( pName );

	// If it is, then it exists in the pack!
	return it != m_Files.end();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a new file to the zip.
//-----------------------------------------------------------------------------
void CZipFile::AddFileToZip( const char *relativename, const char *fullpath )
{
	FILE *temp;
	temp = fopen( fullpath, "rb" );
	if ( !temp )
		return;

	// Determine length
	fseek( temp, 0, SEEK_END );
	int size = ftell( temp );
	fseek( temp, 0, SEEK_SET );
	byte *buf = (byte *)MemAlloc_Alloc( size + 1 );

	// Read data
	fread( buf, size, 1, temp );
	fclose( temp );

	// Now add as a buffer
	AddBufferToZip( relativename, buf, size, false );
	
	g_pMemAlloc->Free( buf );
}

//-----------------------------------------------------------------------------
// Purpose: Removes a file from the zip.
//-----------------------------------------------------------------------------
void CZipFile::RemoveFileFromZip( const char *relativename )
{
	auto it = FindFile( relativename );

	if ( it != m_Files.end() )
	{
		m_Files.erase( it );
	}
}

//---------------------------------------------------------------
//	Purpose: Calculates how many bytes should be added to the extra field
//  to push the start of the file data to the next aligned boundary
//  Output: Required padding size
//---------------------------------------------------------------
unsigned short CZipFile::CalculatePadding( unsigned int filenameLen, unsigned int pos )
{
	if ( m_AlignmentSize == 0 )
	{
		return 0;
	}
	
	unsigned int headerSize = sizeof( ZIP_LocalFileHeader ) + filenameLen;
	return (unsigned short)( m_AlignmentSize - ( ( pos + headerSize ) % m_AlignmentSize ) );
}

//-----------------------------------------------------------------------------
// Purpose: Create the XZIP identifying comment string
// Output : Length
//-----------------------------------------------------------------------------
int CZipFile::MakeXZipCommentString( char *pCommentString )
{
	char tempString[XZIP_COMMENT_LENGTH];

	memset( tempString, 0, sizeof( tempString ) );

	char cFormat = m_bCompatibleFormat ? '1' : '2';
	if ( m_bCompatibleFormat )
	{
		cFormat = '1';
	}
	else if ( !m_bIsUpdateFormat )
	{
		cFormat = '2';
	}
	else
	{
		// update format
		cFormat = '3';
	}

	V_snprintf( tempString, sizeof( tempString ), "XZP%c %d", cFormat, m_AlignmentSize );
	if ( pCommentString )
	{
		memcpy( pCommentString, tempString, sizeof( tempString ) );
	}

	// expected fixed length
	return XZIP_COMMENT_LENGTH;
}

//-----------------------------------------------------------------------------
// Purpose: An XZIP has its configuration in the ascii comment
//-----------------------------------------------------------------------------
void CZipFile::ParseXZipCommentString( const char *pCommentString )
{
	if ( !V_strnicmp( pCommentString, "XZP", 3 ) )
	{
		m_bCompatibleFormat = true;
		if ( pCommentString[3] == '2' )
		{
			m_bCompatibleFormat = false;
		}
		else if ( pCommentString[3] == '3' )
		{
			m_bCompatibleFormat = false;
			m_bIsUpdateFormat = true;
		}

		// parse out the alignement configuration
		if ( !m_bForceAlignment )
		{
			m_AlignmentSize = 0;
			sscanf( pCommentString + 4, "%d", &m_AlignmentSize );
			if ( !IsPowerOfTwo( m_AlignmentSize ) )
			{
				m_AlignmentSize = 0;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the exact size of zip file, with headers and padding
// Output : int
//-----------------------------------------------------------------------------
unsigned int CZipFile::CalculateSize( void )
{
	unsigned int size = 0;
	unsigned int dirHeaders = 0;
	for ( auto &[name, entry] : m_Files )
	{
		CZipEntry *e = &entry;
		
		if ( e->m_Length == 0 )
			continue;

		// local file header
		size += sizeof( ZIP_LocalFileHeader );
		size += strlen( e->m_Name.m_pName );

		// every file has a directory header that duplicates the filename 
		dirHeaders += sizeof( ZIP_FileHeader ) + strlen( e->m_Name.m_pName );

		// calculate padding
		if ( m_AlignmentSize != 0 )
		{
			// round up to next boundary
			unsigned int nextBoundary = ( size + m_AlignmentSize ) & ~( m_AlignmentSize - 1 );
			
			// the directory header also duplicates the padding
			dirHeaders += nextBoundary - size;

			size = nextBoundary;
		}

		// data size
		size += e->m_Length;
	}

	size += dirHeaders;

	// All processed zip files will have a comment string
	size += sizeof( ZIP_EndOfCentralDirRecord ) + MakeXZipCommentString( nullptr );

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Print a directory of files in the zip
//-----------------------------------------------------------------------------
void CZipFile::PrintDirectory( void )
{
	for ( auto &[name, entry] : m_Files )
	{
		CZipEntry *e = &entry;

		Msg( "%s\n", e->m_Name.m_pName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterate through directory
//-----------------------------------------------------------------------------
int CZipFile::GetNextFilename( int id, char *pBuffer, int bufferSize, int &fileSize )
{
	// ids are in-order positions
	id = ( id == -1 ) ? 0 : id + 1;
	if ( id < 0 || id >= (int)m_Files.size() )
	{
		// list is empty
		return -1;
	}

	CZipEntry *e = &std::next( m_Files.begin(), id )->second;

	Q_strncpy( pBuffer, e->m_Name.m_pName, bufferSize );
	fileSize = e->m_Length;

	return id;
}

//-----------------------------------------------------------------------------
// Purpose: Store data out to disk
//-----------------------------------------------------------------------------
void CZipFile::SaveToDisk( FILE *fout )
{
	CFileStream stream( fout );
	SaveDirectory( stream );
}

void CZipFile::SaveToDisk( HANDLE hOutFile )
{
	CFileStream stream( hOutFile );
	SaveDirectory( stream );
}

//-----------------------------------------------------------------------------
// Purpose: Store data out to a byte_writer
//-----------------------------------------------------------------------------
void CZipFile::SaveToBuffer( ks::byte_writer &buf )
{
	CBufferStream stream( buf );
	SaveDirectory( stream );
}

//-----------------------------------------------------------------------------
// Purpose: Store data back out to a stream (could be byte_writer or filestream)
//-----------------------------------------------------------------------------
void CZipFile::SaveDirectory( IWriteStream& stream )
{
	void *pPaddingBuffer = nullptr;
	if ( m_AlignmentSize )
	{
		// get a temp buffer for all padding work
		pPaddingBuffer = MemAlloc_Alloc( m_AlignmentSize );
		memset( pPaddingBuffer, 0x00, m_AlignmentSize );
	}

	if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
	{
		fflush( (FILE *)m_hDiskCacheWriteFile );
	}

	bool bDataWritten = false;
	for ( auto &[name, entry] : m_Files )
	{
		CZipEntry *e = &entry;
		Assert( e );

		// Fix up the offset
		e->m_ZipOffset = stream.Tell();

		if ( e->m_Length > 0 && ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE ) )
		{	
			// get the data back from the write cache
			e->m_pData = MemAlloc_Alloc( e->m_Length );
			if ( e->m_pData )
			{
				CWin32File::FileSeek( m_hDiskCacheWriteFile, e->m_DiskCacheOffset, FILE_BEGIN );
				CWin32File::FileRead( m_hDiskCacheWriteFile, e->m_pData, e->m_Length );
			}
		}

		if ( e->m_Length > 0 && e->m_pData != nullptr )
		{
			ZIP_LocalFileHeader hdr = { 0 };
			hdr.signature = PKID( 3, 4 );
			hdr.versionNeededToExtract = 10;  // This is the version that the winzip that I have writes.
			hdr.flags = 0;
			hdr.compressionMethod = 0; // NO COMPRESSION!
			hdr.lastModifiedTime = 0;
			hdr.lastModifiedDate = 0;

			CRC32_Init( &e->m_ZipCRC );
			CRC32_ProcessBuffer( &e->m_ZipCRC, e->m_pData, e->m_Length );
			CRC32_Final( &e->m_ZipCRC );
			hdr.crc32 = e->m_ZipCRC;
			
			const char *pFilename = e->m_Name.m_pName;
			hdr.compressedSize = e->m_Length;
			hdr.uncompressedSize = e->m_Length;
			hdr.fileNameLength = strlen( pFilename );
			hdr.extraFieldLength = CalculatePadding( hdr.fileNameLength, e->m_ZipOffset );
			int extraFieldLength = hdr.extraFieldLength;
			
			// Swap header in place
			stream.Put( &hdr, sizeof( hdr ) );
			stream.Put( pFilename, strlen( pFilename ) );
			stream.Put( pPaddingBuffer, extraFieldLength );

			// An update format specifically does not place any files
			// except the first file which should be the preload section.
			// All files in an update zip, exist compressed in the preload section.
			if ( m_bCompatibleFormat || !m_bIsUpdateFormat || !bDataWritten )
			{
				// write the data
				stream.Put( e->m_pData, e->m_Length );
				bDataWritten = true;
			}

			if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
			{
				g_pMemAlloc->Free( e->m_pData );

				// temp hackery for the logic below to succeed
				e->m_pData = (void*)0xFFFFFFFF;
			}
		}
	}

	if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
	{
		CWin32File::FileSeek( m_hDiskCacheWriteFile, 0, FILE_END );
	}

	unsigned int centralDirStart = stream.Tell();
	if ( m_AlignmentSize )
	{
		// align the central directory starting position
		unsigned int newDirStart = AlignValue( centralDirStart, m_AlignmentSize );
		int padLength = newDirStart - centralDirStart;
		if ( padLength )
		{
			stream.Put( pPaddingBuffer, padLength );
			centralDirStart = newDirStart;
		}
	}

	int realNumFiles = 0;
	for ( auto &[name, entry] : m_Files )
	{
		CZipEntry *e = &entry;
		Assert( e );
		
		if ( e->m_Length > 0 && e->m_pData != nullptr )
		{
			ZIP_FileHeader hdr = { 0 };
			hdr.signature = PKID( 1, 2 );
			hdr.versionMadeBy = 20;				// This is the version that the winzip that I have writes.
			hdr.versionNeededToExtract = 10;	// This is the version that the winzip that I have writes.
			hdr.flags = 0;
			hdr.compressionMethod = 0;
			hdr.lastModifiedTime = 0;
			hdr.lastModifiedDate = 0;
			hdr.crc32 = e->m_ZipCRC;

			hdr.compressedSize = e->m_Length;
			hdr.uncompressedSize = e->m_Length;
			hdr.fileNameLength = strlen( e->m_Name.m_pName );
			hdr.extraFieldLength = CalculatePadding( hdr.fileNameLength, e->m_ZipOffset );
			hdr.fileCommentLength = 0;
			hdr.diskNumberStart = 0;
			hdr.internalFileAttribs = 0;
			hdr.externalFileAttribs = 0; // This is usually something, but zero is OK as if the input came from stdin
			hdr.relativeOffsetOfLocalHeader = e->m_ZipOffset;
			int extraFieldLength = hdr.extraFieldLength;

			// Swap the header in place
			stream.Put( &hdr, sizeof( hdr ) );
			stream.Put( e->m_Name.m_pName, strlen( e->m_Name.m_pName ) );
			if ( m_bCompatibleFormat )
			{
				stream.Put( pPaddingBuffer, extraFieldLength );
			}

			realNumFiles++;

			if ( m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE )
			{
				// clear out temp hackery
				e->m_pData = nullptr;
			}
		}
	}

	unsigned int centralDirEnd = stream.Tell();
	if ( m_AlignmentSize )
	{
		// align the central directory starting position
		unsigned int newDirEnd = AlignValue( centralDirEnd, m_AlignmentSize );
		int padLength = newDirEnd - centralDirEnd;
		if ( padLength )
		{
			stream.Put( pPaddingBuffer, padLength );
			centralDirEnd = newDirEnd;
		}
	}

	ZIP_EndOfCentralDirRecord rec = { 0 };
	rec.signature = PKID( 5, 6 );
	rec.numberOfThisDisk = 0;
	rec.numberOfTheDiskWithStartOfCentralDirectory = 0;
	rec.nCentralDirectoryEntries_ThisDisk = realNumFiles;
	rec.nCentralDirectoryEntries_Total = realNumFiles;
	rec.centralDirectorySize = centralDirEnd - centralDirStart;
	rec.startOfCentralDirOffset = centralDirStart;

	char commentString[128];
	int commentLength = MakeXZipCommentString( commentString );
	rec.commentLength = commentLength;

	// Swap the header in place
	stream.Put( &rec, sizeof( rec ) );
	stream.Put( commentString, commentLength );

	if ( pPaddingBuffer )
	{
		g_pMemAlloc->Free( pPaddingBuffer );
	}
}

class CZip : public IZip
{
public:
	CZip( const char *pDiskCacheWritePath, bool bSortByName );
	virtual ~CZip();

	virtual void			Reset();

	// Add a single file to a zip - maintains the zip's previous alignment state
	virtual void			AddFileToZip( const char *relativename, const char *fullpath );

	// Whether a file is contained in a zip - maintains alignment
	virtual bool			FileExistsInZip( const char *pRelativeName );

	// Reads a file from the zip - maintains alignement
	virtual std::optional< std::vector< std::byte > >	ReadFileFromZip( const char *pRelativeName, bool bTextMode );
	virtual std::optional< std::vector< std::byte > >	ReadFileFromZip( HANDLE hZipFile, const char *relativename, bool bTextMode );

	// Removes a single file from the zip - maintains alignment
	virtual void			RemoveFileFromZip( const char *relativename );

	// Gets next filename in zip, for walking the directory - maintains alignment
	virtual int				GetNextFilename( int id, char *pBuffer, int bufferSize, int &fileSize );

	// Prints the zip's contents - maintains alignment
	virtual void			PrintDirectory( void );

	// Estimate the size of the Zip (including header, padding, etc.)
	virtual unsigned int	EstimateSize( void );

	// Add buffer to zip as a file with given name - uses current alignment size, default 0 (no alignment)
	virtual void			AddBufferToZip( const char *relativename, void *data, int length, bool bTextMode );

	// Writes out zip file to a buffer - uses current alignment size 
	// (set by file's previous alignment, or a call to ForceAlignment)
	virtual void			SaveToBuffer( ks::byte_writer &outbuf );

	// Writes out zip file to a filestream - uses current alignment size 
	// (set by file's previous alignment, or a call to ForceAlignment)
	virtual void			SaveToDisk( FILE *fout );
	virtual void			SaveToDisk( HANDLE hOutFile );

	// Reads a zip file from a buffer into memory - sets current alignment size to 
	// the file's alignment size, unless overridden by a ForceAlignment call)
	virtual void			ParseFromBuffer( void *buffer, int bufferlength );
	virtual HANDLE			ParseFromDisk( const char *pFilename );

	// Forces a specific alignment size for all subsequent file operations, overriding files' previous alignment size.
	// Return to using files' individual alignment sizes by passing FALSE.
	virtual void			ForceAlignment( bool aligned, bool bCompatibleFormat, unsigned int alignmentSize );

	virtual unsigned int	GetAlignment();

private:
	CZipFile				m_ZipFile;
};

static std::vector< CZip* > g_ZipUtils;

IZip *IZip::CreateZip( const char *pDiskCacheWritePath, bool bSortByName )
{ 
	CZip *pZip = new CZip( pDiskCacheWritePath, bSortByName );
	g_ZipUtils.push_back( pZip );

	return pZip; 
}

void IZip::ReleaseZip( IZip *pZip )
{
	ks::swap_erase_first( g_ZipUtils, (CZip *)pZip );

	delete ((CZip *)pZip);
}

CZip::CZip( const char *pDiskCacheWritePath, bool bSortByName ) : m_ZipFile( pDiskCacheWritePath, bSortByName )
{
	m_ZipFile.Reset();
}

CZip::~CZip()
{
}

void CZip::AddFileToZip( const char *relativename, const char *fullpath )
{
	m_ZipFile.AddFileToZip( relativename, fullpath );
}

bool CZip::FileExistsInZip( const char *pRelativeName )
{
	return m_ZipFile.FileExistsInZip( pRelativeName );
}

std::optional< std::vector< std::byte > > CZip::ReadFileFromZip( const char *pRelativeName, bool bTextMode )
{
	return m_ZipFile.ReadFileFromZip( pRelativeName, bTextMode );
}

std::optional< std::vector< std::byte > > CZip::ReadFileFromZip( HANDLE hZipFile, const char *pRelativeName, bool bTextMode )
{
	return m_ZipFile.ReadFileFromZip( hZipFile, pRelativeName, bTextMode );
}

void CZip::RemoveFileFromZip( const char *relativename )
{
	m_ZipFile.RemoveFileFromZip( relativename );
}

int	CZip::GetNextFilename( int id, char *pBuffer, int bufferSize, int &fileSize )
{
	return m_ZipFile.GetNextFilename( id, pBuffer, bufferSize, fileSize );
}

void CZip::PrintDirectory( void )
{
	m_ZipFile.PrintDirectory();
}

void CZip::Reset()
{
	m_ZipFile.Reset();
}

unsigned int CZip::EstimateSize( void )
{
	return m_ZipFile.CalculateSize();
}

// Add buffer to zip as a file with given name
void CZip::AddBufferToZip( const char *relativename, void *data, int length, bool bTextMode )
{
	m_ZipFile.AddBufferToZip( relativename, data, length, bTextMode );
}

void CZip::SaveToBuffer( ks::byte_writer &outbuf )
{
	m_ZipFile.SaveToBuffer( outbuf );
}

void CZip::SaveToDisk( FILE *fout )
{
	m_ZipFile.SaveToDisk( fout );
}

void CZip::SaveToDisk( HANDLE hOutFile )
{
	m_ZipFile.SaveToDisk( hOutFile );
}

void CZip::ParseFromBuffer( void *buffer, int bufferlength )
{
	m_ZipFile.Reset();
	m_ZipFile.ParseFromBuffer( buffer, bufferlength );
}

HANDLE CZip::ParseFromDisk( const char *pFilename )
{
	m_ZipFile.Reset();
	return m_ZipFile.ParseFromDisk( pFilename );
}

void CZip::ForceAlignment( bool aligned, bool bCompatibleFormat, unsigned int alignmentSize )
{
	m_ZipFile.ForceAlignment( aligned, bCompatibleFormat, alignmentSize );
}

unsigned int CZip::GetAlignment()
{
	return m_ZipFile.GetAlignment();
}

