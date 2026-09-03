#ifndef KISAKSTRIKE_RKHUD_RADAR_H
#define KISAKSTRIKE_RKHUD_RADAR_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"

#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

struct SpottedInfo
{
    // info from ProcessSpottedEntityUpdate
    int entId;
    int classId;
    int originX;
    int originY;
    int originZ;
    int angleYaw;
    bool defuser;
    bool playerWithDefuser;
    bool playerWithC4;

    float timelastSpotted;
};

class RkHudRadar : public RkHudDocument<RkHudRadar>
{
public:
    static const char *kDocument;

    explicit RkHudRadar( const char *value );
    virtual ~RkHudRadar();

    // Overrides from CHudElement
    void LevelInit() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();

    // Hooked msg
    bool MsgFunc_ProcessSpottedEntityUpdate( const ks::net::CCSUsrMsg_ProcessSpottedEntityUpdate &msg );

    void UpdateRadarFrame();
    void UpdateRadarSize();


    float           m_radarWidth;
    float           m_radarHeight;
    float           m_radarCenterX;
    float           m_radarCenterY;
    SpottedInfo     m_spottedPlayers[MAX_PLAYERS];
    SpottedInfo     m_spottedDefuser;
    SpottedInfo     m_spottedC4;

    CUserMessageBinder m_UMCMsgProcessSpottedEntityUpdate;
};


#endif //KISAKSTRIKE_RKHUD_RADAR_H
