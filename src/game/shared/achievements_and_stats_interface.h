#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class AchievementsAndStatsInterface
{
public:
    AchievementsAndStatsInterface() { }

    virtual void DisplayPanel() {}
    virtual void ReleasePanel() {}
	virtual int GetAchievementsPanelMinWidth( void ) const { return 0; }
};
