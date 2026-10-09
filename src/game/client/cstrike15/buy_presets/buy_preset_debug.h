#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//--------------------------------------------------------------------------------------------------------------
// Utility functions for the debugging macros below
bool IsPresetDebuggingEnabled();			///< cl_preset_debug >= 1.0f
bool IsPresetFullCostDebuggingEnabled();	///< cl_preset_debug == 2.0f
bool IsPresetCurrentCostDebuggingEnabled();	///< cl_preset_debug == 3.0f

//--------------------------------------------------------------------------------------------------------------
// Macros for conditional debugging of buy presets
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

template <typename... Args>
inline void PRESET_DEBUG( Args &&...args )
{
	if ( IsPresetDebuggingEnabled() )
		DevMsg( static_cast<Args &&>( args )... );
}
template <typename... Args>
inline void FULLCOST_DEBUG( Args &&...args )
{
	if ( IsPresetFullCostDebuggingEnabled() )
		DevMsg( static_cast<Args &&>( args )... );
}
template <typename... Args>
inline void CURRENTCOST_DEBUG( Args &&...args )
{
	if ( IsPresetCurrentCostDebuggingEnabled() )
		DevMsg( static_cast<Args &&>( args )... );
}

#pragma GCC diagnostic pop
