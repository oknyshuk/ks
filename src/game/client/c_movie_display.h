//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef C_MOVIE_DISPLAY_H
#define C_MOVIE_DISPLAY_H

#include "reflect_annotations.h"

#include "cbase.h"

class [[= ks::reflect::NetTable{ .name = "DT_MovieDisplay" } ]]
      C_MovieDisplay : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_MovieDisplay, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_MovieDisplay();
	~C_MovieDisplay();

	bool IsEnabled( void ) const { return m_bEnabled; }
	bool IsLooping( void ) const { return m_bLooping; }
	bool IsStretchingToFill( void ) const { return m_bStretchToFill; }
	bool IsForcedSlave( void ) const { return m_bForcedSlave; }

	bool IsUsingCustomUVs( void ) const { return m_bUseCustomUVs; }
	bool GetWantsToBeMaster( void ) const { return m_bWantsToBeMaster; }
	void SetMasterAttempted( void ) { m_bWantsToBeMaster = false; m_bForcedSlave = false; }
	float GetUMin( void ) const { return m_flUMin; }
	float GetUMax( void ) const { return m_flUMax; }
	float GetVMin( void ) const { return m_flVMin; }
	float GetVMax( void ) const { return m_flVMax; }
	
	virtual void ReceiveMessage( int classID, bf_read &msg );

	const char *GetMovieFilename( void ) const { return m_szMovieFilename; }
	const char *GetGroupName( void ) const { return m_szGroupName; }

private:
	[[= ks::reflect::Net{} ]] bool	m_bEnabled;
	[[= ks::reflect::Net{} ]] bool	m_bLooping;
	[[= ks::reflect::Net{} ]] char	m_szMovieFilename[128];
	[[= ks::reflect::Net{} ]] char	m_szGroupName[128];
	[[= ks::reflect::Net{} ]] bool	m_bStretchToFill;
	[[= ks::reflect::Net{} ]] bool	m_bForcedSlave;
	[[= ks::reflect::Net{} ]] bool	m_bUseCustomUVs;
	bool	m_bWantsToBeMaster;
	[[= ks::reflect::Net{} ]] float	m_flUMin;
	[[= ks::reflect::Net{} ]] float	m_flUMax;
	[[= ks::reflect::Net{} ]] float	m_flVMin;
	[[= ks::reflect::Net{} ]] float	m_flVMax;
};

#endif //C_MOVIE_DISPLAY_H
