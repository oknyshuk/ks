//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef SND_WAVE_MIXER_H
#define SND_WAVE_MIXER_H



//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CWaveData;
class CAudioMixer;


//-----------------------------------------------------------------------------
// Wave mixer
//-----------------------------------------------------------------------------
CAudioMixer *CreateWaveMixer( CWaveData *data, int format, int channels, int bits );


#endif // SND_WAVE_MIXER_H
