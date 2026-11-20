// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation


import ks.core.basetypes;
import ks.core.platform;
import ks.core.scratcharena;
import ks.math.compressed_vector;
import ks.math.noise;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.string_ops;
import ks.tier1.strtools;

#include "mathlib/expressioncalculator.h"

import std;



//-----------------------------------------------------------------------------
// Parsing helper methods
//-----------------------------------------------------------------------------
bool ParseLiteral( const char *&expr, float &value )
{
	const char *startExpr = expr;
	value = ( float )std::strtod( startExpr, const_cast< char** >( &expr ) );
	return ( startExpr != expr );
}

bool ParseString( const char *&expr, const char *str )
{
	const char *startExpr = expr;
	while ( ( *expr == ' ' ) || ( *expr == '\t' ) )
		expr++; // skip whitespace

	expr = StringAfterPrefix( expr, str );
	if ( expr )
		return true;

	expr = startExpr;
	return false;
}

bool ParseStringList( const char *&expr, const char **pOps, int &nOp )
{
	while ( nOp-- )
	{
		if ( ParseString( expr, pOps[ nOp ] ) )
			return true;
	}
	return false;
}

bool ParseStringList( const char *&expr, const std::vector< std::string > &strings, int &nOp )
{
	while ( nOp-- )
	{
		if ( ParseString( expr, strings[ nOp ].c_str() ) )
			return true;
	}
	return false;
}

int FindString( const std::vector< std::string > &strings, const char *str )
{
	uint sn = std::ssize( strings );
	for ( uint si = 0; si < sn; ++si )
	{
		if ( !V_strcmp( str, strings[ si ].c_str() ) )
			return si;
	}
	return -1;
}

class ParseState_t
{
public:
	ParseState_t( const std::vector<float> &stack, const char *expr )
		: m_stacksize( std::ssize( stack ) ), m_startingExpr( expr ) {}
	void Reset( std::vector<float> &stack, const char *&expr )
	{
		contract_assert( m_stacksize <= std::ssize( stack ) );
		stack.resize( m_stacksize );
		expr = m_startingExpr;
	}

private:
	int m_stacksize;
	const char* m_startingExpr;
};




void CExpressionCalculator::SetVariable( int nVariableIndex, float value )
{
	m_varValues[ nVariableIndex ] = value;
}

void CExpressionCalculator::SetVariable( const char *var, float value )
{
	int vi = FindString( m_varNames, var );
	if ( vi >= 0 )
	{
		m_varValues[ vi ] = value;
	}
	else
	{
		m_varNames.push_back( ks::nz( var ) );
		m_varValues.push_back( value );
	}
}

void CExpressionCalculator::ModifyVariable( const char *var, float value )
{
	int vi = FindString( m_varNames, var );
	if ( vi >= 0 )
	{
		m_varValues[ vi ] += value;
	}
	else
	{
		SetVariable( var, value );
	}
}

int CExpressionCalculator::FindVariableIndex( const char *var )
{
	return FindString( m_varNames, var );
}

bool CExpressionCalculator::Evaluate( float &value )
{
	m_bIsBuildingArgumentList = false;
	m_stack.clear();
	const char *pExpr = m_expr.data();
	bool success = ParseExpr( pExpr );
	if ( success && std::ssize( m_stack ) == 1 )
	{
		value = m_stack.back();
		return true;
	}

	value = 0.0f;
	return false;
}


