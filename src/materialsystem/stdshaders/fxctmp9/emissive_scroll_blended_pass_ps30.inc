#pragma once
// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class emissive_scroll_blended_pass_ps30_Static_Index
{
public:
	// CONSTRUCTOR
	emissive_scroll_blended_pass_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
inline constexpr int shaderStaticTest_emissive_scroll_blended_pass_ps30 = 0;
class emissive_scroll_blended_pass_ps30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	emissive_scroll_blended_pass_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
inline constexpr int shaderDynamicTest_emissive_scroll_blended_pass_ps30 = 0;
static const ShaderComboSemantics_t emissive_scroll_blended_pass_ps30_combos =
{
	"emissive_scroll_blended_pass_ps30", nullptr, 0, nullptr, 0 
};

class ConstructMe_emissive_scroll_blended_pass_ps30
{
public:
	ConstructMe_emissive_scroll_blended_pass_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &emissive_scroll_blended_pass_ps30_combos );
	}
};

static ConstructMe_emissive_scroll_blended_pass_ps30 s_ConstructMe_emissive_scroll_blended_pass_ps30;
