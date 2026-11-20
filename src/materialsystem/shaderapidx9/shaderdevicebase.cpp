// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#define DISABLE_PROTECTED_THINGS
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.threadtools;
import ks.core.types;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.tier1;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.vector_ops;
#include "togl/rendermechanism.h"
#include "shaderdevicebase.h"
import std;
#include "tier2/tier2.h"
#include "filesystem.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapibase.h"
#include "shaderapi/ishadershadow.h"
#include "shaderapi_global.h"
#include "videocfg/videocfg.h"
#include "winutils.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CShaderDeviceBase *g_pShaderDeviceBase;
CShaderAPIBase *g_pShaderAPIBase;
CShaderDeviceMgrBase *g_pShaderDeviceMgrBase;

bool g_bUseShaderMutex = false;	// Shader mutex globals
bool g_bShaderAccessDisallowed;
CShaderMutex g_ShaderMutex;

static void InitShaderAPICVars( )
{
	if ( g_pCVar )
	{
		ConVar_Register();
	}
}



//-----------------------------------------------------------------------------
// Read dx support levels
//-----------------------------------------------------------------------------
inline constexpr const char *SUPPORT_CFG_FILE = "dxsupport_mac.cfg";
inline constexpr const char *SUPPORT_CFG_OVERRIDE_FILE = "dxsupport_override.cfg";


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceMgrBase::CShaderDeviceMgrBase()
{
	m_pDXSupport = nullptr;
}

CShaderDeviceMgrBase::~CShaderDeviceMgrBase()
{
}


