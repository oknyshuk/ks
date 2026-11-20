



import ks.platform;
import ks.simd;

#include "snd_stubs.h"

#include "audio/private/posix_stubs.h"
#include "facade/iengine_voice.inl"

import std;


CEngineVoiceStub *Audio_GetEngineVoiceStub()
{
	static CEngineVoiceStub s_EngineVoiceStub;
	return &s_EngineVoiceStub;
}
