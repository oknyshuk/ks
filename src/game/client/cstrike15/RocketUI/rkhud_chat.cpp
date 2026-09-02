#include "rkhud_chat.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "cs_gamerules.h"
#include "hud_macros.h"
#include "text_message.h"
#include "c_cs_playerresource.h"
#include "localize/ilocalize.h"

#include <rocketui/rmlui.h>

#include "rkpanel_popup.h"

DECLARE_HUDELEMENT( RkHudChat );

const char *RkHudChat::kDocument = "hud_chat.rml";

ConVar rocket_hud_chat_idle_opacity( "rocket_hud_chat_idle_opacity", "0.2", FCVAR_ARCHIVE, "The Opacity of the Chat while it is not active" );
ConVar rocket_hud_chat_active_opacity( "rocket_hud_chat_active_opacity", "0.7", FCVAR_ARCHIVE, "The Opacity of the Chat while typing/new message" );
ConVar rocket_hud_chat_max_entries( "rocket_hud_chat_max_entries", "1000", FCVAR_ARCHIVE, "Chat History Length" );

CON_COMMAND_F( rocket_hud_chat_clear, "Clears the Chat History", FCVAR_NONE )
{
    RkHudChat* pChat = GET_HUDELEMENT( RkHudChat );
    if( !pChat )
        return;
    pChat->ClearChatHistory();
}

static bool __MsgFunc_SayText2( const CCSUsrMsg_SayText2 &msg )
{
    int paramSize = msg.params_size();
    int entID = msg.ent_idx();

    RkHudChat* pChat = GET_HUDELEMENT( RkHudChat );

    if( !pChat )
        return true;

    // from server
    if( entID == 0 && paramSize > 1 )
    {
        pChat->AddChatString( nullptr, msg.params(1).c_str(), RkHudChat::SERVER );
        return true;
    }

    CBasePlayer *speaker = UTIL_PlayerByIndex( entID );
    CBasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer( );

    if( !speaker || !localPlayer )
        return true;

    // params(0) = Name of player
    // params(1) = Message
    if( paramSize > 1 )
    {
        if( (speaker->GetTeamNumber() == localPlayer->GetTeamNumber()) )
            pChat->AddChatString( msg.params(0).c_str(), msg.params(1).c_str(),RkHudChat::FRIEND);
        else
            pChat->AddChatString( msg.params(0).c_str(), msg.params(1).c_str(),RkHudChat::FOE);
    }

    return true;
}
// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
static char* ConvertCRtoNL( char *str )
{
    for ( char *ch = str; *ch != 0; ch++ )
        if ( *ch == '\r' )
            *ch = '\n';
    return str;
}

static void NullLastNewlineFromString( char *str )
{
    int s = strlen( str ) - 1;
    if ( s >= 0 )
    {
        if ( str[s] == '\n' || str[s] == '\r' )
            str[s] = 0;
    }
}

//-----------------------------------------------------------------------------
// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message destination  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next ( optional) one to four strings are parameters for that string ( which can also be message names if they begin with '#')
//-----------------------------------------------------------------------------
static bool __MsgFunc_TextMsg( const CCSUsrMsg_TextMsg &msg )
{
    char szString[2048] = {0};
    int dest = msg.msg_dst();

    wchar_t szBuf[5][256] = {};
    wchar_t outputBuf[256] = {};

    if( !cl_showtextmsg.GetBool() )
        return true;

    for( int i = 0; i < 4; i++ )
    {
        // Allow localizing player names
        if ( const char *pszEntIndex = StringAfterPrefix( msg.params(i).c_str(), "#ENTNAME[" ) )
        {
            int iEntIndex = V_atoi( pszEntIndex );
            wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
            if ( C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* ) GameResources() )
            {
                pCSPR->GetDecoratedPlayerName( iEntIndex, wszPlayerName, sizeof( wszPlayerName ), ( EDecoratedPlayerNameFlag_t ) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
            }
            if ( wszPlayerName[0] )
            {
                szString[0] = 0;
                V_wcscpy_safe( szBuf[ i ], wszPlayerName );
            }
            else if ( const char *pszEndBracket = V_strnchr( pszEntIndex, ']', 64 ) )
            {
                V_strcpy_safe( szString, pszEndBracket + 1 );
            }
            else
            {
                V_strcpy_safe( szString, msg.params(i).c_str() );
            }
        }
        else
        {
            V_strcpy_safe( szString, msg.params(i).c_str() );
        }

        if( szString[0] )
        {
            static char tmpStrBuf[1024];
            V_strncpy( tmpStrBuf, hudtextmessage->LookupString( szString, &dest ), sizeof(tmpStrBuf) );
            bool bTranslated = false;
            if( tmpStrBuf[0] == '#' ) // only translate parameters intended as localization tokens
            {
                const wchar_t *pBuf = g_pLocalize->Find( tmpStrBuf );
                if( pBuf )
                {
                    // copy pBuf into szBuf[i]
                    int nMaxChars = sizeof( szBuf[ i ] ) / sizeof( wchar_t );
                    wcsncpy( szBuf[ i ], pBuf, nMaxChars );
                    szBuf[ i ][ nMaxChars - 1 ] = 0;
                    bTranslated = true;
                }
            }

            if( !bTranslated )
            {
                if( i > 0 )
                {
                    NullLastNewlineFromString( tmpStrBuf );  // these strings are meant for substitution into the main strings, so cull the automatic end newlines
                }
                g_pLocalize->ConvertANSIToUnicode( tmpStrBuf, szBuf[ i ], sizeof( szBuf[ i ] ) );
            }
        }
    }

    //TODO: right now this is just ascii
    int len;
    RkHudChat* pChat;
    g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
    g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
    len = strlen( szString );
    if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
    {
        Q_strncat( szString, "\n", sizeof( szString), 1 );
    }

    switch( dest )
    {
        case HUD_PRINTCENTER:
            // Center popup with RocketUI that deletes itself after 4.5 seconds
            new RocketPopupDocument( ConvertCRtoNL( szString ), 4.5f );
            break;
        case HUD_PRINTTALK:
            pChat = GET_HUDELEMENT( RkHudChat );
            if( !pChat )
                return true;

            pChat->AddChatString( nullptr, ConvertCRtoNL( szString ), RkHudChat::SERVER );
            break;
        case HUD_PRINTNOTIFY:
        case HUD_PRINTCONSOLE:
            Msg( "%s", ConvertCRtoNL( szString ) );
            break;
    }

    return true;
}

// hud_chat.rml routes its input's keydown here (data-event-keydown). Enter sends,
// escape drops out of message mode; every other key belongs to the text field.
// One line of chat. The sender only picks a colour, so it travels as the class
// name hud_chat.rml should put on the username.
struct ChatLine
{
    Rml::String username;
    Rml::String message;
    Rml::String sender_class;

    bool operator==( const ChatLine & ) const = default;
};
struct ChatData
{
    Rml::Vector<ChatLine> lines;

    bool operator==( const ChatData & ) const = default;
} chatData;

static void BindChat( Rml::DataModelConstructor &c )
{
    if ( auto line = c.RegisterStruct<ChatLine>() )
    {
        line.RegisterMember( "username", &ChatLine::username );
        line.RegisterMember( "message", &ChatLine::message );
        line.RegisterMember( "sender_class", &ChatLine::sender_class );
    }
    c.RegisterArray<Rml::Vector<ChatLine>>();

    if ( auto h = c.RegisterStruct<ChatData>() )
        h.RegisterMember( "lines", &ChatData::lines );
    c.Bind( "chat", &chatData );

    c.BindEventCallback( "chat_key", []( Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList & ) {
        RkHudChat *pChat = GET_HUDELEMENT( RkHudChat );
        if ( !pChat || !pChat->m_elemChatInput )
            return;

        const auto key = (Rml::Input::KeyIdentifier)ev.GetParameter<int>( "key_identifier", 0 );

        if ( key == Rml::Input::KI_RETURN )
        {
            auto *input = static_cast<Rml::ElementFormControl *>( pChat->m_elemChatInput );

            char sayBuffer[1024];
            V_snprintf( sayBuffer, sizeof( sayBuffer ), "%s \"%s\"",
                        pChat->GetMessageMode() == MM_SAY ? "say" : "say_team",
                        input->GetValue().c_str() );
            engine->ClientCmd_Unrestricted( sayBuffer );
            input->SetValue( "" );
        }
        else if ( key != Rml::Input::KI_ESCAPE )
        {
            return;
        }

        pChat->StopMessageMode();
    } );
}
RK_HUD_SECTION( BindChat );

// Called on program startup by a Macro with the HUD UI system

