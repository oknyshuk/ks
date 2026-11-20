#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.types;
import ks.platform;
import ks.simd;
// for XING VBR Header flags
inline constexpr auto FRAMES_FLAG = 0x0001;
inline constexpr auto BYTES_FLAG = 0x0002;
inline constexpr auto TOC_FLAG = 0x0004;
inline constexpr auto VBR_SCALE_FLAG = 0x0008;

class CMPAFile;

class CVBRHeader
{
public:
	enum VBRHeaderType
	{
		NoHeader,
		XINGHeader,
		VBRIHeader
	};

	CVBRHeader( CMPAFile* pMPAFile, VBRHeaderType HeaderType, uint32 dwOffset );
	~CVBRHeader(void);

	static bool IsVBRHeaderAvailable( CMPAFile* pMPAFile, VBRHeaderType& HeaderType, uint32& dwOffset );
	bool SeekPoint(float fPercent, uint32& dwSeekPoint);

	uint32 m_dwBytesPerSec;
	uint32 m_dwBytes;		// total number of bytes
	uint32 m_dwFrames;		// total number of frames

private:
	static uint32 m_dwXINGOffsets[2][2];

	static bool CheckID( CMPAFile* pMPAFile, char ch0, char ch1, char ch2, char ch3, uint32& dwOffset );
	static bool CheckXING( CMPAFile* pMPAFile, uint32& dwOffset );
	static bool CheckVBRI( CMPAFile* pMPAFile, uint32& dwOffset );

	bool ExtractLAMETag( uint32 dwOffset );
	bool ExtractXINGHeader( uint32 dwOffset );	
	bool ExtractVBRIHeader( uint32 dwOffset );

	uint32 SeekPointXING(float fPercent)const ;
	uint32 SeekPointVBRI(float fPercent) const;
	uint32 SeekPointByTimeVBRI(float fEntryTimeMS) const;

	CMPAFile* m_pMPAFile;
public:	
	VBRHeaderType m_HeaderType;
	uint32 m_dwOffset;
	uint32 m_dwQuality;	// quality (0..100)
	int* m_pnToc;				// TOC points for seeking (must be freed)
	uint32 m_dwTableSize;	// size of table (number of entries)	

	// only VBRI
	float m_fDelay;	
	uint32 m_dwTableScale;	// for seeking
	uint32 m_dwBytesPerEntry;
    uint32 m_dwFramesPerEntry;
	uint32 m_dwVersion;
};
