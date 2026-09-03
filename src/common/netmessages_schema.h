// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The net protocol schema. This is the definition, not a rendering of one -- there is no
// .proto behind it. Members carry only a field number; public/proto.h derives the wire
// encoding from the member type.
//
// Members are ordered by field number, which is also the order they are written in.

#ifndef KS_NETMESSAGES_SCHEMA_H
#define KS_NETMESSAGES_SCHEMA_H

#include <cstdint>
#include <string>
#include <vector>

#include "proto_types.h"

namespace ks::net
{

enum NET_Messages : int
{
	net_NOP = 0,
	net_Disconnect = 1,
	net_File = 2,
	net_SplitScreenUser = 3,
	net_Tick = 4,
	net_StringCmd = 5,
	net_SetConVar = 6,
	net_SignonState = 7,
	net_PlayerAvatarData = 100,
};

enum CLC_Messages : int
{
	clc_ClientInfo = 8,
	clc_Move = 9,
	clc_VoiceData = 10,
	clc_BaselineAck = 11,
	clc_ListenEvents = 12,
	clc_RespondCvarValue = 13,
	clc_FileCRCCheck = 14,
	clc_LoadingProgress = 15,
	clc_SplitPlayerConnect = 16,
	clc_ClientMessage = 17,
	clc_CmdKeyValues = 18,
	clc_HltvReplay = 20,
};

enum VoiceDataFormat_t : int
{
	VOICEDATA_FORMAT_STEAM = 0,
	VOICEDATA_FORMAT_ENGINE = 1,
};

enum ESplitScreenMessageType : int
{
	MSG_SPLITSCREEN_ADDUSER = 0,
	MSG_SPLITSCREEN_REMOVEUSER = 1,
	MSG_SPLITSCREEN_TYPE_BITS = 1,
};

enum SVC_Messages : int
{
	svc_ServerInfo = 8,
	svc_SendTable = 9,
	svc_ClassInfo = 10,
	svc_SetPause = 11,
	svc_CreateStringTable = 12,
	svc_UpdateStringTable = 13,
	svc_VoiceInit = 14,
	svc_VoiceData = 15,
	svc_Print = 16,
	svc_Sounds = 17,
	svc_SetView = 18,
	svc_FixAngle = 19,
	svc_CrosshairAngle = 20,
	svc_BSPDecal = 21,
	svc_SplitScreen = 22,
	svc_UserMessage = 23,
	svc_EntityMessage = 24,
	svc_GameEvent = 25,
	svc_PacketEntities = 26,
	svc_TempEntities = 27,
	svc_Prefetch = 28,
	svc_Menu = 29,
	svc_GameEventList = 30,
	svc_GetCvarValue = 31,
	svc_PaintmapData = 33,
	svc_CmdKeyValues = 34,
	svc_EncryptedData = 35,
	svc_HltvReplay = 36,
	svc_Broadcast_Command = 38,
};

enum ReplayEventType_t : int
{
	REPLAY_EVENT_CANCEL = 0,
	REPLAY_EVENT_DEATH = 1,
	REPLAY_EVENT_GENERIC = 2,
	REPLAY_EVENT_STUCK_NEED_FULL_UPDATE = 3,
};

struct CMsgVector
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<float> x;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> y;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> z;
};

struct CMsgVector2D
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<float> x;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> y;
};

struct CMsgQAngle
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<float> x;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> y;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> z;
};

struct CMsgRGBA
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> r;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> g;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> b;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> a;
};

struct CNETMsg_Tick
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> tick;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> host_computationtime;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> host_computationtime_std_deviation;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> host_framestarttime_std_deviation;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<uint32_t> hltv_replay_flags;
};

struct CNETMsg_StringCmd
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> command;
};

struct CNETMsg_SignonState
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> signon_state;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> spawn_count;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> num_server_players;
	[[= ks::proto::Pb{ 4 }]] std::vector<std::string> players_networkids;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<std::string> map_name;
};

struct CMsg_CVars
{
	struct CVar
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> name;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> value;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<uint32_t> dictionary_name;
	};

	[[= ks::proto::Pb{ 1 }]] std::vector<CMsg_CVars::CVar> cvars;
};

struct CNETMsg_SetConVar
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<CMsg_CVars> convars;
};

struct CNETMsg_NOP
{
};

struct CNETMsg_Disconnect
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> text;
};

struct CNETMsg_File
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> transfer_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> file_name;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> is_replay_demo_file;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> deny;
};

struct CNETMsg_SplitScreenUser
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> slot;
};

struct CNETMsg_PlayerAvatarData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> accountid;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> rgb;
};

