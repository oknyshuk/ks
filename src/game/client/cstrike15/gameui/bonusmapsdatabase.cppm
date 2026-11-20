// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include "IGameUIFuncs.h"
#include "cdll_int.h"
#include "engine/IEngineSound.h"
#include "iachievementmgr.h"
#include "iengineui.h"
#include "GameUI/IGameUI.h"
#include "game/client/IGameClientExports.h"
module ks.client:cstrike15.gameui.bonusmapsdatabase;

import std;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import ks.tier1.vector_ops;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.core.basetypes;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.core.threadtools;
import ks.core.types;
import ks.tier1.strtools;
import ks.tier1.tokenset;

struct ChallengeDescription_t
{
	char szName[32];
	char szComment[256];

	int iType;

	int iBronze;
	int iSilver;
	int iGold;

	int iBest;
};

struct BonusMapDescription_t
{
	bool bIsFolder;

	char szShortName[64];
	char szFileName[128];

	char szMapFileName[128];
	char szChapterName[128];
	char szImageName[128];

	char szMapName[64];
	char szComment[256];

	bool bLocked;
	bool bComplete;

	std::vector< ChallengeDescription_t >	*m_pChallenges;

	BonusMapDescription_t( void );
};

struct BonusMapChallenge_t
{
	char szFileName[128];
	char szMapName[32];
	char szChallengeName[32];
	int iBest;
};

//-----------------------------------------------------------------------------
// Purpose: Keeps track of bonus maps on disk
//-----------------------------------------------------------------------------
class CBonusMapsDatabase
{

public:
	CBonusMapsDatabase( void );
	~CBonusMapsDatabase();

	bool ReadBonusMapSaveData( void );
	bool WriteSaveData( void );

	const char * GetPath( void ) { return m_szCurrentPath; }
	void RootPath( void );
	void AppendPath( const char *pchAppend );
	void BackPath( void );
	void SetPath( const char *pchPath, int iDirDepth );

	void ClearBonusMapsList( void );
	void ScanBonusMaps( void );
	void RefreshMapData( void );

	int BonusCount( void );
	BonusMapDescription_t * GetBonusData( int iIndex ) { return &(m_BonusMaps[ iIndex ]); }
	int InvalidIndex( void ) { return -1; }
	bool IsValidIndex( int iIndex ) { return ks::is_valid_index( m_BonusMaps, iIndex ); }

	bool GetBlink( void );
	void SetBlink( bool bState );

	bool BonusesUnlocked( void );

	void SetCurrentChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName );
	void GetCurrentChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName );
	void SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold );
	void GetCurrentChallengeObjectives( int &iBronze, int &iSilver, int &iGold );

	bool SetBooleanStatus( const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue );
	bool SetBooleanStatus( const char *pchName, int iIndex, bool bValue );
	bool UpdateChallengeBest( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest );

	float GetCompletionPercentage( void );

	int NumAdvancedComplete( void );
	void NumMedals( int piNumMedals[ 3 ] );

private:

	void AddBonus( const char *pCurrentPath, const char *pDirFileName, bool bIsFolder );
	void BuildSubdirectoryList( const char *pCurrentPath, bool bOutOfRoot );
	void BuildBonusMapsList( const char *pCurrentPath, bool bOutOfRoot );

	void ParseBonusMapData( char const *pszFileName, char const *pszShortName, bool bIsFolder );

private:

	KeyValues	*m_pBonusMapsManifest;

	std::vector< BonusMapDescription_t >	m_BonusMaps;

	KeyValues	*m_pBonusMapSavedData;
	bool		m_bSavedDataChanged;

	int		m_iX360BonusesUnlocked;		// Only used on 360
	bool	m_bHasLoadedSaveData;

	int		m_iDirDepth;
	char	m_szCurrentPath[k_cMaxPath];
	float	m_fCurrentCompletion;
	int		m_iCompletableLevels;

	BonusMapChallenge_t		m_CurrentChallengeNames;
	ChallengeDescription_t	m_CurrentChallengeObjectives;
};

