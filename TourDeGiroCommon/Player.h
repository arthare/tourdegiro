#pragma once
#include "Tools.h"
#include "StatsStore.h"

using namespace std;

void RunPhysics(PHYSICSMODE ePhysics, RACEMODE eRaceMode, const vector<IConstPlayerPtrConst>& lstOthers, const IConstPlayer* pCurrent, float flMaxSpeed, float flSlope, float flPower, float flMass, float flRho, float flSpeed, float cda, float crr, float dt, float* pflResultSpeed, float* pflPosDelta, float* pflAchievedDraft, float* pflAchievedDraftNewtons);
float GetDraft(const float flMe, float flMyLane, const float flOther, float flOtherLane, bool fUseBrakeCalcs);

bool IsGhostPair(const IPlayer* pPlayer1, const IPlayer* pPlayer2);


struct TIMEPOINT
{
  TIMEPOINT(unsigned int tmTime, const PLAYERDIST& flDistance) : tmTime(tmTime),flDistance(flDistance) {}
  PLAYERDIST flDistance;
  unsigned int tmTime;
};

bool operator < (const TIMEPOINT& t1, const TIMEPOINT& t2);
bool operator < (const TIMEPOINT& t1, const PLAYERDIST& flDist);





class PlayerBase : public IPlayer
{
public:
  PlayerBase(const AISELECTION& ai) : m_tmLastCadence(0),m_tmLastHR(0),m_iTeamNumber(-1),m_fdwFlags(0),m_ai(ai),m_lastCadence(90),m_flFinishTime(-1),m_ePowerType(PT_UNKNOWN),m_iPowerSubType(0)
  {
    m_flPowerSum = 0;
    m_flPowerTime = 0;
    m_flDraftingTime = 0;
  };
  
  void SetActionFlags(unsigned int flags, unsigned int mask) 
  {
    m_fdwFlags &= ~mask;
    m_fdwFlags |= flags;
  }
  unsigned int GetActionFlags() const
  {
    return m_fdwFlags;
  }
  virtual void InitPos(unsigned int tmNow, const PLAYERDIST& flDBDistance, const PLAYERDIST& dist, float flLane, float flSpeed, float flMassKg, StatsStore* pStore, int msStartTimeOffset, POWERTYPE ePowerType, int iPowerSubType)
  {
    m_ePowerType = ePowerType;
    m_iPowerSubType = iPowerSubType;

    m_flPowerSum = 0;
    m_flPowerTime = 0;
    m_flDraftingTime = 0;
    
    m_lstDistanceHistory.clear();
    m_flFinishTime = -1;
  }
  virtual unsigned int GetTimeAtDistance(const PLAYERDIST& flDist) const ARTOVERRIDE
  {
    // this should find the first thing that is greater than flDist.
    // so we'll want to decrement the iterator and use the two points as an average
    std::vector<TIMEPOINT>::const_iterator i = std::lower_bound(m_lstDistanceHistory.begin(), m_lstDistanceHistory.end(), flDist);
    if(i != m_lstDistanceHistory.end())
    {
      if(i == m_lstDistanceHistory.begin())
      {
        return i->tmTime;
      }
      else
      {
        TIMEPOINT pt2 = *i;
        TIMEPOINT pt1 = *(i-1);
        const PLAYERDIST flSpan = pt2.flDistance.Minus(pt1.flDistance);
        const float flPct = (flDist.Minus(pt1.flDistance)) / flSpan;
        return (unsigned int)(flPct * pt2.tmTime + (1-flPct)*pt1.tmTime);
      }
    }
    else
    {
      return 0;
    }
  }
  
