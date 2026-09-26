#if !defined( CLIENTSTEAMCONTEXT_H )
#define CLIENTSTEAMCONTEXT_H

#include "steam/steam_api.h"
#include <functional>
#include <vector>

struct SteamLoggedOnChange_t
{
	bool bPreviousLoggedOn;
	bool bLoggedOn;
};

class CClientSteamContext : public CSteamAPIContext
{
public:
	CClientSteamContext();
	~CClientSteamContext();

	void Activate();
	void Shutdown();

	STEAM_CALLBACK( CClientSteamContext, OnSteamServersDisconnected, SteamServersDisconnected_t, m_CallbackSteamServersDisconnected );
	STEAM_CALLBACK( CClientSteamContext, OnSteamServerConnectFailure, SteamServerConnectFailure_t, m_CallbackSteamServerConnectFailure );
	STEAM_CALLBACK( CClientSteamContext, OnSteamServersConnected, SteamServersConnected_t, m_CallbackSteamServersConnected );

	bool BLoggedOn() { return m_bLoggedOn; }
	EUniverse GetConnectedUniverse() { return m_nUniverse; }
	uint32 GetAppID() { return m_nAppID; }
	const CSteamID & GetLocalPlayerSteamID() { return m_SteamIDLocalPlayer; }

	// Allow others to register for a callback when the Steam logged on status changes	
	// InstallCallback returns a token for RemoveCallback.
	using LoggedOnCallback_t = std::function< void ( const SteamLoggedOnChange_t & ) >;
	int InstallCallback( LoggedOnCallback_t callback );
	void RemoveCallback( int nToken );
	
private:
	void UpdateLoggedOnState();
	void InvokeCallbacks( const SteamLoggedOnChange_t &loggedOnStatus );
	
	bool m_bActive;
	bool m_bLoggedOn;
	CSteamID m_SteamIDLocalPlayer;
	EUniverse m_nUniverse;
	uint32 m_nAppID;

	struct LoggedOnCallbackEntry_t
	{
		int nToken;
		LoggedOnCallback_t fn;
	};
	std::vector< LoggedOnCallbackEntry_t > m_LoggedOnCallbacks;
	int m_nNextCallbackToken = 1;
};

CClientSteamContext &ClientSteamContext();			// singleton accessor

#endif // CLIENTSTEAMCONTEXT_H
