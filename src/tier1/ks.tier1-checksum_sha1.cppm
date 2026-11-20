// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.checksum_sha1;

import std;
import ks.core.types;
import ks.platform;
import ks.simd;

export extern "C++" {

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

// If you're compiling big endian, just comment out the following line

typedef union
{
	uint8 c[64];
	uint32 l[16];
} SHA1_WORKSPACE_BLOCK;

// SHA1 hash
inline const unsigned int k_cubHash = 20;
inline const unsigned int k_cchHash = 41; // k_cubHash * 2, plus 1 for terminator
#pragma pack( push, 1 )
typedef	uint8 SHADigest_t[ k_cubHash ];
#pragma pack( pop )

class CSHA1
{
public:
	// Two different formats for ReportHash(...)
	enum
	{
		REPORT_HEX = 0,
		REPORT_DIGIT = 1
	};

	// Constructor and Destructor
	CSHA1();
	~CSHA1() ;

	unsigned int m_state[5];
	unsigned int m_count[2];
	uint8 m_buffer[64];
	uint8 m_digest[k_cubHash];

	void Reset();

	// Update the hash value
	void Update( const void *data, unsigned int len );
	bool HashFile( const char *szFileName );

	// Finalize hash and report
	void Final();
	void ReportHash(char *szReport, uint8 uReportType = REPORT_HEX);
	void GetHash(uint8 *uDest);

private:
	// Private SHA-1 transformation
	void Transform(unsigned int state[5], const uint8 buffer[64]);

	// Member variables
	uint8 m_workspace[64];
	SHA1_WORKSPACE_BLOCK *m_block; // SHA1 pointer to the byte array above
};

// hash comparison function, for use with std::map
bool HashLessFunc( SHADigest_t const &lhs, SHADigest_t const &rhs );

// utility class for manipulating SHA1 hashes in their compact form
struct CSHA
{
public:
	SHADigest_t m_shaDigest;

	CSHA()
	{
		std::memset( m_shaDigest, 0, k_cubHash );
	}

	explicit CSHA( const SHADigest_t rhs )
	{
		std::memcpy( m_shaDigest, rhs, k_cubHash );
	}

	SHADigest_t &SHADigest()
	{
		return m_shaDigest;
	}

	bool operator<( const CSHA &rhs ) const
	{
		return std::memcmp( m_shaDigest, rhs.m_shaDigest, k_cubHash ) < 0;
	}

	bool operator==( const CSHA &rhs ) const
	{
		return std::memcmp( m_shaDigest, rhs.m_shaDigest, k_cubHash ) == 0;
	}

	bool operator!=( const CSHA &rhs ) const
	{
		return !(*this == rhs);
	}

	bool operator==( const SHADigest_t &rhs ) const
	{
		return std::memcmp( m_shaDigest, rhs, k_cubHash ) == 0;
	}

	bool operator!=( const SHADigest_t &rhs ) const
	{
		return !(*this == rhs);
	}

	CSHA &operator=( const SHADigest_t rhs )
	{
		std::memcpy( m_shaDigest, rhs, k_cubHash );
		return *this;
	}

	void AssignTo( SHADigest_t rhs ) const
	{
		std::memcpy( rhs, m_shaDigest, k_cubHash );
	}
};

}