  virtual unsigned int GetTeamNumber() const ARTOVERRIDE {return m_iTeamNumber;}
  virtual const AISELECTION& GetAIType() const ARTOVERRIDE
  {
    return m_ai;
  }
  void SetCadence(unsigned int tmNow, unsigned short cadence) ARTOVERRIDE
  {
    if(cadence > 20 && cadence < 170)
    {
      m_tmLastCadence = tmNow;
      m_lastCadence = cadence;
    }
  }
  void SetHR(unsigned int tmNow, unsigned short hr) ARTOVERRIDE
  {
    if(hr > 20 && hr < 240)
    {
      m_tmLastHR = tmNow;
      m_lasthr = hr;
    }
  }
  unsigned short GetCadence(unsigned int tmNow) const ARTOVERRIDE 
  {
    if(tmNow - m_tmLastCadence > 5000 && !Player_IsAI() && GetPowerType() != CHEATING )
    {
      return 0;
    }
    return m_lastCadence;
  }
  unsigned short GetHR(unsigned int tmNow) const ARTOVERRIDE 
  {
    if(tmNow - m_tmLastHR > 5000 && !Player_IsAI() && GetPowerType() != CHEATING)
    {
      return 0;
    }
    return m_lasthr;
  }
  virtual void SetFinishTime(float fl) {m_flFinishTime = fl;}
  virtual float GetFinishTime() const {return m_flFinishTime;}
  virtual const vector<RECORDEDDATA>& GetPowerHistory() const
  {
    return m_lstHistory;
  }
  virtual bool GetStat(STATID eStat, float* pflValue) const ARTOVERRIDE
  {
    switch(eStat)
    {
    case STATID_DRAFTPCT:
      if(m_flPowerTime > 0)
      {
        *pflValue = m_flDraftingTime / m_flPowerTime;
        return true;
      }
      else
      {
        return false;
      }
      break;
    default:
      return false;
    }
    return false;
  }
  virtual float GetAveragePower() const ARTOVERRIDE
  {
    if(m_flPowerTime > 0) return m_flPowerSum / m_flPowerTime;
    return 0;
  }
  virtual const float GetEnergySpent() const ARTOVERRIDE {return m_flPowerSum;}
  virtual const float GetTimeRidden() const ARTOVERRIDE {return m_flPowerTime;}
  virtual void Tick(float dt)
  {
    if((GetActionFlags() & ::ACTION_FLAG_DEAD) == 0)
    {
      m_flPowerSum += dt * GetPower(); // sum of all the (power*dt) we saw
      m_flPowerTime += dt;; // sum of all the dt we saw.  used for GetAveragePower()
      if(GetPower() >= 0 && GetLastDraft() > 0.38 / 2)
      {
        m_flDraftingTime += dt;
      }
    }
    
    
  }
  virtual POWERTYPE GetPowerType() const ARTOVERRIDE {return m_ePowerType;}
  virtual int GetPowerSubType() const ARTOVERRIDE {return m_iPowerSubType;}
  
protected:
  void UpdateHistory(unsigned int tmNow)
  {
    PLAYERDIST dist = GetDistance();
    while(m_lstDistanceHistory.size() > 0 && dist < m_lstDistanceHistory.back().flDistance)
    {
      m_lstDistanceHistory.pop_back(); // clear out all the data about our past if we've gone backwards.  otherwise weird stuff can happen
    }
    if(m_lstDistanceHistory.size() <= 0 ||
       (tmNow - m_tmLastAddition > 2000 && 
       tmNow > m_lstDistanceHistory[m_lstDistanceHistory.size()-1].tmTime &&
       dist > m_lstDistanceHistory.back().flDistance))
    {
      m_lstDistanceHistory.push_back(TIMEPOINT(tmNow,dist));
      m_tmLastAddition = tmNow;
    }
  }
  void UpdatePowerTrace(const unsigned int tmNow, const RECORDEDDATA& rec)
  {
    bool fGoodToAdd = false;
    if(!Player_IsAI())
    {
      fGoodToAdd = m_lstHistory.size() <= 0 || rec != m_lstHistory.back() || tmNow - m_tmLastHistory > 1500;
    }
    else
    {
      fGoodToAdd = m_lstHistory.size() <= 0 || tmNow - m_tmLastHistory > 2000; // don't update AIs more than once per second.  otherwise we get truly ridiculous amounts of data.
    }

    if(fGoodToAdd)
    {
      // only push new data if it differs substantially from old data
      m_lstHistory.push_back(rec);
      m_tmLastHistory = tmNow;
    }
  }
  void SetTeamNumber(int iTeamNumber)
  {
    m_iTeamNumber = iTeamNumber;
  }
private:
  std::vector<TIMEPOINT> m_lstDistanceHistory;

  vector<RECORDEDDATA> m_lstHistory;
  unsigned int m_tmLastHistory;

  unsigned int m_tmLastAddition; // we don't want to add too frequently, or else this list will be huge
  int m_iTeamNumber;
  unsigned int m_fdwFlags;
  const AISELECTION m_ai;
  unsigned short m_lastCadence;
  unsigned short m_lasthr;
  DWORD m_tmLastCadence;
  DWORD m_tmLastHR;
  float m_flFinishTime;

  float m_flPowerSum; // sum of all the (power*dt) we saw
  float m_flPowerTime; // sum of all the dt we saw.  used for GetAveragePower()
  float m_flDraftingTime;

