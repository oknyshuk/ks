// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// User messages, plus the handful of Game Coordinator types they embed. Same shape as
// netmessages_schema.h: members carry a field number and nothing else.

#ifndef KS_USERMESSAGES_SCHEMA_H
#define KS_USERMESSAGES_SCHEMA_H

#include <cstdint>
#include <string>
#include <vector>

#include "netmessages_schema.h"
#include "proto_types.h"

namespace ks::net
{

enum ECstrike15UserMessages : int
{
	CS_UM_VGUIMenu = 1,
	CS_UM_Geiger = 2,
	CS_UM_Train = 3,
	CS_UM_HudText = 4,
	CS_UM_SayText = 5,
	CS_UM_SayText2 = 6,
	CS_UM_TextMsg = 7,
	CS_UM_HudMsg = 8,
	CS_UM_ResetHud = 9,
	CS_UM_GameTitle = 10,
	CS_UM_Shake = 12,
	CS_UM_Fade = 13,
	CS_UM_Rumble = 14,
	CS_UM_CloseCaption = 15,
	CS_UM_CloseCaptionDirect = 16,
	CS_UM_SendAudio = 17,
	CS_UM_RawAudio = 18,
	CS_UM_VoiceMask = 19,
	CS_UM_RequestState = 20,
	CS_UM_Damage = 21,
	CS_UM_RadioText = 22,
	CS_UM_HintText = 23,
	CS_UM_KeyHintText = 24,
	CS_UM_ProcessSpottedEntityUpdate = 25,
	CS_UM_ReloadEffect = 26,
	CS_UM_AdjustMoney = 27,
	CS_UM_UpdateTeamMoney = 28,
	CS_UM_StopSpectatorMode = 29,
	CS_UM_KillCam = 30,
	CS_UM_DesiredTimescale = 31,
	CS_UM_CurrentTimescale = 32,
	CS_UM_AchievementEvent = 33,
	CS_UM_MatchEndConditions = 34,
	CS_UM_DisconnectToLobby = 35,
	CS_UM_PlayerStatsUpdate = 36,
	CS_UM_DisplayInventory = 37,
	CS_UM_WarmupHasEnded = 38,
	CS_UM_ClientInfo = 39,
	CS_UM_XRankGet = 40,
	CS_UM_XRankUpd = 41,
	CS_UM_CallVoteFailed = 45,
	CS_UM_VoteStart = 46,
	CS_UM_VotePass = 47,
	CS_UM_VoteFailed = 48,
	CS_UM_VoteSetup = 49,
	CS_UM_ServerRankRevealAll = 50,
	CS_UM_SendLastKillerDamageToClient = 51,
	CS_UM_ServerRankUpdate = 52,
	CS_UM_ItemPickup = 53,
	CS_UM_ShowMenu = 54,
	CS_UM_BarTime = 55,
	CS_UM_AmmoDenied = 56,
	CS_UM_MarkAchievement = 57,
	CS_UM_MatchStatsUpdate = 58,
	CS_UM_ItemDrop = 59,
	CS_UM_GlowPropTurnOff = 60,
	CS_UM_SendPlayerItemDrops = 61,
	CS_UM_RoundBackupFilenames = 62,
	CS_UM_SendPlayerItemFound = 63,
	CS_UM_ReportHit = 64,
	CS_UM_XpUpdate = 65,
	CS_UM_QuestProgress = 66,
	CS_UM_ScoreLeaderboardData = 67,
	CS_UM_PlayerDecalDigitalSignature = 68,
	CS_UM_WeaponSound = 69,
	CS_UM_UpdateScreenHealthBar = 70,
	CS_UM_EntityOutlineHighlight = 71,
	CS_UM_SSUI = 72,
	CS_UM_SurvivalStats = 73,
	CS_UM_DisconnectToLobby2 = 74,
	CS_UM_EndOfMatchAllPlayersData = 75,
};

enum ECSUsrMsg_DisconnectToLobby_Action : int
{
	k_ECSUsrMsg_DisconnectToLobby_Action_Default = 0,
	k_ECSUsrMsg_DisconnectToLobby_Action_GoQueue = 1,
};

enum ECsgoGCMsg : int
{
	k_EMsgGCCStrike15_v2_Base = 9100,
	k_EMsgGCCStrike15_v2_MatchmakingStart = 9101,
	k_EMsgGCCStrike15_v2_MatchmakingStop = 9102,
	k_EMsgGCCStrike15_v2_MatchmakingClient2ServerPing = 9103,
	k_EMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate = 9104,
	k_EMsgGCCStrike15_v2_MatchmakingServerReservationResponse = 9106,
	k_EMsgGCCStrike15_v2_MatchmakingGC2ClientReserve = 9107,
	k_EMsgGCCStrike15_v2_MatchmakingClient2GCHello = 9109,
	k_EMsgGCCStrike15_v2_MatchmakingGC2ClientHello = 9110,
	k_EMsgGCCStrike15_v2_MatchmakingGC2ClientAbandon = 9112,
	k_EMsgGCCStrike15_v2_MatchmakingGCOperationalStats = 9115,
	k_EMsgGCCStrike15_v2_MatchmakingOperator2GCBlogUpdate = 9117,
	k_EMsgGCCStrike15_v2_ServerNotificationForUserPenalty = 9118,
	k_EMsgGCCStrike15_v2_ClientReportPlayer = 9119,
	k_EMsgGCCStrike15_v2_ClientReportServer = 9120,
	k_EMsgGCCStrike15_v2_ClientCommendPlayer = 9121,
	k_EMsgGCCStrike15_v2_ClientReportResponse = 9122,
	k_EMsgGCCStrike15_v2_ClientCommendPlayerQuery = 9123,
	k_EMsgGCCStrike15_v2_ClientCommendPlayerQueryResponse = 9124,
	k_EMsgGCCStrike15_v2_WatchInfoUsers = 9126,
	k_EMsgGCCStrike15_v2_ClientRequestPlayersProfile = 9127,
	k_EMsgGCCStrike15_v2_PlayersProfile = 9128,
	k_EMsgGCCStrike15_v2_PlayerOverwatchCaseUpdate = 9131,
	k_EMsgGCCStrike15_v2_PlayerOverwatchCaseAssignment = 9132,
	k_EMsgGCCStrike15_v2_PlayerOverwatchCaseStatus = 9133,
	k_EMsgGCCStrike15_v2_GC2ClientTextMsg = 9134,
	k_EMsgGCCStrike15_v2_Client2GCTextMsg = 9135,
	k_EMsgGCCStrike15_v2_MatchEndRunRewardDrops = 9136,
	k_EMsgGCCStrike15_v2_MatchEndRewardDropsNotification = 9137,
	k_EMsgGCCStrike15_v2_ClientRequestWatchInfoFriends2 = 9138,
	k_EMsgGCCStrike15_v2_MatchList = 9139,
	k_EMsgGCCStrike15_v2_MatchListRequestCurrentLiveGames = 9140,
	k_EMsgGCCStrike15_v2_MatchListRequestRecentUserGames = 9141,
	k_EMsgGCCStrike15_v2_GC2ServerReservationUpdate = 9142,
	k_EMsgGCCStrike15_v2_ClientVarValueNotificationInfo = 9144,
	k_EMsgGCCStrike15_v2_MatchListRequestTournamentGames = 9146,
	k_EMsgGCCStrike15_v2_MatchListRequestFullGameInfo = 9147,
	k_EMsgGCCStrike15_v2_GiftsLeaderboardRequest = 9148,
	k_EMsgGCCStrike15_v2_GiftsLeaderboardResponse = 9149,
	k_EMsgGCCStrike15_v2_ServerVarValueNotificationInfo = 9150,
	k_EMsgGCCStrike15_v2_ClientSubmitSurveyVote = 9152,
	k_EMsgGCCStrike15_v2_Server2GCClientValidate = 9153,
	k_EMsgGCCStrike15_v2_MatchListRequestLiveGameForUser = 9154,
	k_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockRequest = 9156,
	k_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse = 9157,
	k_EMsgGCCStrike15_v2_AccountPrivacySettings = 9158,
	k_EMsgGCCStrike15_v2_SetMyActivityInfo = 9159,
	k_EMsgGCCStrike15_v2_MatchListRequestTournamentPredictions = 9160,
	k_EMsgGCCStrike15_v2_MatchListUploadTournamentPredictions = 9161,
	k_EMsgGCCStrike15_v2_DraftSummary = 9162,
	k_EMsgGCCStrike15_v2_ClientRequestJoinFriendData = 9163,
	k_EMsgGCCStrike15_v2_ClientRequestJoinServerData = 9164,
	k_EMsgGCCStrike15_v2_ClientRequestNewMission = 9165,
	k_EMsgGCCStrike15_v2_GC2ClientTournamentInfo = 9167,
	k_EMsgGC_GlobalGame_Subscribe = 9168,
	k_EMsgGC_GlobalGame_Unsubscribe = 9169,
	k_EMsgGC_GlobalGame_Play = 9170,
	k_EMsgGCCStrike15_v2_AcknowledgePenalty = 9171,
	k_EMsgGCCStrike15_v2_Client2GCRequestPrestigeCoin = 9172,
	k_EMsgGCCStrike15_v2_GC2ClientGlobalStats = 9173,
	k_EMsgGCCStrike15_v2_Client2GCStreamUnlock = 9174,
	k_EMsgGCCStrike15_v2_FantasyRequestClientData = 9175,
	k_EMsgGCCStrike15_v2_FantasyUpdateClientData = 9176,
	k_EMsgGCCStrike15_v2_GCToClientSteamdatagramTicket = 9177,
	k_EMsgGCCStrike15_v2_ClientToGCRequestTicket = 9178,
	k_EMsgGCCStrike15_v2_ClientToGCRequestElevate = 9179,
	k_EMsgGCCStrike15_v2_GlobalChat = 9180,
	k_EMsgGCCStrike15_v2_GlobalChat_Subscribe = 9181,
	k_EMsgGCCStrike15_v2_GlobalChat_Unsubscribe = 9182,
	k_EMsgGCCStrike15_v2_ClientAuthKeyCode = 9183,
	k_EMsgGCCStrike15_v2_GotvSyncPacket = 9184,
	k_EMsgGCCStrike15_v2_ClientPlayerDecalSign = 9185,
	k_EMsgGCCStrike15_v2_ClientLogonFatalError = 9187,
	k_EMsgGCCStrike15_v2_ClientPollState = 9188,
	k_EMsgGCCStrike15_v2_Party_Register = 9189,
	k_EMsgGCCStrike15_v2_Party_Unregister = 9190,
	k_EMsgGCCStrike15_v2_Party_Search = 9191,
	k_EMsgGCCStrike15_v2_Party_Invite = 9192,
	k_EMsgGCCStrike15_v2_Account_RequestCoPlays = 9193,
	k_EMsgGCCStrike15_v2_ClientGCRankUpdate = 9194,
	k_EMsgGCCStrike15_v2_ClientRequestOffers = 9195,
	k_EMsgGCCStrike15_v2_ClientAccountBalance = 9196,
	k_EMsgGCCStrike15_v2_ClientPartyJoinRelay = 9197,
	k_EMsgGCCStrike15_v2_ClientPartyWarning = 9198,
	k_EMsgGCCStrike15_v2_SetEventFavorite = 9200,
	k_EMsgGCCStrike15_v2_GetEventFavorites_Request = 9201,
	k_EMsgGCCStrike15_v2_ClientPerfReport = 9202,
	k_EMsgGCCStrike15_v2_GetEventFavorites_Response = 9203,
	k_EMsgGCCStrike15_v2_ClientRequestSouvenir = 9204,
	k_EMsgGCCStrike15_v2_ClientReportValidation = 9205,
	k_EMsgGCCStrike15_v2_GC2ClientRefuseSecureMode = 9206,
	k_EMsgGCCStrike15_v2_GC2ClientRequestValidation = 9207,
};

enum ECsgoSteamUserStat : int
{
	k_ECsgoSteamUserStat_XpEarnedGames = 1,
	k_ECsgoSteamUserStat_MatchWinsCompetitive = 2,
	k_ECsgoSteamUserStat_SurvivedDangerZone = 3,
};

enum EClientReportingVersion : int
{
	k_EClientReportingVersion_OldVersion = 0,
	k_EClientReportingVersion_BetaVersion = 1,
	k_EClientReportingVersion_SupportsTrustedMode = 2,
};

struct CCSUsrMsg_AchievementEvent
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> achievement;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> count;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> user_id;
};

