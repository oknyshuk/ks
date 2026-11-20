#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//
//  cdll_int.h
//
// 4-23-98  
// JOHN:  client dll interface declarations
//

import std;
import ks.core.color;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;


inline constexpr const char *VDEBUG_OVERLAY_INTERFACE_VERSION = "VDebugOverlay004";

// When used as a duration by a server-side NDebugOverlay:: call, 
// causes the overlay to persist until the next server update.
inline constexpr auto NDEBUG_PERSIST_TILL_NEXT_SERVER = (0.01023f);

class OverlayText_t;

class IVDebugOverlay
{
public:
	void AddEntityTextOverlay(int ent_index, int line_offset, float duration, int r, int g, int b, int a, const char *format, ...);
	void AddBoxOverlay(const Vector& origin, const Vector& mins, const Vector& max, QAngle const& orientation, int r, int g, int b, int a, float duration);
	void AddSphereOverlay(const Vector& vOrigin, float flRadius, int nTheta, int nPhi, int r, int g, int b, int a, float flDuration);
	void AddTriangleOverlay(const Vector& p1, const Vector& p2, const Vector& p3, int r, int g, int b, int a, bool noDepthTest, float duration);
	void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b,bool noDepthTest, float duration);
	[[gnu::format(printf, 4, 5)]] void AddTextOverlay(const Vector& origin, float duration, const char *format, ...);
	[[gnu::format(printf, 5, 6)]] void AddTextOverlay(const Vector& origin, int line_offset, float duration, const char *format, ...);
	void AddScreenTextOverlay(float flXPos, float flYPos,float flDuration, int r, int g, int b, int a, const char *text);
	void AddSweptBoxOverlay(const Vector& start, const Vector& end, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration);
	void AddGridOverlay(const Vector& origin);
	void AddCoordFrameOverlay(const matrix3x4_t& frame, float flScale, int vColorTable[3][3] = nullptr);

	int ScreenPosition(const Vector& point, Vector& screen);
	int ScreenPosition(float flXPos, float flYPos, Vector& screen);

	OverlayText_t *GetFirst( void );
	OverlayText_t *GetNext( OverlayText_t *current );
	void ClearDeadOverlays( void );
	void ClearAllOverlays();

	[[gnu::format(printf, 9, 10)]] void AddTextOverlayRGB(const Vector& origin, int line_offset, float duration, float r, float g, float b, float alpha, const char *format, ...);
	[[gnu::format(printf, 9, 10)]] void AddTextOverlayRGB(const Vector& origin, int line_offset, float duration, int r, int g, int b, int a, const char *format, ...);

	void AddLineOverlayAlpha(const Vector& origin, const Vector& dest, int r, int g, int b, int a, bool noDepthTest, float duration);
	void AddBoxOverlay2( const Vector& origin, const Vector& mins, const Vector& max, QAngle const& orientation, const Color& faceColor, const Color& edgeColor, float duration );

	void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b, int a, float thickness, float duration);

	void PurgeTextOverlays();

	void AddCapsuleOverlay( const Vector &vStart, const Vector &vEnd, const float &flRadius, int r, int g, int b, int a, float flDuration );

private:
	inline void AddTextOverlay(const Vector& origin, int line_offset, float duration, int r, int g, int b, int a, const char *format, ...) {} /* catch improper use of bad interface. Needed because '0' duration can be resolved by compiler to NULL format string (i.e., compiles but calls wrong function) */
};
