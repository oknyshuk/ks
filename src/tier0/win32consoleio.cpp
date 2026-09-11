//======= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ======
//
// Purpose: Win32 Console API helpers
//
//=============================================================================

#include "pch_tier0.h"
#include "win32consoleio.h"


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
// Attach a console to a Win32 GUI process and setup stdin, stdout & stderr
// along with the std::iostream (cout, cin, cerr) equivalents to read and
// write to and from that console
// 
// 1. Ensure the handle associated with stdio is FILE_TYPE_UNKNOWN
//    if it's anything else just return false.  This supports cygwin
//    style command shells like rxvt which setup pipes to processes
//    they spawn
//
// 2. See if the Win32 function call AttachConsole exists in kernel32
//    It's a Windows 2000 and above call.  If it does, call it and see
//    if it succeeds in attaching to the console of the parent process.
//    If that succeeds, return false (for no new console allocated).
//    This supports someone typing the command from a normal windows
//    command window and having the output go to the parent window.
//    It's a little funny because a GUI app detaches so the command
//    prompt gets intermingled with output from this process
//    
// 3. If things get to here call AllocConsole which will pop open
//    a new window and allow output to go to that window.  The
//    window will disappear when the process exists so if it's used
//    for something like a help message then do something like getchar()
//    from stdin to wait for a keypress.  if AllocConsole is called
//    true is returned.
//
// Return: true if AllocConsole() was used to pop open a new windows console
// 
//-----------------------------------------------------------------------------
bool SetupWin32ConsoleIO()
{

	return false;

}

//-----------------------------------------------------------------------------
// Win32 Console Color API Helpers, originally from cmdlib.
// Retrieves the current console color attributes.
//-----------------------------------------------------------------------------
void InitWin32ConsoleColorContext( Win32ConsoleColorContext_t *pContext )
{
	pContext->m_InitialColor = 0;
}

//-----------------------------------------------------------------------------
// Sets the active console foreground color. This function is smart enough to 
// avoid setting the color to something that would be unreadable given
// the user's potentially customized background color. It leaves the 
// background color unchanged.
// Returns: The console's previous foreground color.
//-----------------------------------------------------------------------------
uint16 SetWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, int nRed, int nGreen, int nBlue, int nIntensity )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Restore's the active foreground console color, without distributing the current
// background color.
//-----------------------------------------------------------------------------
void RestoreWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, uint16 prevColor )
{
}