struct CCSUsrMsg_AdjustMoney
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> amount;
};

struct CCSUsrMsg_CallVoteFailed
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> reason;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> time;
};

struct CCSUsrMsg_CloseCaption
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> hash;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> duration;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> from_player;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> cctoken;
};

struct CCSUsrMsg_CloseCaptionDirect
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> hash;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> duration;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> from_player;
};

struct CCSUsrMsg_CurrentTimescale
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<float> cur_timescale;
};

struct CCSUsrMsg_Damage
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> amount;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<CMsgVector> inflictor_world_pos;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> victim_entindex;
};

struct CCSUsrMsg_DesiredTimescale
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<float> desired_timescale;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> duration_realtime_sec;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> interpolator_type;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<float> start_blend_time;
};

struct CCSUsrMsg_DisconnectToLobby
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> dummy;
};

struct CCSUsrMsg_DisplayInventory
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> display;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> user_id;
};

struct CCSUsrMsg_Fade
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> duration;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> hold_time;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> flags;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<CMsgRGBA> clr;
};

struct CCSUsrMsg_GameTitle
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> dummy;
};

struct CCSUsrMsg_Geiger
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> range;
};

struct CCSUsrMsg_GlowPropTurnOff
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> entidx;
};

struct CCSUsrMsg_HintText
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> text;
};

