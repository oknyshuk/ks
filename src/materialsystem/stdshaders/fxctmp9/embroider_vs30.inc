#pragma once
// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class embroider_vs30_Static_Index
{
public:
	// CONSTRUCTOR
	embroider_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
inline constexpr int shaderStaticTest_embroider_vs30 = 0;
class embroider_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	embroider_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
inline constexpr int shaderDynamicTest_embroider_vs30 = 0;
static const ShaderComboSemantics_t embroider_vs30_combos =
{
	"embroider_vs30", nullptr, 0, nullptr, 0 
};

class ConstructMe_embroider_vs30
{
public:
	ConstructMe_embroider_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &embroider_vs30_combos );
	}
};

static ConstructMe_embroider_vs30 s_ConstructMe_embroider_vs30;
