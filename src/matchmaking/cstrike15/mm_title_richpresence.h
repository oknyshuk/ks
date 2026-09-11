//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_TITLE_RICHPRESENCE_H
#define MM_TITLE_RICHPRESENCE_H

#include "mm_title.h"

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings );
void MM_Title_RichPresence_UpdateTeamPropertiesCSGO( KeyValues *pCurrentSettings, KeyValues *pTeamProperties );
void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings );

KeyValues * MM_Title_RichPresence_PrepareForSessionCreate( KeyValues *pSettings );

#endif // MM_TITLE_H
