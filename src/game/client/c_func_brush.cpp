#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_func_brush.h"
#include "toolframework_client.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_FuncBrush, DT_FuncBrush, CFuncBrush )

void C_FuncBrush::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	VPhysicsShadowDataChanged(bCreate, this);
}
