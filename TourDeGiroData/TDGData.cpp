#include "TDGInterface.h"
#include <boost/uuid/uuid.hpp>
#include <set>
#include <iostream>

#ifdef WIN32
#include <Windows.h>
#endif

#include "../ArtLib/ArtTools.h"

using namespace std;

bool IsComputrainer(POWERTYPE eType)
{
  switch(eType)
  {
  case COMPUTRAINER:
  case COMPUTRAINER_ERG:
    return true;
  default:
    return false;
  }
  return false;
}

class WorkoutImpl : public IWorkout
{
public:
  WorkoutImpl(const int iPlanId, const int iWorkoutId, const vector<WORKOUTTARGET>& lstSteps, const vector<string>& lstWebLines) : m_iPlanId(iPlanId),m_iWorkoutId(iWorkoutId),m_lstSteps(lstSteps),m_lengthSeconds(0),m_lstWebLines(lstWebLines)
  {
    for(unsigned int x = 0;x < lstSteps.size(); x++)
    {
      m_lengthSeconds += lstSteps[x].GetLengthSeconds();
    }
  };
  virtual ~WorkoutImpl() {};

  virtual std::string GetWebLine(int i) const ARTOVERRIDE
  {
    if(i >= 0 && i < m_lstWebLines.size())
    {
      return m_lstWebLines[i];
    }
    return std::string();
  }
  virtual bool GetTargetAtTime(float flTime, float flStrength, WORKOUTTARGET* pTarget, float* pflTimeUntilNext) const ARTOVERRIDE
  {
    int iCurrentStart = 0;
    for(unsigned int x = 0;x < m_lstSteps.size(); x++)
    {
      const int iCurrentEnd = iCurrentStart + m_lstSteps[x].GetLengthSeconds();

      if(flTime >= iCurrentStart && flTime <= iCurrentEnd)
      {
        *pTarget = m_lstSteps[x];
        pTarget->SetStrength(flStrength);
        *pflTimeUntilNext = (float)iCurrentEnd - flTime;
        return true;
      }
      iCurrentStart += m_lstSteps[x].GetLengthSeconds();
    }
    *pflTimeUntilNext=0;
    *pTarget = WORKOUTTARGET(0,100,TARGET_FREESPIN,0);
    return false;
  }
  virtual int GetWorkoutId() const ARTOVERRIDE
  {
    return m_iWorkoutId;
  }
  virtual int GetPlanId() const ARTOVERRIDE
  {
    return m_iPlanId;
  }
  virtual int GetLengthSeconds() const ARTOVERRIDE
  {
    return m_lengthSeconds;
  }
  virtual void GetMaxTarget(WORKOUTTARGET* pTarget, float flFTPW) const ARTOVERRIDE
  {
    float flMaxWatts = 0;
    int ixMaxWatts = -1;
    for(unsigned int x = 0;x < m_lstSteps.size(); x++)
    {
      float flThisWatts = m_lstSteps[x].GetWatts(flFTPW);
      if(flThisWatts > flMaxWatts || ixMaxWatts < 0)
      {
        ixMaxWatts = x;
        flMaxWatts = flThisWatts;
      }
    }
    if(ixMaxWatts >= 0 && ixMaxWatts < m_lstSteps.size())
    {  
      *pTarget = m_lstSteps[ixMaxWatts];
      return;
    }
    *pTarget = WORKOUTTARGET();
  }
private:
  vector<WORKOUTTARGET> m_lstSteps;
  vector<string> m_lstWebLines;
  const int m_iWorkoutId;
  const int m_iPlanId;
  int m_lengthSeconds;
};
float GetWorkoutAccuracy(const vector<RECORDEDDATA>& lstOldData, IWorkoutPtrConst spWorkout, float flFTPW)
{
  int cHits = 0; // how many data points hit their target?
  int cPossibles = 0; // how many valid data points were there?
  for(unsigned int x = 0;x < lstOldData.size(); x++)
  {
    const RECORDEDDATA& data = lstOldData[x];
    float flTimeUntilNext=0;
    WORKOUTTARGET target;
    if(data.flTime > 0 && 
       data.flTime < spWorkout->GetLengthSeconds() && 
       spWorkout->GetTargetAtTime(data.flTime,data.flStrength,&target,&flTimeUntilNext))
    {
      cPossibles++;
      float flDesiredWatts = target.GetWatts(flFTPW);
      if(data.power >= flDesiredWatts - 15 && data.power <= flDesiredWatts + 15) // within +/- 15W.  that counts as a hit
      {
        cHits++;
      }
    }
    else
    {
      // there was nothing to do at this point
    }
  }
  DASSERT(cPossibles >= cHits);
  if(cPossibles > 0)
  {
    return (float)cHits / (float)cPossibles;
  }
  return 1.0f;
}

