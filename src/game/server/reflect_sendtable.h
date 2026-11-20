// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#pragma once
import ks.core.vprof;

#define IMPLEMENT_REFLECT_SERVERCLASS( className, tableName )                                    \
	namespace tableName { SendTable g_SendTable; }                                               \
	static const int g_##className##_ReflectSendInit = []                                        \
	{                                                                                            \
		auto &p = ksr::net::table_props<className>();                                    \
		tableName::g_SendTable.Construct( p.data(), (int)p.size(), #tableName );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static ServerClass g_##className##_ClassReg( #className, &tableName::g_SendTable );           \
	ServerClass *className::GetServerClass() { return &g_##className##_ClassReg; }                \
	SendTable *className::m_pClassSendTable = &tableName::g_SendTable;                             \
	int className::YouForgotToImplementOrDeclareServerClass() { return 0; }
#define IMPLEMENT_REFLECT_TABLE_IN( className, tableName )                                        \
	namespace tableName { SendTable g_SendTable; }                                                       \
	static const int g_##className##_##tableName##_ReflectInit = []                               \
	{                                                                                            \
		auto &p = ksr::net::table_props_in<className, #tableName>();                        \
		tableName::g_SendTable.Construct( p.data(), (int)p.size(), #tableName );                    \
		return 1;                                                                                \
	}();
#define IMPLEMENT_REFLECT_TABLE( className, tableName )                                   \
	namespace tableName { SendTable g_SendTable; }                                               \
	static const int g_##className##_ReflectSendInit = []                                        \
	{                                                                                            \
		auto &p = ksr::net::table_props<className>();                                    \
		tableName::g_SendTable.Construct( p.data(), (int)p.size(), #tableName );                  \
		return 1;                                                                                \
	}();
