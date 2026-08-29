//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Server-side counterpart to C_MemoryLog (see c_memorylog.h)
//
//=============================================================================//

#ifndef MEMORYLOG_H
#define MEMORYLOG_H


#include "igamesystem.h"

class CMemoryLog : public CAutoGameSystemPerFrame
{
public:
	// Methods of IGameSystem
	virtual void LevelInitPostEntity();

private:
};


#endif // MEMORYLOG_H
