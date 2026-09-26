//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_func_brush.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncMonitor" } ]]
      C_FuncMonitor : public C_FuncBrush
{
public:
	DECLARE_CLASS( C_FuncMonitor, C_FuncBrush );
	DECLARE_CLIENTCLASS();

// C_BaseEntity.
public:
	virtual bool	ShouldDraw();
};

IMPLEMENT_REFLECT_CLIENTCLASS( C_FuncMonitor, DT_FuncMonitor, CFuncMonitor )

bool C_FuncMonitor::ShouldDraw()
{
	return true;
}
