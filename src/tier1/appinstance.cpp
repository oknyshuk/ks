// ======= Copyright (c) 2009, Valve Corporation, All rights reserved. =========
//
// appinstance.cpp
// 
// Purpose: Provide a simple way to enforce that only one instance of an
//          application is running on a machine at any one time.
//          
// Usage:  declare a global object of CSingleAppInstance type, with the unique name
//         you want to use wrapped in the TEXT( " " ) macro.
//
//		   upon entering main you can check the IsUniqueInstance() method to determine if another instance is running
//         or you can call the CheckForOtherRunningInstances() method to perform the check AND optinally 
//         pop up a message box to the user, and/or have the program terminate
//
// Example:
//
// CSingleAppInstance   g_ThisAppInstance( TEXT("this_source_app") );
//
// void main(int argc,char **argv)
// {
//     if ( g_ThisAppInstance.CheckForOtherRunningInstances() )  return;
//
//	   .. rest of code ..
//
// Notes:  Currently this object only works when IsPlatformWindows() is true
//         i.e. no Xbox 360, linux, or Mac yet..
//         (feel free to impliment)
//
// ===========================================================================




#include "tier0/platform.h"
#include "tier1/appinstance.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"

#include "tier0/memdbgon.h"

#ifdef PLATFORM_WINDOWS_PC 
#include <windows.h>
#endif



// ===========================================================================
//  Constructor - create a named mutex on the PC and see if someone else has
//     already done it.
// ===========================================================================
CSingleAppInstance::CSingleAppInstance( tchar* InstanceName, bool exitOnNotUnique, bool displayMsgIfNotUnique )
{
	// defaults for non-Windows builds
	m_hMutex = NULL;
	m_isUniqueInstance = true;
	
	if ( InstanceName == NULL || V_strlen( InstanceName ) == 0 || V_strlen( InstanceName ) >= MAX_PATH )
	{
		Assert( false );
		return;
	}

#ifdef WIN32
#endif

}



CSingleAppInstance::~CSingleAppInstance()
{
#ifdef WIN32
#endif
}


bool CSingleAppInstance::CheckForOtherRunningInstances( bool exitOnNotUnique, bool displayMsgIfNotUnique )
{


	// We fell through, so act like there are no other instances
	return false;
}



// ===========================================================================
// Static Function - this is used by *other* programs to query the system for 
//     a running program that grabs the named mutex/has a CSingleAppInstance
//     object of the specified name.  
//     It's a way to check if a tool is running or not, (and then presumably
//     lauch it if it is not there).
// ===========================================================================
bool CSingleAppInstance::CheckForRunningInstance( tchar* InstanceName )
{
	// validate input		
	Assert( InstanceName != NULL && V_strlen( InstanceName ) > 0 && V_strlen( InstanceName ) < MAX_PATH );

#ifdef WIN32
#endif
	

	return false;
}