string WORKOUTTARGET::GetDescription(float flFTPW, float flStrength) const
{
  char buf[1000];
  switch(eType)
  {
    case TARGET_FTP_PERCENT:
      snprintf(buf,sizeof(buf),"Hold %d%% FTP (%dW)",(int)(flValue*flStrength*100),(int)(flFTPW*flStrength*flValue));
      break;
    case TARGET_WATTS:
      snprintf(buf,sizeof(buf),"Hold %dW",(int)(flValue));
      break;
    case TARGET_FREESPIN:
      snprintf(buf,sizeof(buf),"Spin freely");
      break;
  }
  return buf;
}
string WORKOUTTARGET::GetShortDescription() const
{
  switch(eType)
  {
    case TARGET_FTP_PERCENT: return "FTP";
    case TARGET_WATTS: return "Watts";
    case TARGET_FREESPIN: return "Spin";
  }
}


boost::shared_ptr<const IWorkout> CreateWorkout(const int iPlanId, const int iWorkoutId, const std::vector<WORKOUTTARGET>& lstSteps, const std::vector<std::string>& lstWebLines)
{
  return boost::shared_ptr<const IWorkout>(new WorkoutImpl(iPlanId,iWorkoutId,lstSteps,lstWebLines));
}
void FormatDistance(char* psz, int cch, float flMeters, int decimals, SPEEDPREF eUnits, bool fIncludeUnits)
{
  char szFormat[100];
  switch(eUnits)
  {
  case IMPERIAL:
  {
    const float flMiles = flMeters / 1609.34f;
    const char* pszUnits = fIncludeUnits ? "mi" : "";
    snprintf(szFormat,sizeof(szFormat),"%%3.%df%s",decimals,pszUnits);
    snprintf(psz,cch,szFormat,flMiles);
    break;
  }
  default:
  case METRIC:
  {
    const float flKm = flMeters / 1000.0f;
    const char* pszUnits = fIncludeUnits ? "km" : "";
    snprintf(szFormat,sizeof(szFormat),"%%3.%df%s",decimals,pszUnits);
    snprintf(psz,cch,szFormat,flKm);
    break;
  }
  }
}

void FormatTotalDistance(char* psz, int cch, const PLAYERDIST& dist, int decimals, SPEEDPREF eUnits, bool fIncludeUnits)
{
  char szLapDist[100];
  FormatDistance(szLapDist,sizeof(szLapDist),dist.flDistPerLap,decimals,eUnits,fIncludeUnits);

  if(dist.iCurrentLap > 1)
  {
    snprintf(psz,cch,"%d x %s",dist.iCurrentLap,szLapDist);
  }
  else
  {
    snprintf(psz,cch,"%s",szLapDist);
  }
}
void FormatCompletedDistance(char* psz, int cch, const PLAYERDIST& dist, int decimals, SPEEDPREF eUnits, bool fIncludeUnits)
{
  char szLapDist[100];
  FormatDistance(szLapDist,sizeof(szLapDist),dist.flDistance,decimals,eUnits,fIncludeUnits);
  if(dist.iCurrentLap > 1)
  {
    snprintf(psz,cch,"%d laps, %s",dist.iCurrentLap,szLapDist);
  }
  else if(dist.iCurrentLap == 1)
  {
    snprintf(psz,cch,"%d lap, %s",dist.iCurrentLap,szLapDist);
  }
  else
  {
    snprintf(psz,cch,"%s",szLapDist);
  }
}

