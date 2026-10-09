#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Create an output wave stream.  Used to record audio for in-engine movies or
// mixer debugging.

extern void WaveCreateTmpFile( const char *filename, int rate, int bits, int channels );
extern void WaveAppendTmpFile( const char *filename, void *buffer, int sampleBits, int numSamples );
extern void WaveFixupTmpFile( const char *filename );