struct CCSUsrMsg_HudMsg
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> channel;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<CMsgVector2D> pos;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<CMsgRGBA> clr1;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<CMsgRGBA> clr2;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> effect;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<float> fade_in_time;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<float> fade_out_time;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<float> hold_time;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<float> fx_time;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<std::string> text;
};

struct CCSUsrMsg_HudText
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> text;
};

struct CCSUsrMsg_ItemDrop
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int64_t> itemid;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<bool> death;
};

struct CCSUsrMsg_KeyHintText
{
	[[= ks::proto::Pb{ 1 }]] std::vector<std::string> hints;
};

struct CCSUsrMsg_KillCam
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> obs_mode;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> first_target;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> second_target;
};

struct CCSUsrMsg_MatchEndConditions
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> fraglimit;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> mp_maxrounds;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> mp_winlimit;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> mp_timelimit;
};

struct PlayerDecalDigitalSignature
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> signature;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> accountid;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> rtime;
	[[= ks::proto::Pb{ 4 }]] std::vector<float> endpos;
	[[= ks::proto::Pb{ 5 }]] std::vector<float> startpos;
	[[= ks::proto::Pb{ 6 }]] std::vector<float> right;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<uint32_t> tx_defidx;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<int32_t> entindex;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<uint32_t> hitbox;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<float> creationtime;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<uint32_t> equipslot;
	[[= ks::proto::Pb{ 12 }]] ks::proto::opt<uint32_t> trace_id;
	[[= ks::proto::Pb{ 13 }]] std::vector<float> normal;
	[[= ks::proto::Pb{ 14 }]] ks::proto::opt<uint32_t> tint_id;
};