const char* StatIdToDesc(const STATID id)
{
  switch(id)
  {
  case STATID_10S_AVG:
    return "10 second";
  case STATID_30S_AVG:
    return "30 second";
  case STATID_60S_AVG:
    return "60 second";
  case STATID_2MIN_AVG:
    return "2 minute";
  case STATID_5MIN_AVG:
    return "5 minute";
  case STATID_10MIN_AVG:
    return "10 minute";
  case STATID_20MIN_AVG:
    return "20 minute";
  case STATID_30MIN_AVG:
    return "30 minute";
  case STATID_45MIN_AVG:
    return "45 minute";
  case STATID_60MIN_AVG:
    return "1 hour";
  case STATID_120MIN_AVG:
    return "2 hour";
  default:
    return "";
  }
  return "";
}
int StatIdToSeconds(const STATID id)
{
  switch(id)
  {
  case STATID_10S_AVG:
    return 10;
  case STATID_30S_AVG:
    return 30;
  case STATID_60S_AVG:
    return 60;
  case STATID_2MIN_AVG:
    return 120;
  case STATID_5MIN_AVG:
    return 300;
  case STATID_10MIN_AVG:
    return 600;
  case STATID_20MIN_AVG:
    return 1200;
  case STATID_30MIN_AVG:
    return 1800;
  case STATID_45MIN_AVG:
    return 2700;
  case STATID_60MIN_AVG:
    return 3600;
  case STATID_120MIN_AVG:
    return 7200;
  default:
    return 600;
  }
  return 600;
}
const char* GetPhysicsModeString(PHYSICSMODE eMode)
{
  CASSERT(PHYSICSMODE_LAST == 7);
  switch(eMode)
  {
  case WEIGHTHANDICAPPED_2013:
    return "Weight-Handicapped";
    break;
  case REALMODEL_2013:
    return "Real-world (early 2013)";
  case REALMODEL_LATE2013:
    return "Real-world (improved aerodynamics)";
  case REALMODEL_LATE2013_NODRAFT:
    return "Real-world (no drafting)";
  case FTPHANDICAPPED_2013:
    return "FTP-handicapped";
  case FTPHANDICAPPED_NODRAFT_2013:
    return "FTP-handicapped (no drafting)";
  case TRAININGMODE:
    return "Training";
  default:
    return "Unknown (try updating your client)";
  }
}
const char* GetPhysicsModeStringShort(PHYSICSMODE eMode)
{
  CASSERT(PHYSICSMODE_LAST == 7);
  switch(eMode)
  {
  case WEIGHTHANDICAPPED_2013:
    return "Weight-Handicapped";
    break;
  case REALMODEL_2013:
    return "Old";
  case REALMODEL_LATE2013:
    return "Normal";
  case REALMODEL_LATE2013_NODRAFT:
    return "Normal (nondrafting)";
  case FTPHANDICAPPED_2013:
    return "FTP-handicapped";
  case FTPHANDICAPPED_NODRAFT_2013:
    return "FTP-handicapped (nodraft)";
  case TRAININGMODE:
    return "Training";
  default:
    return "Unknown";
  }
}
PHYSICSMODE GetDefaultPhysicsMode()
{
  return REALMODEL_LATE2013;
}


bool IsDraftingMode(PHYSICSMODE ePhysics)
{
  CASSERT(PHYSICSMODE_LAST == 7); // update this if you add more physics modes!
  switch(ePhysics)
  {
  case FTPHANDICAPPED_2013:
  case REALMODEL_LATE2013:
  case WEIGHTHANDICAPPED_2013:
  case REALMODEL_2013:
    return true;
  case REALMODEL_LATE2013_NODRAFT:
  case FTPHANDICAPPED_NODRAFT_2013:
    return false;
  case TRAININGMODE:
    return false;
  default:
    return false;
  }
  return false;
}

PLAYERDIST::PLAYERDIST(int laps, float flDistInLap, float flDistPerLap) : iCurrentLap(laps),flDistPerLap(flDistPerLap),flDistance(0)
{
  AddMeters(flDistInLap);
}
bool PLAYERDIST::operator < (const PLAYERDIST& dist) const
{
  return ToMeters() < dist.ToMeters();
}
bool PLAYERDIST::operator > (const PLAYERDIST& dist) const
{
  return ToMeters() > dist.ToMeters();
}
bool PLAYERDIST::operator >= (const PLAYERDIST& dist) const
{
  return ToMeters() >= dist.ToMeters();
}
bool PLAYERDIST::operator <= (const PLAYERDIST& dist) const
{
  return ToMeters() <= dist.ToMeters();
}
bool PLAYERDIST::operator != (const PLAYERDIST& dist) const
{
  if(!IsValid() && !dist.IsValid()) return false;
  if(IsValid() != dist.IsValid()) return true;

  return ToMeters() != dist.ToMeters();
}
float PLAYERDIST::operator / (const PLAYERDIST& dist) const
{
  float distMe = ToMeters();
  float distThem = dist.ToMeters();
  return distMe / distThem;
}
const PLAYERDIST& PLAYERDIST::operator = (const PLAYERDIST& src)
{
  flDistance = src.flDistance;
  iCurrentLap = src.iCurrentLap;
  flDistPerLap = src.flDistPerLap;
  return *this;
}
std::ostream& operator <<(std::ostream& out, const PLAYERDIST& dist)
{
  int meters = (int)dist.flDistance;
  out<<meters;
  out<<"m (Lap ";
  out<<dist.iCurrentLap;
  out<<")";
  return out;
}

