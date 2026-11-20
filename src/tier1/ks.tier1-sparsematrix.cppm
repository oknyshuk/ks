// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2011 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.sparsematrix;

import std;
import ks.math.mathlib;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
export import ks.tier1.utlvector;

export extern "C++" {

/// CSparseMatrix is a matrix which compresses each row individually, not storing the zeros.  NOte,
/// that while you can randomly set any element in a CSparseMatrix, you really want to do it top to
/// bottom or you will have bad perf as data is moved around to insert new elements.
class CSparseMatrix
{
public:
	struct NonZeroValueDescriptor_t
	{
		int m_nColumnNumber;
		float m_flValue;
	};

	struct RowDescriptor_t
	{
		int m_nNonZeroCount;								// number of non-zero elements in the row
		int m_nDataIndex;									// index of NonZeroValueDescriptor_t for the first non-zero value
	};

	int m_nNumRows;
	int m_nNumCols;
	std::vector< RowDescriptor_t > m_rowDescriptors;
	std::vector< NonZeroValueDescriptor_t > m_entries;
	int m_nHighestRowAppendedTo;

	void AdjustAllRowIndicesAfter( int nStartRow, int nDelta );
public:
	[[gnu::always_inline]] inline float Element( int nRow, int nCol ) const;
	void SetElement( int nRow, int nCol, float flValue );
	void SetDimensions( int nNumRows, int nNumCols );
	void AppendElement( int nRow, int nCol, float flValue );
	void FinishedAppending( void );

	[[gnu::always_inline]] inline int Height( void ) const { return m_nNumRows; }
	[[gnu::always_inline]] inline int Width( void ) const { return m_nNumCols; }

};

[[gnu::always_inline]] inline float CSparseMatrix::Element( int nRow, int nCol ) const
{
	contract_assert( nCol < m_nNumCols );
	int nCount = m_rowDescriptors[nRow].m_nNonZeroCount;
	if ( nCount )
	{
		NonZeroValueDescriptor_t const *pValue = &(m_entries[m_rowDescriptors[nRow].m_nDataIndex]);
		do
		{
			int nIdx = pValue->m_nColumnNumber;
			if ( nIdx == nCol )
			{
				return pValue->m_flValue;
			}
			if ( nIdx > nCol )
			{
				break;
			}
			pValue++;
		} while( --nCount );
	}
	return 0;
}

// type-specific overrides of matrixmath template for special case sparse routines

namespace MatrixMath
{
	/// sparse * dense matrix x matrix multiplication
	template<class BTYPE, class OUTTYPE> 
	void MatrixMultiply( CSparseMatrix const &matA, BTYPE const &matB, OUTTYPE *pMatrixOut )
	{
		contract_assert( matA.Width() == matB.Height() );
		pMatrixOut->SetDimensions( matA.Height(), matB.Width() );
		for( int i = 0; i < matA.Height(); i++ )
		{
			for( int j = 0; j < matB.Width(); j++ )
			{
				// compute inner product efficiently because of sparsity
				int nCnt = matA.m_rowDescriptors[i].m_nNonZeroCount;
				int nDataIdx = matA.m_rowDescriptors[i].m_nDataIndex;
				float flDot = 0.0;
				for( int nIdx = 0; nIdx < nCnt; nIdx++ )
				{
					float flAValue = matA.m_entries[nIdx + nDataIdx].m_flValue;
					int nCol = matA.m_entries[nIdx + nDataIdx].m_nColumnNumber;
					flDot += flAValue * matB.Element( nCol, j );
				}
				pMatrixOut->SetElement( i, j, flDot );
			}
		}
	}

	[[gnu::always_inline]] inline void AppendElement( CSparseMatrix &matrix, int nRow, int nCol, float flValue )
	{
		matrix.AppendElement( nRow, nCol, flValue );			// default implementation
	}

	[[gnu::always_inline]] inline void FinishedAppending( CSparseMatrix &matrix )
	{
		matrix.FinishedAppending();
	}

};

}
