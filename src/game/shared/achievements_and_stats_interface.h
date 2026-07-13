//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef ACHIEVEMENTSANDSTATSINTERFACE_H
#define ACHIEVEMENTSANDSTATSINTERFACE_H

#ifdef _WIN32
#pragma once
#endif

class AchievementsAndStatsInterface
{
public:
    AchievementsAndStatsInterface() { }

    virtual void DisplayPanel() {}
    virtual void ReleasePanel() {}
	virtual int GetAchievementsPanelMinWidth( void ) const { return 0; }
};


#endif // ACHIEVEMENTSANDSTATSINTERFACE_H
