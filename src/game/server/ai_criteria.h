#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "tier2/interval.h"
#include "../../public/responserules/response_types.h"

using ResponseRules::ResponseType_t;

// Defined by the response rules library, which is shared with the other side.

#ifndef AI_CriteriaSet
#define AI_CriteriaSet ResponseRules::CriteriaSet 
#endif

typedef ResponseRules::ResponseParams	AI_ResponseParams ;
typedef ResponseRules::CRR_Response		AI_Response;



/*
// An AI response that is dynamically new'ed up and returned from SpeakFindResponse.
class AI_ResponseReturnValue : AI_Response
{

};
*/
