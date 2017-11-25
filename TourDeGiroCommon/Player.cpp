
#include "stdafx.h"
#include "Player.h"
#include "CommStructs.h"
#include "PlayerConsts.h"
#include "../GameAI/GameAI.h"

#define PI 3.14159
float ASSUMED_AVERAGE_POWER = 150; // for players where we don't have knowledge of their historic power, just assume 150
//const float IPlayer::DEFAULT_CDA = 0.3f;
float Player::GetCDA(PHYSICSMODE ePhysics, float flMassKg)
{
  CASSERT(PHYSICSMODE_LAST == 7);
  switch(ePhysics)
  {
  case REALMODEL_2013:
  case WEIGHTHANDICAPPED_2013:
  case FTPHANDICAPPED_2013:
  case FTPHANDICAPPED_NODRAFT_2013:
  default:
    return 0.3; // the old DEFAULT_CDA
  case REALMODEL_LATE2013_NODRAFT:
  case REALMODEL_LATE2013:
    // see CDACalc.xls.  It indicated that CDA gains at about 0.0026 per kilogram gained
    // we want it to be 0.3 @ 80kg
    // y = 0.0026x + b
    // 0.3 = s(80) + b
    // 0.3 - 80s = b
    // b = 0.3 - 80s
    // b = 0.092
    return 0.0025 * flMassKg + 0.1;
  case TRAININGMODE:
    DASSERT(FALSE); // uhhh... you're running a race with "training" physics?
    return 0.4;
  }
}
const float IPlayer::DEFAULT_CRR = 0.0033f;
const float IPlayer::DEFAULT_MASS = 80.0f;


bool operator < (const TIMEPOINT& t1, const TIMEPOINT& t2)
{
  return t1.flDistance < t2.flDistance;
}
bool operator < (const TIMEPOINT& t1, const PLAYERDIST& flDist)
{
  return t1.flDistance < flDist;
}

const float flDiv = (1/CYCLIST_WIDTH);
inline float GetShiftMod(float flXShift)
{
  // y = mx + b
  // y = (1/CYCLIST_WIDTH)x + 1 on one side
  DASSERT(abs(flXShift <= CYCLIST_WIDTH));
  if(flXShift < 0)
  {
    return flDiv*flXShift + 1;
  }
  else
  {
    return -flDiv*flXShift + 1;
  }
}


// returns -1.0f for 100% brake, 1.0f for 100% draft
float GetDraft(const float flMe, float flMyLane, const float flOther, float flOtherLane, bool fDoBrake)
{
  //The idea here is that if fDoBrake is set, they're interested simply in the VALUE of being in a certain spot.  So we want to return negative values if they're going to be on top of a guy to dissuade them from being there.
  //If fDoBrake is not set, then they're interested in the actual drafting value of being in a certain spot.

  //if(flOther < flMe) return 0; // they're too far behind to affect us
  //if(flOther - flMe > 10) return 0; // they're too far ahead to affect us
  //if(flOther == flMe) return 0; // they are me (or at least, they're right on top of us

  // this isn't ourself.
  // the lead of the other guy's rear wheel over the current guy's front wheel
  float flOtherLead = fDoBrake ? (flOther - CYCLIST_LENGTH - flMe) : (flOther - flMe);
  DASSERT(flOtherLead >= -CYCLIST_LENGTH && flOtherLead <= 10.0f);
  if(flOtherLead > 0)
  {
    // the other guy's rear wheel is indeed ahead of us
    float flDraft = GetDraftFromDistance(flOtherLead);
    DASSERT(flDraft >= 0 && flDraft < 0.4);

    // flDraft represents the absolute best draft we could get behind lstOthers[x].  But what if we're not centered behind the draftee?
    const float flShiftMod = GetShiftMod(flMyLane - flOtherLane);
    DASSERT(flShiftMod >= 0 && flShiftMod <= 1.0f);
    flDraft *= flShiftMod; // make the draft calculation only count when we're directly behind the other guy

    return flDraft;
  }
  else if(fDoBrake && flOtherLead < 0 && flOtherLead >= -CYCLIST_LENGTH && GetShiftMod(flMyLane - flOtherLane) > 0)
  {
    // there's a guy on top of us, so return 10% brake
    return -0.1f;
  }
  else
  {
    // the other guy is so far ahead or behind that drafting doesn't really matter
    return 0;
  }
}

