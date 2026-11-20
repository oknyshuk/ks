// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rrbase.h"
import ks.tier1.string_ops;

#if RR_CONCEPTS_ARE_STRINGS
#pragma error("RR_CONCEPTS_ARE_STRINGS no longer supported")
#else

using namespace ResponseRules;

// Used to turn ad-hoc concept from strings into numbers.
CRR_ConceptSymbolTable *g_pRRConceptTable = nullptr; 

// Q&D hack to defer initialization of concept table until I can figure out where it
// really needs to come from.
static void InitializeRRConceptTable()
{
	if (g_pRRConceptTable == nullptr)
	{
		g_pRRConceptTable = new CRR_ConceptSymbolTable( true );
	}
}

// construct from string
CRR_Concept::CRR_Concept(const char *fromString)
{
	InitializeRRConceptTable();
	m_iConcept = g_pRRConceptTable->intern(fromString);
}

CRR_Concept &CRR_Concept::operator=(const char *fromString)
{
	InitializeRRConceptTable();
	m_iConcept = g_pRRConceptTable->intern(fromString);
	return *this;
}

bool CRR_Concept::operator==(const char *pszConcept)
{
	tGenericId otherConcept = g_pRRConceptTable->find(pszConcept);
	return ( otherConcept.data() && otherConcept.data() == m_iConcept.data() );
}

const char *CRR_Concept::GetStringConcept() const
{
	InitializeRRConceptTable();
	contract_assert( ( m_iConcept.data() ) && "AI Concept has invalid string symbol.\n" );
	return ks::nz( m_iConcept.data() );
}

const char *CRR_Concept::GetStringForGenericId(tGenericId genericId)
{
	InitializeRRConceptTable();
	return ks::nz( genericId.data() );
}

#endif
