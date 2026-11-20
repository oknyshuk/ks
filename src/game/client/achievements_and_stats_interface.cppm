// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module ks.client:achievements_and_stats_interface;


class AchievementsAndStatsInterface
{
public:
    AchievementsAndStatsInterface() { }

    virtual void DisplayPanel() {}
    virtual void ReleasePanel() {}
	virtual int GetAchievementsPanelMinWidth( void ) const { return 0; }
};
