#pragma once

#include <RmlUi/Core/Input.h>
#include <inputsystem/ButtonCode.h>

// ButtonCode_t -> RmlUi key identifier.
//
// The digits, letters, keypad digits and function keys are each a contiguous run
// in *both* enums, so they fold into four range checks. What is left is
// punctuation and navigation, which follows no pattern in either enum and stays
// an explicit table.
inline Rml::Input::KeyIdentifier ButtonToRocketKey( ButtonCode_t button )
{
    using namespace Rml::Input;

    if ( button >= KEY_0 && button <= KEY_9 )
        return KeyIdentifier( KI_0 + ( button - KEY_0 ) );
    if ( button >= KEY_A && button <= KEY_Z )
        return KeyIdentifier( KI_A + ( button - KEY_A ) );
    if ( button >= KEY_PAD_0 && button <= KEY_PAD_9 )
        return KeyIdentifier( KI_NUMPAD0 + ( button - KEY_PAD_0 ) );
    if ( button >= KEY_F1 && button <= KEY_F12 )
        return KeyIdentifier( KI_F1 + ( button - KEY_F1 ) );

    struct Mapping
    {
        ButtonCode_t button;
        KeyIdentifier key;
    };
    static constexpr Mapping kRest[] = {
        { KEY_PAD_DIVIDE, KI_DIVIDE },     { KEY_PAD_MULTIPLY, KI_MULTIPLY },
        { KEY_PAD_MINUS, KI_SUBTRACT },    { KEY_PAD_PLUS, KI_ADD },
        { KEY_PAD_ENTER, KI_NUMPADENTER }, { KEY_PAD_DECIMAL, KI_DECIMAL },
        { KEY_LBRACKET, KI_OEM_4 },        { KEY_RBRACKET, KI_OEM_6 },
        { KEY_SEMICOLON, KI_OEM_1 },       { KEY_APOSTROPHE, KI_OEM_7 },
        { KEY_BACKQUOTE, KI_OEM_3 },       { KEY_COMMA, KI_OEM_COMMA },
        { KEY_PERIOD, KI_OEM_PERIOD },     { KEY_SLASH, KI_OEM_2 },
        { KEY_BACKSLASH, KI_OEM_5 },       { KEY_MINUS, KI_OEM_MINUS },
        { KEY_EQUAL, KI_OEM_PLUS },        { KEY_ENTER, KI_RETURN },
        { KEY_SPACE, KI_SPACE },           { KEY_BACKSPACE, KI_BACK },
        { KEY_TAB, KI_TAB },               { KEY_ESCAPE, KI_ESCAPE },
        { KEY_INSERT, KI_INSERT },         { KEY_DELETE, KI_DELETE },
        { KEY_HOME, KI_HOME },             { KEY_END, KI_END },
        { KEY_PAGEUP, KI_PRIOR },          { KEY_PAGEDOWN, KI_NEXT },
        { KEY_BREAK, KI_PAUSE },           { KEY_UP, KI_UP },
        { KEY_LEFT, KI_LEFT },             { KEY_DOWN, KI_DOWN },
        { KEY_RIGHT, KI_RIGHT },
    };

    for ( const Mapping &entry : kRest )
        if ( entry.button == button )
            return entry.key;

    return KI_UNKNOWN;
}
