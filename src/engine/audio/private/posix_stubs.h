#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

// Purpose: Posix win32 replacements - Mocks trivial windows flow

import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import std;

inline constexpr int DSBCAPS_LOCSOFTWARE = 0;

inline constexpr int DSERR_BUFFERLOST = 0;

inline constexpr int DSBSTATUS_BUFFERLOST = 0x02;

inline constexpr int DSSPEAKER_GEOMETRY( int speakerConfig ) { return ( ( speakerConfig ) >> 16 ) & 0xFFFF; }
inline constexpr int DSSPEAKER_CONFIG( int speakerConfig ) { return ( speakerConfig ) & 0xFFFF; }

inline constexpr int DSSPEAKER_HEADPHONE = -1;
inline constexpr int DSSPEAKER_QUAD = -2;
inline constexpr int DSSPEAKER_5POINT1 = -3;
inline constexpr int DSSPEAKER_7POINT1 = -4;

inline constexpr int DISP_CHANGE_SUCCESSFUL = 0;

inline constexpr auto HKEY_CURRENT_USER = nullptr;
inline constexpr auto HKEY_LOCAL_MACHINE = nullptr;
inline constexpr int KEY_QUERY_VALUE = 0;

inline constexpr int KEY_READ = 0;
inline constexpr int KEY_WRITE = 1;
inline constexpr unsigned long KEY_ALL_ACCESS = ~0ul;

inline constexpr int SMTO_ABORTIFHUNG = 0;

inline constexpr int JOY_RETURNX = 0x01;
inline constexpr int JOY_RETURNY = 0x02;
inline constexpr int JOY_RETURNZ = 0x04;
inline constexpr int JOY_RETURNR = 0x08;
inline constexpr int JOY_RETURNU = 0x10;
inline constexpr int JOY_RETURNV = 0x20;

inline constexpr int JOYCAPS_HASPOV = 0x01;
inline constexpr int JOYCAPS_HASU = 0x01;
inline constexpr int JOYCAPS_HASV = 0x01;
inline constexpr int JOYCAPS_HASR = 0x01;
inline constexpr int JOYCAPS_HASZ = 0x01;

inline constexpr int MMSYSERR_NODRIVER = 1;
inline constexpr int JOYERR_NOERROR = 0;
inline constexpr int JOY_RETURNCENTERED = 0;
inline constexpr int JOY_RETURNBUTTONS = 0;
inline constexpr int JOY_RETURNPOV = 0;
inline constexpr int JOY_POVCENTERED = 0;
inline constexpr int JOY_POVFORWARD = 0;
inline constexpr int JOY_POVRIGHT = 0;
inline constexpr int JOY_POVBACKWARD = 0;
inline constexpr int JOY_POVLEFT = 0;

inline constexpr int CCHDEVICENAME = 32;
inline constexpr int CCHFORMNAME = 32;

typedef wchar_t BCHAR;

typedef uint MMRESULT;
typedef std::uintptr_t DWORD_PTR;
typedef const char *LPCSTR;
typedef uint POINTL;

inline constexpr int IDLE_PRIORITY_CLASS = 1;
inline constexpr int HIGH_PRIORITY_CLASS = 2;

typedef struct _devicemode { 
  BCHAR  dmDeviceName[CCHDEVICENAME]; 
  WORD   dmSpecVersion; 
  WORD   dmDriverVersion; 
  WORD   dmSize; 
  WORD   dmDriverExtra; 
  DWORD  dmFields; 
  union u1 {
    struct s {
      short dmOrientation;
      short dmPaperSize;
      short dmPaperLength;
      short dmPaperWidth;
      short dmScale; 
      short dmCopies; 
      short dmDefaultSource; 
      short dmPrintQuality; 
    };
    POINTL dmPosition;
    DWORD  dmDisplayOrientation;
    DWORD  dmDisplayFixedOutput;
  };
  short  dmColor; 
  short  dmDuplex; 
  short  dmYResolution; 
  short  dmTTOption; 
  short  dmCollate; 
  BYTE  dmFormName[CCHFORMNAME]; 
  WORD  dmLogPixels; 
  DWORD  dmBitsPerPel; 
  DWORD  dmPelsWidth; 
  DWORD  dmPelsHeight; 
  union u2 {
    DWORD  dmDisplayFlags; 
    DWORD  dmNup;
  };
  DWORD  dmDisplayFrequency; 
  DWORD  dmICMMethod;
  DWORD  dmICMIntent;
  DWORD  dmMediaType;
  DWORD  dmDitherType;
  DWORD  dmReserved1;
  DWORD  dmReserved2;
  DWORD  dmPanningWidth;
  DWORD  dmPanningHeight;
} DEVMODE, *LPDEVMODE; 

