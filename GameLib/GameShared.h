#pragma once

#include "ArtTools.h"

class ITrainerLoadController;

extern unsigned int g_uFont;


struct LOCALPLAYERDATA : public TourDeGiroLocalPlayer  // represents every to do with a local player on our machine
{
  LOCALPLAYERDATA(string strPlayerName, float flMassKg, POWERTYPE ePowerType, CAMERASTYLE eCamStyle)
    : strPlayerName(strPlayerName),
      flMassKg(flMassKg),
      idLocal(-1),
      idViewing(-1),
      m_sLastPower(0),
      flRoadPos(0),
      flPowerSum(0),
      flTimeSum(0),
      tmLastSample(0),
      m_ePowerType(ePowerType),
      m_eCameraStyle(eCamStyle),
      m_tmLastBackup(0),
      m_fLastPowerWasError(false)
  {

  }
  virtual ~LOCALPLAYERDATA()
  {
  };

  virtual CAMERASTYLE GetCameraStyle() const ARTOVERRIDE
  {
    return m_eCameraStyle;
  }
  virtual POWERTYPE GetPowerType() const ARTOVERRIDE
  {
    return m_ePowerType;
  }
  virtual RECTF GetViewport() const ARTOVERRIDE
  {
    return rcViewport;
  }
  virtual int GetViewingId() const ARTOVERRIDE
  {
    return idViewing;
  }
  virtual int GetLocalId() const ARTOVERRIDE
  {
    return idLocal;
  }
  virtual float GetR() const ARTOVERRIDE
  {
    return m_r;
  }
  virtual float GetG() const ARTOVERRIDE
  {
    return m_g;
  }
  virtual float GetB() const ARTOVERRIDE
  {
    return m_b;
  }
  
  virtual void SetSpectateDistance(float fl)
  {
    this->flRoadPos = fl;
  }
  virtual float GetSpectateDistance() const
  {
    return flRoadPos;
  }
  void SetViewport(const RECTF& rc)
  {
    rcViewport = rc;
  }
  void AddPower(unsigned short sPower, bool fRacing)
  {
    // sPower is the raw power that the sensor detected.
    m_sLastPower = (unsigned short)(sPower);

    const DWORD tmNow = ArtGetTime();
    if(fRacing && tmLastSample != 0)
    {
      const DWORD tmElapsed = tmNow - tmLastSample;
      const float flDT = (float)tmElapsed / 1000.0f;
      flTimeSum += flDT;
      flPowerSum += sPower*flDT;
    }
    tmLastSample = tmNow;
  }
  void ResetPowerAvg()
  {
    flTimeSum = 0;
    flPowerSum = 0;
  }
  float GetAveragePower() const
  {
    if(flTimeSum != 0)
    {
      return flPowerSum / flTimeSum;
    }
    else
    {
      return m_sLastPower;
    }
  }
  float GetTotalPower() const
  {
    return flPowerSum; // returns our total power in J.
  }
  string strPlayerName;
  float flMassKg;
  int idLocal; // who is the local player that this LOCALPLAYERDATA represents?
  int idViewing; // who is this player looking at?
  unsigned short m_sLastPower; // last power: weight-adjusted power used to report to the server.  Used for server/game logic purposes
  float flRoadPos; // for spectating
  RECTF rcViewport;
  boost::shared_ptr<ITrainerLoadController> pLoadController; // can be NULL - but if we're running a sensor (like a computrainer) that can have its load controlled, will be non-NULL

  DWORD tmLastSample; // when was the last power sample?
  float flPowerSum; // sum of all our power samples
  float flTimeSum; // sum of all times between samples (in seconds)
  unsigned short lastCadence;
  float m_r,m_g,m_b;
  POWERTYPE m_ePowerType;
  CAMERASTYLE m_eCameraStyle;
  DWORD m_tmLastBackup; // the last time we saved this localplayer's data to disk (in ArtGetTimes)
  bool m_fLastPowerWasError;
};
