#ifndef KISAKSTRIKE_RKHUD_SCOPE_H
#define KISAKSTRIKE_RKHUD_SCOPE_H

#include <rocketui/rocketui.h>
#include "hudelement.h"

extern ConVar cl_drawhud;

class RkHudScope : public CHudElement {
public:
    explicit RkHudScope(const char *pElementName);
    virtual ~RkHudScope();

    void LevelInit(void);
    virtual void LevelShutdown(void);
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

    Rml::ElementDocument *m_pDocument;
    bool m_bVisible;

    // Fill elements (black areas around scope viewport)
    Rml::Element *m_fillTop;
    Rml::Element *m_fillBottom;
    Rml::Element *m_fillLeft;
    Rml::Element *m_fillRight;

    // Crosshair lines
    Rml::Element *m_lineH;
    Rml::Element *m_lineV;

private:
    void UpdateScope();

    // Animation state
    float m_fAnimInset;
    float m_fLineSpreadDistance;
};

#endif //KISAKSTRIKE_RKHUD_SCOPE_H
