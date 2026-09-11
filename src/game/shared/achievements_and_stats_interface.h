//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef ACHIEVEMENTSANDSTATSINTERFACE_H
#define ACHIEVEMENTSANDSTATSINTERFACE_H


class AchievementsAndStatsInterface
{
public:
    AchievementsAndStatsInterface() { }

    virtual void DisplayPanel() {}
    virtual void ReleasePanel() {}
	virtual int GetAchievementsPanelMinWidth( void ) const { return 0; }
};


#endif // ACHIEVEMENTSANDSTATSINTERFACE_H
