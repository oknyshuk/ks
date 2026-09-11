//========= Copyright (c) 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef STEAMTYPES_H
#define STEAMTYPES_H

// Steam-specific types. Defined here so this header file can be included in other code bases.
#ifndef WCHARTYPES_H
typedef unsigned char uint8;
#endif
#ifndef uint16
typedef unsigned short uint16;
#endif
#ifndef int32
typedef signed long int32;
#endif
#ifndef uint32
typedef unsigned long uint32;
#endif
#ifndef int64
typedef long long int64;
#endif
#ifndef uint64
typedef unsigned long long uint64;
#endif

#ifndef NETADR_H
class netadr_t;
#endif

#endif // STEAMTYPES_H