typedef uint32				MCIERROR;
typedef uint				MCIDEVICEID;

typedef struct {
    DWORD_PTR dwCallback;  
} MCI_GENERIC_PARMS;

typedef struct {
    DWORD_PTR dwCallback; 
    DWORD     dwReturn; 
    DWORD     dwItem; 
    DWORD     dwTrack; 
} MCI_STATUS_PARMS;
 
typedef struct {
    DWORD_PTR dwCallback; 
    DWORD     dwFrom; 
    DWORD     dwTo; 
} MCI_PLAY_PARMS;

typedef struct {
    DWORD_PTR    dwCallback; 
    MCIDEVICEID  wDeviceID; 
    LPCSTR       lpstrDeviceType; 
    LPCSTR       lpstrElementName; 
    LPCSTR       lpstrAlias; 
} MCI_OPEN_PARMS; 

typedef struct {
    DWORD_PTR dwCallback; 
    DWORD     dwTimeFormat; 
    DWORD     dwAudio; 
} MCI_SET_PARMS;


inline constexpr int MCI_OPEN = 0;
inline constexpr int MCI_OPEN_TYPE = 0;
inline constexpr int MCI_OPEN_SHAREABLE = 0;
inline constexpr int MCI_FORMAT_TMSF = 0;
inline constexpr int MCI_SET_TIME_FORMAT = 0;
inline constexpr int MCI_CLOSE = 0;
inline constexpr int MCI_STOP = 0;
inline constexpr int MCI_PAUSE = 0;
inline constexpr int MCI_PLAY = 0;
inline constexpr int MCI_SET = 0;
inline constexpr int MCI_SET_DOOR_OPEN = 0;
inline constexpr int MCI_SET_DOOR_CLOSED = 0;
inline constexpr int MCI_STATUS_READY = 0;
inline constexpr int MCI_STATUS = 0;
inline constexpr int MCI_STATUS_ITEM = 0;
inline constexpr int MCI_STATUS_WAIT = 0;
inline constexpr int MCI_STATUS_NUMBER_OF_TRACKS = 0;
inline constexpr int MCI_CDA_STATUS_TYPE_TRACK = 0;
inline constexpr int MCI_TRACK = 0;
inline constexpr int MCI_WAIT = 0;
inline constexpr int MCI_CDA_TRACK_AUDIO = 0;
inline constexpr int MCI_STATUS_LENGTH = 0;
inline constexpr int MCI_NOTIFY = 0;
inline constexpr int MCI_FROM = 0;
inline constexpr int MCI_TO = 0;
inline constexpr int MCIERR_DRIVER = -1;

inline constexpr int DSERR_ALLOCATED = 0;

#pragma pack(push, 1)
typedef struct tWAVEFORMATEX
{
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
    WORD    wBitsPerSample;
    WORD    cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;

typedef const WAVEFORMATEX *LPCWAVEFORMATEX;


typedef struct waveformat_tag
{
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
} WAVEFORMAT, *PWAVEFORMAT, *NPWAVEFORMAT, *LPWAVEFORMAT;

typedef const WAVEFORMAT *LPCWAVEFORMAT;

typedef struct pcmwaveformat_tag
{
    WAVEFORMAT  wf;
    WORD        wBitsPerSample;
} PCMWAVEFORMAT, *PPCMWAVEFORMAT, *NPPCMWAVEFORMAT, *LPPCMWAVEFORMAT;

typedef const PCMWAVEFORMAT *LPCPCMWAVEFORMAT;

typedef struct adpcmcoef_tag {
	short	iCoef1;
	short	iCoef2;
} ADPCMCOEFSET;

typedef struct adpcmwaveformat_tag {
	WAVEFORMATEX	wfx;
	WORD			wSamplesPerBlock;
	WORD			wNumCoef;
	ADPCMCOEFSET	aCoef[1];
} ADPCMWAVEFORMAT;

#pragma pack(pop)