  POWERTYPE m_ePowerType;
  int       m_iPowerSubType;
};

class Player : public PlayerBase
{
public:
  Player(unsigned int tmNow, int iMasterId, int iDefaultTeamId, string strName, const boost::shared_ptr<const IMap> spMap, DWORD ip, const AISELECTION& ai) 
  : PlayerBase(ai), 
    m_fIsStealth(false),
    m_iMasterId(iMasterId),
    m_iDefaultTeamId(iDefaultTeamId),
    m_strName(strName),
    m_spMap(spMap),
    m_dist(0,0,spMap->GetLapLength()),
    m_startDist(0,0,spMap->GetLapLength()),
    m_flLane(0),m_flMassKg(0),
    m_id(-1),
    m_ip(ip),
    m_tmStart(0) // the elapsed time of the ride that we started at.  So if this guy's ride started 56 seconds into the race, it'll be 56.0.  For the most part, it'll be 0.0 (except for time-trials and people that join races late)
  {
    DASSERT(m_dist.flDistPerLap > 0);
    m_tmLastNonZeroPower = tmNow;
    m_tmAbsoluteStart = tmNow;

    m_mapTimers.clear();
  };
  virtual ~Player() 
  {
    if(Player_IsAI())
    {
      DASSERT(m_mapTimers.size() <= 0);
    }
  };

  PERSONBESTSTATS& GetBestStats() ARTOVERRIDE
  {
    return m_flHistoricAvgPower;
  }
  virtual const PERSONBESTSTATS& GetBestStatsConst() const ARTOVERRIDE  {    return m_flHistoricAvgPower;  }
  virtual const float GetRunningPower(STATID id) const ARTOVERRIDE
  {
    if(Player_IsAI())
    {
      DASSERT(m_mapTimers.size() <= 0);
      return 0;
    }
    else
    {
      map<STATID,TimeAveragerPtr>::const_iterator i = m_mapTimers.find(id);
      if(i != m_mapTimers.end())
      {
        return i->second->GetBestAverage();
      }
      return 0;
    }
  }
  virtual bool IsStealth() const ARTOVERRIDE {return m_fIsStealth;}

  // flDBDistance: what should we put in the DB (generally, m_map.GetStartDistance())
  // flDistance: where does this guy actually start? (usually in a peleton)
  virtual void InitPos(unsigned int tmNow, const PLAYERDIST& DBDist, const PLAYERDIST& dist, float flLane, float flSpeed, float flMassKg, StatsStore* pStore, int msStartTimeOffset, POWERTYPE eType, int iPowerSubType) ARTOVERRIDE
  {
    if(!Player_IsAI())
    {
      for(int x = STATID_10S_AVG; x <= STATID_120MIN_AVG; x++)
      {
        const int cSeconds = StatIdToSeconds((STATID)x);
        m_mapTimers[(STATID)x] = boost::shared_ptr<TimeAverager>(new TimeAverager(cSeconds));
      }
    }

    DASSERT(dist.flDistPerLap > 0);
    PlayerBase::InitPos(tmNow, DBDist, dist, flLane, flSpeed, flMassKg, pStore, msStartTimeOffset, eType, iPowerSubType);
    if(m_id <= 0)
    {
      int iTempTeam = -1;
      m_id = pStore->GetPlayerId(GetMasterId(),GetDefaultTeamId(),GetName(),Player_IsAI(),m_spMap.get(),&m_flHistoricAvgPower,&m_fIsStealth,&iTempTeam,GetSecondsSince1970GMT());
      SetTeamNumber(iTempTeam);
      DASSERT(m_id > 0);
    }
    m_sCurrentPower = 0;
    m_dist = dist;
    m_startDist = DBDist;
    m_flLane = flLane;
    m_flMassKg = flMassKg;
    m_flSpeed = flSpeed;
    m_flLastDraft = 0;

    const vector<RECORDEDDATA>& lstHistory = GetPowerHistory();
    if(lstHistory.size() <= 0 || lstHistory.back().dist.ToMeters() <= 0) // only re-adjust m_tmStart if we're a "virgin" player.  Revived players
    {
      m_tmStart = (float)(tmNow + msStartTimeOffset)/1000.0f;
    }
    else
    {
      DASSERT(m_dist.ToMeters() > 0); // if we're not a virgin player, we had better have moved at some point!
    }
    
    DASSERT(m_dist.flDistPerLap > 0);
  }
  
