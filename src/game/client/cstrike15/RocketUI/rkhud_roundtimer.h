#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include <rocketui/rmlui.h>

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
