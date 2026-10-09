#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

#include "ai_criteria.h"
#include "../../public/responserules/response_types.h"

// using ResponseRules::IResponseFilter;
// using ResponseRules::IResponseSystem;

ResponseRules::IResponseSystem *PrecacheCustomResponseSystem( const char *scriptfile );
ResponseRules::IResponseSystem *BuildCustomResponseSystemGivenCriteria( const char *pszBaseFile, const char *pszCustomName, AI_CriteriaSet &criteriaSet, float flCriteriaScore );
void DestroyCustomResponseSystems();
