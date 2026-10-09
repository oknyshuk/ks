// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "studio.h"
import std;
#include "filesystem.h"

extern IFileSystem *		g_pFileSystem;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

// a string table to speed up searching for sequences in the current virtual model
struct modellookup_t
{
	ks::idict<short> seqTable;
	ks::idict<short> animTable;
};

static std::vector< modellookup_t > g_ModelLookup;
static int g_ModelLookupIndex = -1;

inline bool HasLookupTable()
{
	return g_ModelLookupIndex >= 0 ? true : false;
}

inline ks::idict<short> *GetSeqTable()
{
	return &g_ModelLookup[g_ModelLookupIndex].seqTable;
}

inline ks::idict<short> *GetAnimTable()
{
	return &g_ModelLookup[g_ModelLookupIndex].animTable;
}

class CModelLookupContext
{
public:
	CModelLookupContext(int group, const studiohdr_t *pStudioHdr);
	~CModelLookupContext();

private:
	int		m_lookupIndex;
};

CModelLookupContext::CModelLookupContext(int group, const studiohdr_t *pStudioHdr)
{
	m_lookupIndex = -1;
	if ( group == 0 && pStudioHdr->numincludemodels )
	{
		m_lookupIndex = std::ssize( g_ModelLookup ); g_ModelLookup.emplace_back();
		g_ModelLookupIndex = std::ssize( g_ModelLookup )-1;
	}
}

CModelLookupContext::~CModelLookupContext()
{
	if ( m_lookupIndex >= 0 )
	{
		contract_assert( m_lookupIndex == (std::ssize( g_ModelLookup )-1) );
		ks::swap_remove( g_ModelLookup, m_lookupIndex );
		g_ModelLookupIndex = std::ssize( g_ModelLookup )-1;
	}
}

void virtualmodel_t::AppendModels( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );

	// build a search table if necesary
	CModelLookupContext ctx(group, pStudioHdr);

	AppendSequences( group, pStudioHdr );
	AppendAnimations( group, pStudioHdr );
	AppendBonemap( group, pStudioHdr );
	AppendAttachments( group, pStudioHdr );
	AppendPoseParameters( group, pStudioHdr );
	AppendNodes( group, pStudioHdr );
	AppendIKLocks( group, pStudioHdr );

	struct HandleAndHeader_t
	{
		void				*handle;
		const studiohdr_t	*pHdr;
	};
	HandleAndHeader_t list[64];

	// determine quantity of valid include models in one pass only
	// temporarily cache results off, otherwise FindModel() causes ref counting problems
	int j;
	int nValidIncludes = 0;
	for (j = 0; j < pStudioHdr->numincludemodels; j++)
	{
		// find model (increases ref count)
		void *tmp = nullptr;
		const studiohdr_t *pTmpHdr = pStudioHdr->FindModel( &tmp, pStudioHdr->pModelGroup( j )->pszName() );
		if ( pTmpHdr )
		{
			if ( nValidIncludes >= std::size( list ) )
			{
				// would cause stack overflow
				contract_assert( 0 );
				break;
			}

			list[nValidIncludes].handle = tmp;
			list[nValidIncludes].pHdr = pTmpHdr;
			nValidIncludes++;
		}
	}

	if ( nValidIncludes )
	{
		m_group.reserve( std::ssize( m_group ) + nValidIncludes );
		for (j = 0; j < nValidIncludes; j++)
		{
			int group = std::ssize( m_group ); m_group.emplace_back();
			m_group[group].cache = list[j].handle;
			AppendModels( group, list[j].pHdr );
		}
	}

	UpdateAutoplaySequences( pStudioHdr );
}

void virtualmodel_t::AppendSequences( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int numCheck = std::ssize( m_seq );

	int j, k;


	std::vector< virtualsequence_t > seq;

	seq = m_seq;

	m_group[ group ].masterSeq.assign( pStudioHdr->numlocalseq, {} );

	for (j = 0; j < pStudioHdr->numlocalseq; j++)
	{
		const mstudioseqdesc_t *seqdesc = pStudioHdr->pLocalSeqdesc( j );
		char *s1 = seqdesc->pszLabel();

		if ( HasLookupTable() )
		{
			k = numCheck;
			if ( short *pIndex = ks::find_ptr( *GetSeqTable(), s1 ) )
			{
				k = *pIndex;
			}
		}
		else
		{
			for (k = 0; k < numCheck; k++)
			{
				const studiohdr_t *hdr = m_group[ seq[k].group ].GetStudioHdr();
				char *s2 = hdr->pLocalSeqdesc( seq[k].index )->pszLabel();
				if ( !stricmp( s1, s2 ) )
				{
					break;
				}
			}
		}
		// no duplication
		if (k == numCheck)
		{
			virtualsequence_t tmp;
			tmp.group = group;
			tmp.index = j;
			tmp.flags = seqdesc->flags;
			tmp.activity = seqdesc->activity;
			k = std::ssize( seq ); seq.push_back( tmp );
		}
		else if (m_group[ seq[k].group ].GetStudioHdr()->pLocalSeqdesc( seq[k].index )->flags & STUDIO_OVERRIDE)
		{
			// the one in memory is a forward declared sequence, override it
			virtualsequence_t tmp;
			tmp.group = group;
			tmp.index = j;
			tmp.flags = seqdesc->flags;
			tmp.activity = seqdesc->activity;
			seq[k] = tmp;
		}
		m_group[ group ].masterSeq[ j ] = k;
	}

	if ( HasLookupTable() )
	{
		for ( j = numCheck; j < std::ssize( seq ); j++ )
		{
			const studiohdr_t *hdr = m_group[ seq[j].group ].GetStudioHdr();
			const char *s1 = hdr->pLocalSeqdesc( seq[j].index )->pszLabel();
			// Keep the first, as the linear search does.
			GetSeqTable()->emplace( s1, j );
		}
	}

	m_seq = seq;
}