bool IsLappingMode(int laps, int timedLength)
{
  // in order to be a lapping mode, timedLength MUST be <= 0.
  return laps > 0 && timedLength <= 0;
}
bool IsTimedMode(int laps, int timedLength)
{
  return timedLength > 0;
}

PLAYERDIST operator * (const float fl, const PLAYERDIST& dist)
{
  PLAYERDIST ret(0,dist.ToMeters(),dist.flDistPerLap);
  return ret;
}

void PLAYERDIST::AddMeters(float flMeters)
{
  if(flMeters == 0) return;

  flDistance += flMeters;
  while(flDistance < 0)
  {
    flDistance += flDistPerLap;
  }
  while(flDistance >= flDistPerLap)
  {
    flDistance -= flDistPerLap;
    iCurrentLap++;
  }
}

ORIGDIST operator * (const float& f, const ORIGDIST& o)
{
  ORIGDIST ret;
  ret.v = f*o.v;
  return ret;
}

ORIGDIST operator + (const ORIGDIST& o1, const ORIGDIST& o2)
{
  ORIGDIST ret;
  ret.v = o1.v + o2.v;
  return ret;
}

template<class TInherit>
struct PlayerDataConstCopy : public TInherit
{
  PlayerDataConstCopy(unsigned int tmNow, bool fCopyData, const IConstPlayerData* pSrc)
    :
      m_strName(pSrc->GetName()),
      m_power(pSrc->GetPower()),
      m_hr(pSrc->GetHR(tmNow)),
      m_cadence(pSrc->GetCadence(tmNow)),
      m_masterId(pSrc->GetMasterId()),
      m_id(pSrc->GetId()),
      m_flMassKg(pSrc->GetMassKg()),
      m_flAvgPower(pSrc->GetAveragePower()),
      m_powerType(pSrc->GetPowerType()),
      m_powerSubType(pSrc->GetPowerSubType()),
      m_flEnergySpent(pSrc->GetEnergySpent()),
      m_pbs(pSrc->GetBestStatsConst()),
      m_flTimeRidden(pSrc->GetTimeRidden()),
      m_lstData(fCopyData ? pSrc->GetPowerHistory() : vector<RECORDEDDATA>())
  {
  }
  virtual ~PlayerDataConstCopy() {};
  virtual std::string GetName() const ARTOVERRIDE                   {return m_strName;}
  virtual unsigned short GetPower() const ARTOVERRIDE               {return m_power;}
  virtual unsigned short GetCadence(unsigned int tmNow) const ARTOVERRIDE {return m_cadence;}
  virtual unsigned short GetHR(unsigned int tmNow) const ARTOVERRIDE {return m_hr;}
  virtual int GetMasterId() const ARTOVERRIDE                       {return m_masterId;}
  virtual int GetId() const ARTOVERRIDE                             {return m_id;}
  virtual float GetMassKg() const ARTOVERRIDE                       {return m_flMassKg;}
  virtual float GetAveragePower() const ARTOVERRIDE                 {return m_flAvgPower;}
  virtual const float GetEnergySpent() const ARTOVERRIDE            {return m_flEnergySpent;}
  virtual const float GetTimeRidden() const ARTOVERRIDE             {return m_flTimeRidden;}
  virtual POWERTYPE GetPowerType() const                  {return m_powerType;} 
  virtual int GetPowerSubType() const                     {return m_powerSubType;}
  virtual const PERSONBESTSTATS& GetBestStatsConst() const ARTOVERRIDE     {return m_pbs;}
  
#pragma warning(push)
#pragma warning(disable:4172)
  virtual const std::vector<RECORDEDDATA>& GetPowerHistory() const {DASSERT(m_lstData.size() > 0); return m_lstData;}
#pragma warning(pop)
private:
  const std::string m_strName;
  const unsigned short m_power;
  const unsigned short m_cadence;
  const unsigned short m_hr;
  const int m_masterId;
  const int m_id;
  const float m_flMassKg;
  const float m_flAvgPower;
  const POWERTYPE m_powerType;
  const int m_powerSubType;
  const float m_flEnergySpent;
  const float m_flTimeRidden;
  const PERSONBESTSTATS m_pbs;
  const std::vector<RECORDEDDATA> m_lstData;
};
IConstPlayerDataPtrConst IConstPlayerData::GetConstDataCopy(unsigned int tmNow, bool fCopyData) const
{
  return IConstPlayerDataPtrConst(new PlayerDataConstCopy<IConstPlayerData>(tmNow, fCopyData, this));
}
// a very simple player implementation
struct PlayerConstCopy : public PlayerDataConstCopy<IConstPlayer>
{
public:
  PlayerConstCopy(unsigned int tmNow, const IConstPlayer* pSrc)
    : PlayerDataConstCopy(tmNow,false,pSrc),
      m_flFinishTime(pSrc->GetFinishTime()),
      m_flLane(pSrc->GetLane()),
      m_flDistance(pSrc->GetDistance()),
      m_flSpeed(pSrc->GetSpeed()),
      m_flLastDraft(pSrc->GetLastDraft()),
      m_flLastDraftNewtons(pSrc->GetLastDraftNewtons()),
      m_fIsAI(pSrc->Player_IsAI()),
      m_fIsFrenemy(pSrc->Player_IsFrenemy()),
      m_flStartTime(pSrc->GetStartTime()),
      m_flStartDistance(pSrc->GetStartDistance()),
      m_IP(pSrc->GetIP()),
      m_teamNumber(pSrc->GetTeamNumber()),
      m_fdwActionFlags(pSrc->GetActionFlags()),
      m_aiSelection(pSrc->GetAIType())
  {
    for(int x =0; x < STATID_COUNT; x++)
    {
      rgRunningPowers[x] = pSrc->GetRunningPower((STATID)x);
    }
  }
  virtual float GetFinishTime() const                   {return m_flFinishTime;}
  virtual float GetLane() const                         {return m_flLane;}
  virtual const PLAYERDIST& GetDistance() const ARTOVERRIDE    {return m_flDistance;}
  virtual float GetSpeed() const                        {return m_flSpeed;}
  virtual float GetLastDraft() const                    {return m_flLastDraft;}
  virtual float GetLastDraftNewtons() const             {return m_flLastDraftNewtons;}
  virtual bool Player_IsAI() const                      {return m_fIsAI;}
  virtual bool Player_IsFrenemy() const                 {return m_fIsFrenemy;}
  virtual float GetStartTime() const                    {return m_flStartTime;}
  virtual const PLAYERDIST GetStartDistance() const ARTOVERRIDE {return m_flStartDistance;}
  virtual unsigned int GetIP() const                    {return m_IP;}
  virtual unsigned int GetTimeAtDistance(float flDist)  {DASSERT(FALSE); return 0;}
  virtual unsigned int GetTeamNumber() const            {return m_teamNumber;}
  virtual unsigned int GetActionFlags() const           {return m_fdwActionFlags;}
  virtual void SetActionFlags(unsigned int flags, unsigned int mask) {DASSERT(FALSE);}
  virtual const AISELECTION& GetAIType() const          {return m_aiSelection;}
  virtual float GetReplayOffset() const                 {DASSERT(FALSE); return 0;}