struct CCSUsrMsg_PlayerDecalDigitalSignature
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<PlayerDecalDigitalSignature> data;
};

struct CCSUsrMsg_PlayerStatsUpdate
{
	struct Stat
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> idx;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> delta;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> version;
	[[= ks::proto::Pb{ 4 }]] std::vector<CCSUsrMsg_PlayerStatsUpdate::Stat> stats;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> user_id;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> crc;
};

struct CCSUsrMsg_ProcessSpottedEntityUpdate
{
	struct SpottedEntityUpdate
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> entity_idx;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> class_id;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> origin_x;
		[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> origin_y;
		[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> origin_z;
		[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> angle_y;
		[[= ks::proto::Pb{ 7 }]] ks::proto::opt<bool> defuser;
		[[= ks::proto::Pb{ 8 }]] ks::proto::opt<bool> player_has_defuser;
		[[= ks::proto::Pb{ 9 }]] ks::proto::opt<bool> player_has_c4;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> new_update;
	[[= ks::proto::Pb{ 2 }]] std::vector<CCSUsrMsg_ProcessSpottedEntityUpdate::SpottedEntityUpdate> entity_updates;
};

struct CCSUsrMsg_QuestProgress
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> quest_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> normal_points;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> bonus_points;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> is_event_quest;
};

struct CCSUsrMsg_RadioText
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> msg_dst;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> client;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> msg_name;
	[[= ks::proto::Pb{ 4 }]] std::vector<std::string> params;
};

struct CCSUsrMsg_RawAudio
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> pitch;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> entidx;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> duration;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> voice_filename;
};

