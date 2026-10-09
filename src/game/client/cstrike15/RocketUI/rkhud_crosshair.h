#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include <rocketui/rmlui.h>


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