struct CCLCMsg_ClientInfo
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<ks::proto::fixed32> send_table_crc;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> server_count;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> is_hltv;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> is_replay;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> friends_id;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<std::string> friends_name;
	[[= ks::proto::Pb{ 7 }]] std::vector<ks::proto::fixed32> custom_files;
};

struct CCLCMsg_Move
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<uint32_t> num_backup_commands;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<uint32_t> num_new_commands;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> data;
};

struct CCLCMsg_VoiceData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> data;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<ks::proto::fixed64> xuid;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<VoiceDataFormat_t> format;  // default VOICEDATA_FORMAT_ENGINE
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> sequence_bytes;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<uint32_t> section_number;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<uint32_t> uncompressed_sample_offset;
};

struct CCLCMsg_BaselineAck
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> baseline_tick;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> baseline_nr;
};

struct CCLCMsg_ListenEvents
{
	[[= ks::proto::Pb{ 1 }]] std::vector<ks::proto::fixed32> event_mask;
};

struct CCLCMsg_RespondCvarValue
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> cookie;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> status_code;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> name;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> value;
};

struct CCLCMsg_FileCRCCheck
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> code_path;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> path;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> code_filename;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<std::string> filename;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> file_fraction;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<std::string> md5;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<uint32_t> crc;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<int32_t> file_hash_type;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<int32_t> file_len;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<int32_t> pack_file_id;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<int32_t> pack_file_number;
};

struct CCLCMsg_LoadingProgress
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> progress;
};

struct CCLCMsg_SplitPlayerConnect
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<CMsg_CVars> convars;
};

struct CCLCMsg_CmdKeyValues
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> keyvalues;
};

struct CSVCMsg_ServerInfo
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> protocol;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> server_count;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> is_dedicated;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> is_official_valve_server;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<bool> is_hltv;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<bool> is_replay;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<int32_t> c_os;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<ks::proto::fixed32> map_crc;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<ks::proto::fixed32> client_crc;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<ks::proto::fixed32> string_table_crc;
	[[= ks::proto::Pb{ 11 }]] ks::proto::opt<int32_t> max_clients;
	[[= ks::proto::Pb{ 12 }]] ks::proto::opt<int32_t> max_classes;
	[[= ks::proto::Pb{ 13 }]] ks::proto::opt<int32_t> player_slot;
	[[= ks::proto::Pb{ 14 }]] ks::proto::opt<float> tick_interval;
	[[= ks::proto::Pb{ 15 }]] ks::proto::opt<std::string> game_dir;
	[[= ks::proto::Pb{ 16 }]] ks::proto::opt<std::string> map_name;
	[[= ks::proto::Pb{ 17 }]] ks::proto::opt<std::string> map_group_name;
	[[= ks::proto::Pb{ 18 }]] ks::proto::opt<std::string> sky_name;
	[[= ks::proto::Pb{ 19 }]] ks::proto::opt<std::string> host_name;
	[[= ks::proto::Pb{ 20 }]] ks::proto::opt<uint32_t> public_ip;
	[[= ks::proto::Pb{ 21 }]] ks::proto::opt<bool> is_redirecting_to_proxy_relay;
	[[= ks::proto::Pb{ 22 }]] ks::proto::opt<uint64_t> ugc_map_id;
};

struct CSVCMsg_ClassInfo
{
	struct class_t
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> class_id;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> data_table_name;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> class_name;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> create_on_client;
	[[= ks::proto::Pb{ 2 }]] std::vector<CSVCMsg_ClassInfo::class_t> classes;
};

struct CSVCMsg_SendTable
{
	struct sendprop_t
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> type;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> var_name;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> flags;
		[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> priority;
		[[= ks::proto::Pb{ 5 }]] ks::proto::opt<std::string> dt_name;
		[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> num_elements;
		[[= ks::proto::Pb{ 7 }]] ks::proto::opt<float> low_value;
		[[= ks::proto::Pb{ 8 }]] ks::proto::opt<float> high_value;
		[[= ks::proto::Pb{ 9 }]] ks::proto::opt<int32_t> num_bits;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> is_end;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> net_table_name;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> needs_decoder;
	[[= ks::proto::Pb{ 4 }]] std::vector<CSVCMsg_SendTable::sendprop_t> props;
};

struct CSVCMsg_Print
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> text;
};

struct CSVCMsg_SetPause
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> paused;
};

struct CSVCMsg_SetView
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> entity_index;
};

struct CSVCMsg_CreateStringTable
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> name;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> max_entries;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> num_entries;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> user_data_fixed_size;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> user_data_size;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> user_data_size_bits;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<int32_t> flags;
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<std::string> string_data;
};

