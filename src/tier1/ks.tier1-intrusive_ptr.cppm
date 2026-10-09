// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:intrusive_ptr;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	struct adopt_ref_t
	{
		explicit adopt_ref_t() = default;
	};
	inline constexpr adopt_ref_t adopt_ref{};

	template < class T >
	class intrusive_ptr
	{
	public:
		constexpr intrusive_ptr() noexcept = default;
		constexpr intrusive_ptr( std::nullptr_t ) noexcept {}
		intrusive_ptr( T *p ) : m_p( p )								{ if ( m_p ) m_p->AddRef(); }
		intrusive_ptr( T *p, adopt_ref_t ) noexcept : m_p( p )			{}
		intrusive_ptr( const intrusive_ptr &o ) : m_p( o.m_p )			{ if ( m_p ) m_p->AddRef(); }
		intrusive_ptr( intrusive_ptr &&o ) noexcept : m_p( std::exchange( o.m_p, nullptr ) ) {}

		template < class U >
			requires std::is_convertible_v< U *, T * >
		intrusive_ptr( const intrusive_ptr< U > &o ) : m_p( o.get() )	{ if ( m_p ) m_p->AddRef(); }

		~intrusive_ptr()												{ if ( m_p ) m_p->Release(); }

		intrusive_ptr &operator=( const intrusive_ptr &o )
		{
			intrusive_ptr( o ).swap( *this );
			return *this;
		}

		intrusive_ptr &operator=( intrusive_ptr &&o ) noexcept
		{
			intrusive_ptr( std::move( o ) ).swap( *this );
			return *this;
		}

		intrusive_ptr &operator=( T *p )
		{
			intrusive_ptr( p ).swap( *this );
			return *this;
		}

		intrusive_ptr &operator=( std::nullptr_t )
		{
			reset();
			return *this;
		}

		void reset() noexcept
		{
			if ( m_p )
				std::exchange( m_p, nullptr )->Release();
		}

		void reset( T *p )												{ intrusive_ptr( p ).swap( *this ); }
		void reset( T *p, adopt_ref_t ) noexcept						{ intrusive_ptr( p, adopt_ref ).swap( *this ); }

		T *detach() noexcept											{ return std::exchange( m_p, nullptr ); }
		void swap( intrusive_ptr &o ) noexcept							{ std::swap( m_p, o.m_p ); }

		T *get() const noexcept											{ return m_p; }
		T &operator*() const noexcept									{ return *m_p; }
		T *operator->() const noexcept									{ return m_p; }
		explicit operator bool() const noexcept							{ return m_p != nullptr; }

		friend bool operator==( const intrusive_ptr &a, const intrusive_ptr &b ) noexcept	{ return a.m_p == b.m_p; }
		friend bool operator==( const intrusive_ptr &a, const T *b ) noexcept				{ return a.m_p == b; }
		friend bool operator==( const intrusive_ptr &a, std::nullptr_t ) noexcept			{ return a.m_p == nullptr; }

	private:
		T *m_p = nullptr;
	};
}

}
