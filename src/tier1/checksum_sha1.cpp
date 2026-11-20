// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: Implementation of SHA-1

/*
	100% free public domain implementation of the SHA-1
	algorithm by Dominik Reichl <dominik.reichl@t-online.de>


	=== Test Vectors (from FIPS PUB 180-1) ===

	SHA1("abc") =
		A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D

	SHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") =
		84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1

	SHA1(A million repetitions of "a") =
		34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/


import std;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.checksum_sha1;

constexpr auto MAX_FILE_READ_BUFFER = 8000;

CSHA1::CSHA1()
{
	m_block = (SHA1_WORKSPACE_BLOCK *)m_workspace;

	Reset();
}
CSHA1::~CSHA1()
{
	// Reset();
}
void CSHA1::Reset()
{
	// SHA1 initialization constants
	m_state[0] = 0x67452301;
	m_state[1] = 0xEFCDAB89;
	m_state[2] = 0x98BADCFE;
	m_state[3] = 0x10325476;
	m_state[4] = 0xC3D2E1F0;

	m_count[0] = 0;
	m_count[1] = 0;
}

void CSHA1::Transform(uint32 state[5], const uint8 buffer[64])
{
	uint32 a = 0, b = 0, c = 0, d = 0, e = 0;

	std::memcpy(m_block, buffer, 64);

	// Copy state[] to working vars
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

	auto blk0 = [this]( int i ) -> uint32 { return m_block->l[i] = ( std::rotl( m_block->l[i], 24 ) & 0xFF00FF00 ) | ( std::rotl( m_block->l[i], 8 ) & 0x00FF00FF ); };
	auto blk = [this]( int i ) -> uint32 { return m_block->l[i & 15] = std::rotl( m_block->l[( i + 13 ) & 15] ^ m_block->l[( i + 8 ) & 15] ^ m_block->l[( i + 2 ) & 15] ^ m_block->l[i & 15], 1 ); };

	auto r0 = [&]( uint32 v, uint32 &w, uint32 x, uint32 y, uint32 &z, int i ) { z += ( ( w & ( x ^ y ) ) ^ y ) + blk0( i ) + 0x5A827999 + std::rotl( v, 5 ); w = std::rotl( w, 30 ); };
	auto r1 = [&]( uint32 v, uint32 &w, uint32 x, uint32 y, uint32 &z, int i ) { z += ( ( w & ( x ^ y ) ) ^ y ) + blk( i ) + 0x5A827999 + std::rotl( v, 5 ); w = std::rotl( w, 30 ); };
	auto r2 = [&]( uint32 v, uint32 &w, uint32 x, uint32 y, uint32 &z, int i ) { z += ( w ^ x ^ y ) + blk( i ) + 0x6ED9EBA1 + std::rotl( v, 5 ); w = std::rotl( w, 30 ); };
	auto r3 = [&]( uint32 v, uint32 &w, uint32 x, uint32 y, uint32 &z, int i ) { z += ( ( ( w | x ) & y ) | ( w & x ) ) + blk( i ) + 0x8F1BBCDC + std::rotl( v, 5 ); w = std::rotl( w, 30 ); };
	auto r4 = [&]( uint32 v, uint32 &w, uint32 x, uint32 y, uint32 &z, int i ) { z += ( w ^ x ^ y ) + blk( i ) + 0xCA62C1D6 + std::rotl( v, 5 ); w = std::rotl( w, 30 ); };

	// 4 rounds of 20 operations each. Loop unrolled.
	r0(a,b,c,d,e, 0); r0(e,a,b,c,d, 1); r0(d,e,a,b,c, 2); r0(c,d,e,a,b, 3);
	r0(b,c,d,e,a, 4); r0(a,b,c,d,e, 5); r0(e,a,b,c,d, 6); r0(d,e,a,b,c, 7);
	r0(c,d,e,a,b, 8); r0(b,c,d,e,a, 9); r0(a,b,c,d,e,10); r0(e,a,b,c,d,11);
	r0(d,e,a,b,c,12); r0(c,d,e,a,b,13); r0(b,c,d,e,a,14); r0(a,b,c,d,e,15);
	r1(e,a,b,c,d,16); r1(d,e,a,b,c,17); r1(c,d,e,a,b,18); r1(b,c,d,e,a,19);
	r2(a,b,c,d,e,20); r2(e,a,b,c,d,21); r2(d,e,a,b,c,22); r2(c,d,e,a,b,23);
	r2(b,c,d,e,a,24); r2(a,b,c,d,e,25); r2(e,a,b,c,d,26); r2(d,e,a,b,c,27);
	r2(c,d,e,a,b,28); r2(b,c,d,e,a,29); r2(a,b,c,d,e,30); r2(e,a,b,c,d,31);
	r2(d,e,a,b,c,32); r2(c,d,e,a,b,33); r2(b,c,d,e,a,34); r2(a,b,c,d,e,35);
	r2(e,a,b,c,d,36); r2(d,e,a,b,c,37); r2(c,d,e,a,b,38); r2(b,c,d,e,a,39);
	r3(a,b,c,d,e,40); r3(e,a,b,c,d,41); r3(d,e,a,b,c,42); r3(c,d,e,a,b,43);
	r3(b,c,d,e,a,44); r3(a,b,c,d,e,45); r3(e,a,b,c,d,46); r3(d,e,a,b,c,47);
	r3(c,d,e,a,b,48); r3(b,c,d,e,a,49); r3(a,b,c,d,e,50); r3(e,a,b,c,d,51);
	r3(d,e,a,b,c,52); r3(c,d,e,a,b,53); r3(b,c,d,e,a,54); r3(a,b,c,d,e,55);
	r3(e,a,b,c,d,56); r3(d,e,a,b,c,57); r3(c,d,e,a,b,58); r3(b,c,d,e,a,59);
	r4(a,b,c,d,e,60); r4(e,a,b,c,d,61); r4(d,e,a,b,c,62); r4(c,d,e,a,b,63);
	r4(b,c,d,e,a,64); r4(a,b,c,d,e,65); r4(e,a,b,c,d,66); r4(d,e,a,b,c,67);
	r4(c,d,e,a,b,68); r4(b,c,d,e,a,69); r4(a,b,c,d,e,70); r4(e,a,b,c,d,71);
	r4(d,e,a,b,c,72); r4(c,d,e,a,b,73); r4(b,c,d,e,a,74); r4(a,b,c,d,e,75);
	r4(e,a,b,c,d,76); r4(d,e,a,b,c,77); r4(c,d,e,a,b,78); r4(b,c,d,e,a,79);

	// Add the working vars back into state[]
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;

	// Wipe variables
	a = b = c = d = e = 0;
}

// Use this function to hash in binary data and strings
void CSHA1::Update( const void *pvData, unsigned int len)
{
	const uint8 *data = (const uint8 *)pvData;
	uint32 i = 0, j;

	j = (m_count[0] >> 3) & 63;

	if((m_count[0] += len << 3) < (len << 3)) m_count[1]++;

	m_count[1] += (len >> 29);

	if((j + len) > 63)
	{
		std::memcpy(&m_buffer[j], data, (i = 64 - j));
		Transform(m_state, m_buffer);

		for (; i+63 < len; i += 64)
			Transform(m_state, &data[i]);

		j = 0;
	}
	else i = 0;

	std::memcpy(&m_buffer[j], &data[i], len - i);
}

// Hash in file contents
bool CSHA1::HashFile(const char *szFileName)
{
	uint32 ulFileSize = 0, ulRest = 0, ulBlocks = 0;
	uint32 i = 0;
	uint8 uData[MAX_FILE_READ_BUFFER];
	std::FILE *fIn = nullptr;

	if(szFileName == nullptr) return(false);

	if((fIn = std::fopen(szFileName, "rb")) == nullptr) return(false);

	std::fseek(fIn, 0, k_nSeekEnd);
	ulFileSize = std::ftell(fIn);
	std::fseek(fIn, 0, k_nSeekSet);

	ulRest = ulFileSize % MAX_FILE_READ_BUFFER;
	ulBlocks = ulFileSize / MAX_FILE_READ_BUFFER;

	for(i = 0; i < ulBlocks; i++)
	{
		std::fread(uData, 1, MAX_FILE_READ_BUFFER, fIn);
		Update(uData, MAX_FILE_READ_BUFFER);
	}

	if(ulRest != 0)
	{
		std::fread(uData, 1, ulRest, fIn);
		Update(uData, ulRest);
	}

	std::fclose(fIn);
	fIn = nullptr;

	return(true);
}

void CSHA1::Final()
{
	uint32 i = 0;
	uint8 finalcount[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	for (i = 0; i < 8; i++)
		finalcount[i] = (uint8)((m_count[(i >= 4 ? 0 : 1)]
			>> ((3 - (i & 3)) * 8) ) & 255); // Endian independent

	Update((uint8 *)"\200", 1);

	while ((m_count[0] & 504) != 448)
		Update((uint8 *)"\0", 1);

	Update(finalcount, 8); // Cause a SHA1Transform()

	for (i = 0; i < k_cubHash; i++)
	{
		m_digest[i] = (uint8)((m_state[i >> 2] >> ((3 - (i & 3)) * 8) ) & 255);
	}

	// Wipe variables for security reasons
	i = 0;
	std::memset(m_buffer, 0, sizeof(m_buffer) );
	std::memset(m_state, 0, sizeof(m_state) );
	std::memset(m_count, 0, sizeof(m_count) );
	std::memset(finalcount, 0, sizeof( finalcount) );

	Transform(m_state, m_buffer);
}

// Get the final hash as a pre-formatted string
void CSHA1::ReportHash(char *szReport, uint8 uReportType)
{
	uint8 i = 0;
	char szTemp[12];

	if(szReport == nullptr) return;

	if(uReportType == REPORT_HEX)
	{
		std::sprintf(szTemp, "%02X", m_digest[0]);
		std::strcat(szReport, szTemp);

		for(i = 1; i < k_cubHash; i++)
		{
			std::sprintf(szTemp, " %02X", m_digest[i]);
			std::strcat(szReport, szTemp);
		}
	}
	else if(uReportType == REPORT_DIGIT)
	{
		std::sprintf(szTemp, "%u", m_digest[0]);
		std::strcat(szReport, szTemp);

		for(i = 1; i < k_cubHash; i++)
		{
			std::sprintf(szTemp, " %u", m_digest[i]);
			std::strcat(szReport, szTemp);
		}
	}
	else std::strcpy(szReport, "Error: Unknown report type!");
}

// Get the raw message digest
void CSHA1::GetHash(uint8 *uDest)
{
	std::memcpy(uDest, m_digest, k_cubHash);
}

// utility hash comparison function
bool HashLessFunc( SHADigest_t const &lhs, SHADigest_t const &rhs )
{
	int iRes = std::memcmp( &lhs, &rhs, sizeof( SHADigest_t ) );
	return ( iRes < 0 );
}
