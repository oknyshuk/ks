// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2010 Valve Corporation

// Purpose: Core types for the response rules -- criteria, responses, rules, and matchers.

import ks.vstdlib;
#include "rrbase.h"
import std;
#include "tier2/interval.h"

using namespace ResponseRules;
static void CC_RR_DumpHashInfo( const CCommand &args );

namespace ResponseRules
{
	// ick
	// Wrap string lookup with a hash on the string so that all of the repetitive playerxxx type strings get bucketed out better
	constexpr auto STRING_BUCKETS_COUNT = 64; // Must be power of two
	constexpr auto STRING_BUCKETS_MASK = ( STRING_BUCKETS_COUNT - 1 );

	// Interned copies, never freed: callers keep the pointers.
	std::set< const char *, ks::cstr_less > g_ResponseStrings[ STRING_BUCKETS_COUNT ];

	const char *ResponseCopyString( const char *in )
	{
		if ( !in )
			return nullptr;
		if ( !*in )
			return "";

		int	bucket = ( RR_HASH( in ) & STRING_BUCKETS_MASK );

		std::set< const char *, ks::cstr_less > &list = g_ResponseStrings[ bucket ];

		auto it = list.find( in );
		if ( it != list.end() )
{
			return *it;
		}

		int len = V_strlen( in );
		char *out = new char[ len + 1 ];
		V_memcpy( out, in, len );
		out[ len ] = 0;
		list.insert( out );
		return out;
	}
}

// Installed by the game that hosts the response system.
IEngineEmulator *IEngineEmulator::s_pSingleton = nullptr;

IEngineEmulator *IEngineEmulator::Get()
{
	contract_assert( IEngineEmulator::s_pSingleton && "Response rules fail: no IEngineEmulator" );
	return IEngineEmulator::s_pSingleton;
}


//-----------------------------------------------------------------------------
// Convars 
//-----------------------------------------------------------------------------


ConVar rr_debugresponses( "rr_debugresponses", "0", FCVAR_NONE, "Show verbose matching output (1 for simple, 2 for rule scoring, 3 for noisy). If set to 4, it will only show response success/failure for npc_selected NPCs." );
ConVar rr_debugrule( "rr_debugrule", "", FCVAR_NONE, "If set to the name of the rule, that rule's score will be shown whenever a concept is passed into the response rules system.");
ConVar rr_dumpresponses( "rr_dumpresponses", "0", FCVAR_NONE, "Dump all response_rules.txt and rules (requires restart)" );
ConVar rr_debugresponseconcept( "rr_debugresponseconcept", "", FCVAR_NONE, "If set, rr_debugresponses will print only responses testing for the specified concept" );
constexpr auto RR_DEBUGRESPONSES_SPECIALCASE = 4;



//-----------------------------------------------------------------------------
// Copied from SoundParametersInternal.cpp 
//-----------------------------------------------------------------------------

#define SNDLVL_PREFIX "SNDLVL_"

struct SoundLevelLookup
{
	soundlevel_t	level;
	char const		*name;
};

// NOTE:  Needs to reflect the soundlevel_t enum defined in soundflags.h
static SoundLevelLookup g_pSoundLevels[] =
{
	{ SNDLVL_NONE, "SNDLVL_NONE" },
	{ SNDLVL_20dB, "SNDLVL_20dB" },
	{ SNDLVL_25dB, "SNDLVL_25dB" },
	{ SNDLVL_30dB, "SNDLVL_30dB" },
	{ SNDLVL_35dB, "SNDLVL_35dB" },
	{ SNDLVL_40dB, "SNDLVL_40dB" },
	{ SNDLVL_45dB, "SNDLVL_45dB" },
	{ SNDLVL_50dB, "SNDLVL_50dB" },
	{ SNDLVL_55dB, "SNDLVL_55dB" },
	{ SNDLVL_IDLE, "SNDLVL_IDLE" },
	{ SNDLVL_TALKING, "SNDLVL_TALKING" },
	{ SNDLVL_60dB, "SNDLVL_60dB" },
	{ SNDLVL_65dB, "SNDLVL_65dB" },
	{ SNDLVL_STATIC, "SNDLVL_STATIC" },
	{ SNDLVL_70dB, "SNDLVL_70dB" },
	{ SNDLVL_NORM, "SNDLVL_NORM" },
	{ SNDLVL_75dB, "SNDLVL_75dB" },
	{ SNDLVL_80dB, "SNDLVL_80dB" },
	{ SNDLVL_85dB, "SNDLVL_85dB" },
	{ SNDLVL_90dB, "SNDLVL_90dB" },
	{ SNDLVL_95dB, "SNDLVL_95dB" },
	{ SNDLVL_100dB, "SNDLVL_100dB" },
	{ SNDLVL_105dB, "SNDLVL_105dB" },
	{ SNDLVL_110dB, "SNDLVL_110dB" },
	{ SNDLVL_120dB, "SNDLVL_120dB" },
	{ SNDLVL_130dB, "SNDLVL_130dB" },
	{ SNDLVL_GUNFIRE, "SNDLVL_GUNFIRE" },
	{ SNDLVL_140dB, "SNDLVL_140dB" },
	{ SNDLVL_150dB, "SNDLVL_150dB" },
	{ SNDLVL_180dB, "SNDLVL_180dB" },
};

static soundlevel_t TextToSoundLevel( const char *key )
{
	if ( !key )
	{
		contract_assert( 0 );
		return SNDLVL_NORM;
	}

	int c = std::size( g_pSoundLevels );

	int i;

		for ( i = 0; i < c; i++ )
	{
		SoundLevelLookup *entry = &g_pSoundLevels[ i ];
		if ( !V_strcasecmp( key, entry->name ) )
			return entry->level;
	}

	if ( !V_strnicmp( key, SNDLVL_PREFIX, V_strlen( SNDLVL_PREFIX ) ) )
	{
		char const *val = key + V_strlen( SNDLVL_PREFIX );
		int sndlvl = std::atoi( val );
		if ( sndlvl > 0 && sndlvl <= 180 )
		{
			return ( soundlevel_t )sndlvl;
		}
	}

	DevMsg( "CSoundEmitterSystem:  Unknown sound level %s\n", key );

	return SNDLVL_NORM;
}