  virtual string GetName() const ARTOVERRIDE {return m_strName;}
  virtual float GetLane() const ARTOVERRIDE {return m_flLane;};
  virtual const PLAYERDIST& GetDistance() const ARTOVERRIDE {return m_dist;}
  virtual float GetSpeed() const ARTOVERRIDE {return m_flSpeed;}
  virtual float GetMassKg() const {return m_flMassKg;}
  virtual void Tick(float flRaceTime, int ixMe, PHYSICSMODE ePhysicsMode, RACEMODE eRaceMode, const vector<IConstPlayerPtrConst>& lstOthers, const DraftMap& mapWhoDraftingWho, float dt,SPRINTCLIMBDATA* pPointsScored, std::string* pstrPointDesc, const unsigned int tmNow) ARTOVERRIDE;
  virtual float GetLastDraft() const ARTOVERRIDE {return m_flLastDraft;}
  virtual float GetLastDraftNewtons() const {return this->m_flLastDraftNewtons;}

  virtual unsigned short GetPower() const ARTOVERRIDE
  {
    return m_sCurrentPower;
  }
  virtual void SetPower(int iPower, const DWORD tmNow)
  {
    if(IS_FLAG_SET(GetActionFlags(),ACTION_FLAG_SPECTATOR))
    {
      m_dist = PLAYERDIST(0,iPower*10,m_spMap->GetLapLength());
    }
    else
    {
      DASSERT(m_dist >= m_spMap->GetStartDistance() && m_dist <= m_spMap->GetEndDistance());

      m_sCurrentPower = iPower;

      float flNow = ((float)(tmNow-m_tmAbsoluteStart)/1000.0f);
      RECORDEDDATA rec((unsigned short)iPower,GetDistance().Minus(GetStartDistance()),flNow,m_spMap->GetElevationAtDistance(GetDistance()),GetSpeed(),GetCadence(tmNow),GetHR(tmNow),GetLane());
      UpdatePowerTrace(tmNow,rec);
      if(iPower != 0)
      {
        // nonzero power or we're an AI
        m_tmLastNonZeroPower = tmNow;
      }
    }
  }
  virtual int GetMasterId() const ARTOVERRIDE
  {
    return m_iMasterId;
  }
  virtual int GetDefaultTeamId() const
  {
    return m_iDefaultTeamId;
  }
  virtual int GetId() const ARTOVERRIDE
  {
    return m_id;
  }
  DWORD GetLastPowerTime() const {return m_tmLastNonZeroPower;} // when was the last time this guy had a nonzero power?
  const static int MIN_SPEED = 1; // minimum speed: 3.6km/h
  virtual bool  Player_IsAI() const {return false;};
  virtual bool  Player_IsFrenemy() const {return false;};
  virtual unsigned int GetIP() const {return m_ip;}
  
  virtual const PLAYERDIST GetStartDistance() const ARTOVERRIDE {return m_startDist;}
  virtual float GetStartTime() const ARTOVERRIDE  {return m_tmStart;}
  virtual float GetReplayOffset() const
  {
    return m_tmStart - ((float)m_tmAbsoluteStart)/1000.0f;
  }
  virtual void GetLapTimes(std::vector<LAPDATA>& lstLapTimes) const ARTOVERRIDE
  {
    lstLapTimes = m_lstLapTimes;
  }
  virtual void GetSprintClimbPoints(std::vector<SPRINTCLIMBDATA>& lstPoints) const ARTOVERRIDE
  {
    lstPoints = m_lstScoreData;
  }
  static float GetCDA(PHYSICSMODE ePhyiscs, float flMassKg);
private:
  static float GetDraftInLane(int ixMe, float flWhatLane, float flWhatDistInLap, const vector<IConstPlayerPtrConst>& lstOthers, bool fUseBrakeDraftCalcs)
  {
    // lstOthers is ordered from furthest-in-lap (index zero) to least-far-in-loop (index size-1)
    if(flWhatLane < -ROAD_WIDTH || flWhatLane >= ROAD_WIDTH) return -1; // don't try to lane-change off the road!

    float flLaneDraft = 0; // the worst draft we found in this lane
    for(int x = ixMe-1;x >= 0; x--)
    {
      const IConstPlayer* pOther = lstOthers[x].get();
      DASSERT(!(IS_FLAG_SET(pOther->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS)));
      
      const float& flOtherDistInLap = pOther->GetDistance().flDistance;
      if(flOtherDistInLap - flWhatDistInLap >= 10)
      {
        // this guy was too far ahead.  Since the list is sorted by distance, that means everyone else left will be too
        break;
      }
      const float& flOtherLane = pOther->GetLane();
      const float flLaneDiff = abs(flWhatLane - flOtherLane);
      if(flLaneDiff >= CYCLIST_WIDTH) 
        continue; // buddy is too far from flWhatLane to be relevant

      DASSERT(pOther->GetFinishTime() < 0);


      const float flThisDraft = GetDraft(flWhatDistInLap,flWhatLane,flOtherDistInLap,flOtherLane,fUseBrakeDraftCalcs);
      if(flThisDraft < 0 && fUseBrakeDraftCalcs)
      {
        // we'd be too close to someone at this test lane.
        return -0.1f;
      }
      flLaneDraft = max(flLaneDraft,flThisDraft);
      
    }
    return flLaneDraft;
  }

public:
  static void DoTests();
private:

  static bool IsBetween(const float flMyLane, const float flBlockerLane, const float flTargetLane)
  {
    return (flBlockerLane >= flMyLane+CYCLIST_WIDTH && flTargetLane >= flMyLane && flBlockerLane <= flTargetLane) ||
           (flBlockerLane <= flMyLane-CYCLIST_WIDTH && flTargetLane <= flMyLane && flBlockerLane >= flTargetLane);
  }

  static bool CanGetToLane(const int ixFirstBlocker,const int ixMe, const int ixLastBlocker, const float& flMyDistanceInLap, const float& flMyLane,const vector<IConstPlayerPtrConst>& lstOthers,const float flTargetLane)
  {
    bool fCanGetThere = true;

    for(int ixBlocker = ixFirstBlocker; ixBlocker < ixLastBlocker && fCanGetThere; ixBlocker++)
    {
      if(ixBlocker == ixMe) continue;

      bool fBlocked = true;
      IConstPlayerPtrConst pBlocker(lstOthers[ixBlocker]);
      const float flBlockerLane = pBlocker->GetLane();
      if(!IsBetween(flMyLane, flBlockerLane, flTargetLane))
      {
        // this guy is either a lesser value than me, or he isn't in between me and the target
        fBlocked = false;
      }

      if(fBlocked)
      {
        // there was a guy between me and the target that I was unable to push out of the way.  No point continuing
        fCanGetThere = false;
        break;
      }
    }
    return fCanGetThere;
  }