void GetChallengeMedals( ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal );
CBonusMapsDatabase *BonusMapsDatabase( void );

extern const char g_pszMedalNames[4][8];

// Purpose: Includes all the headers/declarations necessary to access the
//			engine interface

// these stupid set of includes are required to use the cdll_int interface
//#include "wrect.h"

// engine interface

// engine interface singleton accessors
extern IVEngineClient *engine;
extern class IBik *bik;
extern class IEngineUI *engineuifuncs;
extern class IGameUIFuncs *gameuifuncs;
extern class IEngineSound *enginesound;
extern class IAchievementMgr *achievementmgr; 

// Purpose: Defines the interface that the GameUI dll exports

	inline constexpr const char *BACKGROUND_MUSIC_FILENAME = "mainmenu.mp3";

inline constexpr auto MAX_BACKGROUND_MUSIC = 3;

//-----------------------------------------------------------------------------
// Purpose: Implementation of GameUI's exposed interface 
//-----------------------------------------------------------------------------
class CGameUI : public IGameUI
{
public:
	CGameUI();
	~CGameUI();

	virtual void Initialize( CreateInterfaceFn appFactory );
	virtual void Connect( CreateInterfaceFn gameFactory );
	virtual void Start();
	virtual void Shutdown();
	virtual void RunFrame();
	virtual void PostInit();

	// plays the startup mp3 when GameUI starts
	void PlayGameStartupSound();

	// Engine wrappers for activating / hiding the gameUI
	void ActivateGameUI();
	void HideGameUI();

	// Toggle allowing the engine to hide the game UI with the escape key
	void PreventEngineHideGameUI();
	void AllowEngineHideGameUI();

	// notifications
	virtual void OnGameUIActivated();
	virtual void OnGameUIHidden();
	virtual void OLD_OnConnectToServer( const char *game, int IP, int port );	// OLD: use OnConnectToServer2
	virtual void OnConnectToServer2( const char *game, int IP, int connectionPort );
	virtual void OnDisconnectFromServer();
	virtual void OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog );
	virtual void OnLevelLoadingFinished( bool bError, const char *failureReason, const char *extendedReason );
	virtual void StartLoadingScreenForCommand( const char* command );
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues );

	// progress
	virtual bool UpdateProgressBar(float progress, const char *statusText, bool showDialog = true );
	// Shows progress desc, returns previous setting... (used with custom progress bars )
	virtual bool SetShowProgressText( bool show );
	virtual bool UpdateSecondaryProgressBar(float progress, const wchar_t *desc );

	// Xbox 360
	virtual void ShowMessageDialog( const char* messageID, const char* titleID );

	virtual void CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = nullptr, const char* cancelCommand = nullptr, const char* closedCommand = nullptr, const char* pszLegend = nullptr );
	virtual void CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = nullptr, const char* cancelCommand = nullptr, const char* closedCommand = nullptr, const char* pszLegend = nullptr );

	// Allows the level loading progress to show map-specific info
	virtual void SetProgressLevelName( const char *levelName );

 	virtual void NeedConnectionProblemWaitScreen();

	virtual void ShowPasswordUI( char const *pchCurrentPW );

	virtual bool LoadingProgressWantsIsolatedRender( bool bContextValid );

	virtual void RestoreTopLevelMenu();

 	virtual void SetProgressOnStart();
 
 	// state
 	bool IsInLevel();
 	bool IsInBackgroundLevel();
 	bool IsInMultiplayer();
 	bool IsConsoleUI();
 	bool HasSavedThisMenuSession();
 	void SetSavedThisMenuSession( bool bState );
 
 	void ShowLoadingBackgroundDialog();
	void HideLoadingBackgroundDialog();
	bool HasLoadingBackgroundDialog();

	virtual bool IsPlayingFullScreenVideo();

	virtual bool IsTransitionEffectEnabled();

	void SetBackgroundMusicDesired( bool bPlayMusic );

	void SetPreviewBackgroundMusic( const char * pchPreviewMusicPrefix );

	void ReleaseBackgroundMusic( void );

	void StartBackgroundMusicFade( void );

	void PlayQuestAudio( const char * pchAudioFile );
	void StopQuestAudio( void );
	bool IsQuestAudioPlaying( void );

