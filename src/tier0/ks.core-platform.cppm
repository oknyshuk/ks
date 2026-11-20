// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

export module ks.core.platform;

import std;

export import ks.core.types;
export import ks.core.wchartypes;

export using ::stdin;
export using ::stdout;
export using ::stderr;

export extern "C++" {

inline constexpr bool IsPlatformPosix() { return true; }
inline constexpr bool IsPlatformLinux() { return true; }
inline constexpr bool IsPlatformOSX() { return false; }

inline constexpr bool IsLinux() { return IsPlatformLinux(); }
inline constexpr bool IsOSX() { return IsPlatformOSX(); }
inline constexpr bool IsPosix() { return IsPlatformPosix(); }

inline constexpr bool IsDebug() { return false; }
inline constexpr bool IsRetail() { return false; }

//-----------------------------------------------------------------------------
// Portable data types
//-----------------------------------------------------------------------------
// The fixed-width names (int8/uint8/.../float64) are owned by :types; these are the OS-shaped spellings.
typedef void *HWND;
typedef unsigned int DWORD;
typedef unsigned int *LPDWORD;
typedef unsigned short WORD;
typedef void * HINSTANCE;
inline constexpr int k_cMaxPath = PATH_MAX;

inline constexpr int k_nSeekSet = 0;
inline constexpr int k_nSeekCur = 1;
inline constexpr int k_nSeekEnd = 2;
inline constexpr auto VALVE_RAND_MAX = 0x7fff;

inline constexpr int id386 = 0;

template <typename T>
inline constexpr void *INT_TO_POINTER( T i ) { return reinterpret_cast<void *>( static_cast<uintp>( i ) ); }
template <typename T>
inline constexpr int POINTER_TO_INT( T p ) { return static_cast<int>( reinterpret_cast<uintp>( p ) ); }

template <typename T, typename A>
inline constexpr T ALIGN_VALUE( T val, A alignment ) { return ( val + alignment - 1 ) & ~( alignment - 1 ); }

//-----------------------------------------------------------------------------
// Used to break into the debugger
//-----------------------------------------------------------------------------
inline void DebuggerBreak() { __asm__( "int $0x3;" ); }

[[gnu::always_inline]] inline float fsel(float fComparand, float fValGE, float fLT)
{
	return fComparand >= 0 ? fValGE : fLT;
}
[[gnu::always_inline]] inline double fsel(double fComparand, double fValGE, double fLT)
{
	return fComparand >= 0 ? fValGE : fLT;
}

//-----------------------------------------------------------------------------
// Returns true if debugger attached, false otherwise
//-----------------------------------------------------------------------------

bool Plat_IsInDebugSession();

inline void DebuggerBreakIfDebugging() { if ( Plat_IsInDebugSession() ) DebuggerBreak(); }

//-----------------------------------------------------------------------------
// Posix platform helpers
//-----------------------------------------------------------------------------

typedef void *HMODULE;
typedef void *HANDLE;

//-----------------------------------------------------------------------------
// Generally useful platform-independent macros (move to another file?)
//-----------------------------------------------------------------------------

// need macro for constant expression

// Force a function call site -not- to inlined. (useful for profiling)

// Marks the codepath from here until the next branch entry point as unreachable,
// and asserts if any attempt is made to execute it.
inline void UNREACHABLE() { contract_assert( 0 ); }

// In cases where no default is present or appropriate, this causes MSVC to generate
// as little code as possible, and throw an assertion in debug.

inline constexpr int MAX_FILEPATH = 512;

inline constexpr int MAX_PATH = 260;

inline constexpr int MAX_UNICODE_PATH = MAX_PATH;
inline constexpr auto MAX_UNICODE_PATH_IN_UTF8 = MAX_UNICODE_PATH*4;

//-----------------------------------------------------------------------------
// FP exception handling
//-----------------------------------------------------------------------------
//#define CHECK_FLOAT_EXCEPTIONS		1

	inline void SetupFPUControlWord()
	{
		__volatile unsigned short int __cw;
		__asm __volatile ("fnstcw %0" : "=m" (__cw));
		__cw = __cw & 0x0FCC0;	// keep infinity control, keep rounding mode
		__cw = __cw | 0x023F;	// set 53-bit, no exceptions
		__asm __volatile ("fldcw %0" : : "m" (__cw));
	}

//-----------------------------------------------------------------------------
// Portability casting
//-----------------------------------------------------------------------------
template < typename Tdst, typename Tsrc > [[gnu::always_inline]] inline Tdst size_cast( Tsrc val )
{
	static_assert( sizeof( Tdst ) <= sizeof( uint64 ) && sizeof( Tsrc ) <= sizeof( uint64 ), "Okay in my defense there weren't any types larger than 64-bits when this code was written." );

	return ( Tdst )val;
}

//-------------------------------------
// Fast swaps
//-------------------------------------

// No ASM implementation for this yet

//-------------------------------------
// The typically used methods.
//-------------------------------------

// Little-endian only: the Little* forms are the identity.

// Pass floats by pointer for swapping to avoid truncation in the fpu
template <typename T>
inline void LittleFloat( T *pOut, const T *pIn ) { *pOut = *pIn; }

inline uint32 LoadLittleDWord( uint32 *base, unsigned int dwordIndex )
{
	return ( base[dwordIndex] );
}

inline void StoreLittleDWord( uint32 *base, unsigned int dwordIndex, uint32 dword )
{
	base[dwordIndex] = (dword);
}

// Silences a number of warnings on 360 compiles.
inline uint64 CastPtrToUint64( const void *p )
{
	return (uint64)( (uintp)p );
}

inline int64 CastPtrToInt64( const void *p )
{
	return (int64)( (uintp)p );
}

// When in benchmark mode, the timer returns a simple incremented value each time you call it.
//
// It should not be changed after startup unless you really know what you're doing. The only place
// that should do this is the benchmark code itself so it can output a legit duration.
void				Plat_SetBenchmarkMode( bool bBenchmarkMode );	
bool				Plat_IsInBenchmarkMode();

// Same as time()
uint64		    Plat_GetTime();

double			Plat_FloatTime();		// Returns time in seconds since the module was loaded.
uint32			Plat_MSTime();			// Time in milliseconds.
uint64			Plat_USTime();			// Time in microseconds.
uint64			Plat_GetClockStart();	// Snapshot of the clock when app started.
int32			Plat_timezone( void );
int32			Plat_daylight( void );

void				Plat_ExitProcess( int nCode );

// Pauses or resumes VTune sampling. Returns false when no profiler is attached.
bool vtune( bool resume );

inline uint64 Plat_Rdtsc()
{
	uint32 lo, hi;
	__asm__ __volatile__ ( "rdtsc" : "=a" (lo), "=d" (hi));
	return ( ( ( uint64 )hi ) << 32 ) | lo;
}

// b/w compatibility

// Processor Information:
struct CPUInformation
{
	int	 m_Size;		// Size of this structure, for forward compatability.

	uint8 m_nLogicalProcessors;		// Number op logical processors.
	uint8 m_nPhysicalProcessors;	// Number of physical processors

	bool m_bRDTSC : 1,	// Is RDTSC supported?
		 m_bCMOV  : 1,  // Is CMOV supported?
		 m_bFCMOV : 1,  // Is FCMOV supported?
		 m_bSSE	  : 1,	// Is SSE supported?
		 m_bSSE2  : 1,	// Is SSE2 Supported?
		 m_bMMX   : 1,	// Is MMX supported?
		 m_bHT	  : 1;	// Is HyperThreading supported?

	bool m_bSSE3 : 1,
		 m_bSSSE3 : 1,
		 m_bSSE4a : 1,
		 m_bSSE41 : 1,
		 m_bSSE42 : 1,
		 m_bAVX   : 1;  // Is AVX supported?

	int64 m_Speed;						// In cycles per second.

	tchar* m_szProcessorID;				// Processor vendor Identification.
	tchar* m_szProcessorBrand;			// Processor brand string, if available

	uint32 m_nModel;
	uint32 m_nFeatures[ 3 ];
	uint32 m_nL1CacheSizeKb;
	uint32 m_nL1CacheDesc;
	uint32 m_nL2CacheSizeKb;
	uint32 m_nL2CacheDesc;
	uint32 m_nL3CacheSizeKb;
	uint32 m_nL3CacheDesc;

	CPUInformation(): m_Size(0){}
};

const CPUInformation& GetCPUInformation();

//-----------------------------------------------------------------------------
// XBOX Components valid in PC compilation space
//-----------------------------------------------------------------------------

inline constexpr const char *PLATFORM_EXT = "";

inline const char *GetPlatformExt( void )
{
	return PLATFORM_EXT;
}

// flat view, 6 hw threads

// core view, 3 cores with 2 hw threads each

//-----------------------------------------------------------------------------
// There is no requirement that a std::va_list be usable in multiple calls,
// but the Steam code does this.  Linux64 does not support reuse, whereas
// Windows does, so Linux64 breaks on code that was written and working
// on Windows.  Fortunately Linux has va_copy, which provides a simple
// way to let a std::va_list be used multiple times.  Unfortunately Windows
// does not have va_copy, so here we provide things to hide the difference.
//-----------------------------------------------------------------------------

class CReuseVaList
{
public:
	CReuseVaList( std::va_list List )
	{
		__builtin_va_copy( m_ReuseList, List );
	}
	~CReuseVaList()
	{
		__builtin_va_end( m_ReuseList );
	}

	std::va_list m_ReuseList;
};

//-----------------------------------------------------------------------------
// Methods to invoke the constructor, copy constructor, and destructor
//-----------------------------------------------------------------------------

//  Watchdog timer support. Call BeginWatchdogTimer( nn ) to kick the timer off.  if you don't call
// EndWatchdogTimer within nn seconds, the program will kick off an exception.  This is for making
// sure that hung dedicated servers abort (and restart) instead of staying hung. Calling
// EndWatchdogTimer more than once or when there is no active watchdog is fine. Only does anything
// under linux right now. It should be possible to implement this functionality in windows via a
// thread, if desired.

void BeginWatchdogTimer( int nSecs );
void EndWatchdogTimer( void );
void ResetBaseTime( void );							  // reset plat_floattime to 0 for a subprocess

// Use AlignedByteArray_t if you need an appropriately aligned array of T with no constructor (e.g. an inline buffer):
//  - usage:		AlignedByteArray_t< NUM, T >
//  - same as:		byte[ NUM*sizeof(T) ]
//  - BUT:			avoids calling T's constructor
//  - AND:			has same alignment as T
// [ Thanks to CygnusX1: http://stackoverflow.com/questions/5134217/aligning-data-on-the-stack-c ]

// gnuc has the align decoration at the end

// !!! NOTE: if you get a compile error here, you are using VALIGNOF on an abstract type :NOTE !!!

// Use ValidateAlignment to sanity-check alignment usage when allocating arrays of an aligned type
template< class T, int ALIGN >
inline void ValidateAlignmentExplicit(void)
{
	// Alignment must be a power of two
	static_assert( (ALIGN & (ALIGN - 1)) == 0 );
	// Alignment must not imply gaps in the array (which array-of-T storage does not allow for)
	static_assert( ALIGN <= sizeof(T) );
	// Alignment must be a multiple of the size of the object type, or elements will *NOT* be aligned!
	static_assert( (sizeof(T) % ALIGN) == 0 );
	// Alignment should be a multiple of the base alignment of T
//	static_assert( (ALIGN % __alignof( T )) == 0 );
}
template< class T > inline void ValidateAlignment(void) { ValidateAlignmentExplicit<T, __alignof(T)>(); }

template<class T> struct AlignOf_t { AlignOf_t(){} AlignOf_t & operator=(const AlignOf_t &) { return *this; } byte b; T t; };

template < size_t NUM, class T, int ALIGN > struct AlignedByteArrayExplicit_t;
template < size_t NUM, class T > struct AlignedByteArray_t : public AlignedByteArrayExplicit_t< NUM, T, (int)( sizeof( AlignOf_t<T> ) - sizeof( T ) ) > {};

template < size_t NUM, class T, int ALIGN >
struct AlignedByteArrayExplicit_t
{
	// NOTE: verifies alignment in the constructor (which may be wrong if this is heap-allocated, for ALIGN > MEMALLOC_MAX_AUTO_ALIGN)
	AlignedByteArrayExplicit_t() { if ( ( ALIGN - 1 ) & (size_t)this ) DebuggerBreakIfDebugging(); }
	T *Base( void ) { ValidateAlignmentExplicit<T, ALIGN>(); return (T *)&m_Data; }
	const T *Base( void ) const { ValidateAlignmentExplicit<T, ALIGN>(); return (const T *)&m_Data; }
private:
	alignas( ALIGN ) byte m_Data[ NUM * sizeof( T ) ];
};

// Tier0 uses this for faster stricmp.
int V_tier0_stricmp( const char *a, const char *b );
int _V_stricmp( const char *s1, const char *s2 );
int _V_stricmp_NegativeForUnequal( const char *s1, const char *s2 );
int V_strncasecmp( const char *s1, const char *s2, int n );

inline int stricmp( const char *a, const char *b ) { return _V_stricmp( a, b ); }
inline int strcmpi( const char *a, const char *b ) { return _V_stricmp( a, b ); }
inline int strnicmp( const char *a, const char *b, int n ) { return V_strncasecmp( a, b, n ); }

void V_tier0_strncpy( char *a, const char *b, int n );
char *V_tier0_strncat( char *a, const char *b, int n, int m = -1 );
int V_tier0_vsnprintf( char *a, int n, const char *f, std::va_list l ) [[gnu::format(printf, 3, 0)]];
int V_tier0_snprintf( char *a, int n, const char *f, ... ) [[gnu::format(printf, 3, 4)]];

//-----------------------------------------------------------------------------

char const * Plat_GetEnv(char const *pEnvVarName);

bool Plat_GetExecutablePath(char* pBuff, size_t nBuff);

//-----------------------------------------------------------------------------
// Xbox/Console stub types for non-console platforms
// These are remnants of Xbox 360/PS3 code that have been stubbed out
//-----------------------------------------------------------------------------

// XUID is Xbox User ID - stub as uint64
typedef uint64 XUID;

// XNKID is Xbox Network Key ID
struct XNKID
{
	byte ab[8];
};

// Xbox user constants
inline constexpr int XUSER_MAX_COUNT = 4;
inline constexpr auto XPROFILE_SETTING_MAX_SIZE = 1000;

// Xbox profile title data IDs

// Invalid XUID constant
inline constexpr XUID INVALID_XUID = 0;

// Xbox stub functions - always return 0 (primary user)
inline int XBX_GetPrimaryUserId() { return 0; }
inline int XBX_GetUserId( int nSlot ) { return 0; }
inline unsigned int XBX_GetNumGameUsers() { return 1; }

// Xbox language/locale stub functions
inline bool XBX_IsAudioLocalized() { return false; }

// Xbox storage device stubs
inline constexpr auto XBX_INVALID_STORAGE_ID = ((DWORD)-1);
inline int XBX_GetSlotByUserId( int nUserID ) { return 0; }
inline int XBX_GetActiveUserId() { return 0; }

// XCONTENT_DATA - Xbox content data structure (used for DLC)
struct XCONTENT_DATA
{
	wchar_t szDisplayName[128];
	char szFileName[64];
};

// Windows error code stub
inline constexpr int ERROR_SUCCESS = 0;

// Async handle type stub
typedef void* AsyncHandle_t;

// Xbox display safe area constants (percentage of screen)

using ::uint;

inline int _stricmp( const char *a, const char *b ) { return strcasecmp( a, b ); }
inline int _chdir( const char *path ) { return chdir( path ); }
inline void *GetProcAddress( void *hModule, const char *pName ) { return dlsym( hModule, pName ); }
inline int _mkdir( const char *path ) { return mkdir( path, 0777 ); }
inline int _wtoi( const wchar_t *s ) { return static_cast<int>( wcstol( s, nullptr, 10 ) ); }
inline int64 _wtoi64( const wchar_t *s ) { return wcstoll( s, nullptr, 10 ); }

template <class T>
inline void Destruct( T* pMemory )
{
	pMemory->~T();
}

template <>
inline void Destruct( float __attribute__((__vector_size__(16)))* pMemory )
{
}

template <typename T>
inline T WordSwapC( T w )
{
	static_assert( sizeof( T ) == sizeof(uint16) );
	return (T)__builtin_bswap16( (uint16)w );
}

template <typename T>
inline T DWordSwapC( T dw )
{
	static_assert( sizeof( T ) == sizeof(uint32) );
	return (T)__builtin_bswap32( (uint32)dw );
}

template <typename T>
inline T QWordSwapC( T dw )
{
	static_assert( sizeof( T ) == sizeof(uint64) );
	return (T)__builtin_bswap64( (uint64)dw );
}

static_assert( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "little-endian target required" );

inline void SafeSwapFloat( float *pOut, const float *pIn )
{
	*pOut = __builtin_bit_cast( float, __builtin_bswap32( __builtin_bit_cast( uint32, *pIn ) ) );
}

template <class T>
inline T* Construct( T* pMemory )
{
	return ::new( pMemory ) T;
}

template <class T, typename ARG1>
inline T* Construct( T* pMemory, ARG1 a1 )
{
	return ::new( pMemory ) T( a1 );
}

template <class T, typename ARG1, typename ARG2>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2 )
{
	return ::new( pMemory ) T( a1, a2 );
}

