// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation



#undef fopen
import std;
import ks.core;
#include "voice_wavefile.h"

static unsigned long ReadDWord(std::FILE * fp) 
{
	unsigned long ret;  
	std::fread( &ret, 4, 1, fp );
	return ret;
}

static unsigned short ReadWord(std::FILE * fp) 
{
	unsigned short ret; 
	std::fread( &ret, 2, 1, fp );
	return ret;
}

static void WriteDWord(std::FILE * fp, unsigned long val) 
{
	std::fwrite( &val, 4, 1, fp );
}

static void WriteWord(std::FILE * fp, unsigned short val) 
{
	std::fwrite( &val, 2, 1, fp );
}



bool ReadWaveFile(
	const char *pFilename,
	char *&pData,
	int &nDataBytes,
	int &wBitsPerSample,
	int &nChannels,
	int &nSamplesPerSec)
{
	std::FILE * fp = std::fopen(pFilename, "rb");
	if(!fp)
		return false;

	std::fseek( fp, 22, k_nSeekSet );
	
	nChannels = ReadWord(fp);
	nSamplesPerSec = ReadDWord(fp);

	std::fseek(fp, 34, k_nSeekSet);
	wBitsPerSample = ReadWord(fp);

	std::fseek(fp, 40, k_nSeekSet);
	nDataBytes = ReadDWord(fp);
	ReadDWord(fp);
	pData = new char[nDataBytes];
	if(!pData)
	{
		std::fclose(fp);
		return false;
	}
	std::fread(pData, nDataBytes, 1, fp);
	std::fclose( fp );
	return true;
}

bool WriteWaveFile(
	const char *pFilename, 
	const char *pData, 
	int nBytes, 
	int wBitsPerSample, 
	int nChannels, 
	int nSamplesPerSec)
{
	std::FILE * fp = std::fopen(pFilename, "wb");
	if(!fp)
		return false;

	// Write the RIFF chunk.
	std::fwrite("RIFF", 4, 1, fp);
	WriteDWord(fp, 0);
	std::fwrite("WAVE", 4, 1, fp);
	

	// Write the FORMAT chunk.
	std::fwrite("fmt ", 4, 1, fp);
	
	WriteDWord(fp, 0x10);
	WriteWord(fp, 1);	// WAVE_FORMAT_PCM
	WriteWord(fp, (unsigned short)nChannels);	
	WriteDWord(fp, (unsigned long)nSamplesPerSec);
	WriteDWord(fp, (unsigned long)((wBitsPerSample / 8) * nChannels * nSamplesPerSec));
	WriteWord(fp, (unsigned short)((wBitsPerSample / 8) * nChannels));
	WriteWord(fp, (unsigned short)wBitsPerSample);

	// Write the DATA chunk.
	std::fwrite("data", 4, 1, fp);
	WriteDWord(fp, (unsigned long)nBytes);
	std::fwrite(pData, nBytes, 1, fp);


	// Go back and write the length of the riff file.
	unsigned long dwVal = std::ftell(fp) - 8;
	std::fseek( fp, 4, k_nSeekSet );
	WriteDWord(fp, dwVal);

	std::fclose(fp);
	return true;
}