private:

	virtual void StartProgressBar();
	virtual bool ContinueProgressBar(float progressFraction, bool showDialog = true );
	virtual void StopProgressBar(bool bError, const char *failureReason, const char *extendedReason = nullptr);
	virtual bool SetProgressBarStatusText(const char *statusText, bool showDialog = true );

	//!! these functions currently not implemented
	virtual void SetSecondaryProgressBar(float progress /* range [0..1] */);
	virtual void SetSecondaryProgressBarText( const wchar_t *desc );

	bool FindPlatformDirectory(char *platformDir, int bufferSize);
	void GetUpdateVersion( char *pszProd, char *pszVer);
	void ValidateCDKey();

	bool IsBackgroundMusicPlaying( void );
	void UpdateBackgroundMusic( void );

	CreateInterfaceFn m_GameFactory;

	bool m_bTryingToLoadFriends : 1;
	bool m_bActivatedUI : 1;
	bool m_bIsConsoleUI : 1;
	bool m_bHasSavedThisMenuSession : 1;
	bool m_bOpenProgressOnStart : 1;

	int m_iGameIP;
	int m_iGameConnectionPort;
	
	int m_iFriendsLoadPauseFrames;
	int m_iPlayGameStartupSound;

	char m_szPreviousStatusText[128];
	char m_szPlatformDir[MAX_PATH];

	int m_nBackgroundMusicGUID;
	bool m_bBackgroundMusicDesired;
	int m_nBackgroundMusicVersion;
	float m_flBackgroundMusicStopTime;
	const char *m_pMusicExtension;

	const char *m_pPreviewMusicExtension;
	float m_flMainMenuMusicVolume;
	float m_flMasterMusicVolume;

	float m_flQuestAudioTimeEnd;
	float m_flMasterMusicVolumeSavedForMissionAudio;
	float m_flMenuMusicVolumeSavedForMissionAudio;

	int m_nQuestAudioGUID;

	const char * m_pAudioFile;

};

// Purpose: singleton accessor
extern CGameUI &GameUI();

// expose client interface
extern IGameClientExports *GameClientExports();

// Set by the player who "owns" the gameui/settings/etc.
void SetGameUIActiveSplitScreenPlayerSlot( int nSlot );
int GetGameUIActiveSplitScreenPlayerSlot();

void GameUI_MakeSafeName( const char *oldName, char *newName, int newNameBufSize );

//-----------------------------------------------------------------------------
// Useful for game ui since game ui has a single active "splitscreen" owner and since
//  it can gracefully handle non-FCVAR_SS vars without code changes required.
//-----------------------------------------------------------------------------
class CGameUIConVarRef
{
public:
	explicit	CGameUIConVarRef( const char *pName );
				CGameUIConVarRef( const char *pName, bool bIgnoreMissing );
	explicit	CGameUIConVarRef( ConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;

	// Get/Set value
	float GetFloat( int iSlot = -1 ) const;
	int GetInt( int iSlot = -1 ) const;
	float GetMin( int iSlot = -1 ) const;
	float GetMax( int iSlot = -1 ) const;
	bool GetBool( int iSlot = -1 ) const { return !!GetInt(); }
	const char *GetString( int iSlot = -1 ) const;

	void SetValue( const char *pValue, int iSlot = -1 );
	void SetValue( float flValue, int iSlot = -1 );
	void SetValue( int nValue, int iSlot = -1 );
	void SetValue( bool bValue, int iSlot = -1 );

	const char *GetName( int iSlot = -1 ) const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

protected:
	int		GetActiveSplitScreenPlayerSlot() const;
private:
	struct cv_t
	{
		ConVar *m_pConVar;
		ConVar *m_pConVarState;
	};

	cv_t	m_Info[ MAX_SPLITSCREEN_CLIENTS ];
};

// In GAMUI we should never use the regular ConVarRef

[[gnu::always_inline]] inline int CGameUIConVarRef::GetActiveSplitScreenPlayerSlot() const
{
	return GetGameUIActiveSplitScreenPlayerSlot();
}

//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline bool CGameUIConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_Info[ 0 ].m_pConVar->IsFlagSet( nFlags ) != 0 );
}

