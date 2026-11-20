// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_title.h"
import ks.tier1.convar;
import ks.tier1.iconvar;

static ConVar cl_titledataversionblock1( "cl_titledataversionblock1", "14", FCVAR_DEVELOPMENTONLY, "stats for console title data block1 i/o version." );
static ConVar cl_titledataversionblock2( "cl_titledataversionblock2", "8", FCVAR_DEVELOPMENTONLY, "stats for console title data block2 i/o version." );
static ConVar cl_titledataversionblock3( "cl_titledataversionblock3", "48", FCVAR_DEVELOPMENTONLY, "stats for console title data block3 i/o version." );

static TitleDataFieldsDescription_t const * PrepareTitleDataStorageDescription()
{


#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ \
	TitleDataFieldsDescription_t aTDFD = { szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }; \
	s_tdfd.push_back( aTDFD ); \
}


	static std::vector< TitleDataFieldsDescription_t > s_tdfd;




	// END MARKER
	TD_ENTRY( (const char*) nullptr, DB_TD3, DT_0, 0 )

#undef TD_ENTRY


	return s_tdfd.data();
}

TitleDataFieldsDescription_t const * CMatchTitle::DescribeTitleDataStorage()
{
	static TitleDataFieldsDescription_t const *s_pTDFD = PrepareTitleDataStorageDescription();
	return s_pTDFD;
}

TitleAchievementsDescription_t const * CMatchTitle::DescribeTitleAchievements()
{
	static TitleAchievementsDescription_t tad[] =
	{
//#include "left4dead2.xhelp.achtitledesc.txt"
		// END MARKER
		{ nullptr, 0 }
	};

	return tad;
}

TitleAvatarAwardsDescription_t const * CMatchTitle::DescribeTitleAvatarAwards()
{
	static TitleAvatarAwardsDescription_t taad[] =
	{
//#include "left4dead2.xhelp.avawtitledesc.txt"
		// END MARKER
		{ nullptr, 0 }
	};

	return taad;
}

TitleDlcDescription_t const * CMatchTitle::DescribeTitleDlcs()
{
	static TitleDlcDescription_t tdlcs[] =
	{
		//{ PORTAL2_DLCID_COOP_BOT_SKINS,		PORTAL2_DLC_APPID_COOP_BOT_SKINS,	PORTAL2_DLC_PKGID_COOP_BOT_SKINS,	"DLC.0x12" },
		//{ PORTAL2_DLCID_COOP_BOT_HELMETS,	PORTAL2_DLC_APPID_COOP_BOT_HELMETS,	PORTAL2_DLC_PKGID_COOP_BOT_HELMETS,	"DLC.0x13" },
		//{ PORTAL2_DLCID_COOP_BOT_ANTENNA,	PORTAL2_DLC_APPID_COOP_BOT_ANTENNA,	PORTAL2_DLC_PKGID_COOP_BOT_ANTENNA,	"DLC.0x14" },
		// END MARKER
		{ 0, 0, 0 }
	};

	return tdlcs;
}
