#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

class CNoopAnimController
{
public:
	void StartAnimationSequence( const char * ) {}
	void StartAnimationSequence( const char *, bool ) {}
	void StopAnimationSequence( const char * ) {}
	void UpdateAnimations( float ) {}
	void CancelAllAnimations() {}
	void RunAllAnimationsToCompletion() {}
	void RunAnimationCommand( ... ) {}
	float GetAnimationSequenceLength( const char * ) { return 0.0f; }
};
