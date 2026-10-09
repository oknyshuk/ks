// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Read-only whole-file memory mapping.

import ks.platform;
#include "filemapping.h"


bool CFileMapping::Map( const char *pszAbsolutePath )
{
	Unmap();

	if ( !pszAbsolutePath || !pszAbsolutePath[0] )
		return false;

	ks::platform::file_error error = ks::platform::file_error::none;
	ks::platform::mapping *mapped = ks::platform::map_file( pszAbsolutePath, &error );
	if ( mapped == nullptr || error != ks::platform::file_error::none )
		return false;

	m_Mapping = mapped;
	m_Bytes = { static_cast<const std::byte *>( ks::platform::mapping_data( mapped ) ),
	            static_cast<std::size_t>( ks::platform::mapping_size( mapped ) ) };
	return true;
}

void CFileMapping::Unmap()
{
	if ( m_Mapping != nullptr )
	{
		ks::platform::unmap( m_Mapping );
		m_Mapping = nullptr;
	}
	m_Bytes = {};
}

void CFileMapping::PrefetchAll() const
{
	if ( m_Mapping != nullptr )
		ks::platform::mapping_prefetch( m_Mapping );
}
