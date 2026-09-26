//========= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"

#include <assert.h>
#include "tier0/platform.h"
#include "tier0/progressbar.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/memalloc.h"

#endif


static ProgressReportHandler_t pReportHandlerFN;

PLATFORM_INTERFACE void ReportProgress(char const *job_name, int total_units_to_do, int n_units_completed)
{
	if ( pReportHandlerFN )
		(*pReportHandlerFN)( job_name, total_units_to_do, n_units_completed );
}

PLATFORM_INTERFACE ProgressReportHandler_t InstallProgressReportHandler( ProgressReportHandler_t pfn)
{
	ProgressReportHandler_t old = pReportHandlerFN;
	pReportHandlerFN = pfn;
	return old;
}

