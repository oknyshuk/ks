#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Flags for m_DustFlags.
inline constexpr auto DUSTFLAGS_ON = (1<<0); // emit particles..
inline constexpr auto DUSTFLAGS_SCALEMOTES = (1<<1); // scale to keep the same size on screen
inline constexpr auto DUSTFLAGS_FROZEN = (1<<2); // just emit m_SpawnRate # of particles and freeze
inline constexpr auto DUST_NUMFLAGS = 3;