  // ixMe = where are we in the lstOthers vector (which is assumed to be sorted by distance)
  // this lets us only check the players ahead of us for their effect on drafting
  friend void TestDraftCode();
  friend void DoMoveDirTest(const IConstPlayerPtrConst pLeading, const IConstPlayerPtrConst pTrailing, int iExpectedRetForTrailing);
  friend void DoMoveDirTests(boost::shared_ptr<const IMap> spMap);
  const static int MOVEDIR_LEFT=-1; // make this player move left
  const static int MOVEDIR_NOMOVE = 0; // make this player stay still lane-wise
  const static int MOVEDIR_RIGHT=1; // make this player move right
  static int GetMoveDir(const int ixMe, const vector<IConstPlayerPtrConst>& lstOthers, const DraftMap& mapWhoDraftingWho) // returns positive if we should make our lane more positive, negative if we should make our lane more negative
  { 
    DASSERT(ixMe >= 0 && ixMe < (int)lstOthers.size());

    const IConstPlayer* pMe = lstOthers[ixMe].get();
    const float& flMyDistance = pMe->GetDistance().flDistance;
    const float& flMyLane = pMe->GetLane();
    float flBlockerLane = flMyLane;

    // first, we want to find all the draftable people near us
    int ixFirstBlocker = ixMe; // blockers will run from ixFirstBlocker to ixLastBlocker inclusive, and are people that we need to consider for collisions when changing lanes
    int ixFirstTarget = ixMe; // drafting targets (aka people whose asses we will check to see if we want to ride behind them) will run from ixFirstTarget to ixFirstBlocker-1 inclusive, and are people that we may want to draft off of
    int ixLastBlocker = ixMe;
    for(unsigned int x = ixMe+1;x < lstOthers.size(); x++) // checking for people who we're leading by less than CYCLIST_LENGTH (we won't be able to change into their lanes)
    {
      const IConstPlayer* pOther = lstOthers[x].get();
      DASSERT(!(IS_FLAG_SET(pOther->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS)));

      const float flOtherLead = pOther->GetDistance().flDistance - flMyDistance;
      DASSERT(flOtherLead <= 0); // everyone after ixMe in the list should be behind me (or dead-on tied with me)
      if(flOtherLead > -CYCLIST_LENGTH)
      {
        // this guy is behind me, but only barely
        ixLastBlocker = x;
      }
      else
      {
        // this guy is behind me.  which means everyone else will be too
        break;
      }
    }
    for(int x = ixMe-1;x >= 0; x--)
    {
      const IConstPlayer* pOther = lstOthers[x].get();
      DASSERT(!(IS_FLAG_SET(pOther->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS)));

      const float flOtherLead = pOther->GetDistance().flDistance - flMyDistance;
      if(flOtherLead < CYCLIST_LENGTH)
      {
        // this guy is blocking our way
        ixFirstBlocker = x;
        flBlockerLane = pOther->GetLane();
      }
      else if(flOtherLead > CYCLIST_LENGTH && flOtherLead < 10)
      {
        // this guy is draftable.
        ixFirstTarget = x; // we'll gradually advance ixFirstTarget until we stop getting them
      }
      else if(flOtherLead >= 10)
      {
        // this guy is too far ahead.  Which means everyone else will be too.
        break;
      }
    }

    // ok, now we've figured out who we may want to draft off of and who may prevent us from doing that.  Let's go through all the targets, and see if it is possible for us to move towards them
    float flBestDraft = GetDraftInLane(ixMe,flMyLane,flMyDistance,lstOthers,true) + 0.10;
    float flBestLane = flMyLane;
    bool fLeftBias = rand()&1;

    if(flBestDraft < 0)
    {
      // we're stuck behind someone.  Check to see if the lanes to his left or right are better.  If they aren't, check the lanes to the left and right of myself
      const float flBlockLeftCheck = flBlockerLane + (fLeftBias ? CYCLIST_WIDTH : -CYCLIST_WIDTH);
      const float flBlockRightCheck = flBlockerLane + (fLeftBias ? -CYCLIST_WIDTH : CYCLIST_WIDTH);
      const float flDistToLeft = abs(flMyLane - flBlockLeftCheck);
      const float flDistToRight = abs(flMyLane - flBlockRightCheck);

      const float flMyLeftCheck = flMyLane + (fLeftBias ? CYCLIST_WIDTH : -CYCLIST_WIDTH);
      const float flMyRightCheck = flMyLane + (fLeftBias ? -CYCLIST_WIDTH : CYCLIST_WIDTH);
      float flBestDistance = ROAD_WIDTH*2;

      const float flDraftInTargetSpotBlockLeft = GetDraftInLane(ixMe,flBlockLeftCheck,flMyDistance,lstOthers,true); // blocker's left
      if(flDraftInTargetSpotBlockLeft > flBestDraft)
      {
        flBestDraft = flDraftInTargetSpotBlockLeft;
        flBestLane = flBlockLeftCheck;
        flBestDistance = flDistToLeft;
      }
      const float flDraftInTargetSpotBlockRight = GetDraftInLane(ixMe,flBlockRightCheck,flMyDistance,lstOthers,true); // blocker's right
      if(flDraftInTargetSpotBlockRight > flBestDraft || 
         (flDraftInTargetSpotBlockRight == flBestDraft && flDistToRight < flBestDistance)) // the right check is equally drafty, but is closer
      {
        flBestDraft = flDraftInTargetSpotBlockRight;
        flBestLane = flBlockRightCheck;
        flBestDistance = abs(flDistToRight-flBestDistance);
      }
      const float flDraftInTargetSpotMyRight = GetDraftInLane(ixMe,flMyRightCheck,flMyDistance,lstOthers,true); // my right
      if(flDraftInTargetSpotMyRight > flBestDraft || 
         (flDraftInTargetSpotMyRight == flBestDraft && CYCLIST_WIDTH < flBestDistance)) // the right check is equally drafty, but is closer
      {
        flBestDraft = flDraftInTargetSpotMyRight;
        flBestLane = flMyRightCheck;
        flBestDistance = CYCLIST_WIDTH;
      }

      const float flDraftInTargetSpotMyLeft = GetDraftInLane(ixMe,flMyLeftCheck,flMyDistance,lstOthers,true); // my left
      if(flDraftInTargetSpotMyLeft > flBestDraft || 
         (flDraftInTargetSpotMyLeft == flBestDraft && flDistToRight < flBestDistance)) // the right check is equally drafty, but is closer
      {
        flBestDraft = flDraftInTargetSpotMyLeft;
        flBestLane = flMyLeftCheck;
      }
    }

    for(int ixTarget = ixFirstTarget; ixTarget < ixFirstBlocker; ixTarget++)
    {
      const IConstPlayer* pTarget = lstOthers[ixTarget].get();
      
      DASSERT(!(IS_FLAG_SET(pTarget->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS)));

      const float&flTargetLane = pTarget->GetLane();
      const float flLeftCheck = flTargetLane + (fLeftBias ? CYCLIST_WIDTH : -CYCLIST_WIDTH);
      const float flRightCheck = flTargetLane + (fLeftBias ? -CYCLIST_WIDTH : CYCLIST_WIDTH);
      // let's see if any blockers of value stand between us and our target
      
      const float flDraftInTargetSpot = GetDraftInLane(ixMe,flTargetLane,flMyDistance,lstOthers,true);
      if(flDraftInTargetSpot > flBestDraft && CanGetToLane(ixFirstBlocker,ixMe, ixLastBlocker,flMyDistance, flMyLane, lstOthers,flTargetLane))
      {
        // we can get all the way to this target, and we know it is better than our current best
        DASSERT(flDraftInTargetSpot > flBestDraft);
        flBestDraft = flDraftInTargetSpot;
        flBestLane = flTargetLane;
        continue;
      }
      const float flDraftInTargetSpotLeft = GetDraftInLane(ixMe,flLeftCheck,flMyDistance,lstOthers,true);
      if(flLeftCheck >= -ROAD_WIDTH/2 && flDraftInTargetSpotLeft > flBestDraft && CanGetToLane(ixFirstBlocker,ixMe, ixLastBlocker,flMyDistance, flMyLane,lstOthers,flLeftCheck))
      {
        // we can get all the way to this target, and we know it is better than our current best
        DASSERT(flDraftInTargetSpotLeft > flBestDraft);
        flBestDraft = flDraftInTargetSpotLeft;
        flBestLane = flLeftCheck;
        continue;
      }
      const float flDraftInTargetSpotRight = GetDraftInLane(ixMe,flRightCheck,flMyDistance,lstOthers,true);
      if(flRightCheck <= ROAD_WIDTH/2 && flDraftInTargetSpotRight > flBestDraft && CanGetToLane(ixFirstBlocker,ixMe, ixLastBlocker,flMyDistance, flMyLane,lstOthers,flRightCheck))
      {
        // we can get all the way to this target, and we know it is better than our current best
        DASSERT(flDraftInTargetSpotRight > flBestDraft);
        flBestDraft = flDraftInTargetSpotRight;
        flBestLane = flRightCheck;
        continue;
      }
    }

    // now we should have select a best lane
    if(flBestLane < flMyLane - 0.05f)
    {
      return MOVEDIR_LEFT;
    }
    else if(flBestLane > flMyLane + 0.05f)
    {
      return MOVEDIR_RIGHT;
    }
    return MOVEDIR_NOMOVE;
  }
  void AddPoints(const SPRINTCLIMBDATA& data)
  {
    m_lstScoreData.push_back(data);
  }
protected:
  const boost::shared_ptr<const IMap>& GetMap() const {return m_spMap;}
private:
  std::vector<LAPDATA> m_lstLapTimes;
  std::vector<SPRINTCLIMBDATA> m_lstScoreData;
  const boost::shared_ptr<const IMap> m_spMap;
  PERSONBESTSTATS m_flHistoricAvgPower;
  PLAYERDIST m_dist;
  float m_flLane;
  float m_flSpeed;
  float m_flMassKg;
  int m_sCurrentPower;
  float m_flLastDraft; // the draft we got on our last tick, in terms of % of aerodynamic drag saved
  float m_flLastDraftNewtons; // the draft we got on our last tick, in terms of newtons saved
  string m_strName;
  int m_id;
  int m_iMasterId;
  int m_iDefaultTeamId;
  DWORD m_tmLastNonZeroPower;
  DWORD m_tmAbsoluteStart; // when was this guy constructed? (used for power history)
  float m_tmStart; // when did this player start?
  PLAYERDIST m_startDist;

