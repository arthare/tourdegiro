// GameAI.cpp : Defines the exported functions for the DLL application.
//
#include <string>
#include <iostream>
#include <vector>
#include <map>

#include <unordered_map>
using namespace std;

#include <algorithm>
#include <list>
#include <sstream>
#include <fstream>
#include <set>

#include <boost/uuid/uuid.hpp>
#include <boost/shared_ptr.hpp>

#include "GameAI.h"
#include "../TourDeGiroData/TDGInterface.h"
#include "../ArtLib/ArtTools.h"
#include <string>
#include <map>
#include <algorithm>

using namespace std;

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

enum SMARTAI_STATE
{
  LEECH, // keep power-15% until 5-minute average < ratedPower-20%
  PULL, // power + 15% until 5-minute average crosses ratedPower-10% or group size grows past 3
  SPRINT, // when less than 1km left, output a power that will leave us at ratedPower for our overall average at the end
};

SprintClimbPointPtr FindSprintClimb(float flCurrentPlayerDistance, const IMap* pMap, SCORETYPE eDesiredType)
{
  SprintClimbPointPtr spSprintImIn;
  vector<SprintClimbPointPtr> lstSprintClimbs;
  pMap->GetScoringSources(lstSprintClimbs);
  for(unsigned int x = 0;x < lstSprintClimbs.size(); x++)
  {
    SPRINTCLIMBDATA_RAW raw;
    lstSprintClimbs[x]->GetRaw(&raw);
    if(raw.eScoreType != eDesiredType) continue;

    const float flDistEnd = pMap->GetDistanceOfOrigDist(raw.flOrigDistance);
    const float flDistStart = pMap->GetDistanceOfOrigDist(raw.flOrigDistance - raw.flOrigLeadInDist);
    if(flCurrentPlayerDistance >= flDistStart && flCurrentPlayerDistance < flDistEnd)
    {
      // found it!
      return lstSprintClimbs[x];
    }
  }
  return SprintClimbPointPtr();
}

class SmartAI : public IAI
{
public:
  SmartAI(int iPower) : IAI("Savey",iPower),m_flRatedPower((float)iPower),m_eState(LEECH),m_tmFirstMeter(0) 
  {
    //m_flPullPower = RandDouble() * 0.5 + 1.0; // pull: 100-150%
    //m_flLeechPower = RandDouble() * 0.5 + 0.5; // leech: 50-100%
    //m_flSprintFraction = RandDouble() * 0.5;
    m_flPullPower = 1.48f;
    m_flLeechPower = 0.89f;
    m_flSprintFraction = 0.23f;
  };
  virtual ~SmartAI() {};

