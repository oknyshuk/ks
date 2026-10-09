#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Definition of the info_remarkable entity.
//			This entity is a quick and dirty hack to provide writers
//			with some kind of object that characters can remark upon.
//			It is not performant, because it relies upon each character
//			polling over each of these for visibility.  A better approach
//			will be an object that is notified by the engine when it is within
//			a character's view.

#include "reflect_annotations.h"

#include "baseentity.h"
import std;

class CInfoRemarkable : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoRemarkable, CPointEntity );
	DECLARE_DATADESC();
	typedef std::vector< CInfoRemarkable * > tRemarkableList;
	static tRemarkableList *GetListOfAllThatIsRemarkable( void );

	~CInfoRemarkable();
	void	Spawn( void );
	inline void ResetCount( void ) { m_iTimesRemarkedUpon = 0; } // reset the number of times commented upon (eg at map restart)
	
	inline const char *GetRemarkContext( void ) const
		{ return m_szRemarkContext.ToCStr();	}

	int m_iTimesRemarkedUpon;

//FIX protected:
	[[= ks::reflect::Key{ .name = "contextsubject" } ]] string_t	m_szRemarkContext;
};