[[gnu::always_inline]] inline const char *CGameUIConVarRef::GetName( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVar->GetName();
}

[[gnu::always_inline]] inline const char *CGameUIConVarRef::GetBaseName() const
{
	return m_Info[ 0 ].m_pConVar->GetBaseName();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline float CGameUIConVarRef::GetFloat( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline int CGameUIConVarRef::GetInt( int iSlot ) const 
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline const char *CGameUIConVarRef::GetString( int iSlot ) const 
{
	contract_assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetString();
}

[[gnu::always_inline]] inline void CGameUIConVarRef::SetValue( const char *pValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( pValue );
}

[[gnu::always_inline]] inline void CGameUIConVarRef::SetValue( float flValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( flValue );
}

[[gnu::always_inline]] inline void CGameUIConVarRef::SetValue( int nValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( nValue );
}

[[gnu::always_inline]] inline void CGameUIConVarRef::SetValue( bool bValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( bValue ? 1 : 0 );
}

[[gnu::always_inline]] inline const char *CGameUIConVarRef::GetDefault() const
{
	return m_Info[ 0 ].m_pConVarState->GetDefault();
}

//-----------------------------------------------------------------------------

class CGameUiSetActiveSplitScreenPlayerGuard
{
public:
	explicit CGameUiSetActiveSplitScreenPlayerGuard( int slot );
	~CGameUiSetActiveSplitScreenPlayerGuard();
private:
	int	 m_nSaveSlot;
};

#define GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )	CGameUiSetActiveSplitScreenPlayerGuard g_UISSGuard( slot );

#undef GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD

//-----------------------------------------------------------------------------
// Purpose: contains all the data entered about a mod in gameinfo.txt
//-----------------------------------------------------------------------------
class CModInfo
{
public:
	CModInfo();
	~CModInfo();
	void FreeModInfo();

	// loads mod info from gameinfo.txt
	void LoadCurrentGameInfo();

	// loads gameinfo from null-terminated string
	void LoadGameInfoFromBuffer( const char *buffer );

	// data accessors
   	const wchar_t *GetGameTitle();
   	const wchar_t *GetGameTitle2();
	const char *GetGameName();

   	bool IsMultiplayerOnly();
   	bool IsSinglePlayerOnly();

	bool HasPortals();

	bool NoDifficulty();
  	bool NoModels();
  	bool NoHiModel();
  	bool NoCrosshair();
	bool AdvCrosshair();
   	const char *GetFallbackDir();
	bool UseGameLogo();

	KeyValues *GetHiddenMaps();

private:
	wchar_t m_wcsGameTitle[128];
	wchar_t m_wcsGameTitle2[128];
	KeyValues *m_pModData;

	// Deleted copy operations (rule of 3!)
	CModInfo( const CModInfo& ); // = delete
	CModInfo& operator= ( const CModInfo& ); // = delete
};

// singleton accessor
extern CModInfo &ModInfo();

namespace BaseModUI {

	class CAsyncCtxUIOnDeviceAttached;
	class CBaseModFrame;

	extern const tokenset_t< const char * > s_characterPortraits[];

	//=============================================================================
	//
	//=============================================================================

	//
	// ISelectStorageDeviceClient
	//
	//		Client interface for device selector:
	//			async flow is as follows:
	//			client calls into SelectStorageDevice with its parameters established:
	//				GetCtrlrIndex, ForceSelector, AllowDeclined
	//			XUI blade shows up (or implicitly determines which device should be picked based on settings).
	//			if OnSelectError callback fires, then the process failed.
	//			if OnDeviceNotSelected fires, then the process is over, device not picked
	//			if OnDeviceFull fires, then device has insufficient capacity and cannot be used
	//			if OnDeviceSelected fires, then device has been picked and async operations on containers started
	//				should wait for AfterDeviceMounted callback
	//			when AfterDeviceMounted callback fires the device is fully mounted and ready
	//
	class ISelectStorageDeviceClient
	{
	public:
		virtual int  GetCtrlrIndex() = 0;			// Controller index (0, 1, 2 or 3)
		virtual bool ForceSelector() = 0;			// Whether device selector should be forcefully shown
		virtual bool AllowDeclined() = 0;			// Whether declining storage device is allowed
		virtual bool AllowAnyController() = 0;		// Whether any connected controller can be selecting storage or only game-committed

		enum FailReason_t
		{
			FAIL_ERROR,
			FAIL_NOT_SELECTED,
			FAIL_FULL,
			FAIL_CORRUPT
		};
		virtual void OnDeviceFail( FailReason_t eReason ) = 0; // Storage device has not been set
		
		virtual void OnDeviceSelected() = 0;		// After device has been picked in XUI blade, but before mounting symbolic roots and opening containers
		virtual void AfterDeviceMounted() = 0;		// After device has been successfully mounted, configs processed, etc.
	};

	//
	// CChangeStorageDevice
	//
	//		Should be used when user wants to change storage device
	//
class CChangeStorageDevice;   // declared only: it was never implemented

	//
	// UI game data
	//
	class CUIGameData
	{
	public:
		CUIGameData();
		~CUIGameData();

		static CUIGameData* Get();
		static void Shutdown();

		void RunFrame();
		void RunFrame_Storage();
		void RunFrame_Invite();

		void Invite_Confirm();
		void Invite_Connecting();
		bool Invite_IsStorageDeviceValid();

		void OnGameUIPostInit();

		bool CanPlayer2Join();

		void OpenFriendRequestPanel(int index, uint64 playerXuid);
		void OpenInviteUI( char const *szInviteUiType );

		// Listening for match events
		void OnEvent( KeyValues *pEvent );

		bool SignedInToLive();
		bool AnyUserSignedInToLiveWithMultiplayerDisabled();
		bool CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame );
		bool CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg );

		void DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg );
		CBaseModFrame * GetParentWindowForSystemMessageBox();

		const char *GetLocalPlayerName( int iController );

		bool SelectStorageDevice( ISelectStorageDeviceClient *pSelectClient );
		void OnDeviceAttached();
		void OnCompletedAsyncDeviceAttached( CAsyncCtxUIOnDeviceAttached * job );

		void OnGameUIHidden();

		void SetLookSensitivity(float sensitivity);
		float GetLookSensitivity();

		bool IsXUIOpen();

		void OpenWaitScreen( const char * messageText, float minDisplayTime = 3.0f, KeyValues *pSettings = nullptr );
		void UpdateWaitPanel( const char * messageText, float minDisplayTime = 3.0f );
		void UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime = 3.0f );

		void NeedConnectionProblemWaitScreen( void );
		void ShowPasswordUI( char const *pchCurrentPW );
		void FinishPasswordUI( bool bOk );

		char const * GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative );

		void ReloadScheme();

		//
		// Implementation of async jobs
		//	An async job is enqueued by calling "ExecuteAsync" with the proper job context.
		//	Job's function "ExecuteAsync" is called on a separate thread.
		//	After the job finishes the "Completed" function is called on the
		//	main thread.
		//
		class CAsyncJobContext
		{
		public:
			explicit CAsyncJobContext( float flLeastExecuteTime = 0.0f ) : m_flLeastExecuteTime( flLeastExecuteTime ), m_hThreadHandle( nullptr )  {}
			virtual ~CAsyncJobContext() {}

			virtual void ExecuteAsync() = 0;		// Executed on the secondary thread
			virtual void Completed() = 0;			// Executed on the main thread

		public:
			void * volatile m_hThreadHandle;		// Handle to an async job thread waiting for
			float m_flLeastExecuteTime;				// Least amount of time this job should keep executing
		};

		CAsyncJobContext *m_pAsyncJob;
		void ExecuteAsync( CAsyncJobContext *pAsync );

	private:
		bool IsActiveSplitScreenPlayerSpectating( void );

	protected:
		static CUIGameData* m_Instance;
		static bool m_bModuleShutDown;
		bool m_CGameUIPostInit;

		float m_LookSensitivity;

		float m_flShowConnectionProblemTimer;
		float m_flTimeLastFrame;
		bool  m_bShowConnectionProblemActive;

		//XUI info
		bool m_bXUIOpen;

		//storage device info
		bool			m_bWaitingForStorageDeviceHandle;
		AsyncHandle_t	m_hStorageDeviceChangeHandle;
		uint			m_iStorageID;
		int				m_iStorageController;
		ISelectStorageDeviceClient *m_pSelectStorageClient;
		void OnSetStorageDeviceId( int iController, uint nDeviceId );
	};

}

