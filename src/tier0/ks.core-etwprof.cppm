// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: ETW (Event Tracing for Windows) profiling helpers.
// This allows easy insertion of Generic Event markers into ETW/xperf tracing
// which then aids in analyzing the traces and finding performance problems.
// The usage patterns are to use ETWBegin and ETWEnd (typically through the
// convenience class CETWScope) to bracket time-consuming operations. In addition
// ETWFrameMark marks the beginning of each frame, and ETWMark can be used to
// mark other notable events. More event types and providers can be added as needed.
// When recording xperf profiles add Valve-Main+Valve-FrameRate to the list of
// user-mode providers and be sure to register the providers with this sequence
// of commands:
//   xcopy /y game\bin\tier0.dll %temp%
//   wevtutil um src\tier0\ValveETWProvider.man
//   wevtutil im src\tier0\ValveETWProvider.man

export module ks.core:etwprof;

import :platform;

export extern "C++" {

inline bool ETWIsTracingEnabled() { return false; }
inline int64 ETWMark( const char* ) { return 0; }
inline void ETWMarkPrintf( const char *, ... ) { return; }
inline void ETWMark1F( const char *, float ) { }
inline void ETWMark2F( const char *, float , float ) { }
inline void ETWMark3F( const char *, float , float , float ) { }
inline void ETWMark4F( const char *, float , float , float , float ) { }
inline void ETWMark1I( const char *, int ) { }
inline void ETWMark2I( const char *, int , int ) { }
inline void ETWMark3I( const char *, int , int , int ) { }
inline void ETWMark4I( const char *, int , int , int , int ) { }
// Optionally specify one to two strings. They will show up in separate columns in
// summary tables to allow sorting and easier transfer to spreadsheets.
inline void ETWMark1S( const char *, const char* ) { }
inline void ETWMark2S( const char *, const char* , const char* ) { }

inline int64 ETWBegin( const char* ) { return 0; }
inline int64 ETWEnd( const char*, int64 ) { return 0; }
inline void ETWRenderFrameMark( bool  ) {}
inline void ETWSimFrameMark( bool  ) {}
inline int ETWGetRenderFrameNumber() { return 0; }

inline void ETWMouseDown( int nWhichButton, int nX, int nY ) {}
inline void ETWMouseUp( int nWhichButton, int nX, int nY ) {}
inline void ETWMouseMove( int nX, int nY ) {}
inline void ETWMouseWheel( int nWheelDelta, int nX, int nY ) {}
inline void ETWKeyDown( int nScanCode, int nVirtualCode, const char *pChar ) {}

inline void ETWSendPacket( const char *pTo, int nWireSize, int nOutSequenceNR, int nOutSequenceNrAck ) {}
inline void ETWThrottled() {}
inline void ETWReadPacket( const char *pFrom, int nWireSize, int nInSequenceNR, int nOutSequenceNRAck ) {}

// This class calls the ETW Begin and End functions in order to insert a
// pair of events to bracket some work.
class CETWScope
{
public:
	CETWScope( const char* )
	{
	}
private:
	// Private and unimplemented to disable copying.
	CETWScope( const CETWScope& rhs );
	CETWScope& operator=( const CETWScope& rhs );
};

}
