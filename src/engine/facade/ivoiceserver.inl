// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVoiceServer class to CVoiceServer. Included by voiceserver_impl.cpp.

bool IVoiceServer::GetClientListening( int iReceiver, int iSender )
{ return static_cast<CVoiceServer*>(this)->GetClientListening( iReceiver, iSender ); }
bool IVoiceServer::SetClientListening( int iReceiver, int iSender, bool bListen )
{ return static_cast<CVoiceServer*>(this)->SetClientListening( iReceiver, iSender, bListen ); }
bool IVoiceServer::SetClientProximity( int iReceiver, int iSender, bool bUseProximity )
{ return static_cast<CVoiceServer*>(this)->SetClientProximity( iReceiver, iSender, bUseProximity ); }

