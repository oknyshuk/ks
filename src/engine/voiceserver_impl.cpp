// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This module implements the IVoiceServer interface.

#include "server.h"
#include "ivoiceserver.h"

class CVoiceServer : public IVoiceServer
{
public:
	
	bool	GetClientListening(int iReceiver, int iSender)
	{
		// Make into client indices..
		--iReceiver;
		--iSender;

		if(iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 || iSender >= sv.GetClientCount() )
			return false;

		return sv.GetClient(iSender)->IsHearingClient( iReceiver );
	}
	
	bool	SetClientListening(int iReceiver, int iSender, bool bListen)
	{
		// Make into client indices..
		--iReceiver;
		--iSender;
		
		if(iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 || iSender >= sv.GetClientCount() )
			return false;

		CGameClient *cl = sv.Client(iSender);
			
		ConVarRef voice_verbose( "voice_verbose" );
		if ( voice_verbose.GetBool() && ( !!cl->m_VoiceStreams[iReceiver] != !!bListen ) )
		{
			Msg( "* CVoiceServer::SetClientListening:  %s m_VoiceStreams from %s (%s) to %s (%s)\n", bListen ? "Enable" : "Disable", cl->GetClientName(), cl->GetNetChannel() ? cl->GetNetChannel()->GetAddress() : "null", sv.Client(iReceiver)->GetClientName(), sv.Client(iReceiver)->GetNetChannel() ? sv.Client(iReceiver)->GetNetChannel()->GetAddress() : "null" );
		}

		cl->m_VoiceStreams.set( iReceiver, bListen );

		return true;
	}	
	bool	SetClientProximity(int iReceiver, int iSender, bool bUseProximity)
	{
		// Make into client indices..
		--iReceiver;
		--iSender;

		if(iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 || iSender >= sv.GetClientCount() )
			return false;

		CGameClient *cl = sv.Client(iSender);

		cl->m_VoiceProximity.set( iReceiver, bUseProximity );

		return true;
	}	
};

#include "facade/ivoiceserver.inl"



static CVoiceServer s_Singleton_CVoiceServer;
static InterfaceReg s_Reg_CVoiceServer_IVoiceServer( []() -> void * { return static_cast<IVoiceServer *>( &s_Singleton_CVoiceServer ); }, INTERFACEVERSION_VOICESERVER );;
