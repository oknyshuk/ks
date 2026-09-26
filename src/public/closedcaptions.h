//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef CLOSEDCAPTIONS_H
#define CLOSEDCAPTIONS_H

#include "captioncompiler.h"
#include <flat_set>
#include <set>
#include <string>

FORWARD_DECLARE_HANDLE( memhandle_t );

typedef std::flat_multiset< CaptionLookup_t, CCaptionLookupLess > CaptionDictionary_t;
struct AsyncCaption_t
{
	AsyncCaption_t()
	{
		Q_memset( &m_Header, 0, sizeof( m_Header ) );
	}

	struct BlockInfo_t
	{
		int			fileindex;
		int			blocknum;
		memhandle_t handle;

		struct Less
		{
			bool operator()( const BlockInfo_t& lhs, const BlockInfo_t& rhs ) const
			{
				if ( lhs.fileindex != rhs.fileindex )
					return lhs.fileindex < rhs.fileindex;

				return lhs.blocknum < rhs.blocknum;
			}
		};
	};

	AsyncCaption_t( AsyncCaption_t && ) = default;
	AsyncCaption_t &operator=( AsyncCaption_t && ) = default;

	AsyncCaption_t& operator =( const AsyncCaption_t& rhs )
	{
		if ( this == &rhs )
			return *this;

		m_CaptionDirectory = rhs.m_CaptionDirectory;
		m_Header = rhs.m_Header;
		m_DataBaseFile = rhs.m_DataBaseFile;

		m_RequestedBlocks.insert( rhs.m_RequestedBlocks.begin(), rhs.m_RequestedBlocks.end() );

		return *this;
	}

	bool LoadFromFile( char const *pchFullPath );

	std::multiset< BlockInfo_t, BlockInfo_t::Less >	m_RequestedBlocks;

	CaptionDictionary_t		m_CaptionDirectory;
	CompiledCaptionHeader_t	m_Header;
	std::string				m_DataBaseFile;
};

#endif // CLOSEDCAPTIONS_H
