#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// hltvtest.h: hltv test system
//
//////////////////////////////////////////////////////////////////////

import std;
import ks.tier1;

class CHLTVServer;

class CHLTVTestSystem
{
public:
	CHLTVTestSystem(void);
	~CHLTVTestSystem(void);

	void RunFrame();
	bool StartTest(int nClients, const char *pszAddress);
	void RetryTest(int nClients);
	bool StopsTest();

protected:

	std::vector< std::unique_ptr< CHLTVServer > >	m_Servers;
};

extern CHLTVTestSystem *hltvtest;	// The global HLTV server/object. NULL on xbox.