struct CCSUsrMsg_ReloadEffect
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> entidx;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> actanim;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> origin_x;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<float> origin_y;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<float> origin_z;
};

struct CCSUsrMsg_ReportHit
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<float> pos_x;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> pos_y;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> pos_z;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<float> timestamp;
};

struct CCSUsrMsg_RequestState
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> dummy;
};

struct CCSUsrMsg_ResetHud
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> reset;
};

struct CCSUsrMsg_RoundBackupFilenames
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> count;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> index;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> filename;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> nicename;
};

struct CCSUsrMsg_Rumble
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> index;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> data;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> flags;
};

struct CCSUsrMsg_SayText
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> ent_idx;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> text;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> chat;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> textallchat;
};

struct CCSUsrMsg_SayText2
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> ent_idx;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<bool> chat;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> msg_name;
	[[= ks::proto::Pb{ 4 }]] std::vector<std::string> params;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<bool> textallchat;
};

struct ScoreLeaderboardData
{
	struct Entry
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> tag;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> val;
	};

	struct AccountEntries
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> accountid;
		[[= ks::proto::Pb{ 2 }]] std::vector<ScoreLeaderboardData::Entry> entries;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint64_t> quest_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> score;
	[[= ks::proto::Pb{ 3 }]] std::vector<ScoreLeaderboardData::AccountEntries> accountentries;
	[[= ks::proto::Pb{ 5 }]] std::vector<ScoreLeaderboardData::Entry> matchentries;
};

struct CCSUsrMsg_ScoreLeaderboardData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<ScoreLeaderboardData> data;
};

struct CCSUsrMsg_SendAudio
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> radio_sound;
};

struct CCSUsrMsg_SendLastKillerDamageToClient
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> num_hits_given;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> damage_given;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> num_hits_taken;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> damage_taken;
};

struct CEconItemPreviewDataBlock
{
	struct Sticker
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> slot;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> sticker_id;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> wear;
		[[= ks::proto::Pb{ 4 }]] ks::proto::opt<float> scale;
		[[= ks::proto::Pb{ 5 }]] ks::proto::opt<float> rotation;
		[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> tint_id;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> accountid;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint64_t> itemid;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> defindex;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> paintindex;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> rarity;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> quality;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<uint32_t> paintwear;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<uint32_t> paintseed;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<uint32_t> killeaterscoretype;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<uint32_t> killeatervalue;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<std::string> customname;
	[[= ks::proto::Pb{ 12 }]] std::vector<CEconItemPreviewDataBlock::Sticker> stickers;
	[[= ks::proto::Pb{ 13 }]] ks::proto::opt<uint32_t> inventory;
	[[= ks::proto::Pb{ 14 }]] ks::proto::opt<uint32_t> origin;
	[[= ks::proto::Pb{ 15 }]] ks::proto::opt<uint32_t> questid;
	[[= ks::proto::Pb{ 16 }]] ks::proto::opt<uint32_t> dropreason;
	[[= ks::proto::Pb{ 17 }]] ks::proto::opt<uint32_t> musicindex;
	[[= ks::proto::Pb{ 18 }]] ks::proto::opt<int32_t> entindex;
};

struct CCSUsrMsg_SendPlayerItemDrops
{
	[[= ks::proto::Pb{ 1 }]] std::vector<CEconItemPreviewDataBlock> entity_updates;
};

struct CCSUsrMsg_SendPlayerItemFound
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<CEconItemPreviewDataBlock> iteminfo;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> entindex;
};

struct CDataGCCStrike15_v2_TournamentMatchDraft
{
	struct Entry
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> mapid;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> team_id_ct;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> event_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> event_stage_id;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> team_id_0;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> team_id_1;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> maps_count;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> maps_current;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<int32_t> team_id_start;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<int32_t> team_id_veto1;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<int32_t> team_id_pickn;
	[[= ks::proto::Pb{ 10 }]] std::vector<CDataGCCStrike15_v2_TournamentMatchDraft::Entry> drafts;
};