//-----------------------------------------------------------------------------
// Factory used to get at internal interfaces (used by shaderapi + shader dlls)
//-----------------------------------------------------------------------------
static CreateInterfaceFn s_TempFactory;
void *ShaderDeviceFactory( const char *pName, int *pReturnCode )
{
	if (pReturnCode)
	{
		*pReturnCode = IFACE_OK;
	}

	void *pInterface = s_TempFactory( pName, pReturnCode );
	if ( pInterface )
		return pInterface;

	pInterface = Sys_GetFactoryThis()( pName, pReturnCode );
	if ( pInterface )
		return pInterface;

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return nullptr;	
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::Connect( CreateInterfaceFn factory )
{

	contract_assert( !g_pShaderDeviceMgrBase );

	s_TempFactory = factory;

	// Connection/convar registration
	CreateInterfaceFn actualFactory = ShaderDeviceFactory;
	ConnectTier1Libraries( &actualFactory, 1 );
	InitShaderAPICVars();
	ConnectTier2Libraries( &actualFactory, 1 );
	if ( !g_pShaderUtil )
		g_pShaderUtil = (IShaderUtil*)ShaderDeviceFactory( SHADER_UTIL_INTERFACE_VERSION, nullptr );

	g_pShaderDeviceMgrBase = this;

	s_TempFactory = nullptr;

	if ( !g_pShaderUtil || !g_pFullFileSystem || !g_pShaderDeviceMgrBase )
	{
		Warning( "ShaderAPIDx10 was unable to access the required interfaces!\n" );
		return false;
	}

	// NOTE! : Overbright is 1.0 so that Hammer will work properly with the white bumped and unbumped lightmaps.
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	return true;
}

void CShaderDeviceMgrBase::Disconnect()
{

	g_pShaderDeviceMgrBase = nullptr;
	g_pShaderUtil = nullptr;
	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();

	if ( m_pDXSupport )
	{
		m_pDXSupport->deleteThis();
		m_pDXSupport = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CShaderDeviceMgrBase::QueryInterface( const char *pInterfaceName )
{
	if ( !V_stricmp( pInterfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION ) ||
		 !V_stricmp( pInterfaceName, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION ) )
		return Sys_GetFactoryThis()( pInterfaceName, nullptr );
	return nullptr;
}


//-----------------------------------------------------------------------------
// Returns the hardware caps for a particular adapter
//-----------------------------------------------------------------------------
const HardwareCaps_t& CShaderDeviceMgrBase::GetHardwareCaps( int nAdapter ) const
{
	contract_assert( ( nAdapter >= 0 ) && ( nAdapter < GetAdapterCount() ) );
	return m_Adapters[nAdapter].m_ActualCaps;
}


//-----------------------------------------------------------------------------
// Utility methods for reading config scripts
//-----------------------------------------------------------------------------
static inline int ReadHexValue( KeyValues *pVal, const char *pName )
{
	const char *pString = pVal->GetString( pName, nullptr );
	if (!pString)
	{
		return -1;
	}

	char *pTemp;
	int nVal = std::strtol( pString, &pTemp, 16 );
	return (pTemp != pString) ? nVal : -1;
}

static bool ReadBool( KeyValues *pGroup, const char *pKeyName, bool bDefault )
{
	int nVal = pGroup->GetInt( pKeyName, -1 );
	if ( nVal != -1 )
	{
		//		Warning( "\t%s = %s\n", pKeyName, (nVal != false) ? "true" : "false" );
		return (nVal != false);
	}
	return bDefault;
}

static void ReadInt( KeyValues *pGroup, const char *pKeyName, int nInvalidValue, int *pResult )
{
	int nVal = pGroup->GetInt( pKeyName, nInvalidValue );
	if ( nVal != nInvalidValue )
	{
		*pResult = nVal;
		//		Warning( "\t%s = %d\n", pKeyName, *pResult );
	}
}


//-----------------------------------------------------------------------------
// Utility method to copy over a keyvalue
//-----------------------------------------------------------------------------
static void AddKey( KeyValues *pDest, KeyValues *pSrc )
{
	// Note this will replace already-existing values
	switch( pSrc->GetDataType() )
	{
	case KeyValues::TYPE_NONE:
		break;
	case KeyValues::TYPE_STRING:
		pDest->SetString( pSrc->GetName(), pSrc->GetString() );
		break;
	case KeyValues::TYPE_INT:
		pDest->SetInt( pSrc->GetName(), pSrc->GetInt() );
		break;
	case KeyValues::TYPE_FLOAT:
		pDest->SetFloat( pSrc->GetName(), pSrc->GetFloat() );
		break;
	case KeyValues::TYPE_PTR:
		pDest->SetPtr( pSrc->GetName(), pSrc->GetPtr() );
		break;
	case KeyValues::TYPE_WSTRING:
		pDest->SetWString( pSrc->GetName(), pSrc->GetWString() );
		break;
	case KeyValues::TYPE_COLOR:
		pDest->SetColor( pSrc->GetName(), pSrc->GetColor() );
		break;
	default:
		contract_assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Finds if we have a dxlevel-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindDXLevelSpecificConfig( KeyValues *pKeyValues, int nDxLevel )
{
	KeyValues *pGroup = pKeyValues->GetFirstSubKey();
	for( pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nFoundDxLevel = pGroup->GetInt( "name", 0 );
		if( nFoundDxLevel == nDxLevel )
			return pGroup;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Finds if we have a dxlevel and vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindDXLevelAndVendorSpecificConfig( KeyValues *pKeyValues, int nDxLevel, int nVendorID )
{
	KeyValues *pGroup = pKeyValues->GetFirstSubKey();
	for( pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nFoundDxLevel = pGroup->GetInt( "name", 0 );
		int nFoundVendorID = ReadHexValue( pGroup, "VendorID" );
		if( nFoundDxLevel == nDxLevel && nFoundVendorID == nVendorID )
			return pGroup;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Finds if we have a vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindCPUSpecificConfig( KeyValues *pKeyValues, int nCPUMhz, bool bAMD )
{
	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		const char *pName = pGroup->GetString( "name", nullptr );
		if ( !pName )
			continue;

		if ( ( bAMD && V_stristr( pName, "AMD" ) ) || 
			( !bAMD && V_stristr( pName, "Intel" ) ) )
		{
			int nMinMegahertz = pGroup->GetInt( "min megahertz", -1 );
			int nMaxMegahertz = pGroup->GetInt( "max megahertz", -1 );
			if( nMinMegahertz == -1 || nMaxMegahertz == -1 )
				continue;

			if( nMinMegahertz <= nCPUMhz && nCPUMhz < nMaxMegahertz )
				return pGroup;
		}
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Finds if we have a vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindCardSpecificConfig( KeyValues *pKeyValues, int nVendorId, int nDeviceId )
{
	KeyValues *pGroup = pKeyValues->GetFirstSubKey();
	for( pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nFoundVendorId = ReadHexValue( pGroup, "VendorID" );
		int nFoundDeviceIdMin = ReadHexValue( pGroup, "MinDeviceID" );
		int nFoundDeviceIdMax = ReadHexValue( pGroup, "MaxDeviceID" );
		if ( nFoundVendorId == nVendorId && nDeviceId >= nFoundDeviceIdMin && nDeviceId <= nFoundDeviceIdMax )
			return pGroup;
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Finds if we have a vendor-specific config in the support keyvalues
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindMemorySpecificConfig( KeyValues *pKeyValues, int nSystemRamMB )
{
	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		// Used to help us debug this code
//		const char *pDebugName = pGroup->GetString( "name", "blah" );

		int nMinMB = pGroup->GetInt( "min megabytes", -1 );
		int nMaxMB = pGroup->GetInt( "max megabytes", -1 );
		if ( nMinMB == -1 || nMaxMB == -1 )
			continue;

		if ( nMinMB <= nSystemRamMB && nSystemRamMB < nMaxMB )
			return pGroup;
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Finds if we have a texture mem size specific config
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::FindVidMemSpecificConfig( KeyValues *pKeyValues, int nVideoRamMB )
{
	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		int nMinMB = pGroup->GetInt( "min megatexels", -1 );
		int nMaxMB = pGroup->GetInt( "max megatexels", -1 );
		if ( nMinMB == -1 || nMaxMB == -1 )
			continue;

		if ( nMinMB <= nVideoRamMB && nVideoRamMB < nMaxMB )
			return pGroup;
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Methods related to reading DX support levels given particular devices
//-----------------------------------------------------------------------------
KeyValues *CShaderDeviceMgrBase::ReadDXSupportKeyValues()
{
	if ( CommandLine()->CheckParm( "-ignoredxsupportcfg" ) )
		return nullptr;

	if ( m_pDXSupport )
		return m_pDXSupport;

	KeyValues *pCfg = new KeyValues( "dxsupport" );

	const char *pPathID = "EXECUTABLE_PATH";

	// First try to read a game-specific config, if it exists
	if ( !pCfg->LoadFromFile( g_pFullFileSystem, SUPPORT_CFG_FILE, pPathID ) )
	{
		pCfg->deleteThis();
		return nullptr;
	}

	m_pDXSupport = pCfg;
	return pCfg;
}


//-----------------------------------------------------------------------------
// Returns the max dx support level achievable with this board
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::ReadDXSupportLevels( HardwareCaps_t &caps )
{
	// See if the file tells us otherwise
	KeyValues *pCfg = ReadDXSupportKeyValues();
	if ( !pCfg )
		return;

	KeyValues *pDeviceKeyValues = FindCardSpecificConfig( pCfg, caps.m_VendorID, caps.m_DeviceID );
	if ( pDeviceKeyValues )
	{
		// First, set the max dx level
		int nMaxDXSupportLevel = 0;
		ReadInt( pDeviceKeyValues, "setting.MaxDXLevel", 0, &nMaxDXSupportLevel );
		if ( nMaxDXSupportLevel != 0 )
		{
			caps.m_nMaxDXSupportLevel = nMaxDXSupportLevel;
		}

		int nMinDXSupportLevel = 0;
		ReadInt( pDeviceKeyValues, "setting.MinDXLevel", 0, &nMinDXSupportLevel );
		if ( nMinDXSupportLevel != 0 && nMinDXSupportLevel <= caps.m_nMaxDXSupportLevel )
		{
			caps.m_nMinDXSupportLevel = nMinDXSupportLevel;
		}

		// Next, set the preferred dx level
		int nDXSupportLevel = 0;
		ReadInt( pDeviceKeyValues, "setting.DXLevel", 0, &nDXSupportLevel );
		if ( nDXSupportLevel != 0 )
		{
			caps.m_nDXSupportLevel = nDXSupportLevel;
		}
		else
		{
			caps.m_nDXSupportLevel = caps.m_nMaxDXSupportLevel;
		}
	}
}


//-----------------------------------------------------------------------------
// Loads the hardware caps, for cases in which the D3D caps lie or where we need to augment the caps
//-----------------------------------------------------------------------------


void CShaderDeviceMgrBase::LoadHardwareCaps( KeyValues *pGroup, HardwareCaps_t &caps )
{
	if( !pGroup )
		return;

	// don't just blanket kill clip planes on POSIX, only shoot them down if we're running ARB, or asked for nouserclipplanes.
	//FIXME need to take into account the caps bit that GLM can now provide, so NV can use normal clipping and ATI can fall back to fastclip.
	if ( CommandLine()->FindParm("-arbmode") || CommandLine()->CheckParm( "-nouserclip" ) )
	{
		caps.m_UseFastClipping = true;
	}
	else
	{
		caps.m_UseFastClipping = ReadBool( pGroup, "setting.NoUserClipPlanes", caps.m_UseFastClipping );
	}

	caps.m_bNeedsATICentroidHack = ReadBool( pGroup, "setting.CentroidHack", caps.m_bNeedsATICentroidHack );
	caps.m_bDisableShaderOptimizations = ReadBool( pGroup, "setting.DisableShaderOptimizations", caps.m_bDisableShaderOptimizations );
	caps.m_bPreferZPrepass = ReadBool( pGroup, "setting.PreferZPrepass", caps.m_bPreferZPrepass );
	caps.m_bSuppressPixelShaderCentroidHackFixup = ReadBool( pGroup, "setting.SuppressPixelShaderCentroidHackFixup", caps.m_bSuppressPixelShaderCentroidHackFixup );
	caps.m_bPreferTexturesInHWMemory = ReadBool( pGroup, "setting.PreferTexturesInHWMemory", caps.m_bPreferTexturesInHWMemory );
	caps.m_bPreferHardwareSync = ReadBool( pGroup, "setting.PreferHardwareSync", caps.m_bPreferHardwareSync );
	caps.m_bUnsupported = ReadBool( pGroup, "setting.Unsupported", caps.m_bUnsupported );
	
	// dxsupport can only kill CSM support, not forcefully enable it.
	if ( !ReadBool( pGroup, "setting.SupportsCascadedShadowMapping", true ) )
	{
		caps.m_bSupportsCascadedShadowMapping = false;
	}
	
	int nCSMQuality = CSMQUALITY_VERY_LOW;
	ReadInt( pGroup, "setting.CSMQuality", 0, &nCSMQuality );
	caps.m_nCSMQuality = static_cast<uint8>( clamp( nCSMQuality, 0, CSMQUALITY_TOTAL_MODES - 1 ) );
}


//-----------------------------------------------------------------------------
// Reads in the hardware caps from the dxsupport.cfg file
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::ReadHardwareCaps( HardwareCaps_t &caps, int nDxLevel )
{
	KeyValues *pCfg = ReadDXSupportKeyValues();
	if ( !pCfg )
		return;

	// Next, read the hardware caps for that dx support level.
	KeyValues *pDxLevelKeyValues = FindDXLevelSpecificConfig( pCfg, nDxLevel );
	// Look for a vendor specific line for a given dxlevel.
	KeyValues *pDXLevelAndVendorKeyValue = FindDXLevelAndVendorSpecificConfig( pCfg, nDxLevel, caps.m_VendorID );
	// Finally, override the hardware caps based on the specific card
	KeyValues *pCardKeyValues = FindCardSpecificConfig( pCfg, caps.m_VendorID, caps.m_DeviceID );

	// Apply 
	if( pCardKeyValues && ReadHexValue( pCardKeyValues, "MinDeviceID" ) == 0 && ReadHexValue( pCardKeyValues, "MaxDeviceID" ) == 0xffff )
	{
		// The card specific case is a catch all for device ids, so run it before running the dxlevel and card specific stuff.
		LoadHardwareCaps( pDxLevelKeyValues, caps );
		LoadHardwareCaps( pCardKeyValues, caps );
		LoadHardwareCaps( pDXLevelAndVendorKeyValue, caps );
	}
	else
	{
		// The card specific case is a small range of cards, so run it last to override all other configs.
		LoadHardwareCaps( pDxLevelKeyValues, caps );
		// don't run this one since we have a specific config for this card.
		//		LoadHardwareCaps( pDXLevelAndVendorKeyValue, caps );
		LoadHardwareCaps( pCardKeyValues, caps );
	}
}


//-----------------------------------------------------------------------------
// Reads in ConVars + config variables
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::LoadConfig( KeyValues *pKeyValues, KeyValues *pConfiguration )
{
	if( !pKeyValues )
		return;

	if( CommandLine()->FindParm( "-debugdxsupport" ) )
	{
		ks::text_writer tmpBuf;
		pKeyValues->RecursiveSaveToFile( tmpBuf, 0 );
		Warning( "%s\n", tmpBuf.c_str() );
	}
	for( KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup; pGroup = pGroup->GetNextKey() )
	{
		AddKey( pConfiguration, pGroup );
	}
}


//-----------------------------------------------------------------------------
// Computes amount of ram
//-----------------------------------------------------------------------------
static unsigned long GetRam()
{
	MEMORYSTATUS stat;
	stat.dwLength = sizeof( MEMORYSTATUS );
	GlobalMemoryStatus( &stat );
	return ( unsigned long )( stat.dwTotalPhys / ( 1024 * 1024 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Gets the recommended video configuration.
// Called from: 
// 1. Called from gameui.dll->OptionsSubVideo.cpp->COptionsSubVideoAdvancedDlg to fill in the video options panel.
// 2. Called from gameui.dll->OptionsSubVideo.cpp->MarkDefaultSettingsAsRecommended when action is "Reset To Defaults"
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedVideoConfig( int nAdapter, int nVendorID, int nDeviceID, KeyValues *pConfiguration ) 
{


	VidMatConfigData_t configData;
	V_strcpy_safe( configData.szFileName, "cfg/moddefaults_mac.txt" );
	V_strcpy_safe( configData.szPathID, "MOD" );
	configData.pConfigKeys = pConfiguration;
	configData.nVendorID = nVendorID;
	configData.nDeviceID = nDeviceID;
	configData.nSystemMemory = GetRam();
	int nTextureMemorySize = GetVidMemBytes( nAdapter );
	configData.nVideoMemory = nTextureMemorySize / ( 1024 * 1024 );
	configData.bIsVideo = true;
	configData.nDXLevel = g_pHardwareConfigDx8->GetMaxDXSupportLevel();
	GetDesktopResolution( &configData.nPhysicalScreenWidth, &configData.nPhysicalScreenHeight, nAdapter );

	int nModeCount = GetModeCount( nAdapter );
	configData.displayModes.assign( nModeCount, {} );
	for ( int i = 0; i < nModeCount; ++i )
	{
		GetModeInfo( &configData.displayModes[i], nAdapter, i );
	}
	
	// Call into the video.cfg file to setup the video config cvars.
	RecommendedConfig( configData );

	// Update the convars.
	UpdateVideoConfigConVars( configData.pConfigKeys );

	return true;
//#endif
}

//-----------------------------------------------------------------------------
// Purpose: Gets the recommended video configuration.
// Called from: 
// 3. Used by materialsystem.dll->cmaterialsystem.cpp->GenerateConfigFromConfigKeyValues to fill in material system data (MaterialSystem_Config_t)
// 4. Used by materialsystem.dll->cmaterialsystem.cpp->WriteConfigurationInfoToConVars to write the material system data into ConVars
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, int nVendorID, int nDeviceID, KeyValues *pConfiguration )
{

	VidMatConfigData_t configData;
	V_strcpy_safe( configData.szFileName, "../bin/dxsupport.cfg" );
	V_strcpy_safe( configData.szPathID, "GAME" );
	configData.pConfigKeys = pConfiguration;
	configData.nVendorID = nVendorID;
	configData.nDeviceID = nDeviceID;
	configData.nSystemMemory = GetRam();
	int nTextureMemorySize = GetVidMemBytes( nAdapter );
	configData.nVideoMemory = nTextureMemorySize / ( 1024 * 1024 );
	configData.bIsVideo = false;
	configData.nDXLevel = g_pHardwareConfigDx8->GetMaxDXSupportLevel();
	GetDesktopResolution( &configData.nPhysicalScreenWidth, &configData.nPhysicalScreenHeight, nAdapter );

	int nModeCount = GetModeCount( nAdapter );
	configData.displayModes.assign( nModeCount, {} );
	for ( int i = 0; i < nModeCount; ++i )
	{
		GetModeInfo( &configData.displayModes[i], nAdapter, i );
	}

	// Call into the video.cfg file to setup the video config cvars.
	RecommendedConfig( configData );

	return true;
}

//-----------------------------------------------------------------------------
// Gets recommended congifuration for a particular adapter at a particular dx level
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedVideoConfig( int nAdapter, KeyValues *pCongifuration )
{
	contract_assert( nAdapter >= 0 && nAdapter <= GetAdapterCount() );
	MaterialAdapterInfo_t info;
	GetAdapterInfo( nAdapter, info );
	return GetRecommendedVideoConfig( nAdapter, info.m_VendorID, info.m_DeviceID, pCongifuration );
}

//-----------------------------------------------------------------------------
// Gets recommended congifuration for a particular adapter at a particular dx level
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrBase::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pCongifuration )
{
	contract_assert( nAdapter >= 0 && nAdapter <= GetAdapterCount() );
	MaterialAdapterInfo_t info;
	GetAdapterInfo( nAdapter, info );
	return GetRecommendedConfigurationInfo( nAdapter, nDXLevel, info.m_VendorID, info.m_DeviceID, pCongifuration );
}


//-----------------------------------------------------------------------------
// Returns only valid dx levels
//-----------------------------------------------------------------------------
int CShaderDeviceMgrBase::GetClosestActualDXLevel( int nDxLevel ) const
{
	if ( nDxLevel < 92 )
		return 90;
	if ( nDxLevel < 95 )
		return 92;
	if ( nDxLevel < 100 )
		return 95;
    
	return 100;
}

//-----------------------------------------------------------------------------
// Mode change callback
//-----------------------------------------------------------------------------
void CShaderDeviceMgrBase::AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func )
{
	contract_assert( func && ks::find_index( m_ModeChangeCallbacks, func ) < 0 );
	m_ModeChangeCallbacks.push_back( func );
}

void CShaderDeviceMgrBase::RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func )
{
	ks::erase_first( m_ModeChangeCallbacks, func );
}

void CShaderDeviceMgrBase::InvokeModeChangeCallbacks( int screenWidth, int screenHeight )
{
	int nCount = std::ssize( m_ModeChangeCallbacks );
	for ( int i = 0; i < nCount; ++i )
	{
		m_ModeChangeCallbacks[i]();
	}

	nCount = std::ssize( m_DeviceDependentObjects );
	for ( int i = 0; i < nCount; ++i )
	{
		m_DeviceDependentObjects[i]->ScreenSizeChanged( screenWidth, screenHeight );
	}
}

void CShaderDeviceMgrBase::AddDeviceDependentObject( IShaderDeviceDependentObject *pObject )
{
	if ( pObject )
	{
		contract_assert( pObject && ks::find_index( m_DeviceDependentObjects, pObject ) < 0 );
		m_DeviceDependentObjects.push_back( pObject );
	}
}

void CShaderDeviceMgrBase::RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject )
{
	if ( pObject )
	{
		ks::erase_first( m_DeviceDependentObjects, pObject );
	}
}

void CShaderDeviceMgrBase::InvokeDeviceLostNotifications( void )
{
	int nCount = std::ssize( m_DeviceDependentObjects );
	for ( int i = 0; i < nCount; ++i )
	{
		m_DeviceDependentObjects[i]->DeviceLost();
	}
}

void CShaderDeviceMgrBase::InvokeDeviceResetNotifications( IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentParameters, void *pHWnd )
{
	int nCount = std::ssize( m_DeviceDependentObjects );
	for ( int i = 0; i < nCount; ++i )
	{
		m_DeviceDependentObjects[i]->DeviceReset( ( void * )pDevice, ( void * )pPresentParameters, pHWnd );
	}
}

//-----------------------------------------------------------------------------
// Factory to return from SetMode
//-----------------------------------------------------------------------------
void* CShaderDeviceMgrBase::ShaderInterfaceFactory( const char *pInterfaceName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}
	// The facades registered by the shader API classes
	if ( !V_stricmp( pInterfaceName, SHADER_DEVICE_INTERFACE_VERSION ) ||
		 !V_stricmp( pInterfaceName, SHADERAPI_INTERFACE_VERSION ) ||
		 !V_stricmp( pInterfaceName, SHADERSHADOW_INTERFACE_VERSION ) )
		return Sys_GetFactoryThis()( pInterfaceName, pReturnCode );

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
//
// The Base implementation of the shader device
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceBase::CShaderDeviceBase()
{
	m_bInitialized = false;
	m_nAdapter = -1;
	m_hWnd = nullptr;
	m_hWndCookie = nullptr;
	m_dwThreadId = ThreadGetCurrentId();
}

CShaderDeviceBase::~CShaderDeviceBase()
{
}

void CShaderDeviceBase::SetCurrentThreadAsOwner()
{
	m_dwThreadId = ThreadGetCurrentId();
}

void CShaderDeviceBase::RemoveThreadOwner()
{
	m_dwThreadId = 0xFFFFFFFF;
}

bool CShaderDeviceBase::ThreadOwnsDevice() const
{
	if ( ThreadGetCurrentId() == m_dwThreadId )
		return true;
	return false;
}


//-----------------------------------------------------------------------------
// Methods for interprocess communication to release resources
//-----------------------------------------------------------------------------
constexpr auto MATERIAL_SYSTEM_WINDOW_ID = 0xFEEDDEAD;

//-----------------------------------------------------------------------------
// Adds a hook to let us know when other instances are setting the mode
//-----------------------------------------------------------------------------

#define WINDOW_PROC FARPROC


//-----------------------------------------------------------------------------
// Install, remove ability to talk to other shaderapi apps
//-----------------------------------------------------------------------------
void CShaderDeviceBase::InstallWindowHook( void* hWnd )
{
	contract_assert( m_hWndCookie == nullptr );
}

void CShaderDeviceBase::RemoveWindowHook( void* hWnd )
{
}


//-----------------------------------------------------------------------------
// Sends a message to other shaderapi applications
//-----------------------------------------------------------------------------
void CShaderDeviceBase::SendIPCMessage( IPCMessage_t msg )
{
}


//-----------------------------------------------------------------------------
// Find view
//-----------------------------------------------------------------------------
int CShaderDeviceBase::FindView( void* hWnd ) const
{
	/* FIXME: Is this necessary?
	// Look for the view in the list of views
	for (int i = m_Views.Count(); --i >= 0; )
	{
	if (m_Views[i].m_HWnd == (VD3DHWND)hwnd)
	return i;
	}
	*/
	return -1;
}

//-----------------------------------------------------------------------------
// Creates a child window
//-----------------------------------------------------------------------------
bool CShaderDeviceBase::AddView( void* hWnd )
{
	/*
	// If we haven't created a device yet
	if (!Dx9Device())
		return false;

	// Make sure no duplicate hwnds...
	if (FindView(hwnd) >= 0)
		return false;

	// In this case, we need to create the device; this is our
	// default swap chain. This here says we're gonna use a part of the
	// existing buffer and just grab that.
	int view = m_Views.AddToTail();
	m_Views[view].m_HWnd = (VD3DHWND)hwnd;
	//	memcpy( &m_Views[view].m_PresentParamters, m_PresentParameters, sizeof(m_PresentParamters) );

	HRESULT hr;
	hr = Dx9Device()->CreateAdditionalSwapChain( &m_PresentParameters,
	&m_Views[view].m_pSwapChain );
	return !FAILED(hr);
	*/

	return true;
}

void CShaderDeviceBase::RemoveView( void* hWnd )
{
	/*
	// Look for the view in the list of views
	int i = FindView(hwnd);
	if (i >= 0)
	{
	// FIXME		m_Views[i].m_pSwapChain->Release();
	m_Views.FastRemove(i);
	}
	*/
}

//-----------------------------------------------------------------------------
// Activates a child window
//-----------------------------------------------------------------------------
void CShaderDeviceBase::SetView( void* hWnd )
{

	ShaderViewport_t viewport;
	g_pShaderAPIBase->GetViewports( &viewport, 1 );

	// Get the window (*not* client) rect of the view window
	m_ViewHWnd = (VD3DHWND)hWnd;
	GetWindowSize( m_nWindowWidth, m_nWindowHeight );

	// Reset the viewport (takes into account the view rect)
	// Don't need to set the viewport if it's not ready
	g_pShaderAPIBase->SetViewports( 1, &viewport );
}


//-----------------------------------------------------------------------------
// Gets the window size
//-----------------------------------------------------------------------------
void CShaderDeviceBase::GetWindowSize( int& nWidth, int& nHeight ) const
{

	// this matches up to what the threaded material system does
	g_pShaderAPIBase->GetBackBufferDimensions( nWidth, nHeight );

}