void RkHudChat::OnLoad()
{
    m_elemChatInput = m_pDocument->GetElementById( "chat_input" );
    if ( !m_elemChatInput )
    {
        Warning( "hud_chat.rml is missing 'chat_input'\n" );
        return;
    }

    // The chat is always on screen; only its opacity and input focus change.
    m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
}

void RkHudChat::OnUnload()
{
    if ( m_elemChatInput )
    m_elemChatInput = nullptr;
}

// HudElementHelper::CreateAllElements
RkHudChat::RkHudChat(const char *value) : RkHudDocument( value ),
    m_iMode( MM_NONE )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

// Chat's document is visible during normal gameplay (it draws chat history), so
// only actually typing counts as owning input.
RkHudChat::~RkHudChat() noexcept
{
    Unload();
}

void RkHudChat::LevelInit()
{
    HOOK_MESSAGE( SayText2 );
    HOOK_MESSAGE( TextMsg );

    RkHudDocument::LevelInit();
}

void RkHudChat::LevelShutdown()
{
    m_iMode = MM_NONE;

    RkHudDocument::LevelShutdown();
}

// this is called every frame, keep that in mind.
void RkHudChat::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        m_pDocument->SetProperty( "opacity", std::to_string(rocket_hud_chat_active_opacity.GetFloat()) );
    }
    else
    {
        // Do a fade out to the idle opacity level.
        float currentOpacity = m_pDocument->GetProperty("opacity")->Get<float>();
        if( currentOpacity > rocket_hud_chat_idle_opacity.GetFloat() )
            m_pDocument->SetProperty("opacity", std::to_string(currentOpacity - 0.0075f) );
    }
}

// Called every Frame by the hudsystem if ShouldDraw()
void RkHudChat::SetActive( bool bActive )
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudChat::ShouldDraw( void )
{
    if ( //IsTakingAFreezecamScreenshot() ||
            (CSGameRules() && CSGameRules()->IsPlayingTraining()) )
        return false;

    return cl_drawhud.GetBool() && (m_iMode != MM_NONE ) && CHudElement::ShouldDraw();
}

void RkHudChat::StartMessageMode( int mode )
{
    // Already in chat mode.
    if( ChatRaised() )
        return;
    
    if( GetHud().HudDisabled() )
        return;

    m_iMode = mode;

    // Claim the mouse for as long as we are typing (hud_chat.rml is otherwise
    // `pointer-events: none`, so it does not).
    m_pDocument->SetProperty( "pointer-events", "auto" );
    m_elemChatInput->Focus();
}

void RkHudChat::StopMessageMode()
{
    m_iMode = MM_NONE;

    m_pDocument->SetProperty( "pointer-events", "none" );
    m_elemChatInput->Blur();
}

bool RkHudChat::ChatRaised()
{
    return m_iMode != MM_NONE;
}

void RkHudChat::ClearChatHistory()
{
    chatData.lines.clear();
    RkHudDirty( "chat" );
}

void RkHudChat::AddChatString( const char *username, const char *message, MessageSender sender )
{
    if ( !message )
        return;

    ChatLine line;
    line.username = username ? ( Rml::String( username ) + ": " ) : Rml::String();
    line.message = message;
    switch ( sender )
    {
    case RkHudChat::SERVER: line.sender_class = "chat_username_server"; break;
    case RkHudChat::FRIEND: line.sender_class = "chat_username_friend"; break;
    case RkHudChat::FOE:    line.sender_class = "chat_username_foe"; break;
    }

    chatData.lines.push_back( line );

    // FIFO scrollback. The old version tore down *every* line once the cap was
    // reached; this drops just the oldest.
    const int cap = MAX( 1, rocket_hud_chat_max_entries.GetInt() );
    while ( (int)chatData.lines.size() > cap )
        chatData.lines.erase( chatData.lines.begin() );

    RkHudDirty( "chat" );

    // The document has to lay the new line out before its height can be scrolled
    // to. Only possible once the document exists -- messages can arrive before it.
    if ( !m_pDocument )
        return;

    m_pDocument->UpdateDocument();
    m_pDocument->SetScrollTop( m_pDocument->GetScrollHeight() - m_pDocument->GetClientHeight() );
}

void RkHudChat::AddChatString( const wchar_t *username, const wchar_t *message, MessageSender sender )
{
    //TODO: wide strings.
}
