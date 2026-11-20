// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "tier2/resourceprecacher.h"
#include "client_class.h"
#include "predictable_entity.h"
#include "declare_class.h"
#include "reflect_annotations.h"
module ks.client:precache;

import :particlemgr;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.core.basetypes;
import ks.tier1.createinterface;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.interpolatedvar;
import :iinput;
import :sharedvar;
import :cstrike15.cs_player;

class IGameSystem;

//-----------------------------------------------------------------------------
// Responsible for kicking off the precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheRegister : public IGameSystem
{
	// Inherited from IGameSystem
public:
	virtual char const *Name() { return "PrecacheRegister"; }
	virtual bool IsPerFrame() { return false; }
	virtual bool Init();
	virtual void PostInit() {}
	virtual void Shutdown() {}
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity();
	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	// Other public methods
public:
	// constructor, destructor
	CPrecacheRegister() {}
	virtual ~CPrecacheRegister() {}

private:
};

//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
extern PrecacheHandler_t g_PrecacheHandler;
extern CPrecacheRegister *g_pPrecacheRegister;

using namespace ::ks::reflect;

// For introspection

[[gnu::always_inline]] inline constexpr auto UsePrediction() { return 1; }

// CLIENT DLL includes

// Game DLL includes

#define DECLARE_NETWORKCLASS_NOBASE()									\
		DECLARE_CLIENTCLASS_NOBASE()							

// The prediction descriptor DSL is gone: prediction maps are generated from Pred and PredFrom
// annotations by game/shared/reflect_predmap.h, and every one of the 53 in the tree was proven
// byte-equal to its macro-built predecessor before the block was removed. DECLARE_PREDICTABLE
// above stays -- it declares m_PredMap, which the emitter fills.

// On the client .dll this creates a mapping between a classname and
//  a client side class.  Probably could be templatized at some point.

#define IMPLEMENT_NETWORKCLASS(className, dataTable)					\
	IMPLEMENT_CLIENTCLASS(className, dataTable, className)
#define IMPLEMENT_NETWORKCLASS_DT(className, dataTable)					\
	IMPLEMENT_CLIENTCLASS_DT(className, dataTable, className)

// LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED builds its table by reflection

#undef DECLARE_NETWORKCLASS_NOBASE
#undef IMPLEMENT_NETWORKCLASS
#undef IMPLEMENT_NETWORKCLASS_DT

class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;

class CPredictedViewModel : public CBaseViewModel
{
	[[= ks::reflect::NetTable{ .name = "DT_PredictedViewModel" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPredictedViewModel, CBaseViewModel );
public:

	DECLARE_NETWORKCLASS();

	CPredictedViewModel( void );
	virtual ~CPredictedViewModel( void );
							
	virtual void CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles );
	virtual void AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles );
	virtual void ApplyViewModelPitchAndDip( CBasePlayer *owner, Vector& origin, QAngle& angles );

	virtual void CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles );

	BobState_t	&GetBobState() { return m_BobState; }

	virtual bool ShouldPredict( void );

private:
	
	// This is used to lag the angles.
	CInterpolatedVar<QAngle> m_LagAnglesHistory;
	QAngle m_vLagAngles;
	Vector	m_vPredictedOffset;

	CPredictedViewModel( const CPredictedViewModel & ); // not defined, not accessible

protected:
	BobState_t		m_BobState;		// view model head bob state
	QAngle m_vLoweredWeaponOffset;
	float m_flInaccuracyTilt;
	float m_flOldAccuracyDiffSmoothed;

};
