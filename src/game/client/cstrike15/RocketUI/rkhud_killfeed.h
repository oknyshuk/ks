#ifndef KISAKSTRIKE_RKHUD_KILLFEED_H
#define KISAKSTRIKE_RKHUD_KILLFEED_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudKillfeed : public RkHudDocument<RkHudKillfeed>
{
public:
    static const char *kDocument;

    explicit RkHudKillfeed( const char *value );
    virtual ~RkHudKillfeed();

    // Overrides from CHudElement
    void LevelInit() override;
    void LevelShutdown() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();

    // CGameEventListener
    virtual void FireGameEvent( IGameEvent *event );



private:
    void CheckForOldEntries();
    void OnPlayerDeath( IGameEvent *event );
};

#endif //KISAKSTRIKE_RKHUD_KILLFEED_H
