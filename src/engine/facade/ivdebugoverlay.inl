import ks.core.color;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.convar;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVDebugOverlay class to CIVDebugOverlay. Included by debugoverlay.cpp.

void IVDebugOverlay::AddEntityTextOverlay( int ent_index, int line_offset, float duration, int r, int g, int b, int a, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddEntityTextOverlayV( ent_index, line_offset, duration, r, g, b, a, format, args );
	__builtin_va_end( args );
}
void IVDebugOverlay::AddBoxOverlay( const Vector& origin, const Vector& mins, const Vector& max, QAngle const& orientation, int r, int g, int b, int a, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddBoxOverlay( origin, mins, max, orientation, r, g, b, a, duration ); }
void IVDebugOverlay::AddSphereOverlay( const Vector& vOrigin, float flRadius, int nTheta, int nPhi, int r, int g, int b, int a, float flDuration )
{ static_cast<CIVDebugOverlay*>(this)->AddSphereOverlay( vOrigin, flRadius, nTheta, nPhi, r, g, b, a, flDuration ); }
void IVDebugOverlay::AddTriangleOverlay( const Vector& p1, const Vector& p2, const Vector& p3, int r, int g, int b, int a, bool noDepthTest, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddTriangleOverlay( p1, p2, p3, r, g, b, a, noDepthTest, duration ); }
void IVDebugOverlay::AddLineOverlay( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddLineOverlay( origin, dest, r, g, b, noDepthTest, duration ); }
void IVDebugOverlay::AddTextOverlay( const Vector& origin, float duration, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayV( origin, duration, format, args );
	__builtin_va_end( args );
}
void IVDebugOverlay::AddTextOverlay( const Vector& origin, int line_offset, float duration, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayV( origin, line_offset, duration, format, args );
	__builtin_va_end( args );
}
void IVDebugOverlay::AddScreenTextOverlay( float flXPos, float flYPos, float flDuration, int r, int g, int b, int a, const char *text )
{ static_cast<CIVDebugOverlay*>(this)->AddScreenTextOverlay( flXPos, flYPos, flDuration, r, g, b, a, text ); }
void IVDebugOverlay::AddSweptBoxOverlay( const Vector& start, const Vector& end, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration )
{ static_cast<CIVDebugOverlay*>(this)->AddSweptBoxOverlay( start, end, mins, max, angles, r, g, b, a, flDuration ); }
void IVDebugOverlay::AddGridOverlay( const Vector& origin )
{ static_cast<CIVDebugOverlay*>(this)->AddGridOverlay( origin ); }
void IVDebugOverlay::AddCoordFrameOverlay( const matrix3x4_t& frame, float flScale, int vColorTable[3][3] )
{ static_cast<CIVDebugOverlay*>(this)->AddCoordFrameOverlay( frame, flScale, vColorTable ); }
int IVDebugOverlay::ScreenPosition( const Vector& point, Vector& screen )
{ return static_cast<CIVDebugOverlay*>(this)->ScreenPosition( point, screen ); }
int IVDebugOverlay::ScreenPosition( float flXPos, float flYPos, Vector& screen )
{ return static_cast<CIVDebugOverlay*>(this)->ScreenPosition( flXPos, flYPos, screen ); }
OverlayText_t * IVDebugOverlay::GetFirst( void )
{ return static_cast<CIVDebugOverlay*>(this)->GetFirst(); }
OverlayText_t * IVDebugOverlay::GetNext( OverlayText_t *current )
{ return static_cast<CIVDebugOverlay*>(this)->GetNext( current ); }
void IVDebugOverlay::ClearDeadOverlays( void )
{ static_cast<CIVDebugOverlay*>(this)->ClearDeadOverlays(); }
void IVDebugOverlay::ClearAllOverlays()
{ static_cast<CIVDebugOverlay*>(this)->ClearAllOverlays(); }
void IVDebugOverlay::AddTextOverlayRGB( const Vector& origin, int line_offset, float duration, float r, float g, float b, float alpha, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayRGBV( origin, line_offset, duration, r, g, b, alpha, format, args );
	__builtin_va_end( args );
}
void IVDebugOverlay::AddTextOverlayRGB( const Vector& origin, int line_offset, float duration, int r, int g, int b, int a, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayRGBV( origin, line_offset, duration, r, g, b, a, format, args );
	__builtin_va_end( args );
}
void IVDebugOverlay::AddLineOverlayAlpha( const Vector& origin, const Vector& dest, int r, int g, int b, int a, bool noDepthTest, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddLineOverlayAlpha( origin, dest, r, g, b, a, noDepthTest, duration ); }
void IVDebugOverlay::AddBoxOverlay2( const Vector& origin, const Vector& mins, const Vector& max, QAngle const& orientation, const Color& faceColor, const Color& edgeColor, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddBoxOverlay2( origin, mins, max, orientation, faceColor, edgeColor, duration ); }
void IVDebugOverlay::AddLineOverlay( const Vector& origin, const Vector& dest, int r, int g, int b, int a, float thickness, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddLineOverlay( origin, dest, r, g, b, a, thickness, duration ); }
void IVDebugOverlay::PurgeTextOverlays()
{ static_cast<CIVDebugOverlay*>(this)->PurgeTextOverlays(); }
void IVDebugOverlay::AddCapsuleOverlay( const Vector &vStart, const Vector &vEnd, const float &flRadius, int r, int g, int b, int a, float flDuration )
{ static_cast<CIVDebugOverlay*>(this)->AddCapsuleOverlay( vStart, vEnd, flRadius, r, g, b, a, flDuration ); }

