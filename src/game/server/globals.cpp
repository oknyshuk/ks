//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== globals.cpp ========================================================

  DLL-wide global variable definitions.
  They're all defined here, for convenient centralization.
  Source files that need them should "extern ..." declare each
  variable, to better document what globals they care about.

*/

#include "cbase.h"
#include "soundent.h"

Vector			g_vecAttackDir(0.0f, 0.0f, 0.0f);
int				g_iSkillLevel = 0;
bool			g_fGameOver = false;