struct CPreMatchInfoData
{
	struct TeamStats
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> match_info_idxtxt;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> match_info_txt;
		[[= ks::proto::Pb{ 3 }]] std::vector<std::string> match_info_teams;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> predictions_pct;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<CDataGCCStrike15_v2_TournamentMatchDraft> draft;
	[[= ks::proto::Pb{ 5 }]] std::vector<CPreMatchInfoData::TeamStats> stats;
	[[= ks::proto::Pb{ 6 }]] std::vector<int32_t> wins;
};

struct IpAddressMask
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> a;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> b;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> c;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> d;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> bits;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> token;
};

struct PlayerRankingInfo
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> account_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> rank_id;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> wins;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<float> rank_change;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> rank_type_id;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<uint32_t> tv_control;
};

struct TournamentEvent
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> event_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> event_tag;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> event_name;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> event_time_start;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> event_time_end;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> event_public;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<int32_t> event_stage_id;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<std::string> event_stage_name;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<uint32_t> active_section_id;
};

struct TournamentPlayer
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> account_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> player_nick;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> player_name;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> player_dob;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<std::string> player_flag;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<std::string> player_location;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<std::string> player_desc;
};

struct TournamentTeam
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> team_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> team_tag;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> team_flag;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> team_name;
	[[= ks::proto::Pb{ 5 }]] std::vector<TournamentPlayer> players;
};

struct CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve
{
	[[= ks::proto::Pb{ 1 }]] std::vector<uint32_t> account_ids;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> game_type;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint64_t> match_id;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> server_version;
	[[= ks::proto::Pb{ 5 }]] std::vector<PlayerRankingInfo> rankings;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint64_t> encryption_key;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<uint64_t> encryption_key_pub;
	[[= ks::proto::Pb{ 8 }]] std::vector<uint32_t> party_ids;
	[[= ks::proto::Pb{ 9 }]] std::vector<IpAddressMask> whitelist;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<uint64_t> tv_master_steamid;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<TournamentEvent> tournament_event;
	[[= ks::proto::Pb{ 12 }]] std::vector<TournamentTeam> tournament_teams;
	[[= ks::proto::Pb{ 13 }]] std::vector<uint32_t> tournament_casters_account_ids;
	[[= ks::proto::Pb{ 14 }]] ks::proto::opt<uint64_t> tv_relay_steamid;
	[[= ks::proto::Pb{ 15 }]] ks::proto::opt<CPreMatchInfoData> pre_match_data;
	[[= ks::proto::Pb{ 16 }]] ks::proto::opt<uint32_t> rtime32_event_start;
	[[= ks::proto::Pb{ 17 }]] ks::proto::opt<uint32_t> tv_control;
	[[= ks::proto::Pb{ 18 }]] ks::proto::opt<uint32_t> flags;
};

struct CCSUsrMsg_ServerRankRevealAll
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> seconds_till_shutdown;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve> reservation;
};

struct CCSUsrMsg_ServerRankUpdate
{
	struct RankUpdate
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> account_id;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> rank_old;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> rank_new;
		[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> num_wins;
		[[= ks::proto::Pb{ 5 }]] ks::proto::opt<float> rank_change;
		[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> rank_type_id;
	};

	[[= ks::proto::Pb{ 1 }]] std::vector<CCSUsrMsg_ServerRankUpdate::RankUpdate> rank_update;
};

struct CCSUsrMsg_Shake
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> command;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> local_amplitude;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> frequency;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<float> duration;
};

struct CCSUsrMsg_ShowMenu
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> bits_valid_slots;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> display_time;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> menu_string;
};

struct CCSUsrMsg_StopSpectatorMode
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> dummy;
};

struct CCSUsrMsg_TextMsg
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> msg_dst;
	[[= ks::proto::Pb{ 3 }]] std::vector<std::string> params;
};

struct CCSUsrMsg_Train
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> train;
};

struct CCSUsrMsg_VGUIMenu
{
	struct Subkey
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> name;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> str;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> name;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<bool> show;
	[[= ks::proto::Pb{ 3 }]] std::vector<CCSUsrMsg_VGUIMenu::Subkey> subkeys;
};

struct CCSUsrMsg_VoiceMask
{
	struct PlayerMask
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> game_rules_mask;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> ban_masks;
	};

	[[= ks::proto::Pb{ 1 }]] std::vector<CCSUsrMsg_VoiceMask::PlayerMask> player_masks;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<bool> player_mod_enable;
};

