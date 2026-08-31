#ifndef KISAKSTRIKE_RKHUD_BUYMENU_H
#define KISAKSTRIKE_RKHUD_BUYMENU_H

#include <rocketui/rocketui.h>
#include "hudelement.h"

#include <RmlUi/Core/DataModelHandle.h>

extern ConVar cl_drawhud;

class RkHudBuyMenu : public CHudElement
{
public:
    explicit RkHudBuyMenu( const char * value );
    virtual ~RkHudBuyMenu();

    // Overrides from CHudElement
    void LevelInit(void);
    virtual void LevelShutdown(void);
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    // True while this panel legitimately owns mouse/keyboard input (polled).
    bool OwnsInput() const;

    // CGameEventListener
    virtual void FireGameEvent( IGameEvent *event );

    Rml::ElementDocument *m_pInstance;
    bool		m_bVisible;
    Rml::DataModelHandle m_dataModel;

    void UpdateBuyMenu();
    void OnNewFrameBuyMenu();
};

#endif //KISAKSTRIKE_RKHUD_BUYMENU_H
