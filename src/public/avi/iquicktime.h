#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2010 Valve Corporation

import std;
import ks.tier1;

inline constexpr auto QUICKTIME_LOOP_MOVIE = 0x01;
inline constexpr auto QUICKTIME_PRELOAD = 0x02;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct BGR888_t;
class IMaterial;

//-----------------------------------------------------------------------------
// Parameters for creating a new BINK
//-----------------------------------------------------------------------------
struct QuickTimeParams_t
{
	QuickTimeParams_t() :
		m_nFrameRate( 0 ), m_nFrameScale( 1 ), m_nWidth( 0 ), m_nHeight( 0 ),
		m_nSampleRate( 0 ), m_nSampleBits( 0 ), m_nNumChannels( 0 )
	{
		m_pFileName[ 0 ] = 0;
	}

	char		m_pFileName[ 256 ];
	char		m_pPathID[ 256 ];

	// fps = m_nFrameRate / m_nFrameScale
	// for integer framerates, set framerate to the fps, and framescale to 1
	// for ntsc-style framerates like 29.97 (or 23.976 or 59.94),
	// set framerate to 30,000 (or 24,000 or 60,000) and framescale to 1001
	// yes, framescale is an odd naming choice, but it matching MS's AVI api
	int			m_nFrameRate;
	int			m_nFrameScale;

	int			m_nWidth;
	int			m_nHeight;

	// Sound/.wav info
	int			m_nSampleRate;
	int			m_nSampleBits;
	int			m_nNumChannels;
};

//-----------------------------------------------------------------------------
// Handle to an QUICKTIME
//-----------------------------------------------------------------------------
typedef unsigned short QUICKTIMEHandle_t;
enum
{
	QUICKTIMEHANDLE_INVALID = (QUICKTIMEHandle_t)~0
};


//-----------------------------------------------------------------------------
// Handle to an QUICKTIME material
//-----------------------------------------------------------------------------
typedef unsigned short QUICKTIMEMaterial_t;
enum
{
	QUICKTIMEMATERIAL_INVALID = (QUICKTIMEMaterial_t)~0
};


//-----------------------------------------------------------------------------
// Main QUICKTIME interface
//-----------------------------------------------------------------------------
inline constexpr const char *QUICKTIME_INTERFACE_VERSION = "IQuickTime001";

class IQuickTime : public IAppSystem
{
public:
	// Create/destroy a QUICKTIME material (a materialsystem IMaterial)
	virtual QUICKTIMEMaterial_t CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags = 0 ) = 0;
	virtual void DestroyMaterial( QUICKTIMEMaterial_t hMaterial ) = 0;
	
	// Update the frame (if necessary)
	virtual bool Update( QUICKTIMEMaterial_t hMaterial ) = 0;
	
	// Determines if a new frame of the movie is ready for display
	virtual bool ReadyForSwap( QUICKTIMEMaterial_t hMaterial ) = 0;

	// Gets the IMaterial associated with an BINK material
	virtual IMaterial* GetMaterial( QUICKTIMEMaterial_t hMaterial ) = 0;

	// Returns the max texture coordinate of the BINK
	virtual void GetTexCoordRange( QUICKTIMEMaterial_t hMaterial, float *pMaxU, float *pMaxV ) = 0;

	// Returns the frame size of the QUICKTIME Image Frame (stored in a subrect of the material itself)
	virtual void GetFrameSize( QUICKTIMEMaterial_t hMaterial, int *pWidth, int *pHeight ) = 0;

	// Returns the frame rate of the QUICKTIME
	virtual int GetFrameRate( QUICKTIMEMaterial_t hMaterial ) = 0;

	// Sets the frame for an BINK material (use instead of SetTime)
	virtual void SetFrame( QUICKTIMEMaterial_t hMaterial, float flFrame ) = 0;

	// Returns the total frame count of the BINK
	virtual int GetFrameCount( QUICKTIMEMaterial_t hMaterial ) = 0;

	virtual bool SetSoundDevice( void *pDevice ) = 0;
};
