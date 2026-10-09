#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.core;
import std;

class CDebugInputThread: public CThread
{
public:
	std::recursive_mutex m_mx;
	std::string m_inputString;
	bool m_bStop;

	CDebugInputThread()
	{
		m_bStop = false;
	}
	~CDebugInputThread()
	{

	}

	void Stop()
	{
		m_bStop = true;
		CThread::Stop();
	}

	virtual int Run( void )
	{
		return 0;
	}
};

extern CDebugInputThread * g_pDebugInputThread;