  virtual int GetWatts(const float tmNow, const IPlayer* pMe, const std::vector<IConstPlayerPtrConst>& lstOthers, const IMap* pMap) ARTOVERRIDE
  {
    float sCurrentPower = 0;

    const PLAYERDIST flDistance = pMe->GetDistance();
    const PLAYERDIST flDistLeft = pMap->GetEndDistance().Minus(flDistance);
    const float flMapLength = (pMap->GetEndDistance().Minus(pMap->GetStartDistance())).ToMeters();
    const float flSprintDist = min(flMapLength*m_flSprintFraction,5000.0f);


    PLAYERDIST flClosestAhead = PLAYERDIST(pMap->GetLaps(),0,pMap->GetLapLength());
    PLAYERDIST flClosestBehind = PLAYERDIST(pMap->GetLaps(),0,pMap->GetLapLength());
    float flClosestAheadSpeed = 0;
    for(unsigned int x = 0;x < lstOthers.size(); x++)
    {
      PLAYERDIST flOtherAhead = lstOthers[x]->GetDistance().Minus(flDistance);
      if(flOtherAhead.ToMeters() > 0)
      {
        flClosestAheadSpeed = lstOthers[x]->GetSpeed();
        flClosestAhead = min(flOtherAhead,flClosestAhead);
      }
      else
      {
        flClosestBehind = min(-flOtherAhead,flClosestBehind);
      }
    }

    
    SprintClimbPointPtr spCurrentSprintClimb = FindSprintClimb(flDistance.flDistance,pMap,SCORE_SPRINT);
    if(spCurrentSprintClimb)
    {
      // holy crap, we're in a sprint right now!  let's KILL THIS SHIT
      const float flCurrentAverage = pMe->GetAveragePower();
      if(flCurrentAverage > m_flRatedPower*1.15)
      {
        // we've spent too much energy, so let's act normal
      }
      else
      {
        // we have space to waste energy!  LET'S KILL IT
        const float flDistanceToGuyAhead = flClosestAhead.ToMeters();
        if(flDistanceToGuyAhead > 100)
        {
          // we aren't catching that guy.  Let's check out what's happening behind us
          const float flDistanceToGuyBehind = flClosestBehind.ToMeters();
          if(flDistanceToGuyBehind < 20)
          {
            // that guy is too close for comfort...  SPRINT
            return m_flRatedPower * m_flPullPower;
          }
          else
          {
            // he won't catch us...
            return m_flRatedPower * m_flLeechPower;
          }
        }
        else
        {
          // we might catch that guy ahead of us!
          return m_flRatedPower * m_flPullPower;
        }
      }
    }

    switch(m_eState)
    {
    case LEECH:
      sCurrentPower = m_flRatedPower*m_flLeechPower;
      if(flDistLeft.ToMeters() < flSprintDist)
      {
        m_eState = SPRINT;
      }
      else if((flClosestAhead.ToMeters() <= 30 && flClosestAhead.ToMeters() >= 10) || // if there are guys within spitting distance of us
        (flClosestAhead.ToMeters() <= 10 && flClosestAheadSpeed > pMe->GetSpeed()+0.5)) // or the draftable guys immediately ahead are getting away
      {
        //cout<<"Sprinter pulling because there are dudes ahead or close dudes getting away avg = "<<GetAveragePower()<<endl;
        // we're within spitting distance of the guy ahead, so PULL
        m_eState = PULL;
      }
      break;
    case PULL:
      sCurrentPower = m_flRatedPower*m_flPullPower;
      if(flDistLeft.ToMeters() < flSprintDist)
      {
        //cout<<"Sprinter sprinting avg = "<<GetAveragePower()<<endl;
        m_eState = SPRINT;
      }
      else if(pMe->GetAveragePower() > m_flRatedPower*1.15f)
      {
        m_eState = LEECH;
      }
      else if(flClosestAhead.ToMeters() > 30)
      {
        //cout<<"Sprinter leeching because there's no-one ahead avg = "<<GetAveragePower()<<endl;
        m_eState = LEECH;
      }
      else if(flClosestAheadSpeed < pMe->GetSpeed()-0.3 && flClosestAhead.ToMeters() < 5)
      {
        //cout<<"Sprinter leeching because he's in a nice draft zone avg = "<<GetAveragePower()<<endl;
        m_eState = LEECH;
      }
      break;
    case SPRINT:
      const float flClimbLeft = pMap->GetElevationAtDistance(pMap->GetEndDistance()) - pMap->GetElevationAtDistance(flDistance);
      const float flClimbJLeft = flClimbLeft * pMe->GetMassKg() * 9.81f;
      const float flTimeEstimate = flDistLeft.ToMeters() / 10.0f + flClimbJLeft / m_flRatedPower;
      const float flJSpent = pMe->GetEnergySpent();
      const float flTimeSpent = pMe->GetTimeRidden();
      const float flJPermissible = (flTimeSpent + flTimeEstimate) * m_flRatedPower; // how many J do we think we can spend on this map?
      const float flJLeft = flJPermissible - flJSpent;
      if(flTimeEstimate != 0)
      {
        sCurrentPower = max(0.0f,flJLeft / flTimeEstimate);
      }
      else
      {
        sCurrentPower = 0;
      }
      sCurrentPower = min(m_flRatedPower * 1.3f,(float)sCurrentPower); // don't let this savey put out more than 50% more than his rated power
      sCurrentPower = min(900,sCurrentPower); // don't let it get too ridiculous
      break;
    }
    return (int)sCurrentPower;
  }
private:
  SMARTAI_STATE m_eState;
  const float m_flRatedPower;
  unsigned int m_tmFirstMeter;
  string m_strName;
  float m_flPullPower;
  float m_flLeechPower;
  float m_flSprintFraction;
};


// a player that will generally choose to draft, then will blast the final km of the course
class HelperAI: public IAI
{
public:
  HelperAI(int iPower) : IAI("Helpy",iPower),m_idFriendDontSetDirectly(-1),m_flRatedPower((float)iPower),m_eState(LEECH),m_tmFirstMeter(0),m_tmLastFriendChange(0)
  {
  };
  virtual ~HelperAI() {};

