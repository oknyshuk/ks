// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.core.progress;

export using ProgressReportHandler_t = void (*)( char const*, int, int );
export void ReportProgress( char const *job_name, int total_units_to_do, int n_units_completed );
export ProgressReportHandler_t InstallProgressReportHandler( ProgressReportHandler_t pfn );
