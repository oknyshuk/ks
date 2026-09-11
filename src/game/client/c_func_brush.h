//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//=============================================================================//

#ifndef C_FUNC_BRUSH_H
#define C_FUNC_BRUSH_H

#include "reflect_annotations.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncBrush" } ]]
      C_FuncBrush : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncBrush, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	void OnDataChanged( DataUpdateType_t type );

private:
};

#endif	// C_FUNC_BRUSH_H