  virtual int GetWatts(const float tmNow, const IPlayer* pMe, const std::vector<IConstPlayerPtrConst>& lstOthers, const IMap* pMap) ARTOVERRIDE
  {
    float sCurrentPower = 0;

    const float flFinishDist = min(5000,(pMap->GetEndDistance().ToMeters() - pMap->GetStartDistance().ToMeters())*0.2f);
    if(pMe->GetDistance().ToMeters() > pMap->GetEndDistance().ToMeters() - flFinishDist)
    {
      // we're near the finish.  work to make sure we hit our average
      const float flDistLeft = pMap->GetEndDistance().ToMeters() - pMe->GetDistance().ToMeters();
      const float flClimbLeft = pMap->GetElevationAtDistance(pMap->GetEndDistance()) - pMap->GetElevationAtDistance(pMe->GetDistance());
      const float flClimbJLeft = flClimbLeft * pMe->GetMassKg() * 9.81f;
      const float flTimeEstimate = flDistLeft / 10 + flClimbJLeft / m_flRatedPower;
      const float flJSpent = pMe->GetEnergySpent();
      const float flTimeSpent = pMe->GetTimeRidden();
      const float flJPermissible = (flTimeSpent + flTimeEstimate) * m_flRatedPower; // how many J do we think we can spend on this map?
      const float flJLeft = flJPermissible - flJSpent;
      if(flTimeEstimate != 0)
      {
        sCurrentPower = max(flJLeft / flTimeEstimate,0);
      }
      else
      {
        sCurrentPower = 0;
      }
      sCurrentPower = min(m_flRatedPower * 1.3f,sCurrentPower); // don't let this savey put out more than 50% more than his rated power
      sCurrentPower = min(900,sCurrentPower); // don't let it get too ridiculous
      return (int)sCurrentPower;
    }
    else if(lstOthers.size() <= 0) 
    {
      sCurrentPower = m_flRatedPower; // just cruise
      return (int)sCurrentPower;
    }
    // first, check if our friend is still connected
    IConstPlayerPtrConst pFriend;
    for(unsigned int x = 0;x < lstOthers.size() && GetFriendId() != INVALID_PLAYER_ID; x++)
    {
      if(lstOthers[x]->GetId() == GetFriendId())
      {
        pFriend = lstOthers[x];
        break;
      }
    }

    // now let's check if our friend has gotten too far ahead of us.  if he has, we'll want to find a new friend
    if(pFriend && 
       (pFriend->GetDistance().ToMeters() > pMe->GetDistance().ToMeters() + 40 ||
        pMe->GetDistance().ToMeters() > pFriend->GetDistance().ToMeters() + 50) )
    {
      // our friend is way ahead or way behind.  Not very friendly...
      pFriend.reset();
      SetFriendId(INVALID_PLAYER_ID);
    }


    if((GetFriendId() == INVALID_PLAYER_ID || pFriend == NULL) && tmNow - m_tmLastFriendChange > 5)
    {
      // we don't have a friend :-(.  We should find a friend.
      // we want a friend who is nearby
      // we would prefer that friend be human
      vector<IConstPlayerPtrConst> lstFriends = lstOthers;
      FriendSorter fs(pMe);
      sort(lstFriends.begin(), lstFriends.end(), fs);
      pFriend = lstFriends[0];

      SetFriendId(pFriend->GetId());
      m_tmLastFriendChange = tmNow;
    }


    // finally, the friend logic.  If the friend is ahead, we want to try to catch up.  if the friend is between 2-10m behind, we want to give'r
    // if the friend is more than that behind, we want to slow down.  if we're in the final stage of the race, we should work to
    // maintain our average
    if(pFriend)
    {
      if(pFriend->GetDistance().ToMeters() > pMe->GetDistance().ToMeters() - 2.5)
      {
        // friend is ahea or bumping into me: try to catch them!
        sCurrentPower = m_flRatedPower*1.3f;
      }
      else if(pFriend->GetDistance().ToMeters() >= pMe->GetDistance().ToMeters() - 6 && pFriend->GetDistance().ToMeters() < pMe->GetDistance().ToMeters() - 2.5)
      {
        // friend is close behind.  Hooray, we're doing our job
        sCurrentPower = m_flRatedPower*1.15f;
      }
      else
      {
        // friend is out of the draft, slow down to let them catch up
        sCurrentPower = m_flRatedPower*0.3f;
      }
    }
    else
    {
      sCurrentPower = m_flRatedPower;
    }
    return (int)sCurrentPower;
  }

  class FriendSorter
  {
  public:
    FriendSorter(const IPlayer* pMe) : m_me(pMe) {};

