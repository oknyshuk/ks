// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "const.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "itempents.h"
#include "vphysics_interface.h"
#include "physics.h"
#include "engine/ivmodelinfo.h"
#include "prediction.h"
#include "igamemovement.h"
#include "prediction_private.h"
#include "iinput.h"
#include "usercmd.h"
#include "iclientvehicle.h"
#include "con_nprint.h"
#include "datacache/imdlcache.h"


IPredictionSystem *IPredictionSystem::g_pPredictionSystems = nullptr;

extern ConVar cl_predictphysics;

ConVar	cl_predictweapons	( "cl_predictweapons","1", FCVAR_USERINFO | FCVAR_NOT_CONNECTED, "Perform client side prediction of weapon effects." );
ConVar	cl_lagcompensation	( "cl_lagcompensation","1", FCVAR_USERINFO | FCVAR_NOT_CONNECTED, "Perform server side lag compensation of weapon firing events." );
ConVar	cl_showerror		( "cl_showerror", "0", FCVAR_RELEASE, "Show prediction errors, 2 for above plus detailed field deltas." );

static ConVar	cl_idealpitchscale	( "cl_idealpitchscale", "0.8", FCVAR_ARCHIVE );
static ConVar	cl_predictionlist	( "cl_predictionlist", "0", FCVAR_CHEAT, "Show which entities are predicting\n" );

static ConVar	cl_predictionentitydump( "cl_pdump", "-1", FCVAR_CHEAT, "Dump info about this entity to screen." );
static ConVar	cl_predictionentitydumpbyclass( "cl_pclass", "", FCVAR_CHEAT, "Dump entity by prediction classname." );
static ConVar	cl_pred_optimize( "cl_pred_optimize", "2", 0, "Optimize for not copying data if didn't receive a network update (1), and also for not repredicting if there were no errors (2)." );

static ConVar	cl_pred_doresetlatch( "cl_pred_doresetlatch", "1", 0 );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void InvalidateEFlagsRecursive( C_BaseEntity *pEnt, int nDirtyFlags, int nChildFlags = 0 )
{
	pEnt->AddEFlags( nDirtyFlags );
	nDirtyFlags |= nChildFlags;
	for (CBaseEntity *pChild = pEnt->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer())
	{
		InvalidateEFlagsRecursive( pChild, nDirtyFlags );
	}
}

extern IGameMovement *g_pGameMovement;
extern CMoveData *g_pMoveData;

void COM_Log( const char *pszFile, const char *fmt, ...) [[gnu::format(printf, 2, 3)]]; // Log a debug message to specified file ( if pszFile == NULL uses c:\\hllog.txt )
void PhysicsSimulate( void );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrediction::CPrediction( void ) : m_SavedVars( true )
{
	m_bInPrediction = false;

	m_nPreviousStartFrame = -1;
	m_nIncomingPacketNumber = 0;

	m_bPlayerOriginTypedescriptionSearched = false;
	m_bEnginePaused = false;

	m_flLastServerWorldTimeStamp = -1.0f;
}

CPrediction::~CPrediction( void )
{
}

void CPrediction::Init( void )
{
	m_bOldCLPredictValue = cl_predict->GetBool();
}

