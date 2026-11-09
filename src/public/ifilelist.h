#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// This class represents a group of files. Internally, it can represent whole folders of files
// that are in or out of the group. So you can't iterate the list, but you can ask the
// class if a particular filename is in the list.
//
// There are three kinds of list, told apart by m_Kind (the owner is the object the list answers for):
//   KIND_ALLOW_FROM_DISK / KIND_FORCE_MATCH: the pure server whitelist's lists (CPureServerWhitelist);
//   KIND_RELOAD: the files the file system wants the game to reload (an IFileSystem, owned: Release deletes it).
// IsFileInList and Release are defined in engine/pure_server.cpp.
class IFileList
{
public:
	enum Kind_t
	{
		KIND_ALLOW_FROM_DISK,
		KIND_FORCE_MATCH,
		KIND_RELOAD,
	};

	IFileList( Kind_t kind, void *pOwner ) : m_Kind( kind ), m_pOwner( pOwner ) {}

	bool	IsFileInList( const char *pFilename );
	void	Release();

private:
	Kind_t	m_Kind;
	void	*m_pOwner;
};
