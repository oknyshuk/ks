#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

// Max number of properties in a datatable and its children.
inline constexpr int kMaxDatatables = 1024;   // must be a power of 2.
inline constexpr auto MAX_DATATABLE_PROPS = 4096;

inline constexpr auto MAX_ARRAY_ELEMENTS = 2048; // a network array should have more that 1024 elements

inline constexpr float kHighDefault = -121121.121121f;

inline constexpr auto BITS_FULLRES = -1; // Use the full resolution of the type being encoded.
inline constexpr auto BITS_WORLDCOORD = -2; // Encode as a world coordinate.

inline constexpr auto DT_MAX_STRING_BITS = 9;
inline constexpr auto DT_MAX_STRING_BUFFERSIZE = (1<<DT_MAX_STRING_BITS); // Maximum length of a string that can be sent.

#define STRINGBUFSIZE(className, varName)	sizeof( ((className*)0)->varName )

// Gets the size of a variable in a class.
#define PROPSIZEOF(className, varName)		sizeof(((className*)0)->varName)


// SendProp::m_Flags.
inline constexpr auto SPROP_UNSIGNED = (1<<0); // Unsigned integer data.

inline constexpr auto SPROP_COORD = (1<<1); // If this is set, the float/vector is treated like a world coordinate.
										// Note that the bit count is ignored in this case.

inline constexpr auto SPROP_NOSCALE = (1<<2); // For floating point, don't scale into range, just take value as is.

inline constexpr auto SPROP_ROUNDDOWN = (1<<3); // For floating point, limit high value to range minus one bit unit

inline constexpr auto SPROP_ROUNDUP = (1<<4); // For floating point, limit low value to range minus one bit unit

inline constexpr auto SPROP_NORMAL = (1<<5); // If this is set, the vector is treated like a normal (only valid for vectors)
							
inline constexpr auto SPROP_EXCLUDE = (1<<6); // This is an exclude prop (not excludED, but it points at another prop to be excluded).

inline constexpr auto SPROP_XYZE = (1<<7); // Use XYZ/Exponent encoding for vectors.

inline constexpr auto SPROP_INSIDEARRAY = (1<<8); // This tells us that the property is inside an array, so it shouldn't be put into the
										// flattened property list. Its array will point at it when it needs to.

inline constexpr auto SPROP_PROXY_ALWAYS_YES = (1<<9); // Set for datatable props using one of the default datatable proxies like
										// SendProxy_DataTableToDataTable that always send the data to all clients.

inline constexpr auto SPROP_IS_A_VECTOR_ELEM = (1<<10); // Set automatically if SPROP_VECTORELEM is used.

inline constexpr auto SPROP_COLLAPSIBLE = (1<<11); // Set automatically if it's a datatable with an offset of 0 that doesn't change the pointer
										// (ie: for all automatically-chained base classes).
										// In this case, it can get rid of this SendPropDataTable altogether and spare the
										// trouble of walking the hierarchy more than necessary.

inline constexpr auto SPROP_COORD_MP = (1<<12); // Like SPROP_COORD, but special handling for multiplayer games
inline constexpr auto SPROP_COORD_MP_LOWPRECISION = (1<<13); // Like SPROP_COORD, but special handling for multiplayer games where the fractional component only gets a 3 bits instead of 5
inline constexpr auto SPROP_COORD_MP_INTEGRAL = (1<<14); // SPROP_COORD_MP, but coordinates are rounded to integral boundaries
inline constexpr auto SPROP_CELL_COORD = (1<<15); // Like SPROP_COORD, but special encoding for cell coordinates that can't be negative, bit count indicate maximum value
inline constexpr auto SPROP_CELL_COORD_LOWPRECISION = (1<<16); // Like SPROP_CELL_COORD, but special handling where the fractional component only gets a 3 bits instead of 5
inline constexpr auto SPROP_CELL_COORD_INTEGRAL = (1<<17); // SPROP_CELL_COORD, but coordinates are rounded to integral boundaries