struct CCSUsrMsg_VoteFailed
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> team;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> reason;
};

struct CCSUsrMsg_VotePass
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> team;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> vote_type;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> disp_str;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> details_str;
};

struct CCSUsrMsg_VoteSetup
{
	[[= ks::proto::Pb{ 1 }]] std::vector<std::string> potential_issues;
};

struct CCSUsrMsg_VoteStart
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> team;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> ent_idx;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> vote_type;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> disp_str;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<std::string> details_str;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<std::string> other_team_str;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<bool> is_yes_no_vote;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<int32_t> entidx_target;
};

struct CCSUsrMsg_WarmupHasEnded
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> dummy;
};

struct XpProgressData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> xp_points;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> xp_category;
};

struct CMsgGCCstrike15_v2_GC2ServerNotifyXPRewarded
{
	[[= ks::proto::Pb{ 1 }]] std::vector<XpProgressData> xp_progress_data;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> account_id;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> current_xp;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> current_level;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> upgraded_defidx;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> operation_points_awarded;
};

struct CCSUsrMsg_XpUpdate
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<CMsgGCCstrike15_v2_GC2ServerNotifyXPRewarded> data;
};

struct CEngineGotvSyncPacket
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint64_t> match_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> instance_id;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> signupfragment;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> currentfragment;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<float> tickrate;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> tick;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<float> rtdelay;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<float> rcvage;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<float> keyframe_interval;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<uint32_t> cdndelay;
};

struct CMsgGCCStrike15_v2_MatchmakingGC2ServerConfirm
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> token;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> stamp;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint64_t> exchange;
};

struct CMsgGCCStrike15_v2_MatchmakingServerRoundStats
{
	struct DropInfo
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> account_mvp;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint64_t> reservationid;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve> reservation;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> map;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> round;
	[[= ks::proto::Pb{ 5 }]] std::vector<int32_t> kills;
	[[= ks::proto::Pb{ 6 }]] std::vector<int32_t> assists;
	[[= ks::proto::Pb{ 7 }]] std::vector<int32_t> deaths;
	[[= ks::proto::Pb{ 8 }]] std::vector<int32_t> scores;
	[[= ks::proto::Pb{ 9 }]] std::vector<int32_t> pings;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<int32_t> round_result;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<int32_t> match_result;
	[[= ks::proto::Pb{ 12 }]] std::vector<int32_t> team_scores;
	[[= ks::proto::Pb{ 13 }]] ks::proto::opt<CMsgGCCStrike15_v2_MatchmakingGC2ServerConfirm> confirm;
	[[= ks::proto::Pb{ 14 }]] ks::proto::opt<int32_t> reservation_stage;
	[[= ks::proto::Pb{ 15 }]] ks::proto::opt<int32_t> match_duration;
	[[= ks::proto::Pb{ 16 }]] std::vector<int32_t> enemy_kills;
	[[= ks::proto::Pb{ 17 }]] std::vector<int32_t> enemy_headshots;
	[[= ks::proto::Pb{ 18 }]] std::vector<int32_t> enemy_3ks;
	[[= ks::proto::Pb{ 19 }]] std::vector<int32_t> enemy_4ks;
	[[= ks::proto::Pb{ 20 }]] std::vector<int32_t> enemy_5ks;
	[[= ks::proto::Pb{ 21 }]] std::vector<int32_t> mvps;
	[[= ks::proto::Pb{ 22 }]] ks::proto::opt<uint32_t> spectators_count;
	[[= ks::proto::Pb{ 23 }]] ks::proto::opt<uint32_t> spectators_count_tv;
	[[= ks::proto::Pb{ 24 }]] ks::proto::opt<uint32_t> spectators_count_lnk;
	[[= ks::proto::Pb{ 25 }]] std::vector<int32_t> enemy_kills_agg;
	[[= ks::proto::Pb{ 26 }]] ks::proto::opt<CMsgGCCStrike15_v2_MatchmakingServerRoundStats::DropInfo> drop_info;
	[[= ks::proto::Pb{ 27 }]] ks::proto::opt<bool> b_switched_teams;
	[[= ks::proto::Pb{ 28 }]] std::vector<int32_t> enemy_2ks;
};

} // namespace ks::net