bool IsGhostPair(const IPlayer* pPlayer1, const IPlayer* pPlayer2)
{
  const int id1 = pPlayer1->GetId() | GHOST_BIT;
  const int id2 = pPlayer2->GetId() | GHOST_BIT;
  return id1 == id2;
}

void RunPhysics(PHYSICSMODE ePhysics, RACEMODE eRaceMode, const vector<IConstPlayerPtrConst>& lstOthers, const IConstPlayer* pCurrent, float flMaxSpeed, float flSlope, float flPower, float flMass, float flRho, float flSpeed, float cda, float crr, float dt, float* pflResultSpeed, float* pflPosDelta, float* pflAchievedDraft, float* pflAchievedDraftNewtons)
{
  CASSERT(PHYSICSMODE_LAST == 7);
  DASSERT(ePhysics != TRAININGMODE);

  if(flMass == 0) flMass = Player::DEFAULT_MASS;

  if(ePhysics == WEIGHTHANDICAPPED_2013)
  {
    flPower = (flPower / flMass) * Player::DEFAULT_MASS;
    flMass = Player::DEFAULT_MASS;
  }
  else if(ePhysics == FTPHANDICAPPED_2013 || ePhysics == FTPHANDICAPPED_NODRAFT_2013)
  {
    if(pCurrent->Player_IsAI())
    {
      // AIs can just do their thang
    }
    else
    {
      const float flCurrentWKg = flPower / flMass;
      const float flBestWKg = pCurrent->GetBestStatsConst().GetOriginalHistoric();
      if(flBestWKg > 0)
      {
        const float flPct = flCurrentWKg / flBestWKg;
        flPower = flPct * 300;
        flMass = Player::DEFAULT_MASS;
      }
      else
      {
        // just let them do whatever.  They'll get a originalhistoric eventually...
      }
    }
  }

  const float flDistance = flSpeed;
  float flForce = abs(flDistance) >= Player::MIN_SPEED ? flPower / abs(flDistance) : flMass;

  float flNormal = 0;
  float flAero = 0;
  float flRolling = 0;
  {// first: normal force
    const float flAngle= (float)(PI/2 - atan(flSlope));
    const float flForce = (float)(-cos(flAngle)*9.8*flMass);
    flNormal = flForce;
  }
  
  { // aero force
    // aero drag = 0.5 * rho * v^2 * cda
    flAero = (float)(-0.5f * flRho * pow(flSpeed,2) * cda);
  }
  const float flCurDist = pCurrent->GetDistance().flDistance;

  if(eRaceMode == RACEMODE_ROADRACE && IsDraftingMode(ePhysics))
  {
    float flDragRatio = 0; // represents the % reduction in aero drag from drafting
    {
      int ixLast = lstOthers.size(); // index of the last player that matters for drafting

      {
        int ixLow = 0;
        int ixHigh = lstOthers.size();
        while(true) // hunting for first player that matters for drafting
        {
          const int ixTest = (ixLow + ixHigh)/2;
          const IConstPlayer* pPlayer = lstOthers[ixTest].get();
          const float& flDist = pPlayer->GetDistance().flDistance;
          if(ixLow >= ixHigh-1)
          {
            // done
            ixLast = ixTest;
            break;
          }
          else if(flDist < flCurDist)
          {
            // this player is behind current player, so we need to check earlier to find the first guy
            ixHigh = ixTest;
          }
          else if(flDist >= flCurDist)
          {
            // this player is ahead of the current player, so we need to check earlier to find the first guy
            ixLow = ixTest;
          }
          else
          {
          }
        }
      }

      const float flDistTooFar = pCurrent->GetDistance().flDistance + 10.0f;

      const float& flCurLane = pCurrent->GetLane();
      for(int x = max(0,ixLast-1); x >= 0; x--)
      {
        const IConstPlayer* pOther = lstOthers[x].get();
        DASSERT(!(IS_FLAG_SET(pOther->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS)));

        if(pOther != pCurrent)
        {
          const float& flOtherDist = pOther->GetDistance().flDistance;
          if(flOtherDist >= flDistTooFar)
          {
            // this guy is too far ahead to get a draft.
            // since the list will be sorted with furthest-along-in-lap in the earlier indices, this means we're done!
            break;
          }
          const float& flOtherLane = pOther->GetLane();
          const float flLaneOffset = abs(flOtherLane-flCurLane);
          if(flLaneOffset >= CYCLIST_WIDTH) 
            continue; // we're too far left/right for this guy to have a drafting effect on us

          DASSERT(pOther->GetFinishTime() < 0); // you can't include finished players!  There's absolutely no need to be running physics on a finished player or to be using one in drafting calcs

          const float flDragThis = GetDraft(flCurDist, flCurLane, flOtherDist, flOtherLane, false );
          flDragRatio = max(flDragRatio,flDragThis);

        }
      }
    }
    *pflAchievedDraftNewtons = flDragRatio * flAero;
    DASSERT(*pflAchievedDraftNewtons <= 0);
    flAero *= (1-flDragRatio);
  
    *pflAchievedDraft = flDragRatio;
  }

  { // rolling resistance
    flRolling = (float)(-9.81 * flMass * crr);
  }

  flForce += (flAero + flRolling + flNormal);

  const float flAccel = flForce / flMass;
  flSpeed += flAccel*dt;
  *pflResultSpeed = max((float)Player::MIN_SPEED,flSpeed); // minimum speed 1km/h
  *pflResultSpeed = min(flMaxSpeed,*pflResultSpeed);
  *pflPosDelta = *pflResultSpeed*dt;
  DASSERT(!IsNaN(*pflPosDelta));
  DASSERT(!IsNaN(*pflResultSpeed));
}

