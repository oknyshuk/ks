#ifndef KISAKSTRIKE_RKHUD_ROUNDTIMER_H
#define KISAKSTRIKE_RKHUD_ROUNDTIMER_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudRoundTimer : public RkHudDocument<RkHudRoundTimer> {
public:
    static const char *kDocument;

    explicit RkHudRoundTimer( const char *value );
    virtual ~RkHudRoundTimer();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

};
#endif //KISAKSTRIKE_RKHUD_ROUNDTIMER_H
