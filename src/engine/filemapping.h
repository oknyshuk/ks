//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Read-only whole-file memory mapping.
//
//============================================================================//

#ifndef FILEMAPPING_H
#define FILEMAPPING_H


#include <cstddef>
#include <span>
#include <utility>

//-----------------------------------------------------------------------------
// A read-only mapping of an entire file into the process address space.
//
// Move-only; the mapping is released on destruction. A default-constructed or
// failed-to-map object is empty, and callers are expected to fall back to
// ordinary reads in that case -- mapping cannot succeed for files that do not
// exist as their own inode (e.g. contents of a VPK or other pack file).
//
// The mapped bytes are backed by the file, so the file must not be truncated
// while a mapping is live: shrinking it turns subsequent accesses to the
// vanished tail into SIGBUS rather than a short read.
//-----------------------------------------------------------------------------
class CFileMapping
{
public:
	CFileMapping() = default;
	~CFileMapping() { Unmap(); }

	CFileMapping( const CFileMapping & ) = delete;
	CFileMapping &operator=( const CFileMapping & ) = delete;

	CFileMapping( CFileMapping &&other ) noexcept
		: m_Bytes( std::exchange( other.m_Bytes, std::span<const std::byte>{} ) )
	{
	}

	CFileMapping &operator=( CFileMapping &&other ) noexcept
	{
		if ( this != &other )
		{
			Unmap();
			m_Bytes = std::exchange( other.m_Bytes, std::span<const std::byte>{} );
		}
		return *this;
	}

	// Maps pszAbsolutePath read-only, replacing any existing mapping. Returns
	// false and leaves the object empty on failure, including for empty files.
	// pszAbsolutePath must name a real file on disk; resolve engine-relative
	// paths with IFileSystem::RelativePathToFullPath first.
	bool Map( const char *pszAbsolutePath );

	// Releases the mapping. Any pointer into Bytes() dangles afterwards.
	void Unmap();

	// Hints that the whole mapping is about to be read, letting the kernel
	// fault it in with large readahead instead of one page at a time.
	void PrefetchAll() const;

	bool IsMapped() const { return !m_Bytes.empty(); }
	explicit operator bool() const { return IsMapped(); }

	std::span<const std::byte> Bytes() const { return m_Bytes; }

private:
	std::span<const std::byte> m_Bytes;
};

#endif // FILEMAPPING_H
