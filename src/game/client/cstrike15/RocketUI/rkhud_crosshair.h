// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKHUD_CROSSHAIR_H
#define KISAKSTRIKE_RKHUD_CROSSHAIR_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudCrosshair : public RkHudDocument<RkHudCrosshair> {
public:
    static const char *kDocument;

    explicit RkHudCrosshair(const char *pElementName);
    virtual ~RkHudCrosshair();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);


private:
    void UpdateCrosshair();
};

#endif //KISAKSTRIKE_RKHUD_CROSSHAIR_H
