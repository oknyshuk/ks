// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2010 Valve Corporation

// Purpose: Core types for the response rules -- criteria, responses, rules, and matchers.

#include "rrbase.h"

using namespace ResponseRules;


// bizarre function handed down from the misty days of yore
// and the original response system. a lot of stuff uses it
// and I can't be arsed to replace everything with the c stdlib 
// stuff
namespace ResponseRules 
{
	extern const char *ResponseCopyString( const char *in );
};


//-------------------- MATCHER ----------------------------------------------

Matcher::Matcher()
{
	valid = false;
	isnumeric = false;
	notequal = false;
	usemin = false;
	minequals = false;
	usemax = false;
	maxequals = false;
	maxval = 0.0f;
	minval = 0.0f;

	token = nullptr;
	rawtoken = nullptr;
}

void Matcher::Describe( void )
{
	if ( !valid )
	{
		DevMsg( "    invalid!\n" );
		return;
	}
	char sz[ 128 ];

	sz[ 0] = 0;
	int minmaxcount = 0;
	if ( usemin )
	{
		V_snprintf( sz, sizeof( sz ), ">%s%.3f", minequals ? "=" : "", minval );
		minmaxcount++;
	}
	if ( usemax )
	{
		char sz2[ 128 ];
		V_snprintf( sz2, sizeof( sz2 ), "<%s%.3f", maxequals ? "=" : "", maxval );

		if ( minmaxcount > 0 )
		{
			V_strncat( sz, " and ", sizeof( sz ), COPY_ALL_CHARACTERS );
		}
		V_strncat( sz, sz2, sizeof( sz ), COPY_ALL_CHARACTERS );
		minmaxcount++;
	}

	if ( minmaxcount >= 1 )
	{
		DevMsg( "    matcher:  %s\n", sz );
		return;
	}

	if ( notequal )
	{
		DevMsg( "    matcher:  !=%s\n", GetToken() );
		return;
	}

	DevMsg( "    matcher:  ==%s\n", GetToken() );
}

void Matcher::SetToken( char const *s )
{
	token = g_RS.intern( s ).data();
}

void Matcher::SetRaw( char const *raw )
{
	rawtoken = g_RS.intern( raw ).data();
}

char const *Matcher::GetToken()
{
	return ks::nz( token );
}

char const *Matcher::GetRaw()
{
	return ks::nz( rawtoken );
}

//-------------------- CRITERIA ----------------------------------------------

Criteria::Criteria()
{
	value = nullptr;
	weight.SetFloat( 1.0f );
	required = false;
}
Criteria::Criteria(const Criteria& src )
{
	operator=( src );
}

Criteria::~Criteria()
{
	// do nothing because we don't own name and value anymore
}

Criteria& Criteria::operator =(const Criteria& src )
{
	if ( this == &src )
		return *this;

	nameSym = src.nameSym;
	value = ResponseCopyString( src.value );
	weight = src.weight;
	required = src.required;

	matcher = src.matcher;

	subcriteria = src.subcriteria;

	return *this;
}


//-------------------- RESPONSE ----------------------------------------------



ParserResponse::ParserResponse() : m_followup()
{
	type = RESPONSE_NONE;
	value = nullptr;
	weight.SetFloat( 1.0f );
	depletioncount = 0;
	first = false;
	last = false;
}

ParserResponse& ParserResponse::operator =( const ParserResponse& src )
{
	if ( this == &src )
		return *this;
	weight = src.weight;
	type = src.type;
	value = ResponseCopyString( src.value );
	depletioncount = src.depletioncount;
	first = src.first;
	last = src.last;
	params = src.params;

	m_followup.followup_concept = ResponseCopyString(src.m_followup.followup_concept);
	m_followup.followup_contexts = ResponseCopyString(src.m_followup.followup_contexts);
	m_followup.followup_target = ResponseCopyString(src.m_followup.followup_target);
	m_followup.followup_entityioinput = ResponseCopyString(src.m_followup.followup_entityioinput);
	m_followup.followup_entityiotarget = ResponseCopyString(src.m_followup.followup_entityiotarget);
	m_followup.followup_delay = src.m_followup.followup_delay;
	m_followup.followup_entityiodelay = src.m_followup.followup_entityiodelay;

	return *this;
}

ParserResponse::ParserResponse( const ParserResponse& src )
{
	operator=( src );
}

ParserResponse::~ParserResponse()
{
	// nothing to do, since we don't own
	// the strings anymore
}

// ------------ RULE ---------------

Rule::Rule() : m_nForceWeight(0)
{
	m_bMatchOnce = false;
	m_bEnabled = true;
	m_szContext = nullptr;
	m_bApplyContextToWorld = false;
}

Rule& Rule::operator =( const Rule& src )
{
	if ( this == &src )
		return *this;

	m_Criteria = src.m_Criteria;
	m_Responses = src.m_Responses;

	SetContext( src.m_szContext );
	m_bMatchOnce = src.m_bMatchOnce;
	m_bEnabled = src.m_bEnabled;
	m_bApplyContextToWorld = src.m_bApplyContextToWorld;
	m_nForceWeight = src.m_nForceWeight;
	return *this;
}

Rule::Rule( const Rule& src )
{
	operator=(src);
}

Rule::~Rule()
{
}

void Rule::SetContext( const char *context )
{
	// we don't own the data we point to, so just update pointer
	m_szContext = ResponseCopyString( context );
}

// The datamaps of these types are filled in by the game that embeds them in its entities.
datamap_t ResponseRules::ResponseParams::m_DataMap = { .dataClassName = "ResponseParams" };
datamap_t ResponseRules::CRR_Response::m_DataMap = { .dataClassName = "CRR_Response" };
