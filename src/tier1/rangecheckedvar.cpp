// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

bool g_bDoRangeChecks = true;


static int g_nDisables = 0;


CDisableRangeChecks::CDisableRangeChecks()
{
	if ( !ThreadInMainThread() )
		return;
	g_nDisables++;
	g_bDoRangeChecks = false;
}


CDisableRangeChecks::~CDisableRangeChecks()
{
	if ( !ThreadInMainThread() )
		return;
	contract_assert( g_nDisables > 0 );
	--g_nDisables;
	if ( g_nDisables == 0 )
	{
		g_bDoRangeChecks = true;
	}
}
