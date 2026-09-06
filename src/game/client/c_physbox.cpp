//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_physbox.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_PhysBox, DT_PhysBox, CPhysBox )


C_PhysBox::C_PhysBox()
{
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_PhysBox::ShadowCastType()
{
	if (IsEffectActive(EF_NODRAW | EF_NOSHADOW))
		return SHADOWS_NONE;
	return SHADOWS_RENDER_TO_TEXTURE;
}

C_PhysBox::~C_PhysBox()
{
}

