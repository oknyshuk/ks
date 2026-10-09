#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.tier1;

// Purpose: rate limits OOB server queries


// returns false if this IP exceeds rate limits
bool CheckConnectionLessRateLimits( const ns_address &adr );
