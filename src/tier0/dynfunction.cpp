// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Shared library loading and symbol lookup.

import ks.core.dynfunction;
import ks.platform;
import ks.simd;

// First include standard libraries
import std;



typedef ks::platform::library *LibraryHandle;

class CSharedLibraryCache
{
public:
	static CSharedLibraryCache &GetCache()
	{
		static CSharedLibraryCache Singleton;
		return Singleton;
	}

	struct CSharedLibraryItem
	{
		CSharedLibraryItem(LibraryHandle handle, const char *name)
		{
			m_handle = handle;
			m_name = new char[std::strlen(name) + 1];
			m_next = nullptr;
			std::strcpy(m_name, name);
		}

		~CSharedLibraryItem()
		{
			ks::platform::close_library(m_handle);
			delete[] m_name;
			delete m_next;
		}

		char *m_name;
		CSharedLibraryItem *m_next;
		LibraryHandle m_handle;
	};

	CSharedLibraryCache() : m_pList(nullptr) {}
	~CSharedLibraryCache() { CloseAllLibraries(); }

	LibraryHandle GetHandle(const char *name)
	{
		CSharedLibraryItem *item = GetCacheItem(name);
		if (item == nullptr)
		{
			LibraryHandle lib = ks::platform::load_library(name, nullptr);
			if (lib == nullptr)
				return nullptr;

			item = new CSharedLibraryItem(lib, name);
			item->m_next = m_pList;
			m_pList = item;
		}
		return item->m_handle;
	}

	void CloseLibrary(const char *name)
	{
		CSharedLibraryItem *item = GetCacheItem(name);
		if (item)
		{
			contract_assert( item == m_pList );
			m_pList = item->m_next;
			item->m_next = nullptr;
			delete item;
		}
	}

	void CloseAllLibraries()
	{
		delete m_pList;
	}

private:
	CSharedLibraryItem *GetCacheItem(const char *name)
	{
		CSharedLibraryItem *prev = nullptr;
		CSharedLibraryItem *item = m_pList;
		while (item)
		{
			if (std::strcmp(item->m_name, name) == 0)
			{
				// move this item to the front of the list, since there will
				//  probably be a big pile of these lookups in a row
				//  and then none ever again.
				if (prev != nullptr)
				{
					prev->m_next = item->m_next;
					item->m_next = m_pList;
					m_pList = item;
				}
				return item;
			}

			prev = item;
			item = item->m_next;
		}
		return nullptr;  // not found.
	}

	CSharedLibraryItem *m_pList;
};

void *VoidFnPtrLookup_Tier0(const char *libname, const char *fn, void *fallback)
{
	LibraryHandle lib = CSharedLibraryCache::GetCache().GetHandle(libname);
	void *retval = nullptr;
	if (lib != nullptr)
	{
		retval = ks::platform::library_symbol(lib, fn);
	}

	if (retval == nullptr)
		retval = fallback;
	return retval;
}
