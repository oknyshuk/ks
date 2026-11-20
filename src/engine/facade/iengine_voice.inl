import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IEngineVoice class to CEngineVoiceStub. Included by snd_stubs.cpp.

bool IEngineVoice::IsHeadsetPresent( int iController )
{ return static_cast<CEngineVoiceStub*>(this)->IsHeadsetPresent( iController ); }
bool IEngineVoice::IsLocalPlayerTalking( int iController )
{ return static_cast<CEngineVoiceStub*>(this)->IsLocalPlayerTalking( iController ); }
void IEngineVoice::AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags )
{ static_cast<CEngineVoiceStub*>(this)->AddPlayerToVoiceList( xPlayer, iController, uiFlags ); }
void IEngineVoice::RemovePlayerFromVoiceList( XUID xPlayer, int iController )
{ static_cast<CEngineVoiceStub*>(this)->RemovePlayerFromVoiceList( xPlayer, iController ); }
void IEngineVoice::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{ static_cast<CEngineVoiceStub*>(this)->GetRemoteTalkers( pNumTalkers, pRemoteTalkers ); }
bool IEngineVoice::VoiceUpdateData( int iController )
{ return static_cast<CEngineVoiceStub*>(this)->VoiceUpdateData( iController ); }
void IEngineVoice::GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
{ static_cast<CEngineVoiceStub*>(this)->GetVoiceData( iController, ppvVoiceDataBuffer, pnumVoiceDataBytes ); }
void IEngineVoice::VoiceResetLocalData( int iController )
{ static_cast<CEngineVoiceStub*>(this)->VoiceResetLocalData( iController ); }
void IEngineVoice::SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback )
{ static_cast<CEngineVoiceStub*>(this)->SetPlaybackPriority( remoteTalker, iController, iAllowPlayback ); }
void IEngineVoice::PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers )
{ static_cast<CEngineVoiceStub*>(this)->PlayIncomingVoiceData( xuid, pbData, dwDataSize, bAudiblePlayers ); }
void IEngineVoice::RemoveAllTalkers()
{ static_cast<CEngineVoiceStub*>(this)->RemoveAllTalkers(); }

