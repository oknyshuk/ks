#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CCommand;


inline constexpr auto MAX_BUFFER_SIZE = 2048;

//-----------------------------------------------------------------------------
// Purpose: Holds benchmark data & state
//-----------------------------------------------------------------------------
class CBenchmarkResults
{
public:
	CBenchmarkResults();

	bool IsBenchmarkRunning();
	void StartBenchmark( const CCommand &args );
	void StopBenchmark();
	void SetResultsFilename( const char *pFilename );

private:
	bool m_bIsTestRunning;
	char m_szFilename[256];

	int m_flStartTime;
	int m_iStartFrame;
};

inline CBenchmarkResults *GetBenchResultsMgr()
{
	extern CBenchmarkResults g_BenchmarkResults;
	return &g_BenchmarkResults;
}
