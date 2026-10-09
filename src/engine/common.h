#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "worldsize.h"

#include "filesystem.h"
import std;
#include "qlimits.h"

struct cache_user_t;

//============================================================================

inline constexpr auto COM_COPY_CHUNK_SIZE = 1024; // For copying operations


//============================================================================
char *COM_StringCopy(const char *text);	// allocates memory and copys text
void COM_StringFree(const char *text);	// frees memory allocated by COM_StringCopy
void COM_AddNoise( unsigned char *data, int length, int number ); // Changes n random bits in a data block

//============================================================================
extern void COM_WriteFile (const char *filename, void *data, int len);
extern int COM_OpenFile( const char *filename, FileHandle_t* file );
extern void COM_CloseFile( FileHandle_t hFile );
extern void COM_CreatePath (const char *path);
extern int COM_FileSize (const char *filename);
extern int COM_ExpandFilename (char *filename, int maxlength);
extern byte *COM_LoadFile (const char *path, int usehunk, int *pLength);
extern bool COM_IsValidPath( const char *pszFilename );

const char *COM_Parse (const char *data);
const char *COM_ParseLine (const char *data);
int COM_TokenWaiting( const char *buffer );

extern bool com_ignorecolons;
extern	char		com_token[1024];

void COM_Init (void);
void COM_Shutdown( void );
bool COM_CheckGameDirectory( const char *gamedir );
void COM_ParseDirectoryFromCmd( const char *pCmdName, char *pDirName, int maxlen, const char *pDefault );

template <typename T1> [[gnu::always_inline]] inline constexpr auto Bits2Bytes( T1 b ) { return ((b+7)>>3); }

// does a varargs printf into a temp buffer
// Returns char* because of bad historical reasons.
char	*va(const char *format, ...) [[gnu::format(printf, 1, 2)]];
// prints a vector into a temp buffer.
const char    *vstr(Vector& v);

//============================================================================
extern	char	com_basedir[MAX_OSPATH];
extern	char	com_gamedir[MAX_OSPATH];

byte *COM_LoadStackFile (const char *path, void *buffer, int bufsize, int& filesize );
void COM_LoadCacheFile (const char *path, cache_user_t *cu);
byte* COM_LoadFile(const char *path, int usehunk, int *pLength);

void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize );
bool COM_CopyFile( const char *netpath, const char *cachepath );

void COM_SetupLogDir( const char *mapname );
void COM_GetGameDir(char *szGameDir, int maxlen);
int COM_CompareFileTime(const char *filename1, const char *filename2, int *iCompare);
int COM_GetFileTime( const char *pFileName );
const char *COM_ParseFile(const char *data, char *token, int maxtoken);

extern char gszDisconnectReason[256];
extern char gszExtendedDisconnectReason[256];
extern bool gfExtendedError;
void COM_ExplainDisconnection( bool bPrint, const char *fmt, ... ) [[gnu::format(printf, 2, 3)]];

const char *COM_DXLevelToString( int dxlevel );  // convert DX level to string

void COM_Log( const char *pszFile, const char *fmt, ...) [[gnu::format(printf, 2, 3)]]; // Log a debug message to specified file ( if pszFile == NULL uses c:\\hllog.txt )
void COM_LogString( char const *pchFile, char const *pchString );

const char *COM_FormatSeconds( int seconds ); // returns seconds as hh:mm:ss string

const char *COM_GetModDirectory(); // return the mod dir (rather than the complete -game param, which can be a path)
