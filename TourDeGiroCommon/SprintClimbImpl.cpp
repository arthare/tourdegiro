
#include "stdafx.h"
#include "SprintClimbImpl.h"
using namespace std;

class CrossingTimer : public ICrossingTimer
{
public:
  // indicates that a player has been detected crossing the leadin position of a segment
  void StartedSegment(float time, int id)
  {
    DASSERT(time >= 0 && id >= 0);

    unordered_map<int,PLAYERSTATE>::iterator i = m_mapState.find(id);
    if(i == m_mapState.end())
    {
      // never seen this player before
      m_mapState.insert(std::pair<int,PLAYERSTATE>(id,PLAYERSTATE(time)));
    }
    else
    {
      // we've seen this player before
      PLAYERSTATE& state = i->second;
      state.startTime = time;
      DASSERT(state.IsDoingSegment()); // better be doing a segment now that we've done this!
    }
  }

  // indicates that a player is done a segment
  bool DoneSegment(float time, int id, float* pflElapsedTime)
  {
    DASSERT(time >= 0 && id >= 0 && pflElapsedTime);

    unordered_map<int,PLAYERSTATE>::iterator i = m_mapState.find(id);
    if(i == m_mapState.end())
    {
      // never seen this player before
      // this can happen if a player gets inserted into a ride midway through a sprint/climb
      return false; // no data for you!
    }
    else
    {
      // we've seen this player before
      PLAYERSTATE& state = i->second;
      *pflElapsedTime = time - state.startTime;
      state.startTime = -1;
      DASSERT(!state.IsDoingSegment()); // you're done your segment now!
      return true;
    }
  }
private:
  struct PLAYERSTATE
  {
    PLAYERSTATE(float startTime) : startTime(startTime) {};
    float startTime; // when did he start his current segment?

    bool IsDoingSegment() const {return startTime >= 0;}
  };
  unordered_map<int,PLAYERSTATE> m_mapState;
};


float GetScoreForCross(float flFirstPlace, int cCrossersBefore, int cTotalPeople)
{
  // if you're first: you get flFirstPlace
  // if you're halfway: you get flFirstPlace/4
  // if you're last: you get flFirstPlace/8
  const float flPct = (float)cCrossersBefore / (float)cTotalPeople;

  // restarting the above:
  // flPct=0 -> 100%
  // flPct=0.5 -> 25%
  // flPct=1.0 -> 12%
  return 0.2916*flFirstPlace / pow(flPct+0.54,2); // i did math!
}

SprintClimbImpl::SprintClimbImpl(const SPRINTCLIMBDATA_RAW& pt, float flActualDistMeters, float flPositionDistance, float flRise, bool fFinishMode) 
  : m_flPositionDistance(flPositionDistance),
    m_flRise(flRise),
    m_meters((int)flActualDistMeters),
    m_cTotalPeople(1),
    m_strName(pt.szName), 
    m_flPosition(pt.flOrigDistance),
    m_flLeadInDist(pt.flOrigLeadInDist), 
    m_flFirstPoints(pt.flMaxPoints), 
    m_eType(pt.eScoreType), 
    m_fFinishMode(fFinishMode || pt.fIsFinish)
{
  m_spTimer = boost::shared_ptr<ICrossingTimer>(new CrossingTimer());
}

bool SprintClimbImpl::GetPointsEarned
(
  const ORIGDIST& flLastDist, 
  const ORIGDIST& flThisDist, 
  int ixCurLap,
  bool fLastLap,
  SPRINTCLIMBDATA* pScoreData,
  int id,
  float flCurTime
)
{
  if(m_fFinishMode && !fLastLap)
  {
    // we're a finish-lap-only point, and it's not the last lap!
    return false;
  }

  const ORIGDIST flLeadInPos = (m_flPosition - this->m_flLeadInDist);
  if(flLastDist <= flLeadInPos && flThisDist >= flLeadInPos)
  {
    // just started the segment!
    m_spTimer->StartedSegment(flCurTime,id);
  }
  else if(flLastDist <= m_flPosition && flThisDist > m_flPosition)
  {
    // buddy just crossed our line.
    const unsigned int ixHisLap = ixCurLap;
    while(ixHisLap >= this->m_lstFinishersPerLap.size())
    {
      m_lstFinishersPerLap.push_back(0); // if he was the first guy to do this lap, put a zero in
    }

    float flTime = 1e30f;
    if(!m_spTimer->DoneSegment(flCurTime,id,&flTime))
    {
      // for whatever reason this guy's ride didn't count
      flTime = 1e30f;
    }

    const int cCrossersBefore = m_lstFinishersPerLap[ixHisLap];
    float flScore = GetScoreForCross(m_flFirstPoints, cCrossersBefore, m_cTotalPeople);
    *pScoreData = SPRINTCLIMBDATA(m_eType,flScore,ixHisLap,flTime,m_meters,m_flPositionDistance,m_flRise);

    m_lstFinishersPerLap[ixHisLap]++;
    return true;
  }
  
  return false;
}

std::string SprintClimbImpl::GetDescription(int ixLap)
{
  stringstream ss;
  ss<<m_strName<<" (lap "<<(ixLap+1)<<")"<<endl;
  return ss.str();
}
// tells the scoring source how many people to use for scoring calculations.  This will be called at the start of the race, once the server knows how many starters there are
void SprintClimbImpl::SetPeopleCount(int cPeople)
{
  if(cPeople != m_cTotalPeople)
  {
    m_cTotalPeople = cPeople;
    DASSERT(m_lstFinishersPerLap.size() == 0); // only do this before stuff starts happening!
  }
}
void SprintClimbImpl::SetMaxPoints(float flPoints)
{
  m_flFirstPoints = flPoints;
  DASSERT(m_lstFinishersPerLap.size() == 0); // only do this before stuff starts happening!
}

// gets the key description of this thing (position, points, type, and name) for network transmission
void SprintClimbImpl::GetRaw(SPRINTCLIMBDATA_RAW* pRaw) const
{
  pRaw->eScoreType = m_eType;
  pRaw->flOrigDistance = this->m_flPosition;
  pRaw->flOrigLeadInDist = this->m_flLeadInDist;
  pRaw->flMaxPoints = this->m_flFirstPoints;
  pRaw->fIsFinish = this->m_fFinishMode;

  strncpy(pRaw->szName,m_strName.c_str(),sizeof(pRaw->szName)-1);
}

float SprintClimbImpl::GetNextPoints(int iLap) const
{
  const int cCrossersBefore = iLap < m_lstFinishersPerLap.size() ? m_lstFinishersPerLap[iLap] : 0;
  return GetScoreForCross(m_flFirstPoints,cCrossersBefore,m_cTotalPeople);
}