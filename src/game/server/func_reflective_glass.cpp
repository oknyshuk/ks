//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "modelentities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncReflectiveGlass" } ]]
      CFuncReflectiveGlass : public CFuncBrush
{
	DECLARE_CLASS( CFuncReflectiveGlass, CFuncBrush );
	DECLARE_SERVERCLASS();
};

// automatically hooks in the system's callbacks

LINK_ENTITY_TO_CLASS( func_reflective_glass, CFuncReflectiveGlass );

IMPLEMENT_REFLECT_SERVERCLASS( CFuncReflectiveGlass, DT_FuncReflectiveGlass )
