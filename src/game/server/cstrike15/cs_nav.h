#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// nav.h
// Data structures and constants for the Navigation Mesh system
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#include "nav.h"

/**
 * Below are several constants used by the navigation system.
 * @todo Move these into TheNavMesh singleton.
 */
const float BotRadius = 10.0f;				///< circular extent that contains bot

class CNavArea;
class CSNavNode;
