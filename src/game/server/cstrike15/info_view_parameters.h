//====== Copyright (c) 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef INFO_VIEW_PARAMETERS_H
#define INFO_VIEW_PARAMETERS_H

#include "reflect_annotations.h"


class CInfoViewParameters : public CBaseEntity
{
public:
	DECLARE_CLASS( CInfoViewParameters, CBaseEntity );
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "ViewMode" } ]] int m_nViewMode;
};


#endif // INFO_VIEW_PARAMETERS_H