//-----------------------------------------------------------------------------
// Builds a list of variable names from the expression
//-----------------------------------------------------------------------------
bool CExpressionCalculator::BuildVariableListFromExpression( )
{
	m_bIsBuildingArgumentList = true;
	m_stack.clear();
	const char *pExpr = m_expr.data();
	bool bSuccess = ParseExpr( pExpr );
	m_bIsBuildingArgumentList = false;
	if ( !bSuccess || std::ssize( m_stack ) != 1 )
	{
		m_varNames.clear();
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Iterate over variables
//-----------------------------------------------------------------------------
int CExpressionCalculator::VariableCount()
{
	return std::ssize( m_varNames );
}

const char *CExpressionCalculator::VariableName( int nIndex )
{
	return m_varNames[nIndex].c_str();
}



bool CExpressionCalculator::ParseExpr( const char *&expr )
{
	return ( expr != nullptr ) && ParseConditional( expr );
}

bool CExpressionCalculator::ParseConditional( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseOr( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	ParseState_t ps1( m_stack, expr );
	if ( ParseString( expr, "?" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ":" ) &&
		ParseExpr( expr ) )
	{
		float f3 = m_stack.back();
		m_stack.pop_back();
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		m_stack.push_back( f1 != 0.0f ? f2 : f3 );
		return true; // and matched
	}
	ps1.Reset( m_stack, expr );
	return true; // equality (or lower) matched
}

bool CExpressionCalculator::ParseOr( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseAnd( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	ParseState_t ps1( m_stack, expr );
	if ( ParseString( expr, "||" ) &&
		ParseOr( expr ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		m_stack.push_back( ( f1 != 0.0f ) || ( f2 != 0.0f ) ? 1 : 0 );
		return true; // and matched
	}
	ps1.Reset( m_stack, expr );
	return true; // equality (or lower) matched
}

bool CExpressionCalculator::ParseAnd( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseEquality( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	ParseState_t ps1( m_stack, expr );
	if ( ParseString( expr, "&&" ) &&
		ParseAnd( expr ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		m_stack.push_back( ( f1 != 0.0f ) && ( f2 != 0.0f ) ? 1 : 0 );
		return true; // and matched
	}
	ps1.Reset( m_stack, expr );
	return true; // equality (or lower) matched
}

bool CExpressionCalculator::ParseEquality( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseLessGreater( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "==", "!=" };
	int nOp = 2;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseEquality( expr ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nOp )
		{
		case 0: // ==
			m_stack.push_back( f1 == f2 ? 1 : 0 );
			break;
		case 1: // !=
			m_stack.push_back( f1 != f2 ? 1 : 0 );
			break;
		}
		return true; // equality matched
	}
	ps1.Reset( m_stack, expr );
	return true; // lessgreater (or lower) matched
}

bool CExpressionCalculator::ParseLessGreater( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseAddSub( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "<", ">", "<=", ">=" };
	int nOp = 4;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseLessGreater( expr ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nOp )
		{
		case 0: // <
			m_stack.push_back( f1 < f2 ? 1 : 0 );
			break;
		case 1: // >
			m_stack.push_back( f1 > f2 ? 1 : 0 );
			break;
		case 2: // <=
			m_stack.push_back( f1 <= f2 ? 1 : 0 );
			break;
		case 3: // >=
			m_stack.push_back( f1 >= f2 ? 1 : 0 );
			break;
		}
		return true; // inequality matched
	}
	ps1.Reset( m_stack, expr );
	return true; // addsub (or lower) matched
}

bool CExpressionCalculator::ParseAddSub( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseDivMul( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "+", "-" };
	int nOp = 2;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseAddSub( expr ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nOp )
		{
		case 0: // +
			m_stack.push_back( f1 + f2 );
			break;
		case 1: // -
			m_stack.push_back( f1 - f2 );
			break;
		}
		return true; // addsub matched
	}
	ps1.Reset( m_stack, expr );
	return true; // divmul (or lower) matched
}

bool CExpressionCalculator::ParseDivMul( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseUnary( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "*", "/", "%" };
	int nOp = 3;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseDivMul( expr ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nOp )
		{
		case 0: // *
			m_stack.push_back( f1 * f2 );
			break;
		case 1: // /
			m_stack.push_back( f1 / f2 );
			break;
		case 2: // %
			m_stack.push_back( std::fmod( f1, f2 ) );
			break;
		}
		return true; // divmul matched
	}
	ps1.Reset( m_stack, expr );
	return true; // unary (or lower) matched
}

bool CExpressionCalculator::ParseUnary( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pOps[] = { "+", "-", "!" };
	int nOp = 3;

	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseUnary( expr ) )
	{
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nOp )
		{
		case 0: // +
			m_stack.push_back( f1 );
			break;
		case 1: // -
			m_stack.push_back( -f1 );
			break;
		case 2: // !
			m_stack.push_back( f1 == 0 ? 1 : 0 );
			break;
		}
		return true;
	}

	ps.Reset( m_stack, expr );
	if ( ParsePrimary( expr ) )
		return true;

	ps.Reset( m_stack, expr );
	return false;
}

bool CExpressionCalculator::ParsePrimary( const char *&expr )
{
	ks::ScratchArena<> scratch;
	ParseState_t ps( m_stack, expr );

	float value = 0.0f;
	if ( ParseLiteral( expr, value ) )
	{
		m_stack.push_back( value );
		return true;
	}

	ps.Reset( m_stack, expr );
	int nVar = std::ssize( m_varNames );
	if ( ParseStringList( expr, m_varNames, nVar) )
	{
		m_stack.push_back( m_varValues[ nVar ] );
		return true;
	}

	ps.Reset( m_stack, expr );
	if ( ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		return true;
	}

	ps.Reset( m_stack, expr );
	if ( Parse1ArgFunc( expr ) ||
		Parse2ArgFunc( expr ) ||
		Parse3ArgFunc( expr ) ||
//		Parse4ArgFunc( expr ) ||
		Parse5ArgFunc( expr ) )
	{
		return true;
	}

	// If we're parsing it to discover names of variable names, add them here
	if ( !m_bIsBuildingArgumentList )
		return false;

	// Variables can't start with a number
	if ( V_isdigit( *expr ) )
		return false;

	const char *pStart = expr;
	while ( V_isalnum( *expr ) || *expr == '_' )
	{
		++expr;
	}

	std::size_t nLen = (std::size_t)expr - (std::size_t)pStart;
	char *pVariableName = (char*)scratch.AllocBytes( nLen+1 );
	std::memcpy( pVariableName, pStart, nLen );
	pVariableName[nLen] = 0;

	SetVariable( pVariableName, 0.0f );
	m_stack.push_back( 0.0f );
	return true;
}

/*
dtor(d) : converts degrees to radians
rtod(r) : converts radians to degrees

abs(a)     : absolute value
floor(a)   : rounds down to the nearest integer
ceiling(a) : rounds up to the nearest integer
round(a)   : rounds to the nearest integer
sgn(a)     : if a < 0 returns -1 else 1
sqr(a)     : returns a * a
sqrt(a)    : returns sqrt(a)

sin(a)     : sin(a), a is in degrees
asin(a)    : asin(a) returns degrees
cos(a)     : cos(a), a is in degrees
acos(a)    : acos(a) returns degrees
tan(a)     : tan(a), a is in degrees

exp(a)   : returns the exponential function of a
log(a)   : returns the natural logaritm of a
*/
bool CExpressionCalculator::Parse1ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] =
	{
		"abs", "sqr", "sqrt", "sin", "asin", "cos", "acos", "tan",
		"exp", "log", "dtor", "rtod", "floor", "ceiling", "round", "sign"
	};
	int nFunc = 16;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nFunc )
		{
		case 0: // abs
			m_stack.push_back( std::fabs( f1 ) );
			break;
		case 1: // sqr
			m_stack.push_back( f1 * f1 );
			break;
		case 2: // sqrt
			m_stack.push_back( std::sqrt( f1 ) );
			break;
		case 3: // sin
			m_stack.push_back( std::sin( f1 ) );
			break;
		case 4: // asin
			m_stack.push_back( std::asin( f1 ) );
			break;
		case 5: // cos
			m_stack.push_back( std::cos( f1 ) );
			break;
		case 6: // acos
			m_stack.push_back( std::acos( f1 ) );
			break;
		case 7: // tan
			m_stack.push_back( std::tan( f1 ) );
			break;
		case 8: // exp
			m_stack.push_back( std::exp( f1 ) );
			break;
		case 9: // log
			m_stack.push_back( std::log( f1 ) );
			break;
		case 10: // dtor
			m_stack.push_back( DEG2RAD( f1 ) );
			break;
		case 11: // rtod
			m_stack.push_back( RAD2DEG( f1 ) );
			break;
		case 12: // floor
			m_stack.push_back( std::floor( f1 ) );
			break;
		case 13: // ceiling
			m_stack.push_back( std::ceil( f1 ) );
			break;
		case 14: // round
			m_stack.push_back( std::floor( f1 + 0.5f ) );
			break;
		case 15: // sign
			m_stack.push_back( f1 >= 0.0f ? 1.0f : -1.0f );
			break;
		}
		return true;
	}
	return false;
}

