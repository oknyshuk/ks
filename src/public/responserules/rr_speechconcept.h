//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Class data for an AI Concept, an atom of response-driven dialog.
//
// $NoKeywords: $
//=============================================================================//

#ifndef RR_SPEECHCONCEPT_H
#define RR_SPEECHCONCEPT_H


#include <string_view>
#include "tier1/intern_pool.h"

#define RR_CONCEPTS_ARE_STRINGS 0


typedef ks::intern_pool CRR_ConceptSymbolTable;

namespace ResponseRules
{
class CRR_Concept
{
public: // local typedefs
	typedef std::string_view tGenericId; // pooled in g_pRRConceptTable: equal concepts share storage, so == is exact
	tGenericId m_iConcept;

public:
	CRR_Concept() {};	// invalid: null m_iConcept.data()
	// construct concept from a string.
	CRR_Concept(const char *fromString);

	// Return as a string
	const char *GetStringConcept() const;
	static const char *GetStringForGenericId(tGenericId genericId);

	operator tGenericId() const { return m_iConcept; }
	operator const char *() const { return GetStringConcept(); }
	inline bool operator==(const CRR_Concept &other) // default is compare by concept ids
	{
		return m_iConcept.data() == other.m_iConcept.data();
	}
	bool operator==(const char *pszConcept);

protected:

private:
	// dupe a concept
	// CRR_Concept& operator=(CRR_Concept &other);
	CRR_Concept& operator=(const char *fromString);
};
};


#endif
