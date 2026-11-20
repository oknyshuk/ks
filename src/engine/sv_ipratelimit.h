#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.platform;
import ks.simd;
import ks.tier1.ns_address;

// Purpose: rate limits OOB server queries


// returns false if this IP exceeds rate limits
bool CheckConnectionLessRateLimits( const ns_address &adr );