inline constexpr auto SPROP_CHANGES_OFTEN = (1<<18); // this is an often changed field, moved to head of sendtable so it gets a small index

inline constexpr auto SPROP_VARINT = (1<<19); // use var int encoded (google protobuf style), note you want to include SPROP_UNSIGNED if needed, its more efficient

inline constexpr auto SPROP_NUMFLAGBITS_NETWORKED = 20;


// This is server side only, it's used to mark properties whose SendProxy_* functions encode against gpGlobals->tickcount (the only ones that currently do this are
//  m_flAnimTime and m_flSimulationTime.  MODs shouldn't need to mess with this probably
inline constexpr auto SPROP_ENCODED_AGAINST_TICKCOUNT = (1<<20);

// See SPROP_NUMFLAGBITS_NETWORKED for the ones which are networked
inline constexpr auto SPROP_NUMFLAGBITS = 21;

// Used by the SendProp and RecvProp functions to disable debug checks on type sizes.
inline constexpr int kSizeofIgnore = -1;


// Use this to extern send and receive datatables, and reference them.
#define EXTERN_SEND_TABLE(tableName)	namespace tableName {extern SendTable g_SendTable;}
#define EXTERN_RECV_TABLE(tableName)	namespace tableName {extern RecvTable g_RecvTable;}

#define REFERENCE_SEND_TABLE(tableName)	tableName::g_SendTable
#define REFERENCE_RECV_TABLE(tableName)	tableName::g_RecvTable


class SendProp;


typedef enum
{
	DPT_Int=0,
	DPT_Float,
	DPT_Vector,
	DPT_VectorXY, // Only encodes the XY of a vector, ignores Z
	DPT_String,
	DPT_Array,	// An array of the base types (can't be of datatables).
	DPT_DataTable,
	DPT_Int64,
	DPT_NUMSendPropTypes
} SendPropType;


class DVariant
{
public:
	DVariant()						{m_Type = DPT_Float;}
	explicit DVariant(float val)	{m_Type = DPT_Float; m_Float = val;}
				
	const char *ToString()
	{
		static char text[128];

		switch ( m_Type )
		{
			case DPT_Int : 
				V_snprintf( text, sizeof(text), "%i", (int)m_Int );
				break;
			case DPT_Float :
				V_snprintf( text, sizeof(text), "%.3f", m_Float );
				break;
			case DPT_Vector :
				V_snprintf( text, sizeof(text), "(%.3f,%.3f,%.3f)", 
							m_Vector[0], m_Vector[1], m_Vector[2] );
				break;
			case DPT_VectorXY :
				V_snprintf( text, sizeof(text), "(%.3f,%.3f)", 
							m_Vector[0], m_Vector[1] );
				break;
			case DPT_String : 
				if ( m_pString ) 
					return m_pString;
				else
					return "NULL";
				break;
			case DPT_Array :
				V_snprintf( text, sizeof(text), "Array" ); 
				break;
			case DPT_DataTable :
				V_snprintf( text, sizeof(text), "DataTable" ); 
				break;
			case DPT_Int64:
				V_snprintf( text, sizeof(text), "%lld", m_Int64 );
				break;
			default :
				V_snprintf( text, sizeof(text), "DVariant type %i unknown", m_Type ); 
				break;
		}

		return text;
	}

	union
	{
		float	m_Float;
		long	m_Int;
		const char	*m_pString;
		void	*m_pData;	// For DataTables.
		float	m_Vector[3];
		int64	m_Int64;
	};
	SendPropType	m_Type;
};


// This can be used to set the # of bits used to transmit a number between 0 and nMaxElements-1.
constexpr int NumBitsForCount( int nMaxElements )
{
	int nBits = 0;
	while ( nMaxElements > 0 )
	{
		++nBits;
		nMaxElements >>= 1;
	}
	return nBits;
}
