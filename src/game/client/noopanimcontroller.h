#ifndef NOOPANIMCONTROLLER_H
#define NOOPANIMCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

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

#endif // NOOPANIMCONTROLLER_H
