// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef NULL_MUTEX_H
#define NULL_MUTEX_H

namespace ks
{
	struct null_mutex
	{
		static void lock() noexcept				{}
		static void unlock() noexcept			{}
		static bool try_lock() noexcept			{ return true; }
	};
}

#endif // NULL_MUTEX_H