/*
min(a,b)     : if a<b returns a else b
max(a,b)     : if a>b returns a else b
atan2(a,b) : atan2(a/b) returns degrees
pow(a,b) : function returns a raised to the power of b
*/
bool CExpressionCalculator::Parse2ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = { "min", "max", "atan2", "pow" };
	int nFunc = 4;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nFunc )
		{
		case 0: // min
			m_stack.push_back( std::min( f1, f2 ) );
			break;
		case 1: // max
			m_stack.push_back( std::max( f1, f2 ) );
			break;
		case 2: // atan2
			m_stack.push_back( std::atan2( f1, f2 ) );
			break;
		case 3: // pow
			m_stack.push_back( std::pow( f1, f2 ) );
			break;
		}
		return true;
	}
	return false;
}

/*
inrange(x,a,b) : if x is between a and b, returns 1 else returns 0
clamp(x,a,b)   : see bound() above

ramp(value,a,b)        : returns 0 -> 1 as value goes from a to b
lerp(factor,a,b)       : returns a -> b as value goes from 0 to 1

cramp(value,a,b)        : clamp(ramp(value,a,b),0,1)
clerp(factor,a,b)       : clamp(lerp(factor,a,b),a,b)

elerp(x,a,b)         : ramp( 3*x*x - 2*x*x*x, a, b)
//elerp(factor,a,b)    : lerp(lerp(sind(clerp(factor,-90,90)),0.5,1.0),a,b)

noise(a,b,c) : { solid noise pattern (improved perlin noise) indexed with three numbers }
*/

