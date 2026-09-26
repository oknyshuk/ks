//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	This file sets all of our debugging flags.  It should be
//			called before all other header files.
//
// $NoKeywords: $
//=============================================================================//

#ifndef DBGFLAG_H
#define DBGFLAG_H

// The flags that survive here are the ones something actually reads, and that a supported build
// type can actually reach. The rest of this file's original contents are gone:
//
//   DBGFLAG_MEMORY / DBGFLAG_MEMORY_NEWDEL
//       Nothing read them. They selected Valve's own malloc/free override, which this fork
//       replaced with memoverride.cpp's global operator new / delete.
//
//   DBGFLAG_VALIDATE
//       Only reachable under _DEBUG && STEAM. STEAM is never defined here, and the recursive
//       validator it turned on is gone with it.
//
//   DBGFLAG_STRINGS
//       Nothing reads it. String validation did not come across.
//
// DBGFLAG_STRINGS_STRIP is deliberately *not* defaulted anywhere: the headers that consult it
// still guard real content with it, and nothing here should silently make that content vanish.
//
// These flags are activated by dbg.h, which includes this header, so a translation unit that
// wants assertions has to include tier0/dbg.h.

// Turns Assert and friends on. When it is not defined, Assert() compiles to ((void)0) -- see
// the note in dbg.h about why the no-op path must not reference AssertImpl.
#if defined( _DEBUG )

#define DBGFLAG_ASSERT
#define DBGFLAG_ASSERTFATAL
#define DBGFLAG_ASSERTDLG

#else // _DEBUG

// Fatal asserts and the assert dialog stay on in release builds; only plain Assert() is
// compiled out.
#define DBGFLAG_ASSERTFATAL
#define DBGFLAG_ASSERTDLG

#endif // _DEBUG


#endif // DBGFLAG_H