void virtualmodel_t::UpdateAutoplaySequences( const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int autoplayCount = pStudioHdr->CountAutoplaySequences();
	m_autoplaySequences.assign( autoplayCount, {} );
	pStudioHdr->CopyAutoplaySequences( m_autoplaySequences.data(), autoplayCount );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendAnimations( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int numCheck = std::ssize( m_anim );

	std::vector< virtualgeneric_t > anim;
	anim = m_anim;


	int j, k;

	m_group[ group ].masterAnim.assign( pStudioHdr->numlocalanim, {} );

	for (j = 0; j < pStudioHdr->numlocalanim; j++)
	{
		char *s1 = pStudioHdr->pLocalAnimdesc( j )->pszName();
		if ( HasLookupTable() )
		{
			k = numCheck;
			if ( short *pIndex = ks::find_ptr( *GetAnimTable(), s1 ) )
			{
				k = *pIndex;
			}
		}
		else
		{
			for (k = 0; k < numCheck; k++)
			{
				char *s2 = m_group[ anim[k].group ].GetStudioHdr()->pLocalAnimdesc( anim[k].index )->pszName();
				if (stricmp( s1, s2 ) == 0)
				{
					break;
				}
			}
		}
		// no duplication
		if (k == numCheck)
		{
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = std::ssize( anim ); anim.push_back( tmp );
		}

		m_group[ group ].masterAnim[ j ] = k;
	}
	
	if ( HasLookupTable() )
	{
		for ( j = numCheck; j < std::ssize( anim ); j++ )
		{
			const char *s1 = m_group[ anim[j].group ].GetStudioHdr()->pLocalAnimdesc( anim[j].index )->pszName();
			GetAnimTable()->emplace( s1, j );
		}
	}

	m_anim = anim;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendBonemap( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );

	const studiohdr_t *pBaseStudioHdr = m_group[ 0 ].GetStudioHdr( );

	m_group[ group ].boneMap.assign( pBaseStudioHdr->numbones, {} );
	m_group[ group ].masterBone.assign( pStudioHdr->numbones, {} );

	int j, k;

	if (group == 0)
	{
		for (j = 0; j < pStudioHdr->numbones; j++)
		{
			m_group[ group ].boneMap[ j ] = j;
			m_group[ group ].masterBone[ j ] = j;
		}
	}
	else
	{
		for (j = 0; j < pBaseStudioHdr->numbones; j++)
		{
			m_group[ group ].boneMap[ j ] = -1;
		}
		for (j = 0; j < pStudioHdr->numbones; j++)
		{
			// NOTE: studiohdr has a bone table - using the table is ~5% faster than this for alyx.mdl on a P4/3.2GHz
			for (k = 0; k < pBaseStudioHdr->numbones; k++)
			{
				if (stricmp( pStudioHdr->pBone( j )->pszName(), pBaseStudioHdr->pBone( k )->pszName() ) == 0)
				{
					break;
				}
			}
			if (k < pBaseStudioHdr->numbones)
			{
				m_group[ group ].masterBone[ j ] = k;
				m_group[ group ].boneMap[ k ] = j;

				// FIXME: these runtime messages don't display in hlmv
				if ((pStudioHdr->pBone( j )->parent == -1) || (pBaseStudioHdr->pBone( k )->parent == -1))
				{
					if ((pStudioHdr->pBone( j )->parent != -1) || (pBaseStudioHdr->pBone( k )->parent != -1))
					{
						Warning( "%s/%s : missmatched parent bones on \"%s\"\n", pBaseStudioHdr->pszName(), pStudioHdr->pszName(), pStudioHdr->pBone( j )->pszName() );
					}
				}
				else if (m_group[ group ].masterBone[ pStudioHdr->pBone( j )->parent ] != m_group[ 0 ].masterBone[ pBaseStudioHdr->pBone( k )->parent ])
				{
					Warning( "%s/%s : missmatched parent bones on \"%s\"\n", pBaseStudioHdr->pszName(), pStudioHdr->pszName(), pStudioHdr->pBone( j )->pszName() );
				}

				// merge the bone use flags and pass up the chain
				int flags = (pStudioHdr->pBone( j )->flags | pBaseStudioHdr->pBone( k )->flags) & BONE_USED_MASK;
				int n = k;
				while (n != -1 && flags != pBaseStudioHdr->pBone( n )->flags)
				{
					((mstudiobone_t *)pBaseStudioHdr->pBone( n ))->flags |= flags;
					n = pBaseStudioHdr->pBone( n )->parent;
				}
			}
			else
			{
				m_group[ group ].masterBone[ j ] = -1;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendAttachments( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int numCheck = std::ssize( m_attachment );

	std::vector< virtualgeneric_t > attachment;
	attachment = m_attachment;


	int j, k, n;

	m_group[ group ].masterAttachment.assign( pStudioHdr->numlocalattachments, {} );

	for (j = 0; j < pStudioHdr->numlocalattachments; j++)
	{

		n = m_group[ group ].masterBone[ pStudioHdr->pLocalAttachment( j )->localbone ];
		
		// skip if the attachments bone doesn't exist in the root model
		if (n == -1)
		{
			continue;
		}
		
		
		char *s1 = pStudioHdr->pLocalAttachment( j )->pszName();
		for (k = 0; k < numCheck; k++)
		{
			char *s2 = m_group[ attachment[k].group ].GetStudioHdr()->pLocalAttachment( attachment[k].index )->pszName();

			if (stricmp( s1, s2 ) == 0)
			{
				break;
			}
		}
		// no duplication
		if (k == numCheck)
		{
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = std::ssize( attachment ); attachment.push_back( tmp );
		}

		m_group[ group ].masterAttachment[ j ] = k;
	}

	m_attachment = attachment;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendPoseParameters( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int numCheck = std::ssize( m_pose );

	std::vector< virtualgeneric_t > pose;
	pose = m_pose;


	int j, k;

	m_group[ group ].masterPose.assign( pStudioHdr->numlocalposeparameters, {} );

	for (j = 0; j < pStudioHdr->numlocalposeparameters; j++)
	{
		char *s1 = pStudioHdr->pLocalPoseParameter( j )->pszName();
		for (k = 0; k < numCheck; k++)
		{
			char *s2 = m_group[ pose[k].group ].GetStudioHdr()->pLocalPoseParameter( pose[k].index )->pszName();

			if (stricmp( s1, s2 ) == 0)
			{
				break;
			}
		}
		if (k == numCheck)
		{
			// no duplication
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = std::ssize( pose ); pose.push_back( tmp );
		}
		else
		{
			// duplicate, reset start and end to fit full dynamic range
			mstudioposeparamdesc_t *pPose1 = pStudioHdr->pLocalPoseParameter( j );
			mstudioposeparamdesc_t *pPose2 = m_group[ pose[k].group ].GetStudioHdr()->pLocalPoseParameter( pose[k].index );
			float start =  std::min( pPose2->end, std::min( pPose1->end, std::min( pPose2->start, pPose1->start ) ) );
			float end =  std::max( pPose2->end, std::max( pPose1->end, std::max( pPose2->start, pPose1->start ) ) );
			pPose2->start = start;
			pPose2->end = end;
		}

		m_group[ group ].masterPose[ j ] = k;
	}

	m_pose = pose;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendNodes( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int numCheck = std::ssize( m_node );

	std::vector< virtualgeneric_t > node;
	node = m_node;


	int j, k;

	m_group[ group ].masterNode.assign( pStudioHdr->numlocalnodes, {} );

	for (j = 0; j < pStudioHdr->numlocalnodes; j++)
	{
		char *s1 = pStudioHdr->pszLocalNodeName( j );
		for (k = 0; k < numCheck; k++)
		{
			char *s2 = m_group[ node[k].group ].GetStudioHdr()->pszLocalNodeName( node[k].index );

			if (stricmp( s1, s2 ) == 0)
			{
				break;
			}
		}
		// no duplication
		if (k == numCheck)
		{
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = std::ssize( node ); node.push_back( tmp );
		}

		m_group[ group ].masterNode[ j ] = k;
	}

	m_node = node;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


void virtualmodel_t::AppendIKLocks( int group, const studiohdr_t *pStudioHdr )
{
	std::scoped_lock lock( m_Lock );
	int numCheck = std::ssize( m_iklock );

	std::vector< virtualgeneric_t > iklock;
	iklock = m_iklock;

	int j, k;

	for (j = 0; j < pStudioHdr->numlocalikautoplaylocks; j++)
	{
		int chain1 = pStudioHdr->pLocalIKAutoplayLock( j )->chain;
		for (k = 0; k < numCheck; k++)
		{
			int chain2 = m_group[ iklock[k].group ].GetStudioHdr()->pLocalIKAutoplayLock( iklock[k].index )->chain;

			if (chain1 == chain2)
			{
				break;
			}
		}
		// no duplication
		if (k == numCheck)
		{

			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = std::ssize( iklock ); iklock.push_back( tmp );
		}
	}

	m_iklock = iklock;
}