CResponseSystem::ExcludeList_t CResponseSystem::m_DebugExcludeList;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CResponseSystem::CResponseSystem()
{
	token[0] = 0;
	m_bUnget = false;
	m_bCustomManagable = false;

	BuildDispatchTables();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CResponseSystem::~CResponseSystem()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
void CResponseSystem::GetCurrentScript( char *buf, std::size_t buflen )
{
	contract_assert( buf );
	buf[ 0 ] = 0;
	if ( std::ssize( m_ScriptStack ) <= 0 )
		return;

	if ( IEngineEmulator::Get()->GetFilesystem()->String( m_ScriptStack[ 0 ].name, buf, buflen ) )
	{
		return;
	}
	buf[ 0 ] = 0;
}

void CResponseSystem::PushScript( const char *scriptfile, unsigned char *buffer )
{
	ScriptEntry e;
	e.name = IEngineEmulator::Get()->GetFilesystem()->FindOrAddFileName( scriptfile );
	e.buffer = buffer;
	e.currenttoken = (char *)e.buffer;
	e.tokencount = 0;
	m_ScriptStack.insert( m_ScriptStack.begin(), e );
}

void CResponseSystem::PopScript(void)
{
	contract_assert( std::ssize( m_ScriptStack ) >= 1 );
	if ( std::ssize( m_ScriptStack ) <= 0 )
		return;

	m_ScriptStack.erase( m_ScriptStack.begin() + 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResponseSystem::Clear()
{
	// Rules first: they refer to the criteria and responses.
	m_RulePartitions.RemoveAll();
	m_Responses.clear();
	m_Criteria.clear();
	m_Enumerations.clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			found - 
// Output : float
//-----------------------------------------------------------------------------
float CResponseSystem::LookupEnumeration( const char *name, bool& found )
{
	Enumeration *pEnumeration = ks::find_ptr( m_Enumerations, name );
	if ( !pEnumeration )
	{
		found = false;
		return 0.0f;
	}


	found = true;
	return pEnumeration->value;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : matcher - 
//-----------------------------------------------------------------------------
void CResponseSystem::ResolveToken( Matcher& matcher, char *token, std::size_t bufsize, char const *rawtoken )
{
	if ( rawtoken[0] != '[' )
	{
		V_strncpy( token, rawtoken, bufsize );
		return;
	}

	// Now lookup enumeration
	bool found = false;
	float f = LookupEnumeration( rawtoken, found );
	if ( !found )
	{
		V_strncpy( token, rawtoken, bufsize );
		ResponseWarning( "No such enumeration '%s'\n", token );
		return;
	}

	V_snprintf( token, bufsize, "%f", f );
}


static bool AppearsToBeANumber( char const *token )
{
	if ( std::atof( token ) != 0.0f )
		return true;

	char const *p = token;
	while ( *p )
	{
		if ( *p != '0' )
			return false;

		p++;
	}

	return true;
}

void CResponseSystem::ComputeMatcher( Criteria *c, Matcher& matcher )
{
	const char *s = c->value;
	if ( !s )
	{
		matcher.valid = false;
		return;
	}

	const char *in = s;

	char token[ 128 ];
	char rawtoken[ 128 ];

	token[ 0 ] = 0;
	rawtoken[ 0 ] = 0;

	int n = 0;

	bool gt = false;
	bool lt = false;
	bool eq = false;
	bool nt = false;

	bool done = false;
	while ( !done )
	{
		switch( *in )
		{
		case '>':
			{
				gt = true;
				contract_assert( !lt ); // Can't be both
			}
			break;
		case '<':
			{
				lt = true;
				contract_assert( !gt );  // Can't be both
			}
			break;
		case '=':
			{
				eq = true;
			}
			break;
		case ',':
		case '\0':
			{
				rawtoken[ n ] = 0;
				n = 0;

				// Convert raw token to real token in case token is an enumerated type specifier
				ResolveToken( matcher, token, sizeof( token ), rawtoken );

				// Fill in first data set
				if ( gt )
				{
					matcher.usemin = true;
					matcher.minequals = eq;
					matcher.minval = (float)std::atof( token );

					matcher.isnumeric = true;
				}
				else if ( lt )
				{
					matcher.usemax = true;
					matcher.maxequals = eq;
					matcher.maxval = (float)std::atof( token );

					matcher.isnumeric = true;
				}
				else
				{
					if ( *in == ',' )
					{
						// If there's a comma, this better have been a less than or a gt key
						contract_assert( 0 );
					}

					matcher.notequal = nt;

					matcher.isnumeric = AppearsToBeANumber( token );
				}

				gt = lt = eq = nt = false;

				if ( !(*in) )
				{
					done = true;
				}
			}
			break;
		case '!':
			nt = true;
			break;
		default:
			rawtoken[ n++ ] = *in;
			break;
		}

		in++;
	}

	matcher.SetToken( token );
	matcher.SetRaw( rawtoken );
	matcher.valid = true;
}

bool CResponseSystem::CompareUsingMatcher( const char *setValue, Matcher& m, bool verbose /*=false*/ )
{
	if ( !m.valid )
		return false;

	float v = (float)std::atof( setValue );
	if ( setValue[0] == '[' )
	{
		bool found = false;
		v = LookupEnumeration( setValue, found );
	}

	int minmaxcount = 0;

	if ( m.usemin )
	{
		if ( m.minequals )
		{
			if ( v < m.minval )
				return false;
		}
		else
		{
			if ( v <= m.minval )
				return false;
		}

		++minmaxcount;
	}

	if ( m.usemax )
	{
		if ( m.maxequals )
		{
			if ( v > m.maxval )
				return false;
		}
		else
		{
			if ( v >= m.maxval )
				return false;
		}

		++minmaxcount;
	}

	// Had one or both criteria and met them
	if ( minmaxcount >= 1 )
	{
		return true;
	}

	if ( m.notequal )
	{
		if ( m.isnumeric )
		{
			if ( v == (float)std::atof( m.GetToken() ) )
				return false;
		}
		else
		{
			if ( !V_stricmp( setValue, m.GetToken() ) )
				return false;
		}

		return true;
	}

	if ( m.isnumeric )
	{
		// If the setValue is "", the NPC doesn't have the key at all,
		// in which case we shouldn't match "0".
		if ( !setValue || !setValue[0] )
			return false;

		return v == (float)std::atof( m.GetToken() );
	}

	return !V_stricmp( setValue, m.GetToken() ) ? true : false;
}

bool CResponseSystem::Compare( const char *setValue, Criteria *c, bool verbose /*= false*/ )
{
	contract_assert( c );
	contract_assert( setValue );

	bool bret = CompareUsingMatcher( setValue, c->matcher, verbose );

	if ( verbose )
	{
		DevMsg( "'%20s' vs. '%20s' = ", setValue, c->value );

		{
			//DevMsg( "\n" );
			//m.Describe();
		}
	}
	return bret;
}

float CResponseSystem::RecursiveScoreSubcriteriaAgainstRule( const CriteriaSet& set, Criteria *parent, bool& exclude, bool verbose /*=false*/ )
{
	float score = 0.0f;
	for ( CriteriaEntry_t *pCriterion : parent->subcriteria )
	{
		bool excludesubrule = false;
		if (verbose)
		{
			DevMsg( "\n" );
		}
		score += ScoreCriteriaAgainstRuleCriteria( set, pCriterion, excludesubrule, verbose );
	}

	exclude = ( parent->required && score == 0.0f ) ? true : false;

	return score * parent->weight.GetFloat();
}

float CResponseSystem::RecursiveLookForCriteria( const CriteriaSet &criteriaSet, Criteria *pParent )
{
	float flScore = 0.0f;
	for ( CriteriaEntry_t *pCriteria : pParent->subcriteria )
	{
		flScore += LookForCriteria( criteriaSet, pCriteria );
	}

	return flScore;
}

float CResponseSystem::LookForCriteria( const CriteriaSet &criteriaSet, CriteriaEntry_t *pCriteriaEntry )
{
	Criteria *pCriteria = &pCriteriaEntry->second;
	if ( pCriteria->IsSubCriteriaType() )
	{
		return RecursiveLookForCriteria( criteriaSet, pCriteria );
	}

	int iIndex = criteriaSet.FindCriterionIndex( pCriteria->nameSym );
	if ( iIndex == -1 )
		return 0.0f;

	contract_assert( criteriaSet.GetValue( iIndex ) );
	if ( V_stricmp( criteriaSet.GetValue( iIndex ), pCriteria->value ) )
		return 0.0f;

	return 1.0f;
}

float CResponseSystem::ScoreCriteriaAgainstRuleCriteria( const CriteriaSet& set, CriteriaEntry_t *pCriterion, bool& exclude, bool verbose /*=false*/ )
{
	Criteria *c = &pCriterion->second;

	if ( c->IsSubCriteriaType() )
	{
		return RecursiveScoreSubcriteriaAgainstRule( set, c, exclude, verbose );
	}

	if ( verbose )
	{
		DevMsg( "  criterion '%25s':'%15s' ", pCriterion->first.c_str(), CriteriaSet::SymbolToStr(c->nameSym) );
	}

	exclude = false;

	float score = 0.0f;

	const char *actualValue = "";

	/*
	const char * RESTRICT critname = c->name;
	const char * nameDoubleCheck = CriteriaSet::SymbolToStr( CriteriaSet::ComputeCriteriaSymbol( critname ) );
	*/
	int found = set.FindCriterionIndex( c->nameSym );
	if ( found != -1 )
	{
		actualValue = set.GetValue( found );
		if ( !actualValue )
		{
			contract_assert( 0 );
			return score;
		}
	}

	contract_assert( actualValue );

	if ( Compare( actualValue, c, verbose ) )
	{
		float w = set.GetWeight( found );
		score = w * c->weight.GetFloat();

		if ( verbose )
		{
			DevMsg( "matched, weight %4.2f (s %4.2f x c %4.2f)",
				score, w, c->weight.GetFloat() );
		}
	}
	else
	{
		if ( c->required )
		{
			exclude = true;
			if ( verbose )
			{
				DevMsg( "failed (+exclude rule)" );
			}
		}
		else
		{
			if ( verbose )
			{
				DevMsg( "failed" );
			}
		}
	}

	return score;
}

float CResponseSystem::ScoreCriteriaAgainstRule( const CriteriaSet& set, ResponseRulePartition::tRuleDict &dict, int irule, bool verbose /*=false*/ )
{
	Rule * __restrict__ rule = dict[ irule ].get();
	float score = 0.0f;

	bool bBeingWatched = false;

	// See if we're trying to debug this rule
	const char *pszText = rr_debugrule.GetString();
	if ( pszText && pszText[0] && !V_stricmp( pszText, dict.GetElementName( irule ) ) )
	{
		bBeingWatched = true;
	}

	if ( !rule->IsEnabled() )
	{
		if ( bBeingWatched )
		{
			DevMsg("Rule is disabled.\n" );
		}
		return 0.0f;
	}

	if ( bBeingWatched )
	{
		verbose = true;
	}

	if ( verbose )
	{
		DevMsg( "Scoring rule '%s' (%i)\n{\n", dict.GetElementName( irule ), irule+1 );
	}

	// Iterate set criteria
	for ( CriteriaEntry_t *pCriterion : rule->m_Criteria )
	{
		bool exclude = false;
		score += ScoreCriteriaAgainstRuleCriteria( set, pCriterion, exclude, verbose );

		if ( verbose )
		{
			DevMsg( ", score %4.2f\n", score );
		}

		if ( exclude ) 
		{
			score = 0.0f;
			break;
		}
	}

	if ( verbose )
	{
		DevMsg( "}\n" );
	}

	if ( rule->m_nForceWeight > 0 )
	{	// this means override the cumulative weight of criteria and just force the rule's total score,
		// assuming it matched at all.
		return fsel( score - std::numeric_limits<float>::min(), rule->m_nForceWeight, 0 );
	}
	else
	{
	return score;
}
}

void CResponseSystem::DebugPrint( int depth, const char *fmt, ... )
{
	ks::ScratchArena<> scratch;
	int indentchars = 3 * depth;
	char *indent = (char *) scratch.AllocBytes( indentchars + 1);
	indent[ indentchars ] = 0;
	while ( --indentchars >= 0 )
	{
		indent[ indentchars ] = ' ';
	}

	// Dump text to debugging console.
	std::va_list argptr;
	char szText[1024];

	__builtin_va_start (argptr, fmt);
	V_vsnprintf (szText, sizeof( szText ), fmt, argptr);
	__builtin_va_end (argptr);

	DevMsg( "%s%s", indent, szText );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResponseSystem::ResetResponseGroups()
{
	for ( auto &[name, group] : m_Responses )
	{
		group.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make certain responses unavailable by marking them as depleted 
//-----------------------------------------------------------------------------
void CResponseSystem::FakeDepletes( ResponseGroup *g, IResponseFilter *pFilter )
{
	m_FakedDepletes.clear();

	// Fake depletion of unavailable choices
	int c = std::ssize( g->group );
	if ( pFilter && g->ShouldCheckRepeats() )
	{
		for ( int i = 0; i < c; i++ )
		{
			ParserResponse *r = &g->group[ i ];
			if ( r->depletioncount != g->GetDepletionCount() && !pFilter->IsValidResponse( r->GetType(), r->value ) )
			{
				m_FakedDepletes.push_back( i );
				g->MarkResponseUsed( i );
			}
		}
	}

	// Fake depletion of choices that fail the odds check
	for ( int i = 0; i < c; i++ )
	{
		ParserResponse *r = &g->group[ i ];
		if ( RandomInt( 1, 100 ) > r->params.odds )
		{
			m_FakedDepletes.push_back( i );
			g->MarkResponseUsed( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Restore responses that were faked as being depleted 
//-----------------------------------------------------------------------------
void CResponseSystem::RevertFakedDepletes( ResponseGroup *g )
{
	for ( int i = 0; i < std::ssize( m_FakedDepletes ); i++ )
	{
		g->group[ m_FakedDepletes[ i ] ].depletioncount = 0;
	}
	m_FakedDepletes.clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *g - 
// Output : int
//-----------------------------------------------------------------------------
int CResponseSystem::SelectWeightedResponseFromResponseGroup( ResponseGroup *g, IResponseFilter *pFilter )
{
	int c = std::ssize( g->group );
	if ( !c )
	{
		contract_assert( !"Expecting response group with >= 1 elements" );
		return -1;
	}

	FakeDepletes( g, pFilter );

	if ( !g->HasUndepletedChoices() )
	{
		g->ResetDepletionCount();

		FakeDepletes( g, pFilter );

		if ( !g->HasUndepletedChoices() )
			return -1;

		// Disable the group if we looped through all the way
		if ( g->IsNoRepeat() )
		{
			g->SetEnabled( false );
			return -1;
		}
	}

	bool checkrepeats = g->ShouldCheckRepeats();
	int	depletioncount = g->GetDepletionCount();

	float totalweight = 0.0f;
	int slot = -1;

	if ( checkrepeats )
	{
		int check= -1;
		// Snag the first slot right away
		if ( g->HasUndepletedFirst( check ) && check != -1 )
		{
			slot = check;
		}

		if ( slot == -1 && g->HasUndepletedLast( check ) && check != -1 )
		{
			// If this is the only undepleted one, use it now
			int i;
			for ( i = 0; i < c; i++ )
			{
				ParserResponse *r = &g->group[ i ];
				if ( checkrepeats && 
					( r->depletioncount == depletioncount ) )
				{
					continue;
				}		

				if ( r->last )
				{
					contract_assert( i == check );
					continue;
				}

				// There's still another undepleted entry
				break;
			}

			// No more undepleted so use the r->last slot 
			if ( i >= c )
			{
				slot = check;
			}
		}
	}

	if ( slot == -1 )
	{
		for ( int i = 0; i < c; i++ )
		{
			ParserResponse *r = &g->group[ i ];
			if ( checkrepeats && 
				( r->depletioncount == depletioncount ) )
			{
				continue;
			}

			// Always skip last entry here since we will deal with it above
			if ( checkrepeats && r->last )
				continue;

			int prevSlot = slot;

			if ( !totalweight )
			{
				slot = i;
			}

			// Always assume very first slot will match
			totalweight += r->weight.GetFloat();
			if ( !totalweight || IEngineEmulator::Get()->GetRandomStream()->RandomFloat(0,totalweight) < r->weight.GetFloat() )
			{
				slot = i;
			}

			if ( !checkrepeats && slot != prevSlot && pFilter && !pFilter->IsValidResponse( r->GetType(), r->value ) )
			{
				slot = prevSlot;
				totalweight -= r->weight.GetFloat();
			}
		}
	}

	if ( slot != -1 )
		g->MarkResponseUsed( slot );

	// Revert fake depletion of unavailable choices
	RevertFakedDepletes( g );

	return slot;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : searchResult - 
//			depth - 
//			*name - 
//			verbose - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CResponseSystem::ResolveResponse( ResponseSearchResult& searchResult, int depth, const char *name, bool verbose /*= false*/, IResponseFilter *pFilter )
{
	auto itGroup = m_Responses.find( name );
	if ( itGroup == m_Responses.end() )
		return false;

	ResponseGroup *g = &itGroup->second;
	// Group has been disabled
	if ( !g->IsEnabled() )
		return false;

	int c = std::ssize( g->group );
	if ( !c )
		return false;

	int idx = 0;

	if ( g->IsSequential() )
	{
		// See if next index is valid
		int initialIndex = g->GetCurrentIndex();
		bool bFoundValid = false;

		do 
		{
			idx = g->GetCurrentIndex();
			g->SetCurrentIndex( idx + 1 );
			if ( idx >= c )
			{
				if ( g->IsNoRepeat() )
				{
					g->SetEnabled( false );
					return false;
				}
				idx = 0;
				g->SetCurrentIndex( 0 );
			}

			if ( !pFilter || pFilter->IsValidResponse( g->group[idx].GetType(), g->group[idx].value ) )
			{
				bFoundValid = true;
				break;
			}

		} while ( g->GetCurrentIndex() != initialIndex );

		if ( !bFoundValid )
			return false;
	}
	else
	{
		idx = SelectWeightedResponseFromResponseGroup( g, pFilter );
		if ( idx < 0 )
			return false;
	}

	if ( verbose )
	{
		DebugPrint( depth, "%s\n", itGroup->first.c_str() );
		DebugPrint( depth, "{\n" );
		DescribeResponseGroup( g, idx, depth );
	}

	bool bret = true;

	ParserResponse *result = &g->group[ idx ];
	if ( result->type == RESPONSE_RESPONSE )
	{
		// Recurse
		bret = ResolveResponse( searchResult, depth + 1, result->value, verbose, pFilter );
	}
	else
	{
		searchResult.action = result;
		searchResult.group	= g;
	}

	if( verbose )
	{
		DebugPrint( depth, "}\n" );
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *group - 
//			selected - 
//			depth - 
//-----------------------------------------------------------------------------
void CResponseSystem::DescribeResponseGroup( ResponseGroup *group, int selected, int depth )
{
	int c = std::ssize( group->group );

	for ( int i = 0; i < c ; i++ )
	{
		ParserResponse *r = &group->group[ i ];
		DebugPrint( depth + 1, "%s%20s : %40s %5.3f\n",
			i == selected ? "-> " : "   ",
			CRR_Response::DescribeResponse( r->GetType() ),
			r->value,
			r->weight.GetFloat() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *rule - 
// Output : CResponseSystem::Response
//-----------------------------------------------------------------------------
bool CResponseSystem::GetBestResponse( ResponseSearchResult& searchResult, Rule *rule, bool verbose /*=false*/, IResponseFilter *pFilter )
{
	int c = std::ssize( rule->m_Responses );
	if ( !c )
		return false;

	int index = IEngineEmulator::Get()->GetRandomStream()->RandomInt( 0, c - 1 );
	ResponseGroupEntry_t *pGroup = rule->m_Responses[ index ];

	ResponseGroup *g = &pGroup->second;

	// Group has been disabled
	if ( !g->IsEnabled() )
		return false;

	int count = std::ssize( g->group );
	if ( !count )
		return false;

	int responseIndex = 0;

	if ( g->IsSequential() )
	{
		// See if next index is valid
		int initialIndex = g->GetCurrentIndex();
		bool bFoundValid = false;

		do 
		{
			responseIndex = g->GetCurrentIndex();
			g->SetCurrentIndex( responseIndex + 1 );
			if ( responseIndex >= count )
			{
				if ( g->IsNoRepeat() )
				{
					g->SetEnabled( false );
					return false;
				}
				responseIndex = 0;
				g->SetCurrentIndex( 0 );
			}

			if ( !pFilter || pFilter->IsValidResponse( g->group[responseIndex].GetType(), g->group[responseIndex].value ) )
			{
				bFoundValid = true;
				break;
			}

		} while ( g->GetCurrentIndex() != initialIndex );

		if ( !bFoundValid )
			return false;
	}
	else
	{
		responseIndex = SelectWeightedResponseFromResponseGroup( g, pFilter );
		if ( responseIndex < 0 )
			return false;
	}


	ParserResponse *r = &g->group[ responseIndex ];

	int depth = 0;

	if ( verbose )
	{
		DebugPrint( depth, "%s\n", pGroup->first.c_str() );
		DebugPrint( depth, "{\n" );

		DescribeResponseGroup( g, responseIndex, depth );
	}

	bool bret = true;

	if ( r->type == RESPONSE_RESPONSE )
	{
		bret = ResolveResponse( searchResult, depth + 1, r->value, verbose, pFilter );
	}
	else
	{
		searchResult.action = r;
		searchResult.group	= g;
	}

	if ( verbose )
	{
		DebugPrint( depth, "}\n" );
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : set - 
//			verbose - 
// Output : int
// Warning: If you change this, be sure to also change 
//          ResponseSystemImplementationCLI::FindAllRulesMatchingCriteria().
//-----------------------------------------------------------------------------
ResponseRulePartition::tIndex CResponseSystem::FindBestMatchingRule( const CriteriaSet& set, bool verbose, float &scoreOfBestMatchingRule )
{
	std::vector< ResponseRulePartition::tIndex > bestrules; bestrules.reserve( 4 );
	float bestscore = 0.001f;
	scoreOfBestMatchingRule = 0;

	std::inplace_vector< ResponseRulePartition::tRuleDict *, 2 > buckets;
	m_RulePartitions.GetDictsForCriteria( &buckets, set );
	for ( int b = 0 ; b < std::ssize( buckets ) ; ++b )
	{
		ResponseRulePartition::tRuleDict *prules = buckets[b];
		int c = prules->Count();
	int i;
	for ( i = 0; i < c; i++ )
	{
			float score = ScoreCriteriaAgainstRule( set, *prules, i, verbose );
		// Check equals so that we keep track of all matching rules
		if ( score >= bestscore )
		{
			// Reset bucket
			if( score != bestscore )
			{
				bestscore = score;
				bestrules.clear();
			}

			// Add to bucket
				bestrules.push_back( m_RulePartitions.IndexFromDictElem( prules, i ) );
			}
		}
	}

	int bestCount = std::ssize( bestrules );
	if ( bestCount <= 0 )
		return m_RulePartitions.InvalidIdx();

	scoreOfBestMatchingRule = bestscore ;
	if ( bestCount == 1 )
	{
		return bestrules[ 0 ] ;
	}
	else
	{
		// Randomly pick one of the tied matching rules
		int idx = IEngineEmulator::Get()->GetRandomStream()->RandomInt( 0, bestCount - 1 );
		if ( verbose )
		{
			DevMsg( "Found %i matching rules, selecting slot %i\n", bestCount, idx );
		}
		return bestrules[ idx ] ;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : set - 
// Output : CRR_Response
//-----------------------------------------------------------------------------
bool CResponseSystem::FindBestResponse( const CriteriaSet& set, CRR_Response& response, IResponseFilter *pFilter )
{
	bool valid = false;

	int iDbgResponse = rr_debugresponses.GetInt();
	bool showRules = ( iDbgResponse >= 2 && iDbgResponse < RR_DEBUGRESPONSES_SPECIALCASE );
	bool showResult = ( iDbgResponse >= 1 && iDbgResponse < RR_DEBUGRESPONSES_SPECIALCASE );

	// Look for match. verbose mode used to be at level 2, but disabled because the writers don't actually care for that info.
	float scoreOfBestRule;
	ResponseRulePartition::tIndex bestRule = FindBestMatchingRule( set, 
		( iDbgResponse >= 3 && iDbgResponse < RR_DEBUGRESPONSES_SPECIALCASE ), 
		scoreOfBestRule ); 

	ResponseType_t responseType = RESPONSE_NONE;
	ResponseParams rp;

	char ruleName[ 128 ];
	char responseName[ 128 ];
	const char *context;
	bool bcontexttoworld;
	ruleName[ 0 ] = 0;
	responseName[ 0 ] = 0;
	context = nullptr;
	bcontexttoworld = false;
	if ( m_RulePartitions.IsValid( bestRule ) )
	{
		Rule * __restrict__ r = &m_RulePartitions[ bestRule ];

		ResponseSearchResult result;
		if ( GetBestResponse( result, r, showResult, pFilter ) )
		{
			V_strncpy( responseName, result.action->value, sizeof( responseName ) );
			responseType = result.action->GetType();
			rp = result.action->params;
			rp.m_pFollowup = &result.action->m_followup;
		}

		V_strncpy( ruleName, m_RulePartitions.GetElementName( bestRule ), sizeof( ruleName ) );

		// Disable the rule if it only allows for matching one time
		if ( r->IsMatchOnce() )
		{
			r->Disable();
		}
		context = r->GetContext();
		bcontexttoworld = r->IsApplyContextToWorld();

		response.SetMatchScore(scoreOfBestRule);
		valid = true;
	}

	response.Init( responseType, responseName, rp, ruleName, context, bcontexttoworld );

	if ( showResult )
	{
		/*
		// clipped -- chet doesn't really want this info
		if ( valid )
		{
		// Rescore the winner and dump to console
		ScoreCriteriaAgainstRule( set, bestRule, true );
		}
		*/


		if ( valid || showRules )
		{
			const char *pConceptFilter = rr_debugresponseconcept.GetString();
			// Describe the response, too
			if ( V_strlen(pConceptFilter) > 0 && !rr_debugresponseconcept.GetBool() )
			{	// filter for only one concept
				if ( V_stricmp(pConceptFilter, set.GetValue(set.FindCriterionIndex("concept")) ) == 0 )
				{
					response.Describe(&set);
				} // else don't print
			}
			else
			{
				// maybe we need to filter *out* some concepts
				if ( !m_DebugExcludeList.empty() )
				{
					// we are excluding at least one concept
					CRR_Concept test( set.GetValue(set.FindCriterionIndex("concept")) );
					if ( std::find( m_DebugExcludeList.begin(), m_DebugExcludeList.end(), test ) == m_DebugExcludeList.end() )
					{	// if not found in exclude list, then print
						response.Describe(&set);
					}
				}
				else
				{
					// describe everything
				response.Describe(&set);
			}
		}
	}
	}

	return valid;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CResponseSystem::GetAllResponses( std::vector< CRR_Response > *pResponses )
{
	for ( auto &[name, group] : m_Responses )
	{

		for ( int j = 0; j < std::ssize( group.group ); j++)
		{
			ParserResponse &response = group.group[j];
			if ( response.type != RESPONSE_RESPONSE )
			{
				/*
				CRR_Response *pResponse = new CRR_Response;
				pResponse->Init( response.GetType(), response.value, CriteriaSet(), response.params, NULL, NULL, false );
				pResponses->AddToTail(pResponse);
				*/
				pResponses->emplace_back().Init( response.GetType(), response.value, response.params, nullptr, nullptr, false );
			}
		}
	}
}

void CResponseSystem::ParseInclude()
{
	char includefile[ 256 ];
	ParseToken();
	V_snprintf( includefile, sizeof( includefile ), "scripts/%s", token );

	// check if the file is already included
	if ( m_IncludedFiles.contains( includefile ) )
	{
		return;
	}


	// Try and load it
	auto file = IEngineEmulator::Get()->GetFilesystem()->ReadFile( includefile, "GAME" );
	if ( !file )
	{
		DevMsg( "Unable to load #included script %s\n", includefile );
		return;
	}

	const std::string text( ks::text_view( *file ) );
	LoadFromBuffer( includefile, text.c_str() );
}

void CResponseSystem::LoadFromBuffer( const char *scriptfile, const char *buffer )
{
	COM_TimestampedLog( "CResponseSystem::LoadFromBuffer [%s] - Start", scriptfile );
	m_IncludedFiles.insert( scriptfile );
	PushScript( scriptfile, (unsigned char * )buffer );

	if( rr_dumpresponses.GetBool() )
	{
		DevMsg("Reading: %s\n", scriptfile );
	}

	while ( 1 )
	{
		ParseToken();
		if ( !token[0] )
		{
			break;
		}

		unsigned int hash = RR_HASH( token );
		bool bSuccess = Dispatch( token, hash, m_FileDispatch );
		if ( !bSuccess )
		{
			int byteoffset = m_ScriptStack[ 0 ].currenttoken - (const char *)m_ScriptStack[ 0 ].buffer;

			Error( "CResponseSystem::LoadFromBuffer:  Unknown entry type '%s', expecting 'response', 'criterion', 'enumeration' or 'rules' in file %s(offset:%i)\n", 
				token, scriptfile, byteoffset );
			break;
		}
	}

	if ( std::ssize( m_ScriptStack ) == 1 )
	{
		char cur[ 256 ];
		GetCurrentScript( cur, sizeof( cur ) );
		DevMsg( 1, "CResponseSystem:  %s (%i rules, %i criteria, and %i responses)\n",
			cur, m_RulePartitions.Count(), (int)m_Criteria.size(), (int)m_Responses.size() );

		if( rr_dumpresponses.GetBool() )
		{
			DumpRules();
		}
	}

	PopScript();
	COM_TimestampedLog( "CResponseSystem::LoadFromBuffer [%s] - Finish", scriptfile );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResponseSystem::LoadRuleSet( const char *basescript )
{
	float flStart = Plat_FloatTime();
	int length = 0;
	unsigned char *buffer = (unsigned char *)IEngineEmulator::Get()->LoadFileForMe( basescript, &length );
	if ( length <= 0 || !buffer )
	{
		DevMsg( 1, "CResponseSystem:  failed to load %s\n", basescript );
		return;
	}

	m_IncludedFiles.clear();
	LoadFromBuffer( basescript, (const char *)buffer );

	IEngineEmulator::Get()->FreeFile( buffer );

	contract_assert( std::ssize( m_ScriptStack ) == 0 );
	float flEnd = Plat_FloatTime();
	COM_TimestampedLog( "CResponseSystem::LoadRuleSet took %f msec", 1000.0f * ( flEnd - flStart ) );
}

inline ResponseType_t ComputeResponseType( const char *s )
{
	switch ( s[ 0 ] )
	{
	default:
		break;
	case 's':
		switch ( s[ 1 ] )
		{
		default:
			break;
		case 'c':
		return RESPONSE_SCENE;
		case 'e':
		return RESPONSE_SENTENCE;
		case 'p':
		return RESPONSE_SPEAK;
	}
		break;
	case 'r':
		return RESPONSE_RESPONSE;
	case 'p':
		return RESPONSE_PRINT;
	case 'e':
		return RESPONSE_ENTITYIO;
	}

	return RESPONSE_NONE;
}

void CResponseSystem::ParseResponse_Weight( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	newResponse.weight.SetFloat( (float)std::atof( token ) );
}

void CResponseSystem::ParseResponse_PreDelay( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_DELAYBEFORESPEAK;
	rp->predelay.FromInterval( ReadInterval( token ) );
}

void CResponseSystem::ParseResponse_NoDelay( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_DELAYAFTERSPEAK;
	rp->delay.start = 0;
	rp->delay.range = 0;
}

void CResponseSystem::ParseResponse_DefaultDelay( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	rp->flags |= AI_ResponseParams::RG_DELAYAFTERSPEAK;
	rp->delay.start = AIS_DEF_MIN_DELAY;
	rp->delay.range = ( AIS_DEF_MAX_DELAY - AIS_DEF_MIN_DELAY );
}

void CResponseSystem::ParseResponse_Delay( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_DELAYAFTERSPEAK;
	rp->delay.FromInterval( ReadInterval( token ) );
}

void CResponseSystem::ParseResponse_SpeakOnce( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	rp->flags |= AI_ResponseParams::RG_SPEAKONCE;
}

void CResponseSystem::ParseResponse_NoScene( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	rp->flags |= AI_ResponseParams::RG_DONT_USE_SCENE;
}

void CResponseSystem::ParseResponse_StopOnNonIdle( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	rp->flags |= AI_ResponseParams::RG_STOP_ON_NONIDLE;
}

void CResponseSystem::ParseResponse_Odds( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_ODDS;
	rp->odds = clamp( std::atoi( token ), 0, 100 );
}

void CResponseSystem::ParseResponse_RespeakDelay( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_RESPEAKDELAY;
	rp->respeakdelay.FromInterval( ReadInterval( token ) );
}

void CResponseSystem::ParseResponse_WeaponDelay( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_WEAPONDELAY;
	rp->weapondelay.FromInterval( ReadInterval( token ) );
}

void CResponseSystem::ParseResponse_Soundlevel( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	ParseToken();
	rp->flags |= AI_ResponseParams::RG_SOUNDLEVEL;
	rp->soundlevel = (soundlevel_t)TextToSoundLevel( token );
}

void CResponseSystem::ParseResponse_DisplayFirst( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	newResponse.first = true;
	group.m_bHasFirst = true;
}

void CResponseSystem::ParseResponse_DisplayLast( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	newResponse.last = true;
	group.m_bHasLast= true;
}

void CResponseSystem::ParseResponse_Fire( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	// get target name
	bool bSuc = ParseToken();
	if (!bSuc)
	{
		ResponseWarning( "FIRE token in response needs exactly three parameters." );
		return;
	}
	newResponse.m_followup.followup_entityiotarget = ResponseCopyString(token);

	bSuc = ParseToken();
	if (!bSuc)
	{
		ResponseWarning( "FIRE token in response needs exactly three parameters." );
		return;
	}
	newResponse.m_followup.followup_entityioinput = ResponseCopyString(token);

	bSuc = ParseToken();
	if (!bSuc)
	{
		ResponseWarning( "FIRE token in response needs exactly three parameters." );
		return;
	}
	newResponse.m_followup.followup_entityiodelay = std::atof( token );
	/*
	m_followup.followup_entityioinput = ResponseCopyString(src.m_followup.followup_entityioinput);
	m_followup.followup_entityiotarget = ResponseCopyString(src.m_followup.followup_entityiotarget);
	*/
}

void CResponseSystem::ParseResponse_Then( ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	// eg, "subject TALK_ANSWER saidunplant:1 3"
	bool bSuc = ParseToken();
	if (!bSuc)
	{
		contract_assert( false && "THEN token in response lacked any further info.\n" );
		ResponseWarning( "THEN token in response lacked any further info.\n" );
		return;
	}

	newResponse.m_followup.followup_target = ResponseCopyString(token);

	bSuc = ParseToken(); // get another token
	if (!bSuc)
	{
		contract_assert( false && "THEN token in response had a target '%s', but lacked any further info.\n" );
		ResponseWarning( "THEN token in response had a target '%s', but lacked any further info.\n", newResponse.m_followup.followup_target );
		return;
	}

	newResponse.m_followup.followup_concept = ResponseCopyString( token );


	// Okay, this is totally asinine.
	// Because the ParseToken() function will split foo:bar into three tokens
	// (which is reasonable), but we have no safe way to parse the file otherwise
	// because it's all behind an engine interface, it's necessary to parse all
	// the tokens to the end of the line and catenate them, except for the last one
	// which is the delay. That's crap.
	bSuc = ParseToken();
	if (!bSuc)
	{
		contract_assert( false && "THEN token in response lacked contexts.\n" );
		ResponseWarning( "THEN token in response lacked contexts.\n" );
		return;
	}

	// okay, as long as there is at least one more token, catenate the ones we
	// see onto a temporary buffer. When we're down to the last token, that is
	// the delay.
	char buf[4096];
	buf[0] = '\0';
	while ( TokenWaiting() )
	{
		V_strncat( buf, token, 4096 );
		bSuc = ParseToken();
		contract_assert( bSuc && "Token parsing mysteriously failed." );
	}

	// down here, token is the last token, and buf is everything up to there.
	newResponse.m_followup.followup_contexts = ResponseCopyString( buf );

	newResponse.m_followup.followup_delay = std::atof( token );
}

void CResponseSystem::ParseOneResponse( const char *responseGroupName, ResponseGroup& group, ResponseParams *defaultParams )
{
	ParserResponse &newResponse = group.group.emplace_back();
	newResponse.weight.SetFloat( 1.0f );
	// inherit from group if appropriate
	if (defaultParams)
	{
		newResponse.params = *defaultParams;
	}

	ResponseParams *rp = &newResponse.params;

	newResponse.type = ComputeResponseType( token );
	if ( RESPONSE_NONE == newResponse.type )
{
		ResponseWarning( "response entry '%s' with unknown response type '%s'\n", responseGroupName, token );
		return;
}

	ParseToken();
	newResponse.value = ResponseCopyString( token );

	while ( TokenWaiting() )
	{
		ParseToken();

		unsigned int hash = RR_HASH( token );
		if ( DispatchParseResponse( token, hash, m_ResponseDispatch, newResponse, group, rp ) )
		{
			continue;
		}

		ResponseWarning( "response entry '%s' with unknown command '%s'\n", responseGroupName, token );
	}

}

void CResponseSystem::ParseResponseGroup_Start( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			while ( 1 )
			{
				ParseToken();
				if ( !V_stricmp( token, "}" ) )
					break;

				if ( !V_stricmp( token, "permitrepeats" ) )
				{
					newGroup.m_bDepleteBeforeRepeat = false;
					continue;
				}
				else if ( !V_stricmp( token, "sequential" ) )
				{
					newGroup.SetSequential( true );
					continue;
				}
				else if ( !V_stricmp( token, "norepeat" ) )
				{
					newGroup.SetNoRepeat( true );
					continue;
				}

				ParseOneResponse( responseGroupName, newGroup );
			}
		}

void CResponseSystem::ParseResponseGroup_PreDelay( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_DELAYBEFORESPEAK;
			groupResponseParams.predelay.FromInterval( ReadInterval( token ) );
		}

void CResponseSystem::ParseResponseGroup_NoDelay( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_DELAYAFTERSPEAK;
			groupResponseParams.delay.start = 0;
			groupResponseParams.delay.range = 0;
		}

void CResponseSystem::ParseResponseGroup_DefaultDelay( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
	groupResponseParams.flags |= AI_ResponseParams::RG_DELAYAFTERSPEAK;
			groupResponseParams.delay.start = AIS_DEF_MIN_DELAY;
			groupResponseParams.delay.range = ( AIS_DEF_MAX_DELAY - AIS_DEF_MIN_DELAY );
		}

void CResponseSystem::ParseResponseGroup_Delay( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_DELAYAFTERSPEAK;
			groupResponseParams.delay.FromInterval( ReadInterval( token ) );
		}

void CResponseSystem::ParseResponseGroup_SpeakOnce( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
	groupResponseParams.flags |= AI_ResponseParams::RG_SPEAKONCE;
		}

void CResponseSystem::ParseResponseGroup_NoScene( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
	groupResponseParams.flags |= AI_ResponseParams::RG_DONT_USE_SCENE;
		}

void CResponseSystem::ParseResponseGroup_StopOnNonIdle( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
	groupResponseParams.flags |= AI_ResponseParams::RG_STOP_ON_NONIDLE;
		}

void CResponseSystem::ParseResponseGroup_Odds( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_ODDS;
			groupResponseParams.odds = clamp( std::atoi( token ), 0, 100 );
		}

void CResponseSystem::ParseResponseGroup_RespeakDelay( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_RESPEAKDELAY;
			groupResponseParams.respeakdelay.FromInterval( ReadInterval( token ) );
		}

void CResponseSystem::ParseResponseGroup_WeaponDelay( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_WEAPONDELAY;
			groupResponseParams.weapondelay.FromInterval( ReadInterval( token ) );
		}

void CResponseSystem::ParseResponseGroup_Soundlevel( char const *responseGroupName, ResponseGroup &newGroup, AI_ResponseParams &groupResponseParams )
		{
			ParseToken();
	groupResponseParams.flags |= AI_ResponseParams::RG_SOUNDLEVEL;
			groupResponseParams.soundlevel = (soundlevel_t)TextToSoundLevel( token );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResponseSystem::ParseResponse( void )
{
	AI_ResponseParams groupResponseParams; // default response parameters inherited from single line format for group

	// Should have groupname at start
	ParseToken();
	char responseGroupName[ 128 ];
	V_strncpy( responseGroupName, token, sizeof( responseGroupName ) );

	ResponseGroup &newGroup = m_Responses.emplace( responseGroupName, ResponseGroup() )->second;

	while ( 1 )
	{
		ParseToken();

		unsigned int hash = RR_HASH( token );

		// Oops, part of next definition
		if( IsRootCommand( hash ) )
		{
			Unget();
			break;
		}

		if ( DispatchParseResponseGroup( token, hash, m_ResponseGroupDispatch, responseGroupName, newGroup, groupResponseParams ) )
		{
			continue;
		}
			ParseOneResponse( responseGroupName, newGroup );
		}
	}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *criterion - 
//-----------------------------------------------------------------------------
CriteriaEntry_t *CResponseSystem::ParseOneCriterion( const char *criterionName )
{
	char key[ 128 ];
	char value[ 128 ];

	Criteria *pNewCriterion = nullptr;

	CriteriaEntry_t *pEntry;
	if ( m_Criteria.contains( criterionName ) )
	{
		static Criteria dummy;
		pNewCriterion = &dummy;

		ResponseWarning( "Multiple definitions for criteria '%s' [%d]\n", criterionName, RR_HASH( criterionName ) );
		pEntry = nullptr;
	}
	else
	{
		pEntry = &*m_Criteria.try_emplace( criterionName ).first;
		pNewCriterion = &pEntry->second;
	}

	bool gotbody = false;

	while ( TokenWaiting() || !gotbody )
	{
		ParseToken();

		// Oops, part of next definition
		if( IsRootCommand() )
		{
			Unget();
			break;
		}

		if ( !V_stricmp( token, "{" ) )
		{
			gotbody = true;

			while ( 1 )
			{
				ParseToken();
				if ( !V_stricmp( token, "}" ) )
					break;

				// Look up subcriteria
				auto it = m_Criteria.find( token );
				if ( it != m_Criteria.end() )
				{
					pNewCriterion->subcriteria.push_back( &*it );
				}
				else
				{
					ResponseWarning( "Skipping unrecongized subcriterion '%s' in '%s'\n", token, criterionName );
				}
			}
			continue;
		}
		else if ( !V_stricmp( token, "required" ) )
		{
			pNewCriterion->required = true;
		}
		else if ( !V_stricmp( token, "weight" ) )
		{
			ParseToken();
			pNewCriterion->weight.SetFloat( (float)std::atof( token ) );
		}
		else
		{
			contract_assert( pNewCriterion->subcriteria.empty() );

			// Assume it's the math info for a non-subcriteria resposne
			V_strncpy( key, token, sizeof( key ) );
			ParseToken();
			V_strncpy( value, token, sizeof( value ) );

			V_strlower( key );
			pNewCriterion->nameSym = CriteriaSet::ComputeCriteriaSymbol( key );
			pNewCriterion->value = ResponseCopyString( value );

			gotbody = true;
		}
	}

	if ( !pNewCriterion->IsSubCriteriaType() )
	{
		ComputeMatcher( pNewCriterion, pNewCriterion->matcher );
	}

	return pEntry;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *kv - 
//-----------------------------------------------------------------------------
void CResponseSystem::ParseCriterion( void )
{
	// Should have groupname at start
	char criterionName[ 128 ];
	ParseToken();
	V_strncpy( criterionName, token, sizeof( criterionName ) );

	ParseOneCriterion( criterionName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *kv - 
//-----------------------------------------------------------------------------
void CResponseSystem::ParseEnumeration( void )
{
	char enumerationName[ 128 ];
	ParseToken();
	V_strncpy( enumerationName, token, sizeof( enumerationName ) );

	ParseToken();
	if ( V_stricmp( token, "{" ) )
	{
		ResponseWarning( "Expecting '{' in enumeration '%s', got '%s'\n", enumerationName, token );
		return;
	}

	while ( 1 )
	{
		ParseToken();
		if ( !V_stricmp( token, "}" ) )
			break;

		if ( V_strlen( token ) <= 0 )
		{
			ResponseWarning( "Expecting more tokens in enumeration '%s'\n", enumerationName );
			break;
		}

		char key[ 128 ];

		V_strncpy( key, token, sizeof( key ) );
		ParseToken();
		float value = (float)std::atof( token );

		char sz[ 128 ];
		V_snprintf( sz, sizeof( sz ), "[%s::%s]", enumerationName, key );
		V_strlower( sz );

		Enumeration newEnum;
		newEnum.value = value;

		m_Enumerations.emplace( sz, newEnum );
		/*
		else
		{
		ResponseWarning( "Ignoring duplication enumeration '%s'\n", sz );
		}
		*/
	}
}

void CResponseSystem::ParseRule_MatchOnce( Rule &newRule )
		{
			newRule.m_bMatchOnce = true;
		}

void CResponseSystem::ParseRule_ApplyContextToWorld( Rule &newRule )
		{
			newRule.m_bApplyContextToWorld = true;
		}

void CResponseSystem::ParseRule_ApplyContext( Rule &newRule )
		{
			ParseToken();
			if ( newRule.GetContext() == nullptr )
			{
				newRule.SetContext( token );
			}
			else
			{
				CFmtStrN<1024> newContext( "%s,%s", newRule.GetContext(), token );
				newRule.SetContext( newContext );
			}
		}

void CResponseSystem::ParseRule_Response( Rule &newRule )
		{
			// Read them until we run out.
			while ( TokenWaiting() )
			{
				ParseToken();
				auto it = m_Responses.find( token );
				if ( it != m_Responses.end() )
				{
					newRule.m_Responses.push_back( &*it );
				}
				else
				{
			m_bParseRuleValid = false;
			ResponseWarning( "No such response '%s' for rule '%s'\n", token, m_pParseRuleName );
		}
	}
}

/*
void CResponseSystem::ParseRule_ForceWeight( Rule &newRule )
{
	ParseToken();
	if ( token[0] == 0 )
	{
		// no token followed forceweight?
		ResponseWarning( "Forceweight token in rule '%s' did not specify a numerical weight! Ignoring.\n", m_pParseRuleName );
	}
	else
	{
		newRule.m_nForceWeight = atoi(token);
		if ( newRule.m_nForceWeight == 0 )
		{
			ResponseWarning( "Rule '%s' had forceweight '%s', which doesn't work out to a nonzero number. Ignoring.\n", 
				m_pParseRuleName, token );
				}
			}
		}
*/

void CResponseSystem::ParseRule_Criteria( Rule &newRule )
		{
			// Read them until we run out.
			while ( TokenWaiting() )
			{
				ParseToken();

				auto it = m_Criteria.find( token );
				if ( it != m_Criteria.end() )
				{
					newRule.m_Criteria.push_back( &*it );
				}
				else
				{
			m_bParseRuleValid = false;
			ResponseWarning( "No such criterion '%s' for rule '%s'\n", token, m_pParseRuleName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *kv - 
//-----------------------------------------------------------------------------
void CResponseSystem::ParseRule( void )
{
	static int instancedCriteria = 0;

	char ruleName[ 128 ];
	ParseToken();
	V_strncpy( ruleName, token, sizeof( ruleName ) );

	ParseToken();
	if ( V_stricmp( token, "{" ) )
	{
		ResponseWarning( "Expecting '{' in rule '%s', got '%s'\n", ruleName, token );
		return;
				}

	// entries are "criteria", "response" or an in-line criteria to instance
	auto newRule = std::make_unique< Rule >();

	char sz[ 128 ];

	m_bParseRuleValid = true;
	m_pParseRuleName = ruleName;
	while ( 1 )
	{
		ParseToken();
		if ( !V_stricmp( token, "}" ) )
		{
			break;
		}

		if ( V_strlen( token ) <= 0 )
		{
			ResponseWarning( "Expecting more tokens in rule '%s'\n", ruleName );
			break;
			}

		unsigned int hash = RR_HASH( token );
		if ( DispatchParseRule( token, hash, m_RuleDispatch, *newRule ) )
			continue;

		// It's an inline criteria, generate a name and parse it in
		V_snprintf( sz, sizeof( sz ), "[%s%03i]", ruleName, ++instancedCriteria );
		Unget();
		if ( CriteriaEntry_t *pCriterion = ParseOneCriterion( sz ) )
		{
			newRule->m_Criteria.push_back( pCriterion );
		}
	}

	if ( m_bParseRuleValid )
	{
		m_RulePartitions.GetDictForRule( this, newRule.get() ).Insert( ruleName, std::move( newRule ) );
	}
	else
	{
		DevMsg( "Discarded rule %s\n", ruleName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CResponseSystem::GetCurrentToken() const
{
	if ( std::ssize( m_ScriptStack ) <= 0 )
		return -1;

	return m_ScriptStack[ 0 ].tokencount;
}


void CResponseSystem::ResponseWarning( const char *fmt, ... )
{
	std::va_list		argptr;
	char		string[1024];

	__builtin_va_start (argptr, fmt);
	V_vsnprintf(string, sizeof(string), fmt,argptr);
	__builtin_va_end (argptr);

	char cur[ 256 ];
	GetCurrentScript( cur, sizeof( cur ) );
	DevMsg( 1, "%s(token %i) : %s", cur, GetCurrentToken(), string );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CResponseSystem::CopyCriteriaFrom( Rule *pSrcRule, Rule *pDstRule, CResponseSystem *pCustomSystem )
{
	// Add criteria from this rule to global list in custom response system.
	for ( CriteriaEntry_t *pSrcEntry : pSrcRule->m_Criteria )
	{
		Criteria *pSrcCriteria = &pSrcEntry->second;
		if ( pSrcCriteria )
		{
			auto itDst = pCustomSystem->m_Criteria.find( pSrcEntry->first );
			if ( itDst != pCustomSystem->m_Criteria.end() )
			{
				pDstRule->m_Criteria.push_back( &*itDst );
				continue;
			}

			// Add the criteria.
			Criteria dstCriteria;

			dstCriteria.nameSym = pSrcCriteria->nameSym ;
			dstCriteria.value = ResponseCopyString( pSrcCriteria->value );
			dstCriteria.weight = pSrcCriteria->weight;
			dstCriteria.required = pSrcCriteria->required;
			dstCriteria.matcher = pSrcCriteria->matcher;

			for ( CriteriaEntry_t *pSrcSubEntry : pSrcCriteria->subcriteria )
			{
				Criteria *pSrcSubCriteria = &pSrcSubEntry->second;
				if ( pSrcCriteria )
				{
					if ( pCustomSystem->m_Criteria.contains( pSrcSubCriteria->value ) )
						continue;

					// Add the criteria.
					Criteria dstSubCriteria;

					dstSubCriteria.nameSym = pSrcSubCriteria->nameSym ;
					dstSubCriteria.value = ResponseCopyString( pSrcSubCriteria->value );
					dstSubCriteria.weight = pSrcSubCriteria->weight;
					dstSubCriteria.required = pSrcSubCriteria->required;
					dstSubCriteria.matcher = pSrcSubCriteria->matcher;

					dstCriteria.subcriteria.push_back( &*pCustomSystem->m_Criteria.emplace( pSrcSubCriteria->value, dstSubCriteria ).first );
				}
			}

			// A sub criterion added above may already hold the name; share it.
			pDstRule->m_Criteria.push_back( &*pCustomSystem->m_Criteria.emplace( pSrcEntry->first, dstCriteria ).first );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CResponseSystem::CopyResponsesFrom( Rule *pSrcRule, Rule *pDstRule, CResponseSystem *pCustomSystem )
{
	// Add responses from this rule to global list in custom response system.
	for ( ResponseGroupEntry_t *pSrcEntry : pSrcRule->m_Responses )
	{
		ResponseGroup *pSrcResponseGroup = &pSrcEntry->second;
		if ( pSrcResponseGroup )
		{
			// Add response group.			
			ResponseGroup dstResponseGroup;

			dstResponseGroup.m_bDepleteBeforeRepeat = pSrcResponseGroup->m_bDepleteBeforeRepeat;
			dstResponseGroup.m_nDepletionCount = pSrcResponseGroup->m_nDepletionCount;
			dstResponseGroup.m_bHasFirst = pSrcResponseGroup->m_bHasFirst;
			dstResponseGroup.m_bHasLast = pSrcResponseGroup->m_bHasLast;
			dstResponseGroup.m_bSequential = pSrcResponseGroup->m_bSequential;
			dstResponseGroup.m_bNoRepeat = pSrcResponseGroup->m_bNoRepeat;
			dstResponseGroup.m_bEnabled = pSrcResponseGroup->m_bEnabled;
			dstResponseGroup.m_nCurrentIndex = pSrcResponseGroup->m_nCurrentIndex;

			int nSrcResponseCount = std::ssize( pSrcResponseGroup->group );
			for ( int iResponse = 0; iResponse < nSrcResponseCount; ++iResponse )
			{
				ParserResponse *pSrcResponse = &pSrcResponseGroup->group[iResponse];
				if ( pSrcResponse )
				{
					// Add Response
					ParserResponse dstResponse;

					dstResponse.weight = pSrcResponse->weight;
					dstResponse.type = pSrcResponse->type;
					dstResponse.value = ResponseCopyString( pSrcResponse->value );
					dstResponse.depletioncount = pSrcResponse->depletioncount;
					dstResponse.first = pSrcResponse->first;
					dstResponse.last = pSrcResponse->last;

					dstResponseGroup.group.push_back( dstResponse );
				}
			}

			// Each rule gets its own copy of the group.
			pDstRule->m_Responses.push_back( &*pCustomSystem->m_Responses.emplace( pSrcEntry->first, dstResponseGroup ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResponseSystem::CopyEnumerationsFrom( CResponseSystem *pCustomSystem )
{
	for ( auto &[name, srcEnumeration] : m_Enumerations )
	{
		Enumeration dstEnumeration;
		dstEnumeration.value = srcEnumeration.value;
		pCustomSystem->m_Enumerations.emplace( name, dstEnumeration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResponseSystem::CopyRuleFrom( Rule *pSrcRule, ResponseRulePartition::tIndex iRule, CResponseSystem *pCustomSystem )
{
	// Verify data.
	contract_assert( pSrcRule );
	contract_assert( pCustomSystem );
	if ( !pSrcRule || !pCustomSystem )
		return;

	// New rule
	Rule *dstRule = new Rule;

	dstRule->SetContext( pSrcRule->GetContext() );
	dstRule->m_bMatchOnce = pSrcRule->m_bMatchOnce;
	dstRule->m_bEnabled = pSrcRule->m_bEnabled;
	dstRule->m_bApplyContextToWorld = pSrcRule->m_bApplyContextToWorld;

	// Copy off criteria.
	CopyCriteriaFrom( pSrcRule, dstRule, pCustomSystem );

	// Copy off responses.
	CopyResponsesFrom( pSrcRule, dstRule, pCustomSystem );

	// Copy off enumerations - Don't think we use these.
	//	CopyEnumerationsFrom( pCustomSystem );

	// Add rule.
	pCustomSystem->m_RulePartitions.GetDictForRule( this, dstRule ).Insert( m_RulePartitions.GetElementName( iRule ), std::unique_ptr< Rule >( dstRule ) );
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CResponseSystem::DumpRules()
{
	for ( ResponseRulePartition::tIndex idx = m_RulePartitions.First() ;
		m_RulePartitions.IsValid(idx) ;
		idx = m_RulePartitions.Next(idx) )
	{
		Msg("%s\n", m_RulePartitions.GetElementName( idx ) );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CResponseSystem::DumpDictionary( const char *pszName )
{
	Msg( "\nDictionary: %s\n", pszName );

	// int nRuleCount = m_Rules.Count();
	// for ( int iRule = 0; iRule < nRuleCount; ++iRule )
	for ( ResponseRulePartition::tIndex idx = m_RulePartitions.First() ;
		m_RulePartitions.IsValid(idx) ;
		idx = m_RulePartitions.Next(idx) )
	{
		Msg("	Rule %d/%d: %s\n", m_RulePartitions.BucketFromIdx(idx), m_RulePartitions.PartFromIdx( idx ), m_RulePartitions.GetElementName( idx ) );

		Rule *pRule = &m_RulePartitions[idx];

		int nCriteriaCount = std::ssize( pRule->m_Criteria );
		for( int iCriteria = 0; iCriteria < nCriteriaCount; ++iCriteria )
		{
			Criteria *pCriteria = &pRule->m_Criteria[iCriteria]->second;
			Msg( "		Criteria %d: %s %s\n", iCriteria, CriteriaSet::SymbolToStr(pCriteria->nameSym), pCriteria->value );
		}

		int nResponseGroupCount = std::ssize( pRule->m_Responses );
		for ( int iResponseGroup = 0; iResponseGroup < nResponseGroupCount; ++iResponseGroup )
		{
			ResponseGroupEntry_t *pRuleResponse = pRule->m_Responses[iResponseGroup];
			ResponseGroup *pResponseGroup = &pRuleResponse->second;

			Msg( "		ResponseGroup %d: %s\n", iResponseGroup, pRuleResponse->first.c_str() );

			int nResponseCount = std::ssize( pResponseGroup->group );
			for ( int iResponse = 0; iResponse < nResponseCount; ++iResponse )
			{
				ParserResponse *pResponse = &pResponseGroup->group[iResponse];
				Msg( "			Response %d: %s\n", iResponse, pResponse->value );
			}
		}
	}
}

void CResponseSystem::BuildDispatchTables()
{
	m_RootCommandHashes.insert( RR_HASH( "#include" ) );
	m_RootCommandHashes.insert( RR_HASH( "response" ) );
	m_RootCommandHashes.insert( RR_HASH( "enumeration" ) );
	m_RootCommandHashes.insert( RR_HASH( "criterion" ) );
	m_RootCommandHashes.insert( RR_HASH( "criteria" ) );
	m_RootCommandHashes.insert( RR_HASH( "rule" ) );

	m_FileDispatch.emplace( RR_HASH( "#include" ), &CResponseSystem::ParseInclude );
	m_FileDispatch.emplace( RR_HASH( "response" ), &CResponseSystem::ParseResponse );
	m_FileDispatch.emplace( RR_HASH( "criterion" ), &CResponseSystem::ParseCriterion );
	m_FileDispatch.emplace( RR_HASH( "criteria" ), &CResponseSystem::ParseCriterion );
	m_FileDispatch.emplace( RR_HASH( "rule" ), &CResponseSystem::ParseRule );
	m_FileDispatch.emplace( RR_HASH( "enumeration" ), &CResponseSystem::ParseEnumeration );

	m_RuleDispatch.emplace( RR_HASH( "matchonce" ), &CResponseSystem::ParseRule_MatchOnce );
	m_RuleDispatch.emplace( RR_HASH( "applycontexttoworld" ), &CResponseSystem::ParseRule_ApplyContextToWorld );
	m_RuleDispatch.emplace( RR_HASH( "applycontext" ), &CResponseSystem::ParseRule_ApplyContext );
	m_RuleDispatch.emplace( RR_HASH( "response" ), &CResponseSystem::ParseRule_Response );
//	m_RuleDispatch.Insert( RR_HASH( "forceweight" ), &CResponseSystem::ParseRule_ForceWeight );
	m_RuleDispatch.emplace( RR_HASH( "criteria" ), &CResponseSystem::ParseRule_Criteria );
	m_RuleDispatch.emplace( RR_HASH( "criterion" ), &CResponseSystem::ParseRule_Criteria );


	m_ResponseDispatch.emplace( RR_HASH( "weight" ), &CResponseSystem::ParseResponse_Weight );
	m_ResponseDispatch.emplace( RR_HASH( "predelay" ), &CResponseSystem::ParseResponse_PreDelay );
	m_ResponseDispatch.emplace( RR_HASH( "nodelay" ), &CResponseSystem::ParseResponse_NoDelay );
	m_ResponseDispatch.emplace( RR_HASH( "defaultdelay" ), &CResponseSystem::ParseResponse_DefaultDelay );
	m_ResponseDispatch.emplace( RR_HASH( "delay" ), &CResponseSystem::ParseResponse_Delay );
	m_ResponseDispatch.emplace( RR_HASH( "speakonce" ), &CResponseSystem::ParseResponse_SpeakOnce );
	m_ResponseDispatch.emplace( RR_HASH( "noscene" ), &CResponseSystem::ParseResponse_NoScene );
	m_ResponseDispatch.emplace( RR_HASH( "stop_on_nonidle" ), &CResponseSystem::ParseResponse_StopOnNonIdle );
	m_ResponseDispatch.emplace( RR_HASH( "odds" ), &CResponseSystem::ParseResponse_Odds );
	m_ResponseDispatch.emplace( RR_HASH( "respeakdelay" ), &CResponseSystem::ParseResponse_RespeakDelay );
	m_ResponseDispatch.emplace( RR_HASH( "weapondelay" ), &CResponseSystem::ParseResponse_WeaponDelay );
	m_ResponseDispatch.emplace( RR_HASH( "soundlevel" ), &CResponseSystem::ParseResponse_Soundlevel );
	m_ResponseDispatch.emplace( RR_HASH( "displayfirst" ), &CResponseSystem::ParseResponse_DisplayFirst );
	m_ResponseDispatch.emplace( RR_HASH( "displaylast" ), &CResponseSystem::ParseResponse_DisplayLast );
	m_ResponseDispatch.emplace( RR_HASH( "fire" ), &CResponseSystem::ParseResponse_Fire );
	m_ResponseDispatch.emplace( RR_HASH( "then" ), &CResponseSystem::ParseResponse_Then );

	m_ResponseGroupDispatch.emplace( RR_HASH( "{" ), &CResponseSystem::ParseResponseGroup_Start );
	m_ResponseGroupDispatch.emplace( RR_HASH( "predelay" ), &CResponseSystem::ParseResponseGroup_PreDelay );
	m_ResponseGroupDispatch.emplace( RR_HASH( "nodelay" ), &CResponseSystem::ParseResponseGroup_NoDelay );
	m_ResponseGroupDispatch.emplace( RR_HASH( "defaultdelay" ), &CResponseSystem::ParseResponseGroup_DefaultDelay );
	m_ResponseGroupDispatch.emplace( RR_HASH( "delay" ), &CResponseSystem::ParseResponseGroup_Delay );
	m_ResponseGroupDispatch.emplace( RR_HASH( "speakonce" ), &CResponseSystem::ParseResponseGroup_SpeakOnce );
	m_ResponseGroupDispatch.emplace( RR_HASH( "noscene" ), &CResponseSystem::ParseResponseGroup_NoScene );
	m_ResponseGroupDispatch.emplace( RR_HASH( "stop_on_nonidle" ), &CResponseSystem::ParseResponseGroup_StopOnNonIdle );
	m_ResponseGroupDispatch.emplace( RR_HASH( "odds" ), &CResponseSystem::ParseResponseGroup_Odds );
	m_ResponseGroupDispatch.emplace( RR_HASH( "respeakdelay" ), &CResponseSystem::ParseResponseGroup_RespeakDelay );
	m_ResponseGroupDispatch.emplace( RR_HASH( "weapondelay" ), &CResponseSystem::ParseResponseGroup_WeaponDelay );
	m_ResponseGroupDispatch.emplace( RR_HASH( "soundlevel" ), &CResponseSystem::ParseResponseGroup_Soundlevel );
}

bool CResponseSystem::Dispatch( char const *pToken, unsigned int uiHash, CResponseSystem::DispatchMap_t &rMap )
{
	auto it = rMap.find( uiHash );
	if ( it != rMap.end() )
	{
		CResponseSystem::pfnResponseDispatch dispatch = it->second;
		(this->*dispatch)();
		return true;
	}

	return false;
}

bool CResponseSystem::DispatchParseRule( char const *pToken, unsigned int uiHash, ParseRuleDispatchMap_t &rMap, Rule &newRule )
{
	auto it = rMap.find( uiHash );
	if ( it != rMap.end() )
	{
		CResponseSystem::pfnParseRuleDispatch dispatch = it->second;
		(this->*dispatch)( newRule );
		return true;
	}

	return false;
}

bool CResponseSystem::DispatchParseResponse( char const *pToken, unsigned int uiHash, ParseResponseDispatchMap_t &rMap, ParserResponse &newResponse, ResponseGroup& group, AI_ResponseParams *rp )
{
	auto it = rMap.find( uiHash );
	if ( it != rMap.end() )
	{
		CResponseSystem::pfnParseResponseDispatch dispatch = it->second;
		(this->*dispatch)( newResponse, group, rp );
		return true;
	}

	return false;
}

bool CResponseSystem::DispatchParseResponseGroup( char const *pToken, unsigned int uiHash, ParseResponseGroupDispatchMap_t &rMap, char const *responseGroupName, ResponseGroup& newGroup, AI_ResponseParams &groupResponseParams )
{
	auto it = rMap.find( uiHash );
	if ( it != rMap.end() )
	{
		CResponseSystem::pfnParseResponseGroupDispatch dispatch = it->second;
		(this->*dispatch)( responseGroupName, newGroup, groupResponseParams );
		return true;
	}

	return false;
}

unsigned int ResponseRulePartition::GetBucketForSpeakerAndConcept( const char *pszSpeaker, const char *pszConcept, const char *pszSubject )
{
	// make sure is a power of two
	static_assert( ( N_RESPONSE_PARTITIONS & ( N_RESPONSE_PARTITIONS - 1 ) ) == 0 );

	// hash together the speaker and concept strings, and mask off by the bucket mask
	unsigned hashSpeaker = 0; // pszSpeaker ? HashStringCaseless( pszSpeaker ) : 0;
	unsigned hashConcept = pszConcept ? HashStringCaseless( pszConcept ) : 0;
	unsigned hashSubject = pszSubject ? HashStringCaseless( pszSubject ) : 0;
	unsigned hashBrowns = ( ( hashSubject >> 3 ) ^ (hashSpeaker >> 1) ^ hashConcept ) & ( N_RESPONSE_PARTITIONS - 1 );
	return hashBrowns;
}

const char *Rule::GetValueForRuleCriterionByName( CResponseSystem * __restrict__ pSystem, CriteriaSet::CritSymbol_t pCritNameSym )
{
	const char * retval = nullptr;
	// for each rule criterion...
	for ( int i = 0 ; i < std::ssize( m_Criteria ) ; ++i )
	{
		retval = RecursiveGetValueForRuleCriterionByName( pSystem, &m_Criteria[i]->second, pCritNameSym );
		if ( retval != nullptr )
		{
			// we found a result, early out
			break;
		}
	}

	return retval;
}

const Criteria *Rule::GetPointerForRuleCriterionByName( CResponseSystem *pSystem, CriteriaSet::CritSymbol_t pCritNameSym )
{
	const Criteria * retval = nullptr;
	// for each rule criterion...
	for ( int i = 0 ; i < std::ssize( m_Criteria ) ; ++i )
	{
		retval = RecursiveGetPointerForRuleCriterionByName( pSystem, &m_Criteria[i]->second, pCritNameSym );
		if ( retval != nullptr )
		{
			// we found a result, early out
			break;
		}
	}

	return retval;
}

const char *Rule::RecursiveGetValueForRuleCriterionByName( CResponseSystem * __restrict__ pSystem, 
														    const Criteria * __restrict__ pCrit, CriteriaSet::CritSymbol_t pCritNameSym )
{
	contract_assert( pCrit );
	if ( !pCrit ) return nullptr;
	if ( pCrit->IsSubCriteriaType() )
	{
		// test each of the children (depth first)
		const char *pRet = nullptr;
		for ( int i = 0 ; i < std::ssize( pCrit->subcriteria ) ; ++i )
		{
			pRet = RecursiveGetValueForRuleCriterionByName( pSystem, &pCrit->subcriteria[i]->second, pCritNameSym );
			if ( pRet ) // if found something, early out
				return pRet;
		}
	}
	else // leaf criterion
	{
		if ( pCrit->nameSym.data() == pCritNameSym.data() )
		{
			return pCrit->value;
		}
		else
		{
			return nullptr;
		}
	}

	return nullptr;
}


const Criteria *Rule::RecursiveGetPointerForRuleCriterionByName( CResponseSystem *pSystem, const Criteria *pCrit, CriteriaSet::CritSymbol_t pCritNameSym )
{
	contract_assert( pCrit );
	if ( !pCrit ) return nullptr;
	if ( pCrit->IsSubCriteriaType() )
	{
		// test each of the children (depth first)
		const Criteria *pRet = nullptr;
		for ( int i = 0 ; i < std::ssize( pCrit->subcriteria ) ; ++i )
		{
			pRet = RecursiveGetPointerForRuleCriterionByName( pSystem, &pCrit->subcriteria[i]->second, pCritNameSym );
			if ( pRet ) // if found something, early out
				return pRet;
		}
	}
	else // leaf criterion
	{
		if ( pCrit->nameSym.data() == pCritNameSym.data() )
		{
			return pCrit;
		}
		else
		{
			return nullptr;
		}
	}

	return nullptr;
}


#if RR_DUMPHASHINFO_ENABLED
static void CC_RR_DumpHashInfo( const CCommand &args )
{
	defaultresponsesytem.m_InstancedSystems[0]->m_RulePartitions.PrintBucketInfo( defaultresponsesytem.m_InstancedSystems[0] );
}
static ConCommand rr_dumphashinfo( "rr_dumphashinfo", CC_RR_DumpHashInfo, "Statistics on primary hash bucketing of response rule partitions");

void ResponseRulePartition::PrintBucketInfo( CResponseSystem *pSys )
{
	struct bucktuple_t
	{
		int nBucket;
		int nCount;
		bucktuple_t() : nBucket(-1), nCount(-1) {};
		bucktuple_t( int bucket, int count )  : nBucket(bucket), nCount(count) {};

		static int SortCompare( const bucktuple_t * a, const bucktuple_t * b )
		{
			return a->nCount - b->nCount;
		}
	};

	std::vector< bucktuple_t > infos( N_RESPONSE_PARTITIONS, N_RESPONSE_PARTITIONS );

	float nAverage = 0;
	for ( int i = 0 ; i < N_RESPONSE_PARTITIONS ; ++i )
	{
		int count = m_RuleParts[i].Count();
		infos.AddToTail( bucktuple_t( i, count ) );
		nAverage += count;
	}
	nAverage /= N_RESPONSE_PARTITIONS;
	infos.Sort( bucktuple_t::SortCompare );
	Msg( "%d buckets, %d total, %.2f average size\n", N_RESPONSE_PARTITIONS, Count(), nAverage );
	Msg( "8 shortest buckets:\n" );
	for ( int i = 0 ; i < 8 ; ++i )
	{
		Msg("\t%d: %d\n", infos[i].nBucket, infos[i].nCount );
	}
	Msg( "8 longest buckets:\n" );
	for ( int i = infos.Count() - 1 ; i >= infos.Count() - 9 ; --i )
	{
		Msg("\t%d: %d\n", infos[i].nBucket, infos[i].nCount );
	}
	int nempty = 0;
	for ( nempty = 0 ; nempty < infos.Count() ; ++nempty )
	{
		if ( infos[nempty].nCount != 0 )
			break;
	}
	Msg( "%d empty buckets\n", nempty );

	/*
	Msg( " Contents of longest bucket\nwho\tconcept\n" );
	tRuleDict &bucket = m_RuleParts[infos[infos.Count()-1].nBucket];
	for ( tRuleDict::IndexType_t i = bucket.FirstInorder(); bucket.IsValidIndex(i); i = bucket.NextInorder(i) )
	{
		Rule &rule = bucket.Element(i) ;
		Msg("%s\t%s\n", rule.GetValueForRuleCriterionByName( pSys, "who" ), rule.GetValueForRuleCriterionByName( pSys, CriteriaSet::ComputeCriteriaSymbol("concept") ) );
	}
	*/
}
#endif