  bool m_fIsStealth;

  unsigned int m_ip;

  // maps from the statid to the timer that is running for that statid
  map<STATID,boost::shared_ptr<TimeAverager> > m_mapTimers;
};

enum DOPERTYPE
{
  DOPER_STEADY,
  DOPER_HILLMAN,
  DOPER_SURGEY,
  DOPER_SPRINT,
  DOPER_HELPY,

  DOPERTYPE_COUNT,
};

class AIBase : public Player
{
public:
  AIBase(unsigned int tmNow, IAI* pBrain, DOPERTYPE eAIType, int id, string strName, boost::shared_ptr<const IMap> spMap, const AISELECTION& ai, bool fIsFrenemy) : Player(tmNow, AI_MASTER_ID,AI_DEFAULT_TEAM,strName,spMap, ai.ixAI%16, ai), m_pBrain(pBrain), m_fIsFrenemy(fIsFrenemy) {}
  virtual ~AIBase()
  {
  }
  virtual bool  Player_IsAI() const ARTOVERRIDE {return true;}
  virtual bool  Player_IsFrenemy() const ARTOVERRIDE {return m_fIsFrenemy;}

  virtual void Tick(float flRaceTime, int ixMe, PHYSICSMODE ePhysicsMode, RACEMODE eRaceMode, const vector<IConstPlayerPtrConst>& lstOthers, const DraftMap& mapWhoDraftingWho, float dt, SPRINTCLIMBDATA* pPointsScored, std::string* pstrPointDesc, const unsigned int tmNow) ARTOVERRIDE
  {
    int iWatts = m_pBrain->GetWatts(GetTimeRidden(), this,lstOthers, GetMap().get());

    const float flDist = GetDistance().ToMeters();
    const float flLow = (iWatts*4)/5;
    const float flHigh = iWatts;
    if(flDist < 750)
    {
      // 80% power
      iWatts = flLow;
    }
    else if(flDist >= 750 && flDist < 1000)
    {
      const float flPct = (flDist - 750.0f)/250.0f;
      iWatts = (int)((1-flPct)*(flLow) + flPct*flHigh);
    }
    else
    {
      // no modifications.  full power
    }
    SetPower(iWatts, tmNow);
    Player::Tick(flRaceTime, ixMe, ePhysicsMode, eRaceMode ,lstOthers, mapWhoDraftingWho,dt,pPointsScored, pstrPointDesc, tmNow);
  }
private:
  IAI* m_pBrain;
  const bool m_fIsFrenemy;
};

