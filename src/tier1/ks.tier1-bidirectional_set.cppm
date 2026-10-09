// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:bidirectional_set;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	template < class B, class E, std::unsigned_integral S >
	class bidirectional_set
	{
	public:
		using handle_type = S;
		using first_element_fn = S &( * )( B );
		using first_bucket_fn = S &( * )( E );
		static constexpr S invalid = S( ~S( 0 ) );

		void init( first_element_fn firstElement, first_bucket_fn firstBucket )
		{
			m_FirstElement = firstElement;
			m_FirstBucket = firstBucket;
		}

		// Links element into bucket; the pair must not be linked already (see validate_add).
		void add( B bucket, E element )
		{
			contract_assert( m_FirstElement && m_FirstBucket );
			S h = alloc();
			Node &n = m_Nodes[h];
			n.bucket = bucket;
			n.element = element;

			S &firstElement = m_FirstElement( bucket );
			n.prevE = invalid;
			n.nextE = firstElement;
			if ( firstElement != invalid )
				m_Nodes[firstElement].prevE = h;
			firstElement = h;

			S &firstBucket = m_FirstBucket( element );
			n.prevB = invalid;
			n.nextB = firstBucket;
			if ( firstBucket != invalid )
				m_Nodes[firstBucket].prevB = h;
			firstBucket = h;
		}

		// Debug-only check that the pair is not linked yet; walks both chains.
		void validate_add( [[maybe_unused]] B bucket, [[maybe_unused]] E element ) const
		{
		}

		// Unlinks element from every bucket it is in.
		void remove_element( E element )
		{
			contract_assert( m_FirstElement && m_FirstBucket );
			S &head = m_FirstBucket( element );
			for ( S h = head; h != invalid; )
			{
				Node &n = m_Nodes[h];
				S next = n.nextB;
				unlink_from_bucket( n );
				release( h );
				h = next;
			}
			head = invalid;
		}

		// Unlinks every element from bucket.
		void remove_bucket( B bucket )
		{
			contract_assert( m_FirstElement && m_FirstBucket );
			S &head = m_FirstElement( bucket );
			for ( S h = head; h != invalid; )
			{
				Node &n = m_Nodes[h];
				S next = n.nextE;
				unlink_from_element( n );
				release( h );
				h = next;
			}
			head = invalid;
		}

		// Elements of a bucket.
		S first_element( B bucket ) const	{ return m_FirstElement( bucket ); }
		S next_element( S h ) const			{ return m_Nodes[h].nextE; }
		E element( S h ) const				{ return m_Nodes[h].element; }

		// Buckets of an element.
		S first_bucket( E element ) const	{ return m_FirstBucket( element ); }
		S next_bucket( S h ) const			{ return m_Nodes[h].nextB; }
		B bucket( S h ) const				{ return m_Nodes[h].bucket; }

		void reserve( std::size_t n )			{ m_Nodes.reserve( n ); }
		std::size_t size() const					{ return m_nLive; }	// live links
		std::size_t slot_count() const			{ return m_Nodes.size(); }	// one past the highest handle ever issued

		// Drops every link and frees the storage; the callers' chain heads must be reset separately.
		void clear()
		{
			m_Nodes = {};
			m_FreeHead = invalid;
			m_nLive = 0;
		}

	private:
		struct Node
		{
			B bucket;
			E element;
			S prevE, nextE;		// in bucket's element chain; nextE doubles as the free-list link
			S prevB, nextB;		// in element's bucket chain
		};

		S alloc()
		{
			S h;
			if ( m_FreeHead != invalid )
			{
				h = m_FreeHead;
				m_FreeHead = m_Nodes[h].nextE;
			}
			else
			{
				if ( m_Nodes.size() >= std::size_t( invalid ) )
					Error( "ks::bidirectional_set overflow! (exhausted index range)\n" );
				h = S( m_Nodes.size() );
				m_Nodes.emplace_back();
			}
			++m_nLive;
			return h;
		}

		void release( S h )
		{
			m_Nodes[h].nextE = m_FreeHead;
			m_FreeHead = h;
			--m_nLive;
		}

		void unlink_from_bucket( const Node &n )
		{
			if ( n.prevE != invalid )
				m_Nodes[n.prevE].nextE = n.nextE;
			else
				m_FirstElement( n.bucket ) = n.nextE;
			if ( n.nextE != invalid )
				m_Nodes[n.nextE].prevE = n.prevE;
		}

		void unlink_from_element( const Node &n )
		{
			if ( n.prevB != invalid )
				m_Nodes[n.prevB].nextB = n.nextB;
			else
				m_FirstBucket( n.element ) = n.nextB;
			if ( n.nextB != invalid )
				m_Nodes[n.nextB].prevB = n.prevB;
		}

		std::vector< Node > m_Nodes;
		S m_FreeHead = invalid;
		std::size_t m_nLive = 0;
		first_element_fn m_FirstElement = nullptr;
		first_bucket_fn m_FirstBucket = nullptr;
	};
}

}
