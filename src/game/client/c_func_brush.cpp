#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "physics.h"
#include "reflect_recvtable.h"
#include "c_func_brush.h"
#include "toolframework_client.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_FuncBrush, DT_FuncBrush, CFuncBrush )

void C_FuncBrush::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	VPhysicsShadowDataChanged(bCreate, this);
}