void CPrediction::Shutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CPrediction::CheckError( int nSlot, C_BasePlayer *player, int commands_acknowledged )
{
	Vector		origin;
	Vector		delta;
	float		len;
	static int	pos[ MAX_SPLITSCREEN_PLAYERS ];

	// Not in the game yet
	if ( !engine->IsInGame() )
		return;

	// Not running prediction
	if ( !cl_predict->GetInt() )
		return;

	if ( !player )
		return;
	
	// Not predictable yet (flush entity packet?)
	if ( !player->IsIntermediateDataAllocated() )
		return;

	origin = player->GetNetworkOrigin();
		
	const void *slot = player->GetPredictedFrame( commands_acknowledged - 1 );
	if ( !slot )
		return;

	if ( !m_bPlayerOriginTypedescriptionSearched )
	{
		m_bPlayerOriginTypedescriptionSearched = true;
		const typedescription_t *td = CPredictionCopy::FindFlatFieldByName( "m_vecNetworkOrigin", player->GetPredDescMap() );
		if ( td ) 
		{
			m_PlayerOriginTypeDescription.push_back( td );
		}
	}

	if ( m_PlayerOriginTypeDescription.empty() )
		return;

	Vector predicted_origin;

	// Find the origin field in the database
	// Splitting m_vecNetworkOrigin into component fields for prediction *
	V_memcpy( (Vector *)&predicted_origin, (Vector *)( (byte *)slot + m_PlayerOriginTypeDescription[ 0 ]->flatOffset[ TD_OFFSET_PACKED ] ), sizeof( Vector ) );

	// Compare what the server returned with what we had predicted it to be
	VectorSubtract ( predicted_origin, origin, delta );

	len = VectorLength( delta );
	if (len > MAX_PREDICTION_ERROR )
	{	
		// A teleport or something, clear out error
		len = 0;
	}
	else
	{
		if ( len > MIN_PREDICTION_EPSILON )
		{
			player->NotePredictionError( delta );

			if ( cl_showerror.GetInt() >= 1 )
			{
				con_nprint_t np;
				np.fixed_width_font = true;
				np.color[0] = 1.0f;
				np.color[1] = 0.95f;
				np.color[2] = 0.7f;
				np.index = 4 + nSlot * 10 + ( ++pos[ nSlot ] % 10 );
				np.time_to_live = 3.0f;

				engine->Con_NXPrintf( &np, "%d len(%6.3f) (%6.3f %6.3f %6.3f)", nSlot, len, delta.x, delta.y, delta.z );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::ShutdownPredictables( void )
{

	int shutdown_count = 0;
	int release_count = 0;

	for ( int nSlot = 0; nSlot < MAX_SPLITSCREEN_PLAYERS; ++nSlot )
	{
		// Transfer intermediate data from other predictables
		int c = GetPredictables( nSlot )->GetPredictableCount();
		for ( int i = c - 1; i >= 0 ; i-- )
		{
			C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent )
				continue;

			// Shutdown predictables
			if ( ent->GetPredictable() )
			{
				ent->ShutdownPredictable();
				shutdown_count++;
			}
			// Otherwise, release client created entities
			else
			{
				ent->Release();
				release_count++;
			}
		}

		// All gone now...
		contract_assert( GetPredictables( nSlot )->GetPredictableCount() == 0 );
	}

	if ( ( release_count > 0 ) || 
		 ( shutdown_count > 0 ) )
	{
		DevMsg( "Shutdown %i predictable entities and %i client-created entities\n",
			shutdown_count,
			release_count );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::ReinitPredictables( void )
{
	// Go through all entities and init any eligible ones
	int i;
	int c = ClientEntityList().GetHighestEntityIndex();
	for ( i = 0; i <= c; i++ )
	{
		C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
		if ( !e )
			continue;
		
		if ( e->GetPredictable() )
			continue;

		e->CheckInitPredictable( "ReinitPredictables" );
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		Msg( "%d:  Reinitialized %i predictable entities\n",
			nSlot, GetPredictables( nSlot )->GetPredictableCount() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::OnReceivedUncompressedPacket( void )
{
	m_nPreviousStartFrame = -1;
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		Split_t &split = m_Split[ i ];
		split.m_nCommandsPredicted = 0;
		split.m_nServerCommandsAcknowledged = 0;
		split.m_nLastCommandAcknowledged = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : commands_acknowledged - 
//			current_world_update_packet - 
// Output : void CPrediction::PreEntityPacketReceived
//-----------------------------------------------------------------------------
void CPrediction::PreEntityPacketReceived ( int commands_acknowledged, int current_world_update_packet, int server_ticks_elapsed )
{
#if defined( _DEBUG )
	char sz[ 32 ];
	V_snprintf( sz, sizeof( sz ), "preentitypacket%d", commands_acknowledged );
	PREDICTION_TRACKVALUECHANGESCOPE( sz );
#endif
	CVProfScope _( "CPrediction::PreEntityPacketReceived", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Cache off incoming packet #
	m_nIncomingPacketNumber = current_world_update_packet;
	C_BaseEntity::s_nIncomingPacketCommandsAcknowledged = commands_acknowledged;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
	{
		ShutdownPredictables();
		return;
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		C_BasePlayer *current = C_BasePlayer::GetLocalPlayer( nSlot );
		// No local player object?
		if ( !current )
			continue;

		// Transfer intermediate data from other predictables
		int c = GetPredictables( nSlot )->GetPredictableCount();

		int i;
		for ( i = 0; i < c; i++ )
		{
			C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent )
				continue;

			if ( !ent->GetPredictable() )
				continue;

			if ( (commands_acknowledged != server_ticks_elapsed) && ent->PredictionIsPhysicallySimulated() )
			{
				ent->ShiftIntermediateData_TickAdjust( server_ticks_elapsed - commands_acknowledged, m_Split[nSlot].m_nCommandsPredicted );
				m_Split[nSlot].m_bPerformedTickShift = true;
			}

			ent->PreEntityPacketReceived( commands_acknowledged );
			ent->OnPostRestoreData();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called for every packet received( could be multiple times per frame)
//-----------------------------------------------------------------------------
void CPrediction::PostEntityPacketReceived( void )
{
	PREDICTION_TRACKVALUECHANGESCOPE( "postentitypacket" );
	CVProfScope _( "CPrediction::PostEntityPacketReceived", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	C_BaseEntity::s_nIncomingPacketCommandsAcknowledged = -1;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		C_BasePlayer *current = C_BasePlayer::GetLocalPlayer( nSlot );
		// No local player object?
		if ( !current )
			continue;

		int c = GetPredictables( nSlot )->GetPredictableCount();

		// Transfer intermediate data from other predictables
		int i;
		for ( i = 0; i < c; i++ )
		{
			C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent )
				continue;

			if ( !ent->GetPredictable() )
				continue;

			// Always mark as changed
			if ( AddDataChangeEvent( ent, DATA_UPDATE_DATATABLE_CHANGED, &ent->m_DataChangeEventRef ) )
			{
				ent->OnPreDataChanged( DATA_UPDATE_DATATABLE_CHANGED );
			}
			ent->PostEntityPacketReceived();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ent - 
// Output : static bool
//-----------------------------------------------------------------------------
bool CPrediction::ShouldDumpEntity( C_BaseEntity *ent )
{
	int dump_entity = cl_predictionentitydump.GetInt();
	if ( dump_entity != -1 )
	{
		bool dump = false;
		if ( ent->entindex() == -1 )
		{
			dump = ( dump_entity == ent->entindex() ) ? true : false;
		}
		else
		{
			dump = ( ent->entindex() == dump_entity ) ? true : false;
		}

		if ( !dump )
		{
			return false;
		}
	}
	else
	{
		if ( cl_predictionentitydumpbyclass.GetString()[ 0 ] == 0 )
			return false;

		if ( !FClassnameIs( ent, cl_predictionentitydumpbyclass.GetString() ) )
			return false;
	}
	return true;
}

void CPrediction::ShowPredictionListEntry( int listRow, int showlist, C_BaseEntity *ent, int &totalsize, int &totalsize_intermediate )
{
	char sz[ 32 ];
	if ( ent->entindex() == -1 )
	{
		V_snprintf( sz, sizeof( sz ), "handle %u", (unsigned int)ent->GetClientHandle().ToInt() );
	}
	else
	{
		V_snprintf( sz, sizeof( sz ), "%i", ent->entindex() );
	}

	int oIndex = 0;
	if ( ent->GetOwnerEntity() )
	{
		oIndex = ent->GetOwnerEntity()->entindex();
	}
	else if ( ent->IsPlayer() )
	{
		oIndex = ent->entindex();
	}
	else 
	{
		C_BaseViewModel *pVM = ToBaseViewModel( ent );
		if ( pVM && pVM->GetOwner() )
		{
			oIndex = pVM->GetOwner()->entindex();
		}
	}

	con_nprint_t np;
	np.fixed_width_font = true;
	np.color[0] = 0.8f;
	np.color[1] = 1.0f;
	np.color[2] = 1.0f;
	np.time_to_live = 2.0f;
	np.index = listRow;
	if ( showlist >= 2 )
	{
		int size = GetClassMap().GetClassSize( ent->GetClassname() );
		int intermediate_size = ent->GetIntermediateDataSize() * ( MULTIPLAYER_BACKUP + 1 );

		engine->Con_NXPrintf( &np, "%15s %30s(%d) (%5i / %5i bytes): %15s", 
			sz, 
			ent->GetClassname(),
			oIndex,
			size,
			intermediate_size,
			ent->GetPredictable() ? "predicted" : "client created" );

		totalsize += size;
		totalsize_intermediate += intermediate_size;
	}
	else
	{
		engine->Con_NXPrintf( &np, "%15s %30s(%d): %15s", 
			sz, 
			ent->GetClassname(),
			oIndex,
			ent->GetPredictable() ? "predicted" : "client created" );
	}
}

void CPrediction::FinishPredictionList( int listRow, int showlist, int totalsize, int totalsize_intermediate )
{
	if ( !showlist )
		return;

	if ( showlist > 1 )
	{
		con_nprint_t np;
		np.fixed_width_font = true;
		np.color[0] = 0.8f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;
		np.time_to_live = 2.0f;
		np.index = listRow++;
		char sz1[32];
		char sz2[32];

		V_strncpy( sz1, V_pretifymem( (float)totalsize ), sizeof( sz1 ) );
		V_strncpy( sz2, V_pretifymem( (float)totalsize_intermediate ), sizeof( sz2 ) );

		engine->Con_NXPrintf( &np, "%15s %27s (%s / %s)  %14s", 
			"totals:", 
			"",
			sz1,
			sz2,
			"" );
	}

	// Zero out rest of list
	while ( listRow < 20 )
	{
		engine->Con_NPrintf( listRow++, "" );
	}
}

void CPrediction::CheckPredictConvar()
{
	if ( cl_predict->GetBool() != m_bOldCLPredictValue )
	{
		if ( !m_bOldCLPredictValue )
		{
			ReinitPredictables();
		}

		for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			Split_t &split = m_Split[ i ];
			split.m_nCommandsPredicted = 0;
			split.m_nServerCommandsAcknowledged = 0;
			split.m_nLastCommandAcknowledged = 0;
		}
		m_nPreviousStartFrame = -1;
	}

	m_bOldCLPredictValue = cl_predict->GetBool();
}

ConVar cl_prediction_error_timestamps( "cl_prediction_error_timestamps", "0" );
//-----------------------------------------------------------------------------
// Purpose: Called at the end of the frame if any packets were received
// Input  : error_check - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void CPrediction::PostNetworkDataReceived( int commands_acknowledged )
{
	ks::ScratchArena<> scratch;
	CVProfScope _( "CPrediction::PostNetworkDataReceived", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	bool error_check = ( commands_acknowledged > 0 ) ? true : false;
#if defined( _DEBUG )
	char sz[ 32 ];
	V_snprintf( sz, sizeof( sz ), "postnetworkdata%d", commands_acknowledged );
	PREDICTION_TRACKVALUECHANGESCOPE( sz );
#endif
	//Msg( "%i/%i ack %i commands/slot\n",
	//	gpGlobals->framecount,
	//	gpGlobals->tickcount,
	//	commands_acknowledged - 1 );

	bool entityDumped = false;
	bool bPredict = cl_predict->GetBool();
	int showlist = cl_predictionlist.GetInt();

	int listRow = 0;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		Split_t &split = m_Split[ nSlot ];
		split.m_nServerCommandsAcknowledged += commands_acknowledged;
		split.m_bPreviousAckHadErrors = false;
		split.m_bPreviousAckErrorTriggersFullLatchReset = false;
		split.m_EntsWithPredictionErrorsInLastAck.clear();

		if ( bPredict )
		{
			// Don't screw up memory of current player from history buffers if not filling in history buffers during prediction!!!
			int totalsize = 0;
			int totalsize_intermediate = 0;

			// Build list of all predictables
			int c = GetPredictables( nSlot )->GetPredictableCount();
			bool *bHadErrors = scratch.Alloc<bool>( c );
			// Transfer intermediate data from other predictables
			int i;
			for ( i = 0; i < c; i++ )
			{
				C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
				if ( !ent )
					continue;

				if ( !ent->GetPredictable() )
					continue;

				bHadErrors[i] = ent->PostNetworkDataReceived( split.m_nServerCommandsAcknowledged );
				if ( bHadErrors[i] )
				{
					split.m_bPreviousAckHadErrors = true;
					split.m_bPreviousAckErrorTriggersFullLatchReset |= ent->PredictionErrorShouldResetLatchedForAllPredictables();
					split.m_EntsWithPredictionErrorsInLastAck.push_back( ent ); 
				}

				if ( !showlist )
				{
					continue;
				}

				ShowPredictionListEntry( listRow, showlist, ent, totalsize, totalsize_intermediate );
				listRow++;
			}

			//Give entities with predicted fields that are not networked a chance to fix their current values for those fields.
			//We do this in two passes. One pass to fix the fields, then another to save off the changes after they've all finished (to handle interdependancies, portals)
			if( split.m_bPreviousAckHadErrors )
			{
				//give each predicted entity a chance to fix up its non-networked predicted fields
				for ( i = 0; i < c; i++ )
				{
					C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
					if ( !ent )
						continue;

					if ( !ent->GetPredictable() )
						continue;

					ent->HandlePredictionError( bHadErrors[i] );
				}

				//save off any changes
				for ( i = 0; i < c; i++ )
				{
					C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
					if ( !ent )
						continue;

					if ( !ent->GetPredictable() )
						continue;

					ent->SaveData( "PostNetworkDataReceived() Ack Errors", C_BaseEntity::SLOT_ORIGINALDATA, PC_EVERYTHING );
				}
			}

			for ( i = c; --i >= 0; ) //go backwards to maintain ordering on shutdown
			{
				C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
				if ( !ent )
					continue;

				if ( !ent->GetPredictable() )
					continue;

				ent->CheckShutdownPredictable( "CPrediction::PostNetworkDataReceived" );
			}

			FinishPredictionList( listRow, showlist, totalsize, totalsize_intermediate );

			if ( error_check )
			{
				C_BasePlayer *current = C_BasePlayer::GetLocalPlayer( nSlot );
				if ( !current )
					continue;
				CheckError( nSlot, current, split.m_nServerCommandsAcknowledged );
			}
		}

		// Can also look at regular entities
		int dumpentindex = cl_predictionentitydump.GetInt();

		if( split.m_bPreviousAckHadErrors && cl_prediction_error_timestamps.GetBool() )
		{
			Warning( "Prediction errors occurred at %i %f\n", gpGlobals->tickcount, gpGlobals->curtime );
		}
	}

	CheckPredictConvar();

}

//-----------------------------------------------------------------------------
// Purpose: Prepare for running prediction code
// Input  : *ucmd - 
//			*from - 
//			*pHelper - 
//			&moveInput - 
//-----------------------------------------------------------------------------
void CPrediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move ) 
{
	CVProfScope _( "CPrediction::SetupMove", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	move->m_bFirstRunOfFunctions = IsFirstTimePredicted();
	move->m_bGameCodeMovedPlayer = false;
	if ( player->GetPreviouslyPredictedOrigin() != player->GetNetworkOrigin() )
	{
		move->m_bGameCodeMovedPlayer = true;
	}
	
	move->m_nPlayerHandle = player->GetClientHandle();
	move->m_vecVelocity		= player->GetAbsVelocity();
	move->SetAbsOrigin( player->GetNetworkOrigin() );
	move->m_vecOldAngles	= move->m_vecAngles;
	move->m_nOldButtons		= player->m_Local.m_nOldButtons;
	move->m_flClientMaxSpeed = player->m_flMaxspeed;

	move->m_vecAngles		= ucmd->viewangles;
	move->m_vecViewAngles	= ucmd->viewangles;
	move->m_nImpulseCommand = ucmd->impulse;	
	move->m_nButtons		= ucmd->buttons;

	CBaseEntity *pMoveParent = player->GetMoveParent();
	if (!pMoveParent)
	{
		move->m_vecAbsViewAngles = move->m_vecViewAngles;
	}
	else
	{
		matrix3x4_t viewToParent, viewToWorld;
		AngleMatrix( move->m_vecViewAngles, viewToParent );
		ConcatTransforms( pMoveParent->EntityToWorldTransform(), viewToParent, viewToWorld );
		MatrixAngles( viewToWorld, move->m_vecAbsViewAngles );
	}


	// Ingore buttons for movement if at controls
	if (player->GetFlags() & FL_ATCONTROLS)
	{
		move->m_flForwardMove		= 0;
		move->m_flSideMove			= 0;
		move->m_flUpMove			= 0;
	}
	else
	{
		move->m_flForwardMove		= ucmd->forwardmove;
		move->m_flSideMove			= ucmd->sidemove;
		move->m_flUpMove			= ucmd->upmove;
	}
		
	IClientVehicle *pVehicle = player->GetVehicle();
	if (pVehicle)
	{
		pVehicle->SetupMove( player, ucmd, pHelper, move ); 
	}

	// Copy constraint information
	if ( player->m_hConstraintEntity )
		move->m_vecConstraintCenter = player->m_hConstraintEntity->GetAbsOrigin();
	else
		move->m_vecConstraintCenter = player->m_vecConstraintCenter;

	move->m_flConstraintRadius = player->m_flConstraintRadius;
	move->m_flConstraintWidth = player->m_flConstraintWidth;
	move->m_flConstraintSpeedFactor = player->m_flConstraintSpeedFactor;

	
	g_pGameMovement->SetupMovementBounds( move );
}

//-----------------------------------------------------------------------------
// Purpose: Finish running prediction code
// Input  : &move - 
//			*to - 
//-----------------------------------------------------------------------------
void CPrediction::FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	CVProfScope _( "CPrediction::FinishMove", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	player->m_RefEHandle = move->m_nPlayerHandle;

	player->SetAbsVelocity( move->m_vecVelocity );

	player->m_vecNetworkOrigin = move->GetAbsOrigin();
	player->SetPreviouslyPredictedOrigin( move->GetAbsOrigin() );
	
	player->m_Local.m_nOldButtons = move->m_nButtons;


	player->m_flMaxspeed = move->m_flClientMaxSpeed;
	
	m_hLastGround = player->GetGroundEntity();
 
	player->SetLocalOrigin( move->GetAbsOrigin() );

	IClientVehicle *pVehicle = player->GetVehicle();
	if (pVehicle)
	{
		pVehicle->FinishMove( player, ucmd, move ); 
	}

	// Sanity checks
	if ( player->m_hConstraintEntity )
		contract_assert( move->m_vecConstraintCenter == player->m_hConstraintEntity->GetAbsOrigin() );
	else
		contract_assert( move->m_vecConstraintCenter == player->m_vecConstraintCenter );
	contract_assert( move->m_flConstraintRadius == player->m_flConstraintRadius );
	contract_assert( move->m_flConstraintWidth == player->m_flConstraintWidth );
	contract_assert( move->m_flConstraintSpeedFactor == player->m_flConstraintSpeedFactor );
}

//-----------------------------------------------------------------------------
// Purpose: Called before any movement processing
// Input  : *player - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CPrediction::StartCommand( C_BasePlayer *player, CUserCmd *cmd )
{
	CVProfScope _( "CPrediction::StartCommand", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	player->m_pCurrentCommand = cmd;
	player->m_LastCmd = *cmd;
	C_BaseEntity::SetPredictionRandomSeed( cmd );
	C_BaseEntity::SetPredictionPlayer( player );
}

//-----------------------------------------------------------------------------
// Purpose: Called after any movement processing
// Input  : *player - 
//-----------------------------------------------------------------------------
void CPrediction::FinishCommand( C_BasePlayer *player )
{
	CVProfScope _( "CPrediction::FinishCommand", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	player->m_pCurrentCommand = nullptr;
	C_BaseEntity::SetPredictionRandomSeed( nullptr );
	C_BaseEntity::SetPredictionPlayer( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: Called before player thinks
// Input  : *player - 
//			thinktime - 
//-----------------------------------------------------------------------------
void CPrediction::RunPreThink( C_BasePlayer *player )
{
	CVProfScope _( "CPrediction::RunPreThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Run think functions on the player
	if ( !player->PhysicsRunThink() )
		return;

	// Called every frame to let game rules do any specific think logic for the player
	// FIXME:  Do we need to set up a client side version of the gamerules???
	// g_pGameRules->PlayerThink( player );

	player->PreThink();
}

//-----------------------------------------------------------------------------
// Purpose: Runs the PLAYER's thinking code if time.  There is some play in the exact time the think
//  function will be called, because it is called before any movement is done
//  in a frame.  Not used for pushmove objects, because they must be exact.
//  Returns false if the entity removed itself.
// Input  : *ent - 
//			frametime - 
//			clienttimebase - 
// Output : void CPlayerMove::RunThink
//-----------------------------------------------------------------------------
void CPrediction::RunThink (C_BasePlayer *player, double frametime )
{
	CVProfScope _( "CPrediction::RunThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	int thinktick = player->GetNextThinkTick();

	if ( thinktick <= 0 || thinktick > player->m_nTickBase )
		return;
	
	player->SetNextThink( TICK_NEVER_THINK );

	// Think
	player->Think();
}

//-----------------------------------------------------------------------------
// Purpose: Called after player movement
// Input  : *player - 
//			thinktime - 
//			frametime - 
//-----------------------------------------------------------------------------
void CPrediction::RunPostThink( C_BasePlayer *player )
{
	CVProfScope _( "CPrediction::RunPostThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Run post-think
	player->PostThink();
}

//-----------------------------------------------------------------------------
// Purpose: Checks if the player is standing on a moving entity and adjusts velocity and 
//  basevelocity appropriately
// Input  : *player - 
//			frametime - 
//-----------------------------------------------------------------------------
void CPrediction::CheckMovingGround( C_BasePlayer *player, double frametime )
{
	CBaseEntity	    *groundentity;

	if ( player->GetFlags() & FL_ONGROUND )
	{
		groundentity = player->GetGroundEntity();
		if ( groundentity && ( groundentity->GetFlags() & FL_CONVEYOR) )
		{
			Vector vecNewVelocity;
			groundentity->GetGroundVelocityToApply( vecNewVelocity );
			if ( player->GetFlags() & FL_BASEVELOCITY )
			{
				vecNewVelocity += player->GetBaseVelocity();
			}
			player->SetBaseVelocity( vecNewVelocity );
			player->AddFlag( FL_BASEVELOCITY );
		}
	}

	if ( !( player->GetFlags() & FL_BASEVELOCITY ) )
	{
		// Apply momentum (add in half of the previous frame of velocity first)
		player->ApplyAbsVelocityImpulse( (1.0 + ( frametime * 0.5 )) * player->GetBaseVelocity() );
		player->SetBaseVelocity( vec3_origin );
	}

	player->RemoveFlag( FL_BASEVELOCITY );
}

//-----------------------------------------------------------------------------
// Purpose: Predicts a single movement command for player
// Input  : *moveHelper - 
//			*player - 
//			*u - 
//-----------------------------------------------------------------------------
void CPrediction::RunCommand( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	CVProfScope _( "CPrediction::RunCommand", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
#if defined( _DEBUG )
	char sz[ 32 ];
	V_snprintf( sz, sizeof( sz ), "runcommand%04d", ucmd->command_number );
	PREDICTION_TRACKVALUECHANGESCOPE( sz );
#endif


	StartCommand( player, ucmd );

	// Set globals appropriately
	gpGlobals->curtime		= player->m_nTickBase * TICK_INTERVAL;
	gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;

	// Add and subtract buttons we're forcing on the player
	ucmd->buttons |= player->m_afButtonForced;
	// ucmd->buttons &= ~player->m_afButtonDisabled; // MAY WANT TO DO THIS LATER!!!

	g_pGameMovement->StartTrackPredictionErrors( player );

// TODO
// TODO:  Check for impulse predicted?

	// Do weapon selection
	if ( ucmd->weaponselect != 0 )
	{
		C_BaseCombatWeapon *weapon = ToBaseCombatWeapon( CBaseEntity::Instance( ucmd->weaponselect ) );
		if ( weapon )
		{
			player->SelectItem( weapon->GetName(), ucmd->weaponsubtype );
		}
	}

	// Latch in impulse.
	IClientVehicle *pVehicle = player->GetVehicle();
	if ( ucmd->impulse )
	{
		// Discard impulse commands unless the vehicle allows them.
		// FIXME: UsingStandardWeapons seems like a bad filter for this. 
		// The flashlight is an impulse command, for example.
		if ( !pVehicle || player->UsingStandardWeaponsInVehicle() )
		{
			player->m_nImpulse = ucmd->impulse;
		}
	}

	// Get button states
	player->UpdateButtonState( ucmd->buttons );

// TODO
	CheckMovingGround( player, gpGlobals->frametime );

// TODO
//	g_pMoveData->m_vecOldAngles = player->pl.v_angle;

	// Copy from command to player unless game .dll has set angle using fixangle
	// if ( !player->pl.fixangle )
	{
		player->SetLocalViewAngles( ucmd->viewangles );
	}

	// Call standard client pre-think
	RunPreThink( player );

	// Call Think if one is set
	RunThink( player, TICK_INTERVAL );

	// Setup input.
	{
	
		SetupMove( player, ucmd, moveHelper, g_pMoveData );
	}


	{
		CVProfScope _( "CPrediction::ProcessMovement", 0, "CPrediction::ProcessMovement", false, BUDGETFLAG_OTHER );

		// RUN MOVEMENT
		if ( !pVehicle )
		{
			contract_assert( g_pGameMovement );
			g_pGameMovement->ProcessMovement( player, g_pMoveData );
		}
		else
		{
			pVehicle->ProcessMovement( player, g_pMoveData );
		}
	}

	FinishMove( player, ucmd, g_pMoveData );

	// Let server invoke any needed impact functions
	{ CVProfScope _( "moveHelper->ProcessImpacts(cl)", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	moveHelper->ProcessImpacts();
	};

	RunPostThink( player );

	g_pGameMovement->FinishTrackPredictionErrors( player );

	FinishCommand( player );
	g_pGameMovement->Reset();  // fixes a crash: when loading highlights twice or after previously loaded map, there was a dirty player pointer in game movement

	if( !m_bEnginePaused && gpGlobals->frametime > 0 )
	{
		player->m_nTickBase++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: In the forward direction, creates rays straight down and determines the
//  height of the 'floor' hit for each forward test.  Then, if the samples show that the
//  player is about to enter an up/down slope, sets *idealpitch to look up or down that slope
//  as appropriate
//-----------------------------------------------------------------------------
void CPrediction::SetIdealPitch ( int nSlot, C_BasePlayer *player, const Vector& origin, const QAngle& angles, const Vector& viewheight )
{
	Vector	forward;
	Vector	top, bottom;
	float	floor_height[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;
	trace_t tr;

	if ( player->GetGroundEntity() == nullptr )
		return;
	
	// Don't do this on the 360..

	AngleVectors( angles, &forward );
	forward[2] = 0;

	MDLCACHE_CRITICAL_SECTION();

	// Now move forward by 36, 48, 60, etc. units from the eye position and drop lines straight down
	//  160 or so units to see what's below
	for (i=0 ; i<MAX_FORWARD ; i++)
	{
		VectorMA( origin, (i+3)*12, forward, top );
		
		top[2] += viewheight[ 2 ];

		VectorCopy( top, bottom );

		bottom[2] -= 160;

		UTIL_TraceLine( top, bottom, MASK_SOLID, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );

		// looking at a wall, leave ideal the way it was
		if ( tr.allsolid )
			return;	

		// near a dropoff/ledge
		if ( tr.fraction == 1 )
			return;	
		
		floor_height[i] = top[2] + tr.fraction*( bottom[2] - top[2] );
	}
	
	dir = 0;
	steps = 0;
	for (j=1 ; j<i ; j++)
	{
		step = floor_height[j] - floor_height[j-1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && ( step-dir > ON_EPSILON || step-dir < -ON_EPSILON ) )
			return;		// mixed changes

		steps++;	
		dir = step;
	}
	
	Split_t &split = m_Split[ nSlot ];
	if (!dir)
	{
		split.m_flIdealPitch = 0;
		return;
	}
	
	if (steps < 2)
		return;
	split.m_flIdealPitch = -dir * cl_idealpitchscale.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Walk backward through predictables looking for ClientCreated entities
//  such as projectiles which were
// 1) not actually ack'd by the server or
// 2) were ack'd and made dormant and can now safely be removed
// Input  : last_command_packet - 
//-----------------------------------------------------------------------------
void CPrediction::RemoveStalePredictedEntities( int nSlot, int sequence_number )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::RestoreOriginalEntityState( int nSlot )
{
	CVProfScope _( "CPrediction::RestoreOriginalEntityState", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "restore" );

	contract_assert( C_BaseEntity::IsAbsRecomputationsEnabled() );

	// Transfer intermediate data from other predictables
	int c = GetPredictables( nSlot )->GetPredictableCount();
	for ( int i = 0; i < c; ++i )
	{
		C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent || !ent->GetPredictable() )
			continue;

		ent->RestoreData( "RestoreOriginalEntityState", C_BaseEntity::SLOT_ORIGINALDATA, PC_EVERYTHING );
		ent->OnPostRestoreData();
	}
}

void CPrediction::ResetSimulationTick()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	// Make sure simulation occurs at most once per entity per usercmd
	for (int  i = 0; i < GetPredictables( nSlot )->GetPredictableCount(); i++ )
	{
		C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
		if ( entity && entity->GetSplitUserPlayerPredictionSlot() == nSlot )
		{
			entity->m_nSimulationTick = -1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : current_command - 
//			curtime - 
//			*cmd - 
//			*tcmd - 
//			*localPlayer - 
//-----------------------------------------------------------------------------
void CPrediction::RunSimulation( int current_command, float curtime, CUserCmd *cmd, C_BasePlayer *localPlayer )
{
	CVProfScope _( "CPrediction::RunSimulation", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	contract_assert( localPlayer );
	C_CommandContext *ctx = localPlayer->GetCommandContext();
	contract_assert( ctx );
	
	ctx->needsprocessing = true;
	ctx->cmd = *cmd;
	ctx->command_number = current_command;

	IPredictionSystem::SuppressEvents( !IsFirstTimePredicted() );

	ResetSimulationTick();
	
	// Don't used cached numpredictables since entities can be created mid-prediction by the player
	for ( int i = 0; i < GetPredictables( nSlot )->GetPredictableCount(); i++ )
	{
		// Always reset
		gpGlobals->curtime		= curtime;
		gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;

		C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );

		if ( !entity )
			continue;

		if ( entity->GetSplitUserPlayerPredictionSlot() != nSlot )
			continue;

		bool islocal = ( localPlayer == entity ) ? true : false;

		// Local player simulates first, if this assert fires then the predictables list isn't sorted 
		//  correctly (or we started predicting C_World???)
		if ( islocal && !IsValidSplitScreenSlot( i ) )
			continue;

		// Player can't be this so cull other entities here
		if ( entity->GetFlags() & FL_STATICPROP )
		{
			continue;
		}

		// Player is not actually in the m_SimulatedByThisPlayer list, of course
		if ( entity->IsPlayerSimulated() )
		{
			continue;
		}

		if ( AddDataChangeEvent( entity, DATA_UPDATE_DATATABLE_CHANGED, &entity->m_DataChangeEventRef ) )
		{
			entity->OnPreDataChanged( DATA_UPDATE_DATATABLE_CHANGED );
		}

		// Certain entities can be created locally and if so created, should be 
		//  simulated until a network update arrives
		if ( entity->IsClientCreated() )
		{
			// Only simulate these on new usercmds
			if ( !IsFirstTimePredicted() )
				continue;

			entity->PhysicsSimulate();
		}
		else
		{
			entity->PhysicsSimulate();
		}

		// Don't update last networked data here!!!
		entity->OnLatchInterpolatedVariables( LATCH_SIMULATION_VAR | LATCH_ANIMATION_VAR | INTERPOLATE_OMIT_UPDATE_LAST_NETWORKED );
	}

	// Only call the physics update for the entire physics system if we support predicted physics.
	if ( cl_predictphysics.GetBool() )
	{
		PhysicsSimulate();
	}
	
	// Always reset after running command
	IPredictionSystem::SuppressEvents( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::Untouch( int nSlot )
{
	int numpredictables = GetPredictables( nSlot )->GetPredictableCount();

	// Loop through all entities again, checking their untouch if flagged to do so
	int i;
	for ( i = 0; i < numpredictables; i++ )
	{
		C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
		if ( !entity )
			continue;

		if ( !entity->GetCheckUntouch() )
			continue;

		entity->PhysicsCheckForEntityUntouch();
	}
}

void CPrediction::StorePredictionResults( int nSlot, int predicted_frame )
{
	CVProfScope _( "CPrediction::StorePredictionResults", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "save" );

	int c = GetPredictables( nSlot )->GetPredictableCount();

	// Now save off all of the results
	for ( int i = 0; i < c; ++i )
	{
		C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
		if ( !entity )
			continue;

		// Certain entities can be created locally and if so created, should be 
		//  simulated until a network update arrives
		if ( !entity->GetPredictable() )
			continue;

		// FIXME: The lack of this call inexplicably actually creates prediction errors
		InvalidateEFlagsRecursive( entity, EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSANGVELOCITY );
		entity->SaveData( "StorePredictionResults", predicted_frame, PC_EVERYTHING );
		
		//if we're keeping first frame results, copy them now
		if( m_Split[ nSlot ].m_bFirstTimePredicted && (entity->m_pIntermediateData_FirstPredicted[0] != nullptr) )
		{
			entity->m_nIntermediateData_FirstPredictedShiftMarker = predicted_frame + 1;
			memcpy( entity->m_pIntermediateData_FirstPredicted[predicted_frame + 1], entity->m_pIntermediateData[predicted_frame], entity->GetPredDescMap()->m_nPackedSize );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove - 
//			previous_last_slot - 
//-----------------------------------------------------------------------------
void CPrediction::ShiftIntermediateDataForward( int nSlot, int slots_to_remove, int number_of_commands_run )
{
	CVProfScope _( "CPrediction::ShiftIntermediateDataForward", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "shift" );

	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	int c = GetPredictables( nSlot )->GetPredictableCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent )
			continue;

		if ( !ent->GetPredictable() )
			continue;

		ent->ShiftIntermediateDataForward( slots_to_remove, number_of_commands_run );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove - 
//-----------------------------------------------------------------------------
void CPrediction::ShiftFirstPredictedIntermediateDataForward( int nSlot, int slots_to_remove )
{
	if( (slots_to_remove == 0) || (slots_to_remove >= MULTIPLAYER_BACKUP)  )
		return;

	CVProfScope _( "CPrediction::ShiftFirstPredictedIntermediateDataForward", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "shift" );

	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	int c = GetPredictables( nSlot )->GetPredictableCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent )
			continue;

		if ( !ent->GetPredictable() )
			continue;

		ent->ShiftFirstPredictedIntermediateDataForward( slots_to_remove );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : predicted_frame - 
//-----------------------------------------------------------------------------
void CPrediction::RestoreEntityToPredictedFrame( int nSlot, int predicted_frame )
{
	CVProfScope _( "CPrediction::RestoreEntityToPredictedFrame", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "restoretopred" );

	if ( !C_BasePlayer::GetLocalPlayer( nSlot ) )
		return;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	int c = GetPredictables( nSlot )->GetPredictableCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent )
			continue;

		if ( !ent->GetPredictable() )
			continue;

		ent->RestoreData( "RestoreEntityToPredictedFrame", predicted_frame, PC_EVERYTHING );
		ent->OnPostRestoreData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Computes starting destination for intermediate prediction data results and
//  does any fixups required by network optimization
// Input  : received_new_world_update - 
//			incoming_acknowledged - 
// Output : int
//-----------------------------------------------------------------------------
int CPrediction::ComputeFirstCommandToExecute( int nSlot, bool received_new_world_update, int incoming_acknowledged, int outgoing_command )
{
	int destination_slot = 1;
	int skipahead = 0;

	Split_t &split = m_Split[ nSlot ];

	// If we didn't receive a new update ( or we received an update that didn't ack any new CUserCmds -- 
	//  so for the player it should be just like receiving no update ), just jump right up to the very 
	//  last command we created for this very frame since we probably wouldn't have had any errors without 
	//  being notified by the server of such a case.
	// NOTE:  received_new_world_update only gets set to false if cl_pred_optimize >= 1
	if ( !received_new_world_update || !split.m_nServerCommandsAcknowledged )
	{
		if( !split.m_bPerformedTickShift )
		{
			// this is where we would normally start
			int start = incoming_acknowledged + 1;
			// outgoing_command is where we really want to start
			skipahead = std::max( 0, ( outgoing_command - start ) );
			// Don't start past the last predicted command, though, or we'll get prediction errors
			skipahead = std::min( skipahead, split.m_nCommandsPredicted );

			// Always restore since otherwise we might start prediction using an "interpolated" value instead of a purely predicted value
			RestoreEntityToPredictedFrame( nSlot, skipahead - 1 );

			//Msg( "%i/%i no world, skip to %i restore from slot %i\n", 
			//	gpGlobals->framecount,
			//	gpGlobals->tickcount,
			//	skipahead,
			//	skipahead - 1 );
		}
	}
	else
	{
		// Otherwise, there is a second optimization, wherein if we did receive an update, but no
		//  values differed (or were outside their epsilon) and the server actually acknowledged running
		//  one or more commands, then we can revert the entity to the predicted state from last frame, 
		//  shift the # of commands worth of intermediate state off of front the intermediate state array, and
		//  only predict the usercmd from the latest render frame.
		if ( cl_pred_optimize.GetInt() >= 2 && 
			!split.m_bPreviousAckHadErrors && 
			split.m_nCommandsPredicted > 0 && 
			split.m_nServerCommandsAcknowledged <= split.m_nCommandsPredicted &&
			!split.m_bPerformedTickShift )
		{
			// Copy all of the previously predicted data back into entity so we can skip repredicting it
			// This is the final slot that we previously predicted
			RestoreEntityToPredictedFrame( nSlot, split.m_nCommandsPredicted - 1 );

			// Shift intermediate state blocks down by # of commands ack'd
			ShiftIntermediateDataForward( nSlot, split.m_nServerCommandsAcknowledged, split.m_nCommandsPredicted );
			
			// Only predict new commands (note, this should be the same number that we could compute
			//  above based on outgoing_command - incoming_acknowledged - 1
			skipahead = ( split.m_nCommandsPredicted - split.m_nServerCommandsAcknowledged );

			//Msg( "%i/%i optimize2, skip to %i restore from slot %i\n", 
			//	gpGlobals->framecount,
			//	gpGlobals->tickcount,
			//	skipahead,
			//	split.m_nCommandsPredicted - 1 );
		}
		else
		{
			if( split.m_bPreviousAckHadErrors )
			{
				int count = GetPredictables( nSlot )->GetPredictableCount();
				for ( int i = 0; i < count; i++ )
				{
					C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
					if ( !ent )
						continue;

					ent->m_bEverHadPredictionErrorsForThisCommand = true; //this bool will get pulled forward until a new command is predicted for the first time
				}
			}

			if ( ( split.m_bPreviousAckHadErrors && cl_pred_doresetlatch.GetBool() ) || 
				cl_pred_doresetlatch.GetInt() == 2 )
			{
				// Both players should have == time base, etc.
				C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( nSlot );
				
				// If an entity gets a prediction error, then we want to clear out its interpolated variables
				// so we don't mix different samples at the same timestamps. We subtract 1 tick interval here because
				// if we don't, we'll have 3 interpolation entries with the same timestamp as this predicted
				// frame, so we won't be able to interpolate (which leads to jerky movement in the player when
				// ANY entity like your gun gets a prediction error).
				float flPrev = gpGlobals->curtime;
				gpGlobals->curtime = pLocalPlayer->GetTimeBase() - TICK_INTERVAL;

				if( split.m_bPreviousAckErrorTriggersFullLatchReset || (cl_pred_doresetlatch.GetInt() == 2) )
				{				
					for ( int i = 0; i < GetPredictables( nSlot )->GetPredictableCount(); i++ )
					{
						C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
						if ( entity )
						{
							entity->ResetLatched();
						}
					}
				}
				else
				{
					//individual latch resets
					for ( int i = 0; i < std::ssize( split.m_EntsWithPredictionErrorsInLastAck ); i++ )
					{
						C_BaseEntity *entity = split.m_EntsWithPredictionErrorsInLastAck[i];
						if( entity )
						{
							//ensure it's still in our predictable list
							for ( int j = 0; j < GetPredictables( nSlot )->GetPredictableCount(); j++ )
							{
								if( entity == GetPredictables( nSlot )->GetPredictable( j ) )
								{
									entity->ResetLatched();
									break;
								}
							}
						}
					}
				}

				gpGlobals->curtime = flPrev;
			}
		}
	}

	ShiftFirstPredictedIntermediateDataForward( nSlot, split.m_nServerCommandsAcknowledged );

	destination_slot += skipahead;

	// Always reset these values now that we handled them
	split.m_nCommandsPredicted	= 0;
	split.m_bPreviousAckHadErrors = false;
	split.m_nServerCommandsAcknowledged = 0;
	split.m_bPerformedTickShift = false;
	return destination_slot;
}

//-----------------------------------------------------------------------------
// Actually does the prediction work, returns false if an error occurred
//-----------------------------------------------------------------------------
bool CPrediction::PerformPrediction( int nSlot, C_BasePlayer *localPlayer, bool received_new_world_update, int incoming_acknowledged, int outgoing_command )
{
	MDLCACHE_CRITICAL_SECTION();
	CVProfScope _( "CPrediction::PerformPrediction", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	contract_assert( localPlayer );
	// This makes sure that we are allowed to sample the world when it may not be ready to be sampled
	contract_assert( C_BaseEntity::IsAbsQueriesValid() );
	contract_assert( C_BaseEntity::IsAbsRecomputationsEnabled() );

	// Start at command after last one server has processed and 
	//  go until we get to targettime or we run out of new commands
	int i = ComputeFirstCommandToExecute( nSlot, received_new_world_update, incoming_acknowledged, outgoing_command );

	contract_assert( i >= 1 );

	// This is a hack to get the CTriggerAutoGameMovement auto duck triggers to correctly deal with prediction.
	// Here we just untouch any triggers the player was touching (since we might have teleported the origin 
	// backward from it's previous position) and then re-touch any triggers it's currently in
	localPlayer->SetCheckUntouch( true );
	localPlayer->PhysicsCheckForEntityUntouch();
	localPlayer->PhysicsTouchTriggers();

	// undo interpolation changes for entities we stand on
	C_BaseEntity *ground = localPlayer->GetGroundEntity();

	while ( ground && ground->entindex() > 0 )
	{
		ground->MoveToLastReceivedPosition();
		ground = ground->GetMoveParent();
	}


	Split_t &split = m_Split[ nSlot ];
	
	physenv->DoneReferencingPreviousCommands( incoming_acknowledged - 1 );

	bool bTooMany = outgoing_command - incoming_acknowledged >= MULTIPLAYER_BACKUP;

	while ( !bTooMany )
	{
		// Incoming_acknowledged is the last usercmd the server acknowledged having acted upon
		int current_command		= incoming_acknowledged + i;

		// We've caught up to the current command.
		if ( current_command > outgoing_command )
			break;

		CUserCmd *cmd = input->GetUserCmd( nSlot, current_command );
		if ( !cmd )
		{
			bTooMany = true;
			break;	
		}

		contract_assert( i < MULTIPLAYER_BACKUP );

		// Is this the first time predicting this
		split.m_bFirstTimePredicted = !cmd->hasbeenpredicted;

		// Set globals appropriately
		float curtime		= ( localPlayer->m_nTickBase ) * TICK_INTERVAL;

		if( physenv->IsPredicted() )
		{
			physenv->SetPredictionCommandNum( current_command );

			if( (m_Split[ nSlot ].m_nCommandsPredicted == 0) && (i == 1) )
			{
				if( !m_Split[ nSlot ].m_bFirstTimePredicted )
				{
					for ( int j = 0; j < GetPredictables( nSlot )->GetPredictableCount(); j++ )
					{
						C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( j );

						if( entity->m_bEverHadPredictionErrorsForThisCommand )
						{
							// Always reset
							gpGlobals->curtime		= curtime;
							gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;				

							const byte *predictedFrame = (const byte *)entity->GetFirstPredictedFrame( 0 );
							entity->VPhysicsCompensateForPredictionErrors( predictedFrame );
						}
					}
				}
				//else - if lag is so low that we only predict every command once, do we need to keep around old data to base our corrections on?
			}
		}

		if( !cmd->hasbeenpredicted )
		{
			int count = GetPredictables( nSlot )->GetPredictableCount();
			for ( int i = 0; i < count; ++i )
			{
				C_BaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );

				if( entity )
				{
					entity->m_bEverHadPredictionErrorsForThisCommand = false;
				}
			}
		}

		RunSimulation( current_command, curtime, cmd, localPlayer );

		gpGlobals->curtime		= curtime;
		gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;

		// Call untouch on any entities no longer predicted to be touching
		Untouch( nSlot );

		// Store intermediate data into appropriate slot
		StorePredictionResults( nSlot, i - 1 ); // Note that I starts at 1

		split.m_nCommandsPredicted = i;

		if ( current_command == outgoing_command )
		{
			localPlayer->m_nFinalPredictedTick = localPlayer->m_nTickBase;
		}

		// Mark that we issued any needed sounds, of not done already
		cmd->hasbeenpredicted = true;

		// Copy the state over.
		i++;
	}

	// Somehow we looped past the end of the list (severe lag), don't predict at all
	return !bTooMany;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startframe - 
//			validframe - 
//			incoming_acknowledged - 
//			outgoing_command - 
//-----------------------------------------------------------------------------
void CPrediction::Update( int startframe, bool validframe, 
						 int incoming_acknowledged, int outgoing_command )
{
	CVProfScope _( "CPrediction::Update", 0, VPROF_BUDGETGROUP_PREDICTION, false, BUDGETFLAG_OTHER );

	m_bEnginePaused = engine->IsPaused();

	bool received_new_world_update = true;

	// HACK!
	float flTimeStamp = engine->GetLastTimeStamp();
	bool bTimeStampChanged = m_flLastServerWorldTimeStamp != flTimeStamp;
	m_flLastServerWorldTimeStamp = flTimeStamp;

	// Still starting at same frame, so make sure we don't do extra prediction ,etc.
	if ( ( m_nPreviousStartFrame == startframe ) && 
		cl_pred_optimize.GetBool() &&
		cl_predict->GetInt() && bTimeStampChanged )
	{
		received_new_world_update = false;
	}

	m_nPreviousStartFrame = startframe;

	// Save off current timer values, etc.
	m_SavedVars = *gpGlobals;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );
		_Update( nSlot, received_new_world_update, validframe, incoming_acknowledged, outgoing_command );
	}

	// Restore current timer values, etc.
	*gpGlobals = m_SavedVars;
}

//-----------------------------------------------------------------------------
// Do the dirty deed of predicting the local player
//-----------------------------------------------------------------------------
void CPrediction::_Update( int nSlot, bool received_new_world_update, bool validframe, 
						 int incoming_acknowledged, int outgoing_command )
{

	QAngle viewangles;

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer( nSlot );
	if ( !localPlayer )
		return;

	m_Split[nSlot].m_nLastCommandAcknowledged = incoming_acknowledged;

	// Always using current view angles no matter what
	// NOTE: ViewAngles are always interpreted as being *relative* to the player
	engine->GetViewAngles( viewangles );
	localPlayer->SetLocalAngles( viewangles );

	if ( !validframe )
	{
		return;
	}

	// If we are not doing prediction, copy authoritative value into velocity and angle.
	if ( !cl_predict->GetInt() )
	{
		// When not predicting, we at least must make sure the player
		// view angles match the view angles...
		localPlayer->SetLocalViewAngles( viewangles );
		return;
	}

	// This is cheesy, but if we have entities that are parented to attachments on other entities, then 
	// it'll wind up needing to get a bone transform.
	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );

		// Invalidate bone cache on predictables
		int c = GetPredictables( nSlot )->GetPredictableCount();
		for ( int i = 0; i < c; ++i )
		{
			C_BaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent || 
				 !ent->GetBaseAnimating() || 
				 !ent->GetPredictable() )
				continue;
			static_cast< C_BaseAnimating * >( ent )->InvalidateBoneCache();
		}

		// Remove any purely client predicted entities that were left "dangling" because the 
		//  server didn't acknowledge them or which can now safely be removed
		RemoveStalePredictedEntities( nSlot, incoming_acknowledged );
		// Restore objects back to "pristine" state from last network/world state update
		if ( received_new_world_update )
		{
			RestoreOriginalEntityState( nSlot );
		}

		m_bInPrediction = true;
		bool bValid = PerformPrediction( nSlot, localPlayer, received_new_world_update, incoming_acknowledged, outgoing_command );
		m_bInPrediction = false;
		if ( !bValid )
		{
			return;
		}
	}

	// Overwrite predicted angles with the actual view angles
	localPlayer->SetLocalAngles( viewangles );

	// This allows us to sample the world when it may not be ready to be sampled
	contract_assert( C_BaseEntity::IsAbsQueriesValid() );
	

}



//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrediction::IsFirstTimePredicted( void ) const
{
	return m_Split[ GET_ACTIVE_SPLITSCREEN_SLOT() ].m_bFirstTimePredicted;
}



//-----------------------------------------------------------------------------
// Purpose: For verifying/fixing operations that don't save/load in a datatable very well
// Output : Returns the how many commands the server has processed and sent results for
//-----------------------------------------------------------------------------
int CPrediction::GetLastAcknowledgedCommandNumber( void ) const
{
	return m_Split[ GET_ACTIVE_SPLITSCREEN_SLOT() ].m_nLastCommandAcknowledged;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CPrediction::GetViewOrigin( Vector& org )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
	{
		org.Init();
	}
	else 
	{
		org = player->GetLocalOrigin();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CPrediction::SetViewOrigin( Vector& org )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	player->SetLocalOrigin( org );
	player->m_vecNetworkOrigin = org;

	player->m_iv_vecOrigin.Reset( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::GetViewAngles( QAngle& ang )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
	{
		ang.Init();
	}
	else 
	{
		ang = player->GetLocalAngles();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::SetViewAngles( QAngle& ang )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	player->SetViewAngles( ang );
	player->m_iv_angRotation.Reset( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::GetLocalViewAngles( QAngle& ang )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
	{
		ang.Init();
	}
	else 
	{
		ang = player->pl.v_angle;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::SetLocalViewAngles( QAngle& ang )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	player->SetLocalViewAngles( ang );
}

//-----------------------------------------------------------------------------
// Purpose: For determining that predicted creation entities are un-acked and should
//  be deleted
// Output : int
//-----------------------------------------------------------------------------
int CPrediction::GetIncomingPacketNumber( void ) const
{
	return m_nIncomingPacketNumber;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrediction::InPrediction( void ) const
{
	return m_bInPrediction;
}

float CPrediction::GetSavedTime() const
{
	return m_SavedVars.curtime;
}