    bool operator () (const IConstPlayer* p1, const IConstPlayer* p2) const
    {
      float fl1 = RateFriend(p1);
      float fl2 = RateFriend(p2);
      return fl1 < fl2;
    }
    bool operator () (IConstPlayerPtrConst p1, IConstPlayerPtrConst p2) const
    {
      float fl1 = RateFriend(p1.get());
      float fl2 = RateFriend(p2.get());
      return fl1 < fl2;
    }
  private:
    // lower scores -> better friends
    float RateFriend(const IConstPlayer* p) const
    {
      if(p == m_me) return 1e30f; // we always want to sort to the back
      if(p->Player_IsAI())
      {
        if(m_me->GetAIType() == p->GetAIType())
        {
          return 1e30f; // helpys shouldn't choose other helpys as friends
        }
      }
      // the friend has to be close to us, preferably behind us
      // the friend is preferred to be human
      // lower scores are better
      float flScore = m_me->GetDistance().flDistance - p->GetDistance().flDistance;
      if(flScore < 0)
      {
        // they're ahead of us.  We may not want this guy as a friend
        flScore *= -5;
      }

      if(p->Player_IsAI())
      {
        flScore *= 10; // if they're an AI, we don't want to be a friend.
      }
      if(HelperAI::s_mapHelpyVictims[p->GetId()] > 1) flScore *= 10; // this guy already has lots of helpy friends.
      return flScore;
    }
  private:
    const IPlayer* m_me;
  };
private:
  int GetFriendId() const {return m_idFriendDontSetDirectly;}
  void SetFriendId(int id)
  {
    if(m_idFriendDontSetDirectly != INVALID_PLAYER_ID)
    {
      s_mapHelpyVictims[m_idFriendDontSetDirectly]--;
    }
    m_idFriendDontSetDirectly = id;

    if(m_idFriendDontSetDirectly != INVALID_PLAYER_ID)
    {
      s_mapHelpyVictims[m_idFriendDontSetDirectly]++;
    }
  }
private:
  static unordered_map<int,int> s_mapHelpyVictims; // who are the helpys helping?
  SMARTAI_STATE m_eState;
  const float m_flRatedPower;
  float m_tmFirstMeter;
  float m_tmLastFriendChange;

  int m_idFriendDontSetDirectly;
};
unordered_map<int,int> HelperAI::s_mapHelpyVictims;

// a player that outputs a constant wattage for testing
class DopedPlayer : public IAI
{
public:
  DopedPlayer(int iPower) : IAI("Dopey",iPower),m_iDopePower(iPower) 
  {
  };
  virtual ~DopedPlayer() {};
  virtual int GetWatts(const float tmNow, const IPlayer* pMe, const std::vector<IConstPlayerPtrConst>& lstOthers, const IMap* pMap) 
  {
    float flSlope = pMap->GetSlopeAtDistance(pMe->GetDistance());
    if(flSlope < 0)
    {
      return m_iDopePower*(1+flSlope);
    }
    else
    {
      return m_iDopePower;
    }
  }
private:
  int m_iDopePower;
  string m_strName;
};

// hill man: maintains his dopey power, but increases by twice the hill slope.  So if he's climbing a 10% slope and has base power 300W, he'll rock 330W.  Likewise going downhill
class HillMan : public IAI
{
public:
  HillMan(int iPower) : IAI("HillMan",iPower),m_iDopePower(iPower) 
  {
  };
  virtual ~HillMan() {};
  virtual int GetWatts(const float tmNow, const IPlayer* pMe, const std::vector<IConstPlayerPtrConst>& lstOthers, const IMap* pMap) 
  {
    float flSlope = pMap->GetSlopeAtDistance(pMe->GetDistance());
    return (int)( (float)m_iDopePower * (1+flSlope));
  }
private:
  int m_iDopePower;
};


// surgey: maintains the specified power, but does it as 3 seconds of high power, then 6 seconds of half power
class Surgey : public IAI
{
public:
  Surgey(int iPower) : IAI("Surgey",iPower),m_iDopePower(iPower) 
  {
    m_iInterval = ((rand() % 10)+25)*1000;
  };
  virtual ~Surgey() {};
  virtual int GetWatts(const float tmNow, const IPlayer* pMe, const std::vector<IConstPlayerPtrConst>& lstOthers, const IMap* pMap) 
  {
    // 3*high + 6*low = 9*m_iDopePower
    // low = 0.90 * m_iDopePower
    // thus:
    // 3*high = 3.6*m_iDopePower
    // high = 1.2*m_iDopePower
    const int low = m_iDopePower *0.9;
    const int high = m_iDopePower * 1.2;
    
    unsigned int tmNowTicks = (unsigned int)(tmNow * 1000);
    const int coastInterval = m_iInterval*3;
    tmNowTicks = tmNowTicks % coastInterval;
    if(tmNowTicks >= 0 && tmNowTicks <= m_iInterval)
    {
      return high; // 30 out of 90 seconds, do high power
    }
    else
    {
      return low; // 60 out of 90 seconds, do low power
    }
  }
private:
  int m_iDopePower;
  int m_iInterval; // our surge interval in ms
};


namespace GameAI
{
  DllExport int GetAICount(void)
  {
	  return 9;
  }
  IAI* GetAI(int ix, int iTargetWatts)
  {
    switch(ix)
    {
    case 0:
    case 1:
      return new Surgey(iTargetWatts);
    case 2:
    case 3:
      return new DopedPlayer(iTargetWatts);
    case 4:
    case 5:
      return new ::SmartAI(iTargetWatts);
    case 6:
    case 7:
      return new ::HillMan(iTargetWatts);
    case 8:
      return new ::HelperAI(iTargetWatts);
    }
    return NULL;
  }
  void FreeAI(IAI* pAI)
  {
    delete pAI;
  }
} // end namespace