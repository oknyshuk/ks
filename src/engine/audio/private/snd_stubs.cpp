



import ks.tier1;

#include "snd_stubs.h"

#include "audio/private/posix_stubs.h"

import std;


CEngineVoiceStub *Audio_GetEngineVoiceStub()
{
	static CEngineVoiceStub s_EngineVoiceStub;
	return &s_EngineVoiceStub;
}