void Player::Tick(float flRaceTime, int ixMe, PHYSICSMODE ePhysicsMode, RACEMODE eRaceMode, const vector<IConstPlayerPtrConst>& lstOthers, const DraftMap& mapWhoDraftingWho, float dt, SPRINTCLIMBDATA* pPointsScored, std::string* pstrPointDesc, const unsigned int tmNow)
{
  DASSERT(this->GetDistance() >= m_spMap->GetStartDistance() && this->GetDistance() <= m_spMap->GetEndDistance());
  DASSERT(ixMe >= 0 && ixMe < lstOthers.size()); // ixMe must represent this guy's index into lstOthers.  If he's not in lstOthers (like if he's a finished or spectating player), then I don't know why you're calling this function
  // power = force * distance
  // force = power / distance
  // distance = how far we went in dt
  float flNewSpeed = 0;
  float flDistDelta = 0;
  const int iPower = GetPower();


  if((GetActionFlags() & ACTION_FLAG_MOVES) == 0 || Player_IsAI() ) // they haven't given us a move order.
  {
    int iMoveDir = 0;
    switch(eRaceMode)
    {
    case RACEMODE_ROADRACE:
      iMoveDir = GetMoveDir(ixMe, lstOthers, mapWhoDraftingWho);
      break;
    default:
    case RACEMODE_TIMETRIAL:
      iMoveDir = 0;
      break;
    }
    
    if(iMoveDir > 0) SetActionFlags(ACTION_FLAG_LEFT, ACTION_FLAG_MOVES);
    else if(iMoveDir < 0) SetActionFlags(ACTION_FLAG_RIGHT, ACTION_FLAG_MOVES);
    else SetActionFlags(0, ACTION_FLAG_MOVES);
  }

  if(GetActionFlags() & ACTION_FLAG_RIGHT)
  {
    const float flShift = -dt * MAX_LANECHANGE_SPEED;
    m_flLane = max(m_flLane + flShift,-ROAD_WIDTH);
  }
  else if(GetActionFlags() & ACTION_FLAG_LEFT)
  {
    const float flShift = dt * MAX_LANECHANGE_SPEED;
    m_flLane = min(m_flLane + flShift,ROAD_WIDTH);
  }

  RunPhysics(ePhysicsMode,
              eRaceMode,
              lstOthers,
              this,
              m_spMap->GetMaxSpeedAtDistance(GetDistance()),
              m_spMap->GetSlopeAtDistance(GetDistance()),
              iPower,
              GetMassKg(),
              m_spMap->GetRhoAtDistance(GetDistance().flDistance),
              GetSpeed(),
              Player::GetCDA(ePhysicsMode,GetMassKg()),
              Player::DEFAULT_CRR,
              dt,
              &flNewSpeed,
              &flDistDelta,
              &m_flLastDraft,
              &m_flLastDraftNewtons);

  PlayerBase::Tick(dt);
  m_flSpeed = flNewSpeed;

  const PLAYERDIST distOld = m_dist;
  m_dist.AddMeters(flDistDelta);

  if(IsTimedMode(m_spMap->GetLaps(),m_spMap->GetTimedLength()))
  {
    // if we're timed-mode, just let the dude advance
  }
  else
  {
    m_dist = min(this->m_spMap->GetEndDistance(),m_dist); // don't go further than the map end
    m_dist = max(this->m_spMap->GetStartDistance(),m_dist); // don't somehow go backwards!
  }

  const PLAYERDIST distNew = m_dist;

  // handling lap data
  if(distNew.iCurrentLap > distOld.iCurrentLap)
  {
    // we just finished a lap!
    float flLastLapEnd = 0;
    if(m_lstLapTimes.size() > 0)
    {
      // it wasn't our first, so last-lap start will be different
      flLastLapEnd = m_lstLapTimes.back().tmStart + m_lstLapTimes.back().time;
    }
    const float flLapTime = GetTimeRidden() - flLastLapEnd;
    
    LAPDATA lap(flLapTime, GetEnergySpent()/1000.0, flLastLapEnd, ixMe);
    m_lstLapTimes.push_back(lap);
  }
  
  { // checking for sprint/climb points
    vector<SprintClimbPointPtr> lstSprintClimbs;
    m_spMap->GetScoringSources(lstSprintClimbs);
    if(lstSprintClimbs.size() > 0)
    {
      const ORIGDIST origOld = m_spMap->GetOrigDistAtDistance(distOld.flDistance);
      const ORIGDIST origNew = m_spMap->GetOrigDistAtDistance(distNew.flDistance);

      for(unsigned int ixPoint = 0; ixPoint < lstSprintClimbs.size(); ixPoint++)
      {
        SPRINTCLIMBDATA scoringData;
        if(lstSprintClimbs[ixPoint]->GetPointsEarned(origOld,origNew,distNew.iCurrentLap,distNew.iCurrentLap == m_spMap->GetLaps()-1,&scoringData,this->m_id,flRaceTime) && scoringData.eType != SCORE_NONE)
        {
          AddPoints(scoringData);
          *pPointsScored = scoringData;
          *pstrPointDesc = lstSprintClimbs[ixPoint]->GetDescription(distNew.iCurrentLap);
        }
      }
    }
  }
  
  if(!Player_IsAI())
  {
    for(map<STATID,boost::shared_ptr<TimeAverager> >::const_iterator i = m_mapTimers.begin(); i != m_mapTimers.end(); i++)
    {
      i->second->AddData(GetTimeRidden(),iPower);
    }
  }
}