void IVPhysicsDebugOverlay::AddEntityTextOverlay( int ent_index, int line_offset, float duration, int r, int g, int b, int a, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddEntityTextOverlayV( ent_index, line_offset, duration, r, g, b, a, format, args );
	__builtin_va_end( args );
}
void IVPhysicsDebugOverlay::AddBoxOverlay( const Vector& origin, const Vector& mins, const Vector& max, QAngle const& orientation, int r, int g, int b, int a, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddBoxOverlay( origin, mins, max, orientation, r, g, b, a, duration ); }
void IVPhysicsDebugOverlay::AddTriangleOverlay( const Vector& p1, const Vector& p2, const Vector& p3, int r, int g, int b, int a, bool noDepthTest, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddTriangleOverlay( p1, p2, p3, r, g, b, a, noDepthTest, duration ); }
void IVPhysicsDebugOverlay::AddLineOverlay( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration )
{ static_cast<CIVDebugOverlay*>(this)->AddLineOverlay( origin, dest, r, g, b, noDepthTest, duration ); }
void IVPhysicsDebugOverlay::AddTextOverlay( const Vector& origin, float duration, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayV( origin, duration, format, args );
	__builtin_va_end( args );
}
void IVPhysicsDebugOverlay::AddTextOverlay( const Vector& origin, int line_offset, float duration, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayV( origin, line_offset, duration, format, args );
	__builtin_va_end( args );
}
void IVPhysicsDebugOverlay::AddScreenTextOverlay( float flXPos, float flYPos, float flDuration, int r, int g, int b, int a, const char *text )
{ static_cast<CIVDebugOverlay*>(this)->AddScreenTextOverlay( flXPos, flYPos, flDuration, r, g, b, a, text ); }
void IVPhysicsDebugOverlay::AddSweptBoxOverlay( const Vector& start, const Vector& end, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration )
{ static_cast<CIVDebugOverlay*>(this)->AddSweptBoxOverlay( start, end, mins, max, angles, r, g, b, a, flDuration ); }
void IVPhysicsDebugOverlay::AddTextOverlayRGB( const Vector& origin, int line_offset, float duration, float r, float g, float b, float alpha, const char *format, ... )
{
	std::va_list args;
	__builtin_va_start( args, format );
	static_cast<CIVDebugOverlay*>(this)->AddTextOverlayRGBV( origin, line_offset, duration, r, g, b, alpha, format, args );
	__builtin_va_end( args );
}

