//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Point entity used to create templates out of other entities or groups of entities
//
//=============================================================================//

#ifndef POINT_TEMPLATE_H
#define POINT_TEMPLATE_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#define MAX_NUM_TEMPLATES		16

struct template_t
{
	int			iTemplateIndex;
	VMatrix		matEntityToTemplate;

	DECLARE_SIMPLE_DATADESC();
};

void PrecachePointTemplates();

//-----------------------------------------------------------------------------

void ScriptInstallPreSpawnHook();
bool ScriptPreInstanceSpawn( CScriptScope *pScriptScope, CBaseEntity *pChild, string_t iszKeyValueData );
void ScriptPostSpawn( CScriptScope *pScriptScope, CBaseEntity **ppEntities, int nEntities );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointTemplate : public CLogicalEntity
{
	DECLARE_CLASS( CPointTemplate, CLogicalEntity );
public:
	DECLARE_DATADESC();

	virtual void	Spawn( void );
	virtual void	Precache();

	// Template initialization
	void			StartBuildingTemplates( void );
	void			FinishBuildingTemplates( void );

	// Template Entity accessors
	int				GetNumTemplateEntities( void );
	CBaseEntity		*GetTemplateEntity( int iTemplateNumber );
	void			AddTemplate( CBaseEntity *pEntity, const char *pszMapData, int nLen );
	bool			ShouldRemoveTemplateEntities( void );
	bool			AllowNameFixup();

	// Templates accessors
	int				GetNumTemplates( void );
	int				GetTemplateIndexForTemplate( int iTemplate );

	// Template instancing
	bool			CreateInstance( const Vector &vecOrigin, const QAngle &vecAngles, CUtlVector<CBaseEntity*> *pEntities, CBaseEntity *pEntityMaker = NULL, bool bCreateTime = false );
	void			CreationComplete( const CUtlVector<CBaseEntity*> &entities );

	// Inputs
	[[= ks::reflect::Input{ .name = "ForceSpawn", .type = FIELD_VOID } ]] void			InputForceSpawn( inputdata_t &inputdata );

	virtual void	PerformPrecache();

private:
	[[= ks::reflect::Key{ .name = "Template16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Template15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Template14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Template13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Template12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Template11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Template10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Template09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Template08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Template07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Template06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Template05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Template04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Template03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Template02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Template01", .index = 0 } ]] string_t						m_iszTemplateEntityNames[MAX_NUM_TEMPLATES];

	// List of map entities this template targets. Built inside our Spawn().
	// It's only valid between Spawn() & Activate(), because the map entity parsing
	// code removes all the entities in it once it finishes turning them into templates.
	CUtlVector< CBaseEntity * >		m_hTemplateEntities;

	// List of templates, generated from our template entities.
	CUtlVector< template_t >		m_hTemplates;

	[[= ks::reflect::Key{ .name = "OnEntitySpawned" } ]] COutputEvent					m_pOutputOnSpawned;
};

#endif // POINT_TEMPLATE_H
