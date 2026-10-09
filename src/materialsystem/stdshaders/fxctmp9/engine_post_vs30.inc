#pragma once
// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class engine_post_vs30_Static_Index
{
public:
	// CONSTRUCTOR
	engine_post_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
inline constexpr int shaderStaticTest_engine_post_vs30 = 0;
class engine_post_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	engine_post_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
inline constexpr int shaderDynamicTest_engine_post_vs30 = 0;
static const ShaderComboSemantics_t engine_post_vs30_combos =
{
	"engine_post_vs30", nullptr, 0, nullptr, 0 
};

class ConstructMe_engine_post_vs30
{
public:
	ConstructMe_engine_post_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &engine_post_vs30_combos );
	}
};

static ConstructMe_engine_post_vs30 s_ConstructMe_engine_post_vs30;
