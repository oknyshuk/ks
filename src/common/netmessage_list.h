#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// The closed set of net messages: one entry per message class, with the CNetMessagePB instance it is
// (or derives from). INetMessage dispatches on the entry's kind (see netmessages.cpp), a message
// instance finds its kind through net_msg_index.

#define KS_NET_MESSAGE_CLASSES( X ) \
	X( CNETMsg_Tick_t, CNetMessagePB< ks::net::net_Tick, ks::net::CNETMsg_Tick > ) \
	X( CNETMsg_StringCmd_t, CNetMessagePB< ks::net::net_StringCmd, ks::net::CNETMsg_StringCmd, INetChannelInfo::STRINGCMD > ) \
	X( CNETMsg_PlayerAvatarData_t, CNetMessagePB< ks::net::net_PlayerAvatarData, ks::net::CNETMsg_PlayerAvatarData, INetChannelInfo::PAINTMAP > ) \
	X( CNETMsg_SignonState_t, CNetMessagePB< ks::net::net_SignonState, ks::net::CNETMsg_SignonState, INetChannelInfo::SIGNON > ) \
	X( CNETMsg_SetConVar_t, CNetMessagePB< ks::net::net_SetConVar, ks::net::CNETMsg_SetConVar, INetChannelInfo::STRINGCMD > ) \
	X( CCLCMsg_FileCRCCheck_t, CNetMessagePB< ks::net::clc_FileCRCCheck, ks::net::CCLCMsg_FileCRCCheck > ) \
	X( CNETMsg_NOP_t, CNetMessagePB< ks::net::net_NOP, ks::net::CNETMsg_NOP > ) \
	X( CNETMsg_Disconnect_t, CNetMessagePB< ks::net::net_Disconnect, ks::net::CNETMsg_Disconnect > ) \
	X( CNETMsg_File_t, CNetMessagePB< ks::net::net_File, ks::net::CNETMsg_File > ) \
	X( CNETMsg_SplitScreenUser_t, CNetMessagePB< ks::net::net_SplitScreenUser, ks::net::CNETMsg_SplitScreenUser > ) \
	X( CCLCMsg_SplitPlayerConnect_t, CNetMessagePB< ks::net::clc_SplitPlayerConnect, ks::net::CCLCMsg_SplitPlayerConnect > ) \
	X( CCLCMsg_Move_t, CNetMessagePB< ks::net::clc_Move, ks::net::CCLCMsg_Move, INetChannelInfo::MOVE, false > ) \
	X( CCLCMsg_ClientInfo_t, CNetMessagePB< ks::net::clc_ClientInfo, ks::net::CCLCMsg_ClientInfo > ) \
	X( CCLCMsg_VoiceData_t, CNetMessagePB< ks::net::clc_VoiceData, ks::net::CCLCMsg_VoiceData, INetChannelInfo::VOICE, false > ) \
	X( CCLCMsg_BaselineAck_t, CNetMessagePB< ks::net::clc_BaselineAck, ks::net::CCLCMsg_BaselineAck > ) \
	X( CCLCMsg_ListenEvents_t, CNetMessagePB< ks::net::clc_ListenEvents, ks::net::CCLCMsg_ListenEvents > ) \
	X( CCLCMsg_RespondCvarValue_t, CNetMessagePB< ks::net::clc_RespondCvarValue, ks::net::CCLCMsg_RespondCvarValue > ) \
	X( CCLCMsg_LoadingProgress_t, CNetMessagePB< ks::net::clc_LoadingProgress, ks::net::CCLCMsg_LoadingProgress > ) \
	X( CCLCMsg_CmdKeyValues_t, CNetMessagePB< ks::net::clc_CmdKeyValues, ks::net::CCLCMsg_CmdKeyValues > ) \
	X( CCLCMsg_HltvReplay_t, CNetMessagePB< ks::net::clc_HltvReplay, ks::net::CCLCMsg_HltvReplay > ) \
	X( CSVCMsg_ServerInfo_t, CNetMessagePB< ks::net::svc_ServerInfo, ks::net::CSVCMsg_ServerInfo, INetChannelInfo::SIGNON > ) \
	X( CSVCMsg_ClassInfo_t, CNetMessagePB< ks::net::svc_ClassInfo, ks::net::CSVCMsg_ClassInfo, INetChannelInfo::SIGNON > ) \
	X( CSVCMsg_SendTable_t, CNetMessagePB< ks::net::svc_SendTable, ks::net::CSVCMsg_SendTable, INetChannelInfo::SIGNON > ) \
	X( CSVCMsg_Print_t, CNetMessagePB< ks::net::svc_Print, ks::net::CSVCMsg_Print, INetChannelInfo::GENERIC, false > ) \
	X( CSVCMsg_SetPause_t, CNetMessagePB< ks::net::svc_SetPause, ks::net::CSVCMsg_SetPause > ) \
	X( CSVCMsg_SetView_t, CNetMessagePB< ks::net::svc_SetView, ks::net::CSVCMsg_SetView > ) \
	X( CSVCMsg_CreateStringTable_t, CNetMessagePB< ks::net::svc_CreateStringTable, ks::net::CSVCMsg_CreateStringTable, INetChannelInfo::SIGNON > ) \
	X( CSVCMsg_UpdateStringTable_t, CNetMessagePB< ks::net::svc_UpdateStringTable, ks::net::CSVCMsg_UpdateStringTable, INetChannelInfo::STRINGTABLE > ) \
	X( CSVCMsg_VoiceInit_t, CNetMessagePB< ks::net::svc_VoiceInit, ks::net::CSVCMsg_VoiceInit, INetChannelInfo::SIGNON > ) \
	X( CSVCMsg_VoiceData_t, CNetMessagePB< ks::net::svc_VoiceData, ks::net::CSVCMsg_VoiceData, INetChannelInfo::VOICE, false > ) \
	X( CSVCMsg_FixAngle_t, CNetMessagePB< ks::net::svc_FixAngle, ks::net::CSVCMsg_FixAngle, INetChannelInfo::GENERIC, false > ) \
	X( CSVCMsg_Prefetch_t, CNetMessagePB< ks::net::svc_Prefetch, ks::net::CSVCMsg_Prefetch, INetChannelInfo::SOUNDS > ) \
	X( CSVCMsg_CrosshairAngle_t, CNetMessagePB< ks::net::svc_CrosshairAngle, ks::net::CSVCMsg_CrosshairAngle > ) \
	X( CSVCMsg_BSPDecal_t, CNetMessagePB< ks::net::svc_BSPDecal, ks::net::CSVCMsg_BSPDecal > ) \
	X( CSVCMsg_SplitScreen_t, CNetMessagePB< ks::net::svc_SplitScreen, ks::net::CSVCMsg_SplitScreen > ) \
	X( CSVCMsg_GetCvarValue_t, CNetMessagePB< ks::net::svc_GetCvarValue, ks::net::CSVCMsg_GetCvarValue > ) \
	X( CSVCMsg_Menu_t, CNetMessagePB< ks::net::svc_Menu, ks::net::CSVCMsg_Menu, INetChannelInfo::GENERIC, false > ) \
	X( CSVCMsg_UserMessage_t, CNetMessagePB< ks::net::svc_UserMessage, ks::net::CSVCMsg_UserMessage, INetChannelInfo::USERMESSAGES, false > ) \
	X( CSVCMsg_PaintmapData_t, CNetMessagePB< ks::net::svc_PaintmapData, ks::net::CSVCMsg_PaintmapData, INetChannelInfo::PAINTMAP > ) \
	X( CSVCMsg_GameEvent_t, CNetMessagePB< ks::net::svc_GameEvent, ks::net::CSVCMsg_GameEvent, INetChannelInfo::EVENTS > ) \
	X( CSVCMsg_GameEventList_t, CNetMessagePB< ks::net::svc_GameEventList, ks::net::CSVCMsg_GameEventList > ) \
	X( CSVCMsg_TempEntities_t, CNetMessagePB< ks::net::svc_TempEntities, ks::net::CSVCMsg_TempEntities, INetChannelInfo::TEMPENTS, false > ) \
	X( CSVCMsg_PacketEntities_t, CNetMessagePB< ks::net::svc_PacketEntities, ks::net::CSVCMsg_PacketEntities, INetChannelInfo::ENTITIES > ) \
	X( CSVCMsg_Sounds_t, CNetMessagePB< ks::net::svc_Sounds, ks::net::CSVCMsg_Sounds, INetChannelInfo::SOUNDS > ) \
	X( CSVCMsg_EntityMsg_t, CNetMessagePB< ks::net::svc_EntityMessage, ks::net::CSVCMsg_EntityMsg, INetChannelInfo::ENTMESSAGES, false > ) \
	X( CSVCMsg_CmdKeyValues_t, CNetMessagePB< ks::net::svc_CmdKeyValues, ks::net::CSVCMsg_CmdKeyValues > ) \
	X( CSVCMsg_EncryptedData_t, CNetMessagePB< ks::net::svc_EncryptedData, ks::net::CSVCMsg_EncryptedData, INetChannelInfo::ENCRYPTED > ) \
	X( CSVCMsg_HltvReplay_t, CNetMessagePB< ks::net::svc_HltvReplay, ks::net::CSVCMsg_HltvReplay, INetChannelInfo::ENTITIES > ) \
	X( CSVCMsg_Broadcast_Command_t, CNetMessagePB< ks::net::svc_Broadcast_Command, ks::net::CSVCMsg_Broadcast_Command, INetChannelInfo::STRINGCMD > )