float ramp( float x, float a, float b )
{
	return ( x - a ) / ( b - a );
}

float V_lerp( float x, float a, float b )
{
	return a + x * ( b - a );
}

float smoothstep( float x )
{
	return 3*x*x - 2*x*x*x;
}

bool CExpressionCalculator::Parse3ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = { "inrange", "clamp", "ramp", "lerp", "cramp", "clerp", "elerp", "noise" };
	int nFunc = 8;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f3 = m_stack.back();
		m_stack.pop_back();
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nFunc )
		{
		case 0: // inrange
			m_stack.push_back( ( f1 >= f2 ) && ( f1 <= f3 ) ? 1.0f : 0.0f );
			break;
		case 1: // clamp
			m_stack.push_back( clamp( f1, f2, f3 ) );
			break;
		case 2: // ramp
			m_stack.push_back( ramp( f1, f2, f3 ) );
			break;
		case 3: // lerp
			m_stack.push_back( V_lerp( f1, f2, f3 ) );
			break;
		case 4: // cramp
			m_stack.push_back( clamp( ramp( f1, f2, f3 ), 0, 1 ) );
			break;
		case 5: // clerp
			m_stack.push_back( clamp( V_lerp( f1, f2, f3 ), f2, f3 ) );
			break;
		case 6: // elerp
			m_stack.push_back( V_lerp( smoothstep( f1 ), f2, f3 ) );
			break;
		case 7: // noise
			m_stack.push_back( ImprovedPerlinNoise( Vector( f1, f2, f3 ) ) );
			break;
		}
		return true;
	}
	return false;
}

//bool CExpressionCalculator::Parse4ArgFunc( const char *&expr );

/*
rescale (X,Xa,Xb,Ya,Yb) : lerp(ramp(X,Xa,Xb),Ya,Yb)
crescale(X,Xa,Xb,Ya,Yb) : clamp(rescale(X,Xa,Xb,Ya,Yb),Ya,Yb)
*/
float rescale( float x, float a, float b, float c, float d )
{
	return V_lerp( ramp( x, a, b ), c, d );
}

bool CExpressionCalculator::Parse5ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = { "rescale", "crescale" };
	int nFunc = 2;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f5 = m_stack.back();
		m_stack.pop_back();
		float f4 = m_stack.back();
		m_stack.pop_back();
		float f3 = m_stack.back();
		m_stack.pop_back();
		float f2 = m_stack.back();
		m_stack.pop_back();
		float f1 = m_stack.back();
		m_stack.pop_back();
		switch ( nFunc )
		{
		case 0: // rescale
			m_stack.push_back( rescale( f1, f2, f3, f4, f5 ) );
			break;
		case 1: // crescale
			m_stack.push_back( clamp( rescale( f1, f2, f3, f4, f5 ), f4, f5 ) );
			break;
		}
		return true;
	}
	return false;
}

CExpressionCalculator::CExpressionCalculator( const CExpressionCalculator& x )
{
	*this = x;
}

CExpressionCalculator& CExpressionCalculator::operator=( const CExpressionCalculator& x )
{
	m_expr = x.m_expr;
	m_varNames = x.m_varNames;
	m_varValues = x.m_varValues;
	m_stack = x.m_stack;
	m_bIsBuildingArgumentList = x.m_bIsBuildingArgumentList;
	return *this;
}

float EvaluateExpression( char const *pExpr, float flValueToReturnIfFailure )
{
	CExpressionCalculator myEvaluator( pExpr );
	float flResult;
	bool bSuccess = myEvaluator.Evaluate( flResult );
	return ( bSuccess ) ? flResult : flValueToReturnIfFailure;
}
