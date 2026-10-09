#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Class data for an AI Concept, an atom of response-driven dialog.

#include "../../public/responserules/response_types.h"
#include "entitylist.h"

class CAI_Concept : public ResponseRules::CRR_Concept
{
public:
	CAI_Concept() {};
	// construct concept from a string.
	CAI_Concept(const char *fromString) : CRR_Concept(fromString) {} ;

	// get/set BS
	inline EHANDLE GetSpeaker() const { return m_hSpeaker; }
	inline void SetSpeaker(EHANDLE val) { m_hSpeaker = val; }

	/*
	inline EHANDLE GetTarget() const { return m_hTarget; }
	inline void SetTarget(EHANDLE val) { m_hTarget = val; }
	inline EHANDLE GetTopic() const { return m_hTopic; }
	inline void SetTopic(EHANDLE val) { m_hTopic = val; }
	*/

protected:
	EHANDLE m_hSpeaker;

	/*
	EHANDLE m_hTarget;
	EHANDLE m_hTopic;
	*/
};

typedef CAI_Concept AIConcept_t;
