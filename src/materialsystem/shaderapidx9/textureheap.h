#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Portal2 Console is not using due to amount of memory free on Xbox and RSX memory on PS3.
// The desired console pattern is to have a similar footprint, because PS3 does not have a streaming solution and it has enough texture memory
// the texture content choices for the consoles will be made to adapt.

// Uncomment to allow system to operate
//#define SUPPORTS_TEXTURE_STREAMING
