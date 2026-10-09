#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// ---------------------------
//  Navigation Type Bits
// ---------------------------
enum Navigation_t 
{
	NAV_NONE = -1,	// error condition
	NAV_GROUND = 0,	// walk/run
	NAV_JUMP,		// jump/leap
	NAV_FLY,		// can fly, move all around
	NAV_CLIMB,		// climb ladders
	NAV_CRAWL,		// crawl
};
