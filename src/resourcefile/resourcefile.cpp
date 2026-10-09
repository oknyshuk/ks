// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "resourcefile/resourcefile.h"

//-----------------------------------------------------------------------------
// Does this resource file contain a particular block?
//-----------------------------------------------------------------------------
bool Resource_IsBlockDefined( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id )
{
	contract_assert( pHeader->m_nVersion == RESOURCE_FILE_HEADER_VERSION );
	if ( pHeader->m_nVersion != RESOURCE_FILE_HEADER_VERSION )
		return false;

	for ( int i = 0; i < pHeader->m_ResourceBlocks.Count(); ++i )
	{
		const ResourceBlockEntry_t &block = pHeader->m_ResourceBlocks[i];
		if ( block.m_nBlockType == id )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Gets the data associated with a particular data block
//-----------------------------------------------------------------------------
const void *Resource_GetBlock( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id )
{
	contract_assert( pHeader->m_nVersion == RESOURCE_FILE_HEADER_VERSION );
	if ( pHeader->m_nVersion != RESOURCE_FILE_HEADER_VERSION )
		return nullptr;

	for ( int i = 0; i < pHeader->m_ResourceBlocks.Count(); ++i )
	{
		const ResourceBlockEntry_t &block = pHeader->m_ResourceBlocks[i];
		if ( block.m_nBlockType == id )
			return block.m_pBlockData;
	}
	return nullptr;
}
