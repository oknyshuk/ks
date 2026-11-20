#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core.platform;
import ks.platform;
import ks.simd;
class IMatchVoice
{
public:
	// Whether remote player talking can be visualized / audible
	bool CanPlaybackTalker( XUID xuidTalker );

	// Whether we are explicitly muting a remote player
	bool IsTalkerMuted( XUID xuidTalker );

	// Whether we are muting any player on the player's machine
	bool IsMachineMuted( XUID xuidPlayer );

	// Whether voice recording mode is currently active
	bool IsVoiceRecording();

	// Enable or disable voice recording
	void SetVoiceRecording( bool bRecordingEnabled );

	// Enable or disable voice mute for a given talker
	void MuteTalker( XUID xuidTalker, bool bMute );
};