struct CSVCMsg_UpdateStringTable
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> table_id;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> num_changed_entries;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> string_data;
};

struct CSVCMsg_VoiceInit
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> quality;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> codec;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> version;  // default 0
};

struct CSVCMsg_VoiceData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> client;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<bool> proximity;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<ks::proto::fixed64> xuid;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> audible_mask;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<std::string> voice_data;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<bool> caster;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<VoiceDataFormat_t> format;  // default VOICEDATA_FORMAT_ENGINE
	[[= ks::proto::Pb{ 8 }]] ks::proto::opt<int32_t> sequence_bytes;
	[[= ks::proto::Pb{ 9 }]] ks::proto::opt<uint32_t> section_number;
	[[= ks::proto::Pb{ 10 }]] ks::proto::opt<uint32_t> uncompressed_sample_offset;
};

struct CSVCMsg_FixAngle
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> relative;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<CMsgQAngle> angle;
};

struct CSVCMsg_CrosshairAngle
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<CMsgQAngle> angle;
};

struct CSVCMsg_Prefetch
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> sound_index;
};

struct CSVCMsg_BSPDecal
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<CMsgVector> pos;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> decal_texture_index;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> entity_index;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> model_index;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<bool> low_priority;
};

struct CSVCMsg_SplitScreen
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<ESplitScreenMessageType> type;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> slot;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> player_index;
};

struct CSVCMsg_GetCvarValue
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> cookie;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> cvar_name;
};

struct CSVCMsg_Menu
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> dialog_type;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> menu_key_values;
};

struct CSVCMsg_UserMessage
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> msg_type;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> msg_data;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> passthrough;
};

struct CSVCMsg_PaintmapData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> paintmap;
};

struct CSVCMsg_GameEvent
{
	struct key_t
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> type;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> val_string;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> val_float;
		[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> val_long;
		[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> val_short;
		[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> val_byte;
		[[= ks::proto::Pb{ 7 }]] ks::proto::opt<bool> val_bool;
		[[= ks::proto::Pb{ 8 }]] ks::proto::opt<uint64_t> val_uint64;
		[[= ks::proto::Pb{ 9 }]] ks::proto::opt<std::string> val_wstring;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> event_name;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> eventid;
	[[= ks::proto::Pb{ 3 }]] std::vector<CSVCMsg_GameEvent::key_t> keys;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> passthrough;
};

struct CSVCMsg_GameEventList
{
	struct key_t
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> type;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> name;
	};

	struct descriptor_t
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> eventid;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<std::string> name;
		[[= ks::proto::Pb{ 3 }]] std::vector<CSVCMsg_GameEventList::key_t> keys;
	};

	[[= ks::proto::Pb{ 1 }]] std::vector<CSVCMsg_GameEventList::descriptor_t> descriptors;
};

struct CSVCMsg_TempEntities
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> reliable;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> num_entries;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> entity_data;
};

struct CSVCMsg_PacketEntities
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> max_entries;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> updated_entries;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<bool> is_delta;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<bool> update_baseline;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> baseline;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> delta_from;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<std::string> entity_data;
};

struct CSVCMsg_Sounds
{
	struct sounddata_t
	{
		[[= ks::proto::Pb{ 1 }]] ks::proto::opt<ks::proto::sint32> origin_x;
		[[= ks::proto::Pb{ 2 }]] ks::proto::opt<ks::proto::sint32> origin_y;
		[[= ks::proto::Pb{ 3 }]] ks::proto::opt<ks::proto::sint32> origin_z;
		[[= ks::proto::Pb{ 4 }]] ks::proto::opt<uint32_t> volume;
		[[= ks::proto::Pb{ 5 }]] ks::proto::opt<float> delay_value;
		[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> sequence_number;
		[[= ks::proto::Pb{ 7 }]] ks::proto::opt<int32_t> entity_index;
		[[= ks::proto::Pb{ 8 }]] ks::proto::opt<int32_t> channel;
		[[= ks::proto::Pb{ 9 }]] ks::proto::opt<int32_t> pitch;
		[[= ks::proto::Pb{ 10 }]] ks::proto::opt<int32_t> flags;
		[[= ks::proto::Pb{ 11 }]] ks::proto::opt<uint32_t> sound_num;
		[[= ks::proto::Pb{ 12 }]] ks::proto::opt<ks::proto::fixed32> sound_num_handle;
		[[= ks::proto::Pb{ 13 }]] ks::proto::opt<int32_t> speaker_entity;
		[[= ks::proto::Pb{ 14 }]] ks::proto::opt<int32_t> random_seed;
		[[= ks::proto::Pb{ 15 }]] ks::proto::opt<int32_t> sound_level;
		[[= ks::proto::Pb{ 16 }]] ks::proto::opt<bool> is_sentence;
		[[= ks::proto::Pb{ 17 }]] ks::proto::opt<bool> is_ambient;
	};

	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<bool> reliable_sound;
	[[= ks::proto::Pb{ 2 }]] std::vector<CSVCMsg_Sounds::sounddata_t> sounds;
};

struct CSVCMsg_EntityMsg
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> ent_index;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> class_id;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<std::string> ent_data;
};

struct CSVCMsg_CmdKeyValues
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> keyvalues;
};

struct CSVCMsg_EncryptedData
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> encrypted;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> key_type;
};

struct CSVCMsg_HltvReplay
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> delay;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<int32_t> primary_target;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<int32_t> replay_stop_at;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> replay_start_at;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<int32_t> replay_slowdown_begin;
	[[= ks::proto::Pb{ 6 }]] ks::proto::opt<int32_t> replay_slowdown_end;
	[[= ks::proto::Pb{ 7 }]] ks::proto::opt<float> replay_slowdown_rate;
};

struct CCLCMsg_HltvReplay
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<int32_t> request;
	[[= ks::proto::Pb{ 2 }]] ks::proto::opt<float> slowdown_length;
	[[= ks::proto::Pb{ 3 }]] ks::proto::opt<float> slowdown_rate;
	[[= ks::proto::Pb{ 4 }]] ks::proto::opt<int32_t> primary_target_ent_index;
	[[= ks::proto::Pb{ 5 }]] ks::proto::opt<float> event_time;
};

struct CSVCMsg_Broadcast_Command
{
	[[= ks::proto::Pb{ 1 }]] ks::proto::opt<std::string> cmd;
};

} // namespace ks::net

// Every message, for generic iteration.
#define KS_NET_MESSAGES( X ) \
	X( CMsgVector ) \
	X( CMsgVector2D ) \
	X( CMsgQAngle ) \
	X( CMsgRGBA ) \
	X( CNETMsg_Tick ) \
	X( CNETMsg_StringCmd ) \
	X( CNETMsg_SignonState ) \
	X( CMsg_CVars ) \
	X( CNETMsg_SetConVar ) \
	X( CNETMsg_NOP ) \
	X( CNETMsg_Disconnect ) \
	X( CNETMsg_File ) \
	X( CNETMsg_SplitScreenUser ) \
	X( CNETMsg_PlayerAvatarData ) \
	X( CCLCMsg_ClientInfo ) \
	X( CCLCMsg_Move ) \
	X( CCLCMsg_VoiceData ) \
	X( CCLCMsg_BaselineAck ) \
	X( CCLCMsg_ListenEvents ) \
	X( CCLCMsg_RespondCvarValue ) \
	X( CCLCMsg_FileCRCCheck ) \
	X( CCLCMsg_LoadingProgress ) \
	X( CCLCMsg_SplitPlayerConnect ) \
	X( CCLCMsg_CmdKeyValues ) \
	X( CSVCMsg_ServerInfo ) \
	X( CSVCMsg_ClassInfo ) \
	X( CSVCMsg_SendTable ) \
	X( CSVCMsg_Print ) \
	X( CSVCMsg_SetPause ) \
	X( CSVCMsg_SetView ) \
	X( CSVCMsg_CreateStringTable ) \
	X( CSVCMsg_UpdateStringTable ) \
	X( CSVCMsg_VoiceInit ) \
	X( CSVCMsg_VoiceData ) \
	X( CSVCMsg_FixAngle ) \
	X( CSVCMsg_CrosshairAngle ) \
	X( CSVCMsg_Prefetch ) \
	X( CSVCMsg_BSPDecal ) \
	X( CSVCMsg_SplitScreen ) \
	X( CSVCMsg_GetCvarValue ) \
	X( CSVCMsg_Menu ) \
	X( CSVCMsg_UserMessage ) \
	X( CSVCMsg_PaintmapData ) \
	X( CSVCMsg_GameEvent ) \
	X( CSVCMsg_GameEventList ) \
	X( CSVCMsg_TempEntities ) \
	X( CSVCMsg_PacketEntities ) \
	X( CSVCMsg_Sounds ) \
	X( CSVCMsg_EntityMsg ) \
	X( CSVCMsg_CmdKeyValues ) \
	X( CSVCMsg_EncryptedData ) \
	X( CSVCMsg_HltvReplay ) \
	X( CCLCMsg_HltvReplay ) \
	X( CSVCMsg_Broadcast_Command )

#endif // KS_NETMESSAGES_SCHEMA_H
