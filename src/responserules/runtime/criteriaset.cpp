//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "rrbase.h"

#include "tier1/map_ops.h"

using namespace ResponseRules;

//-----------------------------------------------------------------------------
// Case-insensitive criteria symbol table
//-----------------------------------------------------------------------------
ks::intern_pool CriteriaSet::sm_CriteriaSymbols( true );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *raw - 
//			*key - 
//			keylen - 
//			*value - 
//			valuelen - 
//			*duration -
// Output : static bool
//-----------------------------------------------------------------------------
const char *SplitContext( const char *raw, char *key, int keylen, char *value, int valuelen, float *duration, const char *entireContext )
{
	char *colon1 = Q_strstr( raw, ":" );
	if ( !colon1 )
	{
		DevMsg( "SplitContext:  warning, ignoring context '%s', missing colon separator!\n", raw );
		*key = *value = 0;
		return nullptr;
	}

	int len = colon1 - raw;
	Q_strncpy( key, raw, std::min( len + 1, keylen ) );
	key[ std::min( len, keylen - 1 ) ] = 0;

	bool last = false;
	char *end = Q_strstr( colon1 + 1, "," );
	if ( !end )
	{
		int remaining = Q_strlen( colon1 + 1 );
		end = colon1 + 1 + remaining;
		last = true;
	}

	char *colon2 = Q_strstr( colon1 + 1, ":" );
	if ( colon2 && ( colon2 < end ) )
	{
		if ( duration )
			*duration = atof( colon2 + 1 );

		char durationStartChar = *(colon2 + 1);
		if ( durationStartChar < '0' || durationStartChar > '9' )
		{
			DevMsg( "SplitContext:  warning, ignoring context '%s', missing comma separator!  Entire context was '%s'.\n", raw, entireContext );
			*key = *value = 0;
			return nullptr;
		}

		len = std::min<std::common_type_t<long int, int>>( colon2 - ( colon1 + 1 ), valuelen - 1 );
		Q_strncpy( value, colon1 + 1, len + 1 );
		value[ len ] = 0;
	}
	else
	{
		if ( duration )
			*duration = 0.0;

		len = std::min<std::common_type_t<long int, int>>( end - ( colon1 + 1 ), valuelen - 1 );
		Q_strncpy( value, colon1 + 1, len + 1 );
		value[ len ] = 0;
	}

	return last ? nullptr : end + 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CriteriaSet::CriteriaSet() : m_bOverrideOnAppend(true), 
	m_nNumPrefixedContexts(0)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CriteriaSet::CriteriaSet( const CriteriaSet& src ) : m_Lookup( src.m_Lookup ), m_nNumPrefixedContexts(src.m_nNumPrefixedContexts)
{
}

CriteriaSet::CriteriaSet( const char *criteria, const char *value ) : m_bOverrideOnAppend(true)
{
	AppendCriteria(criteria,value);
}


//-----------------------------------------------------------------------------
// Computes a symbol for the criteria
//-----------------------------------------------------------------------------
CriteriaSet::CritSymbol_t CriteriaSet::ComputeCriteriaSymbol( const char *criteria )
{
	return sm_CriteriaSymbols.intern( criteria );
}


//-----------------------------------------------------------------------------
// Computes a symbol for the criteria
//-----------------------------------------------------------------------------
void CriteriaSet::AppendCriteria( CriteriaSet::CritSymbol_t criteria, const char *value, float weight )
{
	auto it = LowerBound( criteria );
	int idx = (int)( it - m_Lookup.begin() );
	if ( it == m_Lookup.end() || it->criterianame.data() != criteria.data() )
	{
		CritEntry_t entry;
		entry.criterianame = criteria;
		MEM_ALLOC_CREDIT();
		m_Lookup.insert( it, entry );
		if ( criteria.starts_with( kAPPLYTOWORLDPREFIX ) )
		{
			m_nNumPrefixedContexts += 1;
		}
	}
	else // criteria already existed
	{
		// bail out if override existing criteria is not allowed
		if ( !m_bOverrideOnAppend )
			return; 
	}

	CritEntry_t *entry = &m_Lookup[ idx ];
	entry->SetValue( value );
	entry->weight = weight;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *criteria - 
//			"" - 
//			1.0f - 
//-----------------------------------------------------------------------------
void CriteriaSet::AppendCriteria( const char *pCriteriaName, const char *value /*= ""*/, float weight /*= 1.0f*/ )
{
	AppendCriteria( ComputeCriteriaSymbol( pCriteriaName ), value, weight );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *criteria - 
//			"" - 
//			1.0f - 
//-----------------------------------------------------------------------------
void	CriteriaSet::AppendCriteria( const char *criteria, float value, float weight /*= 1.0f*/ )
{
	char buf[32];
	V_snprintf( buf, 32, "%f", value );
	AppendCriteria( criteria, buf, weight );
}


//-----------------------------------------------------------------------------
// Removes criteria in a set
//-----------------------------------------------------------------------------
void CriteriaSet::RemoveCriteria( const char *criteria )
{
	const int idx = FindCriterionIndex( criteria );
	if ( idx == -1 )
		return;

	if ( criteria[0] == kAPPLYTOWORLDPREFIX )
	{
		Assert( m_nNumPrefixedContexts > 0 );
		m_nNumPrefixedContexts = isel( m_nNumPrefixedContexts - 1, m_nNumPrefixedContexts - 1, 0 );
	}
	RemoveCriteria( idx, false );
}

// bTestForIndex tells us whether the calling function has already checked for a 
// $ prefix and decremented m_nNumPrefixedContexts appropriately (false), 
// or if this function should do that (true).
void CriteriaSet::RemoveCriteria( int idx, bool bTestForPrefix )
{
	Assert( IsValidIndex(idx) );
	if ( bTestForPrefix )
	{
		if ( m_Lookup[idx].criterianame.starts_with( kAPPLYTOWORLDPREFIX ) )
		{
			Assert( m_nNumPrefixedContexts > 0 );
			m_nNumPrefixedContexts = isel( m_nNumPrefixedContexts - 1, m_nNumPrefixedContexts - 1, 0 );
		}
	}
	m_Lookup.erase( m_Lookup.begin() + idx );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CriteriaSet::GetCount() const
{
	return (int)m_Lookup.size();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CriteriaSet::FindCriterionIndex( CritSymbol_t criteria ) const
{
	auto it = LowerBound( criteria );
	return ( it == m_Lookup.end() || it->criterianame.data() != criteria.data() ) ? -1 : (int)( it - m_Lookup.begin() );
}

int CriteriaSet::FindCriterionIndex( const char *name ) const
{
	// A name that was never pooled can't be in any set.
	CritSymbol_t criteria = sm_CriteriaSymbols.find( name );
	return criteria.data() ? FindCriterionIndex( criteria ) : -1;
}


//-----------------------------------------------------------------------------
// Returns the name symbol
//-----------------------------------------------------------------------------
CriteriaSet::CritSymbol_t CriteriaSet::GetNameSymbol( int nIndex ) const
{
	if ( nIndex < 0 || nIndex >= (int)m_Lookup.size() )
		return {};

	const CritEntry_t *entry = &m_Lookup[ nIndex ];
	return entry->criterianame;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CriteriaSet::GetName( int index ) const
{
	if ( index < 0 || index >= (int)m_Lookup.size() )
		return "";
	else
	{
		return ks::nz( m_Lookup[ index ].criterianame.data() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CriteriaSet::GetValue( int index ) const
{
	if ( index < 0 || index >= (int)m_Lookup.size() )
		return "";

	const CritEntry_t *entry = &m_Lookup[ index ];
	return entry->value ? entry->value : "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : float
//-----------------------------------------------------------------------------
float CriteriaSet::GetWeight( int index ) const
{
	if ( index < 0 || index >= (int)m_Lookup.size() )
		return 1.0f;

	const CritEntry_t *entry = &m_Lookup[ index ];
	return entry->weight;
}


//-----------------------------------------------------------------------------
// Purpose: Merge another criteria set into this one.
//-----------------------------------------------------------------------------
void CriteriaSet::Merge( const CriteriaSet * RESTRICT otherCriteria )
{
	Assert(otherCriteria);
	if (!otherCriteria)
		return;

	// for now, just duplicate everything.
	int count = otherCriteria->GetCount();
	EnsureCapacity( count + GetCount() );
	for ( int i = 0 ; i < count ; ++i )
	{
		AppendCriteria( otherCriteria->GetNameSymbol(i), otherCriteria->GetValue(i), otherCriteria->GetWeight(i) );
	}
}

void CriteriaSet::Merge( const char *modifiers ) // add criteria parsed from a text string
{
	// Always include any optional modifiers
	if ( modifiers == nullptr )
		return;

	char copy_modifiers[ 255 ];
	const char *pCopy;
	char key[ 128 ] = { 0 };
	char value[ 128 ] = { 0 };

	Q_strncpy( copy_modifiers, modifiers, sizeof( copy_modifiers ) );
	pCopy = copy_modifiers;

	while( pCopy )
	{
		pCopy = SplitContext( pCopy, key, sizeof( key ), value, sizeof( value ), nullptr, modifiers );

		if( *key && *value )
		{
			AppendCriteria( key, value, 1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CriteriaSet::Describe() const
{
	// build an alphabetized representation of the set for printing
	// Names are unique: the symbol table is case-insensitive.
	std::map<const char *, const CritEntry_t *, ks::cstr_iless> m_TempMap;

	for ( const CritEntry_t &entry : m_Lookup )
	{
		m_TempMap.emplace( ks::nz( entry.criterianame.data() ), &entry );
	}

	for ( auto &[name, entry] : m_TempMap )
	{
		if ( entry->weight != 1.0f )
		{
			DevMsg( "  %20s = '%s' (weight %f)\n", name, entry->value ? entry->value : "", entry->weight );
		}
		else
		{
			DevMsg( "  %20s = '%s'\n", name, entry->value ? entry->value : "" );
		}
	}

	/*
	for ( const CritEntry_t &entry : m_Lookup )
	{
		const char *pCriteriaName = ks::nz( entry.criterianame.data() );
		if ( entry.weight != 1.0f )
		{
			DevMsg( "  %20s = '%s' (weight %f)\n", pCriteriaName, entry.value ? entry.value : "", entry.weight );
		}
		else
		{
			DevMsg( "  %20s = '%s'\n", pCriteriaName, entry.value ? entry.value : "" );
		}
	}
	*/
}


void CriteriaSet::Reset()
{
	m_Lookup.clear();
	m_Lookup.shrink_to_fit();
}

void CriteriaSet::WriteToEntity( CBaseEntity *pEntity )
{
	AssertMsg( false, "CriteriaSet::WriteToEntity has not been ported from l4d2.\n" );
}

int CriteriaSet::InterceptWorldSetContexts( CriteriaSet * RESTRICT pFrom, CriteriaSet * RESTRICT pSetOnWorld )
{
	// Assert( pFrom ); Assert( pTo ); Assert( pSetOnWorld );
	Assert( pSetOnWorld != pFrom );
	Assert( pSetOnWorld->GetCount() == 0 );

	if ( pFrom->m_nNumPrefixedContexts == 0 )
	{
		// nothing needs to be done to it.
		return 0;
	}

#ifdef DEBUG
	// save this off for later error checking.
	const int nPrefixedContexts = pFrom->m_nNumPrefixedContexts;
#endif

	// make enough space for the expected output quantity.
	pSetOnWorld->EnsureCapacity( pFrom->m_nNumPrefixedContexts ); 

	// initialize a buffer with the "world" prefix (so we can use strncpy instead of snprintf and be much faster)
	char buf[80] = { 'w', 'o', 'r', 'l', 'd', '\0' };
	const unsigned int PREFIXLEN = 5; // strlen("world")

	// create a second tree that has the appropriately renamed criteria,
	// then swap it into pFrom
	CriteriaSet rewrite;
	rewrite.EnsureCapacity( pFrom->GetCount() + 1 );

	for ( int i = pFrom->Head(); pFrom->IsValidIndex(i); i = pFrom->Next(i) )
	{
		const char *pszName = pFrom->GetName( i );
		if ( pszName[0] == CriteriaSet::kAPPLYTOWORLDPREFIX )
		{	// redirect to the world contexts
			V_strncpy( buf+PREFIXLEN, pszName+1, sizeof(buf) - PREFIXLEN );
			rewrite.AppendCriteria( buf, pFrom->GetValue(i), pFrom->GetWeight(i) );
			pSetOnWorld->AppendCriteria( pszName+1, pFrom->GetValue(i), pFrom->GetWeight(i) );
			buf[PREFIXLEN] = 0;
		}
		else
		{	// does not need to be fiddled; do not write back to world
			rewrite.AppendCriteria( pFrom->GetNameSymbol(i), pFrom->GetValue(i), pFrom->GetWeight(i) );
		}
	}
	AssertMsg2( pSetOnWorld->GetCount() == nPrefixedContexts, "Count of $ persistent RR contexts is inconsistent (%d vs %d)! Call Elan.",
		pSetOnWorld->GetCount(), nPrefixedContexts	);

	pFrom->m_nNumPrefixedContexts = 0;
	pFrom->m_Lookup.swap(rewrite.m_Lookup);
	return pSetOnWorld->GetCount();
}
