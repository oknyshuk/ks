// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

module;

module ks.core;

static ProgressReportHandler_t pReportHandlerFN;

void ReportProgress(char const *job_name, int total_units_to_do, int n_units_completed)
{
	if ( pReportHandlerFN )
		(*pReportHandlerFN)( job_name, total_units_to_do, n_units_completed );
}

ProgressReportHandler_t InstallProgressReportHandler( ProgressReportHandler_t pfn)
{
	ProgressReportHandler_t old = pReportHandlerFN;
	pReportHandlerFN = pfn;
	return old;
}
