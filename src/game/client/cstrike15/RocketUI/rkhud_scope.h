#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include <rocketui/rmlui.h>


#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudScope : public RkHudDocument<RkHudScope> {
public:
    static const char *kDocument;

    explicit RkHudScope(const char *pElementName);
    virtual ~RkHudScope();

    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);


private:
    void UpdateScope();

    // Animation state
    float m_fAnimInset;
    float m_fLineSpreadDistance;
};