template <class T, typename ARG1, typename ARG2, typename ARG3>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2, ARG3 a3 )
{
	return ::new( pMemory ) T( a1, a2, a3 );
}

template <class T, typename ARG1, typename ARG2, typename ARG3, typename ARG4>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2, ARG3 a3, ARG4 a4 )
{
	return ::new( pMemory ) T( a1, a2, a3, a4 );
}

template <class T, typename ARG1, typename ARG2, typename ARG3, typename ARG4, typename ARG5>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2, ARG3 a3, ARG4 a4, ARG5 a5 )
{
	return ::new( pMemory ) T( a1, a2, a3, a4, a5 );
}

template <class T>
inline T* CopyConstruct( T* pMemory, T const& src )
{
	return ::new( pMemory ) T(src);
}

template <class T>
inline T* MoveConstruct( T* pMemory, T&& src )
{
	return ::new( pMemory ) T( static_cast<T&&>( src ) );
}

template <typename T>
inline T Rotl( T v, int n )
{
	constexpr int w = int( sizeof( T ) * 8 );
	n %= w;
	if ( n < 0 ) n += w;
	return (T)( ( v << n ) | ( v >> ( ( w - n ) % w ) ) );
}

template <typename T>
inline T Rotr( T v, int n ) { return Rotl<T>( v, -n ); }

[[gnu::always_inline]] inline uint32 RotateBitsLeft32( uint32 nValue, int nRotateBits )  { return Rotl( nValue, nRotateBits ); }
[[gnu::always_inline]] inline uint64 RotateBitsLeft64( uint64 nValue, int nRotateBits )  { return Rotl( nValue, nRotateBits ); }
[[gnu::always_inline]] inline uint32 RotateBitsRight32( uint32 nValue, int nRotateBits ) { return Rotr( nValue, nRotateBits ); }
[[gnu::always_inline]] inline uint64 RotateBitsRight64( uint64 nValue, int nRotateBits ) { return Rotr( nValue, nRotateBits ); }

void Plat_GetLocalTime( struct tm *pNow );
void Plat_ConvertToLocalTime( uint64 nTime, struct tm *pNow );
struct tm *Plat_localtime( const time_t *timep, struct tm *result );
void Plat_GetTimeString( struct tm *pTime, char *pOut, int nMaxBytes );
void Platform_gmtime( uint64 nTime, struct tm *pTime );
time_t Plat_timegm( struct tm *timeptr );
inline struct tm *Plat_gmtime( const time_t *timep, struct tm *result ) { Platform_gmtime( *timep, result ); return result; }
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize );

}