// PlayerCompareForDraft: used to figure out where players should be for physics and drafting calcs
bool PlayerCompareForDraft(const IConstPlayer* i,const IConstPlayer* j)
{
  float flFinishI = i->GetFinishTime();
  float flFinishJ = j->GetFinishTime();

  if(flFinishI > 0 && flFinishJ <= 0) return true; // i has a finish time, j does not
  if(flFinishI <= 0 && flFinishJ > 0) return false; // j has a finish time, i does not
  // so either they both have finish times, or they both don't
  if(flFinishI > 0 && flFinishJ > 0) return flFinishI < flFinishJ; // they both have finish times, so go with whoever was faster

  DASSERT(flFinishI <= 0 && flFinishJ <= 0); // at this point, they should both NOT have finish times, so we should rank by distance
  if(i->GetDistance().flDistance != j->GetDistance().flDistance)
  {
    return i->GetDistance().flDistance > j->GetDistance().flDistance; // if i is ahead, then i should be ranked ahead
  }
  else
  {
    return i->GetId() < j->GetId();
  }
}
// PlayerCompareForRank: used to figure out where players should be for physics and drafting calcs
bool PlayerCompareForRank(const IConstPlayer* i,const IConstPlayer* j)
{
  float flFinishI = i->GetFinishTime();
  float flFinishJ = j->GetFinishTime();

  if(flFinishI > 0 && flFinishJ <= 0) return true; // i has a finish time, j does not
  if(flFinishI <= 0 && flFinishJ > 0) return false; // j has a finish time, i does not
  // so either they both have finish times, or they both don't
  if(flFinishI > 0 && flFinishJ > 0) return flFinishI < flFinishJ; // they both have finish times, so go with whoever was faster

  DASSERT(flFinishI <= 0 && flFinishJ <= 0); // at this point, they should both NOT have finish times, so we should rank by distance
  if(i->GetDistance() != j->GetDistance())
  {
    return i->GetDistance() > j->GetDistance(); // if i is ahead, then i should be ranked ahead
  }
  else
  {
    return i->GetId() < j->GetId();
  }
}

