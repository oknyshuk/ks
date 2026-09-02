#ifndef KISAKSTRIKE_RKHUD_INFOBAR_H
#define KISAKSTRIKE_RKHUD_INFOBAR_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudInfoBar : public RkHudDocument<RkHudInfoBar> {
public:
    static const char *kDocument;

    explicit RkHudInfoBar(const char *value);
    virtual ~RkHudInfoBar();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);


};


#endif //KISAKSTRIKE_RKHUD_INFOBAR_H
