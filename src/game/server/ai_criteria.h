#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.math;
#include "tier2/interval.h"
import std;
#include "../../public/responserules/response_types.h"

using ResponseRules::ResponseType_t;

extern const char *SplitContext( const char *raw, char *key, int keylen, char *value, int valuelen, float *duration, const char *entireContext );

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
