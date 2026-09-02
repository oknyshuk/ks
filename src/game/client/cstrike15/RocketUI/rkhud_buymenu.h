#ifndef KISAKSTRIKE_RKHUD_BUYMENU_H
#define KISAKSTRIKE_RKHUD_BUYMENU_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudBuyMenu : public RkHudDocument<RkHudBuyMenu>
{
public:
    static const char *kDocument;

    explicit RkHudBuyMenu( const char * value );
    virtual ~RkHudBuyMenu();

    // Overrides from CHudElement
    void LevelInit() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

    // CGameEventListener
    virtual void FireGameEvent( IGameEvent *event );


    void UpdateBuyMenu();
    void OnNewFrameBuyMenu();
};

#endif //KISAKSTRIKE_RKHUD_BUYMENU_H
