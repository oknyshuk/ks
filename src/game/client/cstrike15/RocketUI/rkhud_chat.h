#ifndef KISAKSTRIKE_HUD_CHAT_H
#define KISAKSTRIKE_HUD_CHAT_H

#include "rkhud_document.h"
#include "iclientmode.h" // message mode enum

#include <rocketui/rocketui.h>

extern ConVar cl_drawhud;
extern ConVar cl_showtextmsg;

class RkHudChat : public RkHudDocument<RkHudChat> {
public:
    static const char *kDocument;

    enum MessageSender
    {
        SERVER,
        FRIEND,
        FOE,
    };
    explicit RkHudChat(const char *value);
    virtual ~RkHudChat();

    bool ChatRaised();

    void AddChatString( const char *username, const char *message, MessageSender sender );
    void AddChatString( const wchar_t *username, const wchar_t *message, MessageSender sender );
    void ClearChatHistory();

    // Starts the typing sequence.
    void StartMessageMode(int mode);
    void StopMessageMode();
    int GetMessageMode()
    {
        return m_iMode;
    }

    // Overrides from CHudElement
    void LevelInit() override;
    void LevelShutdown() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();
    void OnUnload();

    Rml::Element *m_elemChatInput;

    int			m_iMode;

    CUserMessageBinder m_UMCMsgSayText2;
    CUserMessageBinder m_UMCMsgTextMsg;
};

#endif //KISAKSTRIKE_HUD_CHAT_H