//extern ConVar x360_audio_english;

extern ConVar demo_ui_enable;
extern ConVar demo_connect_string;

uint64 GetDlcInstalledMask();

bool GameModeHasDifficulty( char const *szGameMode );
bool GameModeHasRoundLimit( char const *szGameMode );
bool GameModeIsSingleChapter( char const *szGameMode );
char const * GameModeGetDefaultDifficulty( char const *szGameMode );

struct DifficultyItem
{
	int m_difficultyValue;
	const char* m_difficultyStringValue;
	const char* m_difficultyUnlocalizedName;
	const char* m_difficultyLocKey;
};

struct GameModeItem
{
	int m_Value;
	const char* m_StringValue;
	const char* m_UnlocalizedName;
	const char* m_LocKey;
};

extern const GameModeItem gGameModeItems[2];

const char *GameModeStringFromInt( int i );
const char *GameModeLocKeyFromInt( int iMode );
int GameModeIntFromString( const char *szString );

// 
// RemapText_t arrText[] = {
// 	{ "", "#SessionError_Unknown", RemapText_t::MATCH_FULL },
// 	{ "n/a", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
// 	{ "create", "#SessionError_Create", RemapText_t::MATCH_FULL },
// 	{ "connect", "#SessionError_Connect", RemapText_t::MATCH_FULL },
// 	{ "full", "#SessionError_Full", RemapText_t::MATCH_FULL },
// 	{ "lock", "#SessionError_Lock", RemapText_t::MATCH_FULL },
// 	{ "kicked", "#SessionError_Kicked", RemapText_t::MATCH_FULL },
// 	{ "migrate", "#SessionError_Migrate", RemapText_t::MATCH_FULL },
// 	{ NULL, NULL, RemapText_t::MATCH_FULL }
// };
// szReason = RemapText_t::RemapRawText( arrText, szReason );
// 
struct RemapText_t
{
	char const *m_szRawText;
	char const *m_szRemapText;

	enum MatchPolicy_t
	{
		MATCH_FULL,
		MATCH_SUBSTR,
		MATCH_START
	};
	MatchPolicy_t m_eMatchPolicy;

	inline bool Match( char const *szRawText )
	{
		switch( m_eMatchPolicy )
		{
		case MATCH_FULL:
			return !V_stricmp( szRawText, m_szRawText );
		case MATCH_SUBSTR:
			return V_stristr( szRawText, m_szRawText ) != nullptr;
		case MATCH_START:
			return StringHasPrefix( szRawText, m_szRawText );
		default:
			return false;
		}
	}

	inline static char const * RemapRawText( RemapText_t *pRemapTable, char const *szRawText )
	{
		for ( ; pRemapTable && pRemapTable->m_szRawText; ++ pRemapTable )
		{
			if ( pRemapTable->Match( szRawText ) )
			{
				return pRemapTable->m_szRemapText;
			}
		}
		return szRawText;
	}
};
