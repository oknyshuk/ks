//--------------------------------------------------------------------------------------------------------
// Copyright (c) 2007 Turtle Rock Studios, Inc.

#if !defined( C_BASETOGGLE_H )
#define C_BASETOGGLE_H

#include "reflect_annotations.h"


#include "c_baseentity.h"


//copy/paste from server subs.cpp
enum togglemovetypes_t
{
	MOVE_TOGGLE_NONE = 0,
	MOVE_TOGGLE_LINEAR = 1,
	MOVE_TOGGLE_ANGULAR = 2,
};

//--------------------------------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_BaseToggle" } ]]
      C_BaseToggle: public C_BaseEntity
{
public:
	DECLARE_CLASS( C_BaseToggle, C_BaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE(); //elevators

	virtual void GetGroundVelocityToApply( Vector &vecGroundVel );
	virtual bool ShouldPredict( void );
	virtual C_BasePlayer *GetPredictionOwner( void );
	virtual void PhysicsSimulate( void );
	virtual bool PredictionErrorShouldResetLatchedForAllPredictables( void ) { return false; }
	virtual bool PredictionIsPhysicallySimulated( void );
	virtual void PostDataUpdate( DataUpdateType_t updateType );

	Vector PredictPosition( float fCurTime );

	EHANDLE m_hPredictionOwner;
	Vector m_vLastNetworked;
	float m_fLastNetworkedTime;
	
	[[= ks::reflect::Net{} ]] Vector m_vecFinalDest;
	[[= ks::reflect::Net{} ]] int m_movementType;
	[[= ks::reflect::Net{} ]] float m_flMoveTargetTime;
	bool m_bWasPredictingMotion;
};


//--------------------------------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_BaseButton" } ]]
      C_BaseButton: public C_BaseToggle
{
public:
	DECLARE_CLASS( C_BaseButton, C_BaseToggle );
	DECLARE_CLIENTCLASS();

	C_BaseButton()
	{
	}

	virtual bool IsPotentiallyUsable( void );

private:
	[[= ks::reflect::Net{} ]] bool m_usable;
};


//--------------------------------------------------------------------------------------------------------
//
//	CButtonTimed - func_button_timed
//
//--------------------------------------------------------------------------------------------------------
class C_ButtonTimed: public C_BaseButton
{
public:
	DECLARE_CLASS( C_BaseButton, C_BaseButton );
	DECLARE_CLIENTCLASS();
	
	C_ButtonTimed();

	const char * GetUseString( void ) const
	{
		return m_sUseString;
	}

	const char * GetUseSubString( void ) const
	{
		return m_sUseSubString;
	}

private:
	char m_sUseString[64];
	char m_sUseSubString[64];
};



#endif // C_BASETOGGLE_H