#define KS_USER_MESSAGES( X ) \
	X( CCSUsrMsg_AchievementEvent ) \
	X( CCSUsrMsg_AdjustMoney ) \
	X( CCSUsrMsg_CallVoteFailed ) \
	X( CCSUsrMsg_CloseCaption ) \
	X( CCSUsrMsg_CloseCaptionDirect ) \
	X( CCSUsrMsg_CurrentTimescale ) \
	X( CCSUsrMsg_Damage ) \
	X( CCSUsrMsg_DesiredTimescale ) \
	X( CCSUsrMsg_DisconnectToLobby ) \
	X( CCSUsrMsg_DisplayInventory ) \
	X( CCSUsrMsg_Fade ) \
	X( CCSUsrMsg_GameTitle ) \
	X( CCSUsrMsg_Geiger ) \
	X( CCSUsrMsg_GlowPropTurnOff ) \
	X( CCSUsrMsg_HintText ) \
	X( CCSUsrMsg_HudMsg ) \
	X( CCSUsrMsg_HudText ) \
	X( CCSUsrMsg_ItemDrop ) \
	X( CCSUsrMsg_KeyHintText ) \
	X( CCSUsrMsg_KillCam ) \
	X( CCSUsrMsg_MatchEndConditions ) \
	X( PlayerDecalDigitalSignature ) \
	X( CCSUsrMsg_PlayerDecalDigitalSignature ) \
	X( CCSUsrMsg_PlayerStatsUpdate ) \
	X( CCSUsrMsg_ProcessSpottedEntityUpdate ) \
	X( CCSUsrMsg_QuestProgress ) \
	X( CCSUsrMsg_RadioText ) \
	X( CCSUsrMsg_RawAudio ) \
	X( CCSUsrMsg_ReloadEffect ) \
	X( CCSUsrMsg_ReportHit ) \
	X( CCSUsrMsg_RequestState ) \
	X( CCSUsrMsg_ResetHud ) \
	X( CCSUsrMsg_RoundBackupFilenames ) \
	X( CCSUsrMsg_Rumble ) \
	X( CCSUsrMsg_SayText ) \
	X( CCSUsrMsg_SayText2 ) \
	X( ScoreLeaderboardData ) \
	X( CCSUsrMsg_ScoreLeaderboardData ) \
	X( CCSUsrMsg_SendAudio ) \
	X( CCSUsrMsg_SendLastKillerDamageToClient ) \
	X( CEconItemPreviewDataBlock ) \
	X( CCSUsrMsg_SendPlayerItemDrops ) \
	X( CCSUsrMsg_SendPlayerItemFound ) \
	X( CDataGCCStrike15_v2_TournamentMatchDraft ) \
	X( CPreMatchInfoData ) \
	X( IpAddressMask ) \
	X( PlayerRankingInfo ) \
	X( TournamentEvent ) \
	X( TournamentPlayer ) \
	X( TournamentTeam ) \
	X( CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve ) \
	X( CCSUsrMsg_ServerRankRevealAll ) \
	X( CCSUsrMsg_ServerRankUpdate ) \
	X( CCSUsrMsg_Shake ) \
	X( CCSUsrMsg_ShowMenu ) \
	X( CCSUsrMsg_StopSpectatorMode ) \
	X( CCSUsrMsg_TextMsg ) \
	X( CCSUsrMsg_Train ) \
	X( CCSUsrMsg_VGUIMenu ) \
	X( CCSUsrMsg_VoiceMask ) \
	X( CCSUsrMsg_VoteFailed ) \
	X( CCSUsrMsg_VotePass ) \
	X( CCSUsrMsg_VoteSetup ) \
	X( CCSUsrMsg_VoteStart ) \
	X( CCSUsrMsg_WarmupHasEnded ) \
	X( XpProgressData ) \
	X( CMsgGCCstrike15_v2_GC2ServerNotifyXPRewarded ) \
	X( CCSUsrMsg_XpUpdate ) \
	X( CEngineGotvSyncPacket ) \
	X( CMsgGCCStrike15_v2_MatchmakingGC2ServerConfirm ) \
	X( CMsgGCCStrike15_v2_MatchmakingServerRoundStats )

#endif // KS_USERMESSAGES_SCHEMA_H