float GetDraftlessForce(float flSpeed, float flSlope, float flRho, float cda, float crr, float flMass)
{
  float flTotal = 0;
  {// first: normal force
    const float flAngle= (float)(PI/2 - atan(flSlope));
    const float flForce = (float)(-cos(flAngle)*9.8*flMass);
    flTotal += flForce;
  }

  { // aero force
    // aero drag = 0.5 * rho * v^2 * cda
    flTotal += (float)(-0.5f * flRho * pow(flSpeed,2) * cda);
  }
  { // rolling resistance
    flTotal += (float)(-9.81 * flMass * crr);
  }
  return flTotal;
}

vector<AIDLL> GrovelAIs()
{
  vector<AIDLL> lstAIs;

  AIDLL dll;
  dll.cAIs = GameAI::GetAICount();
  dll.pfnMakeAI = GameAI::GetAI;
  dll.pfnFreeAI = GameAI::FreeAI;
  dll.strDLL = L"Lib";
  dll.hMod = 0;

  lstAIs.push_back(dll);

  return lstAIs;
}

void BuildDraftingMap(const vector<IConstPlayerPtrConst>& lstToUpdate, DraftMap& mapDrafting)
{
  float flLastDist = 1e30;
  bool fLastFinished=lstToUpdate.size() > 0 ? lstToUpdate.front()->GetFinishTime() > 0 : false;
  for(unsigned int x = 0;x < lstToUpdate.size(); x++)
  {
    DASSERT(lstToUpdate[x]->GetDistance().flDistance <= flLastDist || lstToUpdate[x]->GetFinishTime() > 0 || fLastFinished);
    fLastFinished = lstToUpdate[x]->GetFinishTime() > 0;
    flLastDist = lstToUpdate[x]->GetDistance().flDistance; // each player in the list should be less far along than the player before
  }

  for(int x = lstToUpdate.size()-1; x >= 0; x--)
  {
    DRAFTINGINFO info;
    info.fIsSameTeam = false;

    IConstPlayerPtrConst pMe = lstToUpdate[x];
    for(int ixOther = x -1; ixOther >= 0; ixOther--)
    {
      IConstPlayerPtrConst pOther = lstToUpdate[ixOther];
      const float flOtherLead = pOther->GetDistance().flDistance - pMe->GetDistance().flDistance;
      DASSERT(flOtherLead >= 0 || pOther->GetFinishTime() > 0);
      if(flOtherLead > 10)
      {
        // they're too far ahead for us to be drafting
      }
      else if(pOther->GetLane() >= pMe->GetLane() - CYCLIST_WIDTH/2 && pOther->GetLane() <= pMe->GetLane() + CYCLIST_WIDTH/2)
      {
        if(pOther->GetTeamNumber() == pMe->GetTeamNumber())
        {
          info.fIsSameTeam = true; // they're drafting someone on the same team
          break;
        }
      }
    }
    mapDrafting[pMe] = info;
  }
}