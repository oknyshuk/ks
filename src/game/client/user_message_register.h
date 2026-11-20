// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define USER_MESSAGE_REGISTER( msgName ) \
	static CUserMessageRegister< ks::net::CS_UM_##msgName, ks::net::CCSUsrMsg_##msgName > userMessageRegister_##msgName( __MsgFunc_##msgName );
