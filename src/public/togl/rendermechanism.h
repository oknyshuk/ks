
#pragma once


// Undefine Source Engine macros that DXVK's windows_base.h will redefine
#undef TRUE
#undef FALSE
#undef WAIT_OBJECT_0
#undef WAIT_ABANDONED
#undef WAIT_TIMEOUT
#undef WAIT_FAILED

#include "tier0/commonmacros.h"
#pragma push_macro("ARRAYSIZE")
#undef ARRAYSIZE

#include <d3d9.h>
#include "togl/dxabstract.h"
#include "togl/dxabstract_types.h"

#pragma pop_macro("ARRAYSIZE")

typedef void* VD3DHWND;
typedef void* VD3DHANDLE;


#define	GLMPRINTF(args)
#define	GLMPRINTSTR(args)
#define	GLMPRINTTEXT(args)
