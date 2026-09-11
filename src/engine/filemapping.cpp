//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Read-only whole-file memory mapping.
//
//============================================================================//

#include "filemapping.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CFileMapping::Map( const char *pszAbsolutePath )
{
	Unmap();

	if ( !pszAbsolutePath || !pszAbsolutePath[0] )
		return false;

	const int fd = ::open( pszAbsolutePath, O_RDONLY | O_CLOEXEC );
	if ( fd < 0 )
		return false;

	// The descriptor is only needed to establish the mapping; the mapping keeps
	// its own reference to the underlying file, so close it either way.
	struct stat st;
	const bool bStat = ( ::fstat( fd, &st ) == 0 );
	const bool bRegular = bStat && S_ISREG( st.st_mode );
	const size_t nSize = bRegular ? static_cast<size_t>( st.st_size ) : 0;

	if ( nSize == 0 )
	{
		::close( fd );
		return false;
	}

	void *pBase = ::mmap( nullptr, nSize, PROT_READ, MAP_PRIVATE, fd, 0 );
	::close( fd );

	if ( pBase == MAP_FAILED )
		return false;

	m_Bytes = { static_cast<const std::byte *>( pBase ), nSize };
	return true;
}

void CFileMapping::Unmap()
{
	if ( m_Bytes.empty() )
		return;

	::munmap( const_cast<void *>( static_cast<const void *>( m_Bytes.data() ) ), m_Bytes.size() );

	m_Bytes = {};
}

void CFileMapping::PrefetchAll() const
{
	if ( !m_Bytes.empty() )
	{
		::madvise( const_cast<void *>( static_cast<const void *>( m_Bytes.data() ) ), m_Bytes.size(), MADV_WILLNEED );
	}
}
