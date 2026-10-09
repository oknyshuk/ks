#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Special case hash table for console commands

import std;
import ks.tier1;


// This is a hash table class very similar to the CUtlHashFast, but
// modified specifically so that we can look up ConCommandBases
// by string names without having to actually store those strings in
// the dictionary, and also iterate over all of them. 
// It uses separate chaining: each key hashes to a bucket, each
// bucket is a linked list of hashed commands. We store the hash of 
// the command's string name as well as its pointer, so we can do 
// the linked list march part of the Find() operation more quickly.
class CConCommandHash
{
public:
	// Address of the entry; entries never move while they are in the hash
	typedef intp CCommandHashHandle_t;
	typedef unsigned int HashKey_t;

protected:
	struct HashEntry_t
	{
		HashKey_t m_uiKey;
		ConCommandBase *m_Data;

		HashEntry_t(unsigned int _hash, ConCommandBase * _cmd)
			: m_uiKey(_hash), m_Data(_cmd) {};

		HashEntry_t(){};
	};

	typedef std::list<HashEntry_t> bucket_t;

public:

	// Constructor/Deconstructor.
	CConCommandHash();
	~CConCommandHash();

	// Memory.
	void Purge( bool bReinitialize );

	// Invalid handle.
	static CCommandHashHandle_t InvalidHandle( void )	{ return ( CCommandHashHandle_t )~0; }
	inline bool IsValidHandle( CCommandHashHandle_t hHash ) const;

	/// Initialize.
	void Init( void ); // bucket count is hardcoded in enum below.

	/// Get hash value for a concommand
	static inline HashKey_t Hash( const ConCommandBase *cmd );

	// Size not available; count is meaningless for multilists.
	// int Count( void ) const;

	// Insertion.
	CCommandHashHandle_t Insert( ConCommandBase *cmd );
	CCommandHashHandle_t FastInsert( ConCommandBase *cmd );

	// Removal.
	void Remove( CCommandHashHandle_t hHash ) __restrict__;
	void RemoveAll( void );

	// Retrieval.
	inline CCommandHashHandle_t Find( const char *name ) const;
	CCommandHashHandle_t Find( const ConCommandBase *cmd ) const __restrict__;
	// A convenience version of Find that skips the handle part
	// and returns a pointer to a concommand, or NULL if none was found.
	inline ConCommandBase * FindPtr( const char *name ) const;

	inline ConCommandBase * &operator[]( CCommandHashHandle_t hHash );
	inline ConCommandBase *const &operator[]( CCommandHashHandle_t hHash ) const;

#ifdef _DEBUG
	// Dump a report to MSG
	void Report( void );
#endif

	// Iteration
	struct CCommandHashIterator_t
	{
		int bucket;
		bucket_t::const_iterator it;

		CCommandHashIterator_t() : bucket(-1) {}
		CCommandHashIterator_t(int _bucket, bucket_t::const_iterator _it)
			: bucket(_bucket), it(_it) {};
	};
	inline CCommandHashIterator_t First() const;
	inline CCommandHashIterator_t Next( const CCommandHashIterator_t &hHash ) const;
	inline bool IsValidIterator( const CCommandHashIterator_t &iter ) const;
	inline ConCommandBase *operator[]( const CCommandHashIterator_t &iter ) const { return iter.it->m_Data;  }
private:
	// a find func where we've already computed the hash for the string.
	// (hidden private in case we decide to invent a custom string hash func
	//  for this class)
	CCommandHashHandle_t Find( const char *name, HashKey_t hash) const __restrict__;

protected:
	enum 
	{
		kNUM_BUCKETS = 256,
		kBUCKETMASK  = kNUM_BUCKETS - 1,
	};

	static CCommandHashHandle_t ToHandle( const HashEntry_t &entry ) { return (CCommandHashHandle_t)&entry; }
	static HashEntry_t *FromHandle( CCommandHashHandle_t hHash ) { return (HashEntry_t *)hHash; }

	// Each bucket holds its entries newest first
	std::vector< bucket_t >	m_aBuckets;
};

inline bool CConCommandHash::IsValidHandle( CCommandHashHandle_t hHash ) const
{
	return hHash != InvalidHandle();
}


inline CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name ) const
{
	return Find( name, HashStringCaseless(name) );
}

inline ConCommandBase * &CConCommandHash::operator[]( CCommandHashHandle_t hHash )
{
	return ( FromHandle( hHash )->m_Data );
}

inline ConCommandBase *const &CConCommandHash::operator[]( CCommandHashHandle_t hHash ) const
{
	return ( FromHandle( hHash )->m_Data );
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the index of the first element
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashIterator_t CConCommandHash::First() const
{
	// walk through the buckets to find the first one that has some data
	int bucketCount = std::ssize( m_aBuckets );
	for ( int bucket = 0 ; bucket < bucketCount ; ++bucket )
	{
		const bucket_t &list = m_aBuckets[bucket];
		if ( !list.empty() )
			return CCommandHashIterator_t( bucket, list.begin() );
	}

	// if we are down here, the list is empty 
	return CCommandHashIterator_t();
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the next element after
// the param one. Or an invalid iterator.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashIterator_t 
CConCommandHash::Next( const CConCommandHash::CCommandHashIterator_t &iter ) const
{
	// look for the next entry in the current bucket
	bucket_t::const_iterator next = std::next( iter.it );
	if ( next != m_aBuckets[iter.bucket].end() )
	{
		// this bucket still has more elements in it
		return CCommandHashIterator_t(iter.bucket, next);
	}

	// otherwise look for the next bucket with data
	int bucketCount = std::ssize( m_aBuckets );
	for ( int bucket = iter.bucket+1 ; bucket < bucketCount ; ++bucket )
	{
		const bucket_t &list = m_aBuckets[bucket];
		if ( !list.empty() )
			return CCommandHashIterator_t( bucket, list.begin() );
	}

	// if we're here, there's no more data to be had
	return CCommandHashIterator_t();
}

bool CConCommandHash::IsValidIterator( const CCommandHashIterator_t &iter ) const
{
	return iter.bucket >= 0;
}

inline CConCommandHash::HashKey_t CConCommandHash::Hash( const ConCommandBase *cmd )
{
	return HashStringCaseless( cmd->GetName() );
}

inline ConCommandBase * CConCommandHash::FindPtr( const char *name ) const
{
	CCommandHashHandle_t handle = Find(name);
	if (handle == InvalidHandle())
	{
		return nullptr;
	}
	else
	{
		return (*this)[handle];
	}
}