typedef __declspec (dllexport) int (*PFNAICOUNT)();
typedef __declspec (dllexport) IAI* (*PFNMAKEAI)(const int ix, int iTargetWatts);
typedef __declspec (dllexport) void (*PFNFREEAI)(IAI*);
struct AIDLL
{
  HMODULE hMod;
  wstring strDLL;
  int cAIs;
  PFNMAKEAI pfnMakeAI;
  PFNFREEAI pfnFreeAI;
  vector<double> lstTotalTimes;
};
vector<AIDLL> GrovelAIs();

bool PlayerCompareForDraft(const IConstPlayer* i,const IConstPlayer* j);
bool PlayerCompareForRank(const IConstPlayer* i,const IConstPlayer* j);

template <class T>
struct PlayerRankCompare
{
  PlayerRankCompare(const T* pClient) : pClient(pClient) {}

  bool operator() (const IConstPlayer* i,const IConstPlayer* j) const
  {
    return PlayerCompareForRank(i,j);
  }
  bool operator() (IConstPlayerPtrConst i,const boost::shared_ptr<IConstPlayer> j) const
  {
    return PlayerCompareForRank(i.get(),j.get());
  }
  bool operator() (IConstPlayerPtrConst i,IConstPlayerPtrConst j) const
  {
    return PlayerCompareForRank(i.get(),j.get());
  }
  const T* pClient; // this is lame: makes it compile, otherwise STL tries calling the constructor with () rather than operator()
};
template <class T>
struct PlayerDraftCompare
{
  PlayerDraftCompare(const T* pClient) : pClient(pClient) {}

  bool operator() (const IConstPlayer* i,const IConstPlayer* j) const
  {
    return PlayerCompareForDraft(i,j);
  }
  bool operator() (IConstPlayerPtrConst i,IConstPlayerPtrConst j) const
  {
    return PlayerCompareForDraft(i.get(),j.get());
  }
  const T* pClient; // this is lame: makes it compile, otherwise STL tries calling the constructor with () rather than operator()
};

// returns the force (in newtons) that we'd expect for a given speed, cda, crr, and mass
float GetDraftlessForce(float flSpeed, float flSlope, float flRho, float cda, float crr, float flMass);

void BuildDraftingMap(const vector<IConstPlayerPtrConst>& lstToUpdate, DraftMap& mapDrafting);
