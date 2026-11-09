// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchSystem class to CMatchSystem. Included by the source that defines CMatchSystem.

IPlayerManager * IMatchSystem::GetPlayerManager()
{ return static_cast<CMatchSystem*>(this)->GetPlayerManager(); }
IMatchVoice * IMatchSystem::GetMatchVoice()
{ return static_cast<CMatchSystem*>(this)->GetMatchVoice(); }
