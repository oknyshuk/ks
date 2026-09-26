//========= Copyright (c) , Valve Corporation, All rights reserved. ============//

#ifndef DBGINPUT_HDR
#define DBGINPUT_HDR

#include "threadtools.h"
#include <mutex>

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

#endif
