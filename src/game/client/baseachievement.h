// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define DECLARE_ACHIEVEMENT_( className, achievementID, achievementName, gameDirFilter, iPointValue, bHidden ) \
static CBaseAchievement *Create_##className( void )					\
{																		\
	CBaseAchievement *pAchievement = new className( );					\
	pAchievement->SetAchievementID( achievementID );					\
	pAchievement->SetName( achievementName );							\
	pAchievement->SetPointValue( iPointValue );							\
	pAchievement->SetHideUntilAchieved( bHidden );						\
	if ( gameDirFilter ) pAchievement->SetGameDirFilter( gameDirFilter ); \
	return pAchievement;												\
};																		\
static CBaseAchievementHelper g_##className##_Helper( Create_##className );
#define DECLARE_ACHIEVEMENT( className, achievementID, achievementName, iPointValue ) \
	DECLARE_ACHIEVEMENT_( className, achievementID, achievementName, nullptr, iPointValue, false )