  virtual bool GetStat(STATID eStat, float* pflValue) const {DASSERT(FALSE); return false;}
  virtual unsigned int GetTimeAtDistance(const PLAYERDIST& flDist) const {DASSERT(FALSE); return 0;}
  virtual const float GetRunningPower(STATID id) const
  {
    return rgRunningPowers[id];
  }
  virtual void GetLapTimes(std::vector<LAPDATA>& lstTimes) const ARTOVERRIDE
  {
    DASSERT(FALSE);
  }
  virtual IConstPlayerPtrConst GetConstCopy(unsigned int tmNow) const
  {
    DASSERT(FALSE);
    return IPlayerPtrConst(); // you deserve to crash
  }
  virtual bool IsStealth() const {return false;}
private:
  float rgRunningPowers[STATID_COUNT];

  const float m_flFinishTime;
  const float m_flLane;
  const PLAYERDIST m_flDistance;
  const float m_flSpeed;
  const float m_flLastDraft;
  const float m_flLastDraftNewtons;
  const bool m_fIsAI;
  const bool m_fIsFrenemy;
  const float m_flStartTime;
  const PLAYERDIST m_flStartDistance;
  const unsigned int m_IP;
  const unsigned int m_teamNumber;
  const unsigned int m_fdwActionFlags;
  const AISELECTION m_aiSelection;
};
IConstPlayerPtrConst IConstPlayer::GetConstCopy(unsigned int tmNow) const
{
  return IConstPlayerPtrConst(new PlayerConstCopy(tmNow, this));
}

