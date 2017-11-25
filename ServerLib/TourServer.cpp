#include "stdafx.h"
#include "TourServer.h"
#include "ThreadedResultsSaver.h"

std::string GetServerLogName(int iServerId)
{
  char sz[100];
  snprintf(sz,sizeof(sz),"Server%d",iServerId);
  return sz;
}

int g_gameThdId=0;

// this sorts a vector of players by who should get an update.
// concept: we should send updates in proportion to a player's distance from the target multiplied by the time since we last sent an update to the target
// Distant riders shouldn't get frequent updates.  close riders should be updated very frequently.
class GenericUpdateCriterion
{
public:
  GenericUpdateCriterion(const vector<ConstServerPlayer>& lstCentral, DWORD tmNow) : m_lstCentral(lstCentral), m_tmNow(tmNow) {};

  bool operator() (const ConstServerPlayer& id1, const ConstServerPlayer& id2) const
  {
    /*for(unsigned int x = 0;x < m_lstCentral.size(); x++)
    {
      if(m_lstCentral[x] == id1) return true; // if one of the comparees is one of the local players for this client, then they always win
      if(m_lstCentral[x] == id2) return false;
    }*/
    float fl1 = BuildValue(id1);
    float fl2 = BuildValue(id2);
    return fl1 > fl2; // if the first id has a bigger value, it should be sorted first
  }
protected:
  virtual float BuildValue(const ConstServerPlayer& id) const = 0;
  vector<ConstServerPlayer> m_lstCentral;
  const DWORD m_tmNow;
};
class PosUpdateCriterion : public GenericUpdateCriterion
{
public:
  PosUpdateCriterion(const vector<ConstServerPlayer>& lstCentral, DWORD tmNow, const boost::shared_ptr<const IMap> spMap) : GenericUpdateCriterion(lstCentral,tmNow), m_spMap(spMap) {};

  static float GetValue(const vector<ConstServerPlayer>&lstCentral, DWORD tmNow, const boost::shared_ptr<const IMap> spMap, const ConstServerPlayer& sfFromPlayer)
  {
    float flMinDist = 1e30f;
    if(IS_FLAG_SET(sfFromPlayer.p->GetActionFlags(),ACTION_FLAG_SPECTATOR))
    {
      return 0; // spectators should NEVER get data sent about them
    }
    for(unsigned x = 0;x < lstCentral.size(); x++)
    {
      float flDist = abs(lstCentral[x].p->GetDistance().flDistance - sfFromPlayer.p->GetDistance().flDistance);
      if(lstCentral[x].p->GetDistance() > spMap->GetEndDistance() && sfFromPlayer.p->GetDistance() > spMap->GetEndDistance()) // if both the central guy AND the compared guy have finished, treat them as far apart, because they don't need pos updates
      {
        flDist = 5000; // we don't need frequent position updates for fellow finishers, so consider the distance between this central dude and this rider to be large
      }
      if(flDist < flMinDist)
      {
        flMinDist = flDist;
      }
    }

    float flTimeSinceUpdate = (float)sfFromPlayer.GetCachedTime(POSITION_UPDATE,lstCentral[0].p->GetId(),tmNow);
    if(flTimeSinceUpdate > 5000) flTimeSinceUpdate*=4; // prioritize people that haven't been talked about in a while
    if(flTimeSinceUpdate < MINIMUM_UPDATE_DELAY) 
    {
      return 0; // we don't want this guy anywhere near the top of the list if the recipient has already heard about him
    }
    float flDist = flMinDist;
    if(abs(flDist) < 5) flDist = 1.0f;

    return (10000/(flDist+0.1))*flTimeSinceUpdate;

  }
  virtual float BuildValue(const ConstServerPlayer& sfFromPlayer) const ARTOVERRIDE
  {
    return GetValue(m_lstCentral, m_tmNow, m_spMap, sfFromPlayer);
  }
private:
  const boost::shared_ptr<const IMap> m_spMap;
};
class NameUpdateCriterion : public GenericUpdateCriterion
{
public:
  NameUpdateCriterion(const vector<ConstServerPlayer>& lstCentral, DWORD tmNow) : GenericUpdateCriterion(lstCentral,tmNow) {};
  virtual float BuildValue(const ConstServerPlayer& sfFromPlayer) const ARTOVERRIDE
  {
    if(IS_FLAG_SET(sfFromPlayer.p->GetActionFlags(),ACTION_FLAG_SPECTATOR))
    {
      return 1e30f; // spectators should NEVER get data sent about them
    }
    // for names, the only thing that matters is the last time we sent the update.  We don't care how close someone is to the player
    const float flTimeSinceUpdate = sfFromPlayer.GetTimeSinceLastSend(NAME_UPDATE,m_lstCentral[0].p->GetId(),m_tmNow);
    return flTimeSinceUpdate;
  }
};
class ResultUpdateCriterion : public GenericUpdateCriterion
{
public:
  ResultUpdateCriterion(const vector<ConstServerPlayer>& lstCentral, DWORD tmNow) : GenericUpdateCriterion(lstCentral,tmNow) {};
  virtual float BuildValue(const ConstServerPlayer& sfFromPlayer) const ARTOVERRIDE
  {
    if(IS_FLAG_SET(sfFromPlayer.p->GetActionFlags(),ACTION_FLAG_SPECTATOR))
    {
      return 1e30f; // spectators should NEVER get data sent about them (also... spectators can't finish)
    }
    // for result, the only thing that matters is the last time we sent the update.  We don't care how close someone is to the player
    const float flTimeSinceUpdate = sfFromPlayer.GetTimeSinceLastSend(RESULT_UPDATE,m_lstCentral[0].p->GetId(),m_tmNow);
    return flTimeSinceUpdate;
  }
};

bool StateData::AddDoper(const int iPower, const int massKg, const std::string& name, bool fIsFrenemy, const vector<AIDLL>& lstAIs)
{
  vector<AISELECTION> lstPossibilities;
  for(unsigned int x = 0;x < lstAIs.size(); x++)
  {
    for(int y = 0;y < lstAIs[x].cAIs; y++)
    {
      lstPossibilities.push_back(AISELECTION(x,y));
    }
  }
  
  const int ix = rand() % lstPossibilities.size();
  const AISELECTION& ai = lstPossibilities[ix];
  const AIDLL& dll = lstAIs[ai.ixDLL];
  IAI* pBrain = dll.pfnMakeAI(ai.ixAI,iPower);

  std::string strName = name.length() > 0 ? name : pBrain->GetName();

  ServerPlayer sfDoper(IPlayerPtr(new AIBase(m_tmNow, pBrain,(DOPERTYPE)ai.ixAI,::AI_MASTER_ID,strName,m_spMapConst,ai, fIsFrenemy)));

  DASSERT(fIsFrenemy == sfDoper.p->Player_IsFrenemy());

  ((Player*)sfDoper.p.get())->InitPos(m_tmNow, 
                                      m_spMapConst->GetStartDistance(),
                                      m_spMapConst->GetStartDistance(),
                                      0,
                                      0,
                                      massKg ? massKg : Player::DEFAULT_MASS,
                                      m_pStatStore.get(),
                                      0,
                                      AI_POWER,
                                      0); // in the future, this could be something like "dead last current player -500m", so that new connections would start at the end of the race
  if(AddLocal(sfDoper))
  {
    cout<<"Added "<<sfDoper.p->GetName()<<" is frenemy? "<<sfDoper.p->Player_IsFrenemy()<<endl;
    return true;
  }
  else
  {
    cout<<sfDoper.p->GetName()<<" ("<<sfDoper.p->GetId()<<") was already present"<<endl;
    sfDoper.p.reset();
    return false;
  }
}

// SCHEDULE CHECKER
////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////
DWORD WINAPI ScheduleThreadProc(LPVOID pv)
{
  ScheduleChecker* pServer = (ScheduleChecker*)pv;
  pServer->ThreadProc();
  return 0;
}

ScheduleChecker::ScheduleChecker(const int iServerId, StatsStorePtr pStats) 
  : m_pStats(pStats),
    m_iServerId(iServerId),
    m_fChangedSinceLastCheck(false),
    m_fShutdown(false)
{
  m_hThread = boost::thread(ScheduleThreadProc,this);
}
ScheduleChecker::~ScheduleChecker()
{
  Shutdown();
  m_hThread.join();
}
void ScheduleChecker::Shutdown()
{
  m_fShutdown = true;
}
void ScheduleChecker::ThreadProc()
{
  const int CHECK_RATE = 7500;
  unsigned int tmLastChecked = 0;
  while(!m_fShutdown)
  {
    const unsigned int tmNow = ArtGetTime();
    if(tmNow - tmLastChecked > CHECK_RATE)
    {
      // time to check if we have a scheduled race coming up
      tmLastChecked = tmNow;
      SCHEDULEDRACE sfRace;
      m_pStats->GetNextScheduledRace(m_iServerId, &sfRace);
      if(sfRace.IsValid())
      {
        AutoLeaveCS _cs(m_cs);
        // whoa, a valid race!  let's see if it is sooner than our current one
        DWORD tmUnixNow = ::GetSecondsSince1970GMT();
        if(m_upcoming.tmStart < tmUnixNow - 300)
        {
          //cout<<"my current race is too old (race is at "<<m_upcoming.tmStart<<", now is "<<tmUnixNow<<").  Killing"<<endl;
          m_upcoming = SCHEDULEDRACE();
        }
        if(!m_upcoming.IsValid() || sfRace.tmStart < m_upcoming.tmStart || (sfRace.scheduleId == m_upcoming.scheduleId && sfRace.tmStart != m_upcoming.tmStart) )
        {
          //cout<<"Scheduled race detected: "<<sfRace.strRaceName<<" on map "<<sfRace.iMapId<<endl;
          // either we don't have an upcoming race or the new race is sooner than our previously-scheduled upcoming race
          m_upcoming = sfRace;
          m_fChangedSinceLastCheck = true;
        }
        else
        {
          //cout<<"next scheduled race is not before current upcoming race: "<<sfRace.tmStart<<" >= "<<m_upcoming.tmStart<<endl;
        }
      }
      else
      {
        //cout<<GetCurrentThreadId()<<": "<<"Next scheduled race ("<<sfRace.scheduleId<<") is not valid"<<endl;
      }
    }
    ArtSleep(CHECK_RATE / 3);
  }
}
  
bool ScheduleChecker::AddScheduledRace(const SCHEDULEDRACE& sr)
{
  AutoLeaveCS _cs(m_cs);
  m_upcoming = sr;
  m_fChangedSinceLastCheck = true;
  return true;
}
bool ScheduleChecker::GetNextScheduledRace(SCHEDULEDRACE* pOut)
{
  AutoLeaveCS _cs(m_cs);
  bool fRet = false;
  if(m_upcoming.IsValid())
  {
    fRet = true;
    *pOut = m_upcoming;
    m_fChangedSinceLastCheck = false;
  }
  return fRet;
}


DWORD WINAPI _GameThdProc(LPVOID pv)
{
  TourServer* pServer = (TourServer*)pv;
  return pServer->GameThdProc();
}

void StateData::DoIdlenessCheck(const ServerPlayer& p, DWORD tmNow)
{
  if(IS_FLAG_SET(p.p->GetActionFlags(), ACTION_FLAG_SPECTATOR))
  {
    return; // nothing to do, spectators can't be idle
  }

  if(p.p->GetDistance() >= m_spMapConst->GetStartDistance() && p.p->GetDistance() < m_spMapConst->GetEndDistance())
  {
    // this human is still on-course
    const DWORD dwLastPowerTime = ((Player*)p.p.get())->GetLastPowerTime();
    if(tmNow - dwLastPowerTime > 60000)
    {
      if((p.p->GetActionFlags() & (ACTION_FLAG_IDLE | ACTION_FLAG_DEAD)))
      {
        // weve already marked this guy as idle or dead (or he's allowed to be idle...), no need for further action
      }
      else
      {
        cout<<p.p->GetName()<<" hasn't had a nonzero power in a minute.  He's considered dead"<<endl;
        p.p->SetActionFlags(ACTION_FLAG_IDLE,ACTION_FLAG_IDLE); // this guy hasn't had a nonzero power for 60 seconds - he's dead
        p.SetLastDataTime(tmNow);
                
        char szMessage[100];
        _snprintf(szMessage,sizeof(szMessage),"%s is idle - booting in 5 minutes",p.p->GetName().c_str());
        SendServerChat(szMessage,SendToEveryone());
      }
    }
    else
    {
      DWORD fdwCurrent = p.p->GetActionFlags();
      if(fdwCurrent & ACTION_FLAG_IDLE)
      {
        char szMessage[100];
        _snprintf(szMessage,sizeof(szMessage),"%s isn't idle anymore",p.p->GetName().c_str());
        SendServerChat(szMessage,SendToEveryone());
      }
      p.p->SetActionFlags(0,ACTION_FLAG_IDLE);
    }
  }
  else
  {
    // if they're done, don't boot them
    p.p->SetActionFlags(0,ACTION_FLAG_IDLE);
  }
}

DWORD WINAPI SaverProc(LPVOID pv);

class ThreadedResultsSaver
{
public:
  ThreadedResultsSaver(int iMasterId, StatsStorePtr pStatStore, int iMapId, int iRaceId, const PLAYERRESULTPtr& result) : m_pStatStore(pStatStore), m_iMapId(iMapId),m_iRaceId(iRaceId),m_result(result)
  {
    m_hThread = boost::thread(SaverProc,this);
    DASSERT(iRaceId > 0);
  }
  virtual ~ThreadedResultsSaver()
  {

  }
  void ThreadProc()
  {
    int idRaceEntry = 0;
    
    m_result->BuildAvgPowers();
    DWORD tmRecordTime=0;
    m_pStatStore->StoreRaceResult(m_iRaceId, m_iMapId, RACEEND_FINISH, m_result, &idRaceEntry,&tmRecordTime);
    m_pStatStore->StoreRaceReplay(idRaceEntry,m_result->fIsAI,NULL, &m_result->m_lstHistory);
  }
private:

  boost::thread m_hThread;
  const int m_iRaceId;
  const int m_iMapId;
  PLAYERRESULTPtr m_result;
  boost::shared_ptr<StatsStore> m_pStatStore;
};

DWORD WINAPI SaverProc(LPVOID pv)
{
  ThreadedResultsSaver* pSaver = (ThreadedResultsSaver*)pv;
  pSaver->ThreadProc();
  delete pSaver;
  return 0;
}

class OnlyNonStealth
{
public:
  OnlyNonStealth(IPlayerPtr pPlayer) :pPlayer(pPlayer) {};

  bool ShouldSendTo(const IPlayer* p) const
  {
    if(pPlayer->IsStealth()) 
      return p->GetId() == pPlayer->GetId(); // if the source player is stealth, only send to himself
    else
      return !p->IsStealth(); // if the source player isn't stealth, then send to any other non-stealth players (this punishes stealthy players for being hermits
  }
private:
  IPlayerPtr pPlayer;
};
class SendToPlayer
{
public:
  SendToPlayer(int idTarget) :idTarget(idTarget) {};

  bool ShouldSendTo(const IPlayer* p) const
  { 
    return p->GetId() == idTarget;
  }
private:
  int idTarget;
};

void StateData::CheckWattsPBs(IPlayerPtr pPlayer)
{
  static int tmLastNotify = 0;
  const int MS_BETWEEN_NOTIFIES = 30000;
  
  const int tmNow = m_tmNow;
  const int flElapsed = (float)(tmNow - m_tmStart) / 1000.0f;

  PERSONBESTSTATS& best = pPlayer->GetBestStats();
  bool fIsFakeFTP =false;
  const float flFTPNow = best.GetHistoricPowerWkg(&fIsFakeFTP);

  for(int statid = STATID_10S_AVG; statid <= STATID_120MIN_AVG; statid++)
  {
    const bool fWasSetBefore = best.IsStatSet((STATID)statid);
    const float flCurPower = pPlayer->GetRunningPower((STATID)statid) / pPlayer->GetMassKg();

    const char* pszDesc = StatIdToDesc((STATID)statid);

    const bool fBetterThanExistingStat = (!fWasSetBefore || pPlayer->GetBestStats().GetMax((STATID)statid) < flCurPower);
    if(!pPlayer->Player_IsAI() && // nonAI
        fBetterThanExistingStat// stat either wasn't set before, or they beat the old one
       )
    {
      // they've improved!

      char szBumpedFTP[200];
      szBumpedFTP[0] = 0;

      if(StatIdToSeconds((STATID)statid) < flElapsed)
      { // part 1: bump the stat, and check if that results in a new FTP
        best.SetMax((STATID)statid,flCurPower);

        if(flFTPNow > best.GetOriginalHistoric()*1.03f || // big improvement (sandbagger...)
          (!fIsFakeFTP && !fWasSetBefore)) // isn't fake now, and this stat wasn't set before.  This would imply that setting this stat has caused the guy to have a nonfake FTP
        {
          best.SetOriginalHistoric(flFTPNow);
          snprintf(szBumpedFTP,sizeof(szBumpedFTP)," (FTP is now %3.2fW/kg)",flFTPNow);
        }
      }

      if(best.IsStatSet((STATID)statid) && pszDesc[0] != 0) // if it was set before and we have a description of it, then let's send a notify!
      {
        // this guy had a historic best before, and it just got beat
        char szSend[200];
        const float flWKg = flCurPower;

        if(tmNow - tmLastNotify > MS_BETWEEN_NOTIFIES) // don't send public blasts too frequently
        {
          snprintf(szSend,sizeof(szSend),"%s just did a new %s personal best of %3.1fW/kg!%s",pPlayer->GetName().c_str(),pszDesc,flWKg,szBumpedFTP);
          OnlyNonStealth crit(pPlayer);
          SendServerChat(szSend,crit);
          tmLastNotify = tmNow;
        }
        if(szBumpedFTP[0])
        {
          // they got a new FTP!
          snprintf(szSend,sizeof(szSend),"You (%s) just did a new %s personal best of %3.1fW/kg!%s",pPlayer->GetName().c_str(),pszDesc,flWKg,szBumpedFTP);
          SendToPlayer crit(pPlayer->GetId());
          SendServerChat(szSend,crit);
        }
      }

    }
  }
}
DWORD TourServer::GameThdProc()
{
  g_gameThdId = ArtGetCurrentThreadId();

  SetThreadName(ArtGetCurrentThreadId(),"Game Thread");

  {
    AutoLeaveCS _cs(m_cs); // ensuring that the constructor is done...  shared_from_this will break if we're not done the constructor
    ArtSleep(50); // ultra-lame insurance that the taking-ownership from the shared_ptr is done
    
    boost::shared_ptr<TourServer> spThis = boost::shared_ptr<TourServer>(SharedPtr()); // make sure that the GameThdProc is the last guy to shut down
  }

  int counter = 0;
  static DWORD tmLastUpdate = ArtGetTime() - 500;
  while(true)
  {
    int msWait = 16;
    counter++;
    {

      queue<QUEUEDUPDATE> qLocal;
      {
        AutoLeaveCS _cs(m_csQueue);
        qLocal = m_qUpdates;
        while(m_qUpdates.size() > 0) m_qUpdates.pop();
      }
      DWORD tmNow;
      {
        AutoLeaveCS _cs(m_csTransaction);
        tmNow  = ArtGetTime();
        StateDataPtr spCurState = GetWriteableState();
        if(spCurState->GetShutdown() || TourServer_inherited::GetShutdown())
        {
          break;
        }

        spCurState->ApplyClientStateQueue(qLocal);
        spCurState->DoGameTick(&msWait,m_pSchedCheck.get(),this->m_lstAIs,&tmLastUpdate, m_lstMaps,m_iServerId);
        SetState(spCurState);
      }
      
      static DWORD tmLastAlive = tmNow;
      if(tmNow - tmLastAlive > 10*60*1000)
      {
        m_pStatStore->AddAction("Still Alive",GetServerLogName(m_iServerId));
        tmLastAlive = tmNow;
      }
      
    } // end of critsec scope
    ArtSleep(msWait);
  }

  cout<<"game thread exiting"<<endl;
  {
    AutoLeaveCS _cs(m_csTransaction);
    StateDataPtr spCurState = this->GetWriteableState();
    
    spCurState->Cleanup(m_lstMaps, m_iServerId); // makes sure that the race has been finished, stats have been stored, etc
    SetState(spCurState);
  }
  cout<<"game thread: saved final bits, returning now"<<endl;
  return 0;
}
void TourServer::SetShutdown()
{
  TourServer_inherited::SetShutdown();
  {
    AutoLeaveCS _cs(m_csTransaction);
    StateDataPtr spCurState = GetWriteableState();
    spCurState->SetShutdown();
    SetState(spCurState);
  }
}
void StateData::Cleanup(const vector<MAPDATA>& lstMaps, int iServerId)
{
  Restart(RACEEND_RESTART, lstMaps, iServerId);
}

int PickPlayers(vector<ConstServerPlayer>& lstPlayers, vector<unsigned int>& lstSelectionIndices, DWORD tmNow, S2C_UPDATETYPE eType, const vector<ConstServerPlayer>& lstCentral, boost::shared_ptr<Map> spMap)
{
  // let's pick the players that are going to be in this update.

  if(lstPlayers.size() <= 0) return 150;
  int msWait = 0;

  PosUpdateCriterion pos(lstCentral,tmNow, spMap);
  NameUpdateCriterion names(lstCentral,tmNow);
  ResultUpdateCriterion res(lstCentral,tmNow);
  // found the central player
  switch(eType)
  {
  case POSITION_UPDATE:
    sort(lstPlayers.begin(),lstPlayers.end(),pos);
    break;
  case NAME_UPDATE:
    sort(lstPlayers.begin(),lstPlayers.end(),names);
    break;
  case RESULT_UPDATE:
    sort(lstPlayers.begin(),lstPlayers.end(),res);
    break;
  default:
    // we're hopefully doing an update that doesn't need a set of players picked for it
    DASSERT(eType == CHAT_UPDATE || eType == SYNC_UPDATE || eType == TT_UPDATE);
    break;
  }

  for(unsigned int x = 0; x < lstPlayers.size() && x < MAX_PLAYERS; x++)
  {
    const int msSinceLastSend = lstPlayers[x].GetTimeSinceLastSend(eType,lstCentral[0].p->GetId(),tmNow);
    msWait = max(msWait,MINIMUM_UPDATE_DELAY - msSinceLastSend); // if this player was last updated 15ms ago, then we need to wait UPDATE_DELAY-15 milliseconds before updating him again
  }
  return msWait;
}

void StateDataConst::BuildAllPlayerListConst
(
  vector<ConstServerPlayer>& lstPlayers
) const
{
  lstPlayers.reserve(m_mapPlayerz.size());
  for(ConstServerPlayerMap::const_iterator i = m_mapPlayerz.begin(); i != m_mapPlayerz.end(); i++) {lstPlayers.push_back(i->second);}
}
void StateDataConst::BuildCentralPlayerListConst(const vector<int>& lstIds, const vector<ConstServerPlayer>& lstPlayers, vector<ConstServerPlayer>& lstOutput)
{
  lstOutput.reserve(lstIds.size());
  for(unsigned int x = 0; x < lstPlayers.size(); x++)
  {
    for(unsigned int y = 0; y < lstIds.size(); y++)
    {
      DASSERT(lstPlayers[x].p);
      if(lstPlayers[x].p->GetId() == lstIds[y])
      {
        lstOutput.push_back(lstPlayers[x]);
        break;
      }
    }
  }
}
void StateData::BuildAllPlayerList(vector<ServerPlayer>& lstPlayers)
{
  lstPlayers.reserve(m_mapNextPlayerz.size() + m_mapLocalsNext.size());
  for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++) {lstPlayers.push_back(i->second);}
  for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) {lstPlayers.push_back(i->second);}
}

int ServerTimeTrialState::BuildTTUpdate(const vector<IConstPlayerPtrConst>& lstTargets, TIMETRIALUPDATE& ttUpdate)
{
  class TimeFinder : public TTDataModder<SERVERTT>
  {
  public:
    TimeFinder(IConstPlayerPtrConst target, int* ixUsed, TIMETRIALUPDATE* pTT) : m_target(target),m_pixUsed(ixUsed),m_pTT(pTT) {};

    virtual bool Do(int iDist, int idThatDidIt, SERVERTT& data) ARTOVERRIDE
    {
      if(*m_pixUsed >= TIMETRIALUPDATE::TT_PLAYER_COUNT) return false; // we're done

      if(data.setSentTo.find(m_target->GetId()) == data.setSentTo.end())
      {
        // hasn't been sent to this player yet
        data.setSentTo.insert(m_target->GetId()); // mark that we've sent this to our target player now
        const int ix = *m_pixUsed;
        m_pTT->ids[ix] = idThatDidIt;
        m_pTT->segment[ix] = iDist / TimeTrialState::s_minInterval;
        m_pTT->times[ix] = data.flTime;
        (*m_pixUsed)++;
      }
      return true;
    };
  private:
    IConstPlayerPtrConst m_target;
    int* m_pixUsed;
    TIMETRIALUPDATE* m_pTT;
  };
  int ixUsed = 0;
  for(unsigned int x = 0;x < lstTargets.size(); x++)
  {
    // if we find a time that is:
    // ahead of the current player
    // not yet sent to the current player
    // then we should send it
    TimeFinder find(lstTargets[x],&ixUsed,&ttUpdate);
    Apply(&find);
    if(ixUsed >= 16) break;
  }
  return ixUsed;
}
void ServerTimeTrialState::BuildObscureNameUpdate(const vector<IConstPlayerPtrConst>& lstTargets, PLAYERNAMEUPDATE& nameUpdate)
{
  AutoLeaveCS _cs(*m_pCS);
  // go through our obscure names DB, and if target 0 hasn't heard of it yet, then send it
  int ixUsed = 0;
  for(unordered_map<int,SENTNAME>::iterator i = mapNames.begin(); i != mapNames.end() && ixUsed < nameUpdate.NAME_COUNT; i++)
  {
    const int id = i->first;
    SENTNAME& name = i->second;
    if(name.setSentTo.find(lstTargets[0]->GetId()) == name.setSentTo.end())
    {
      // this guy hasn't been sent this name yet
      nameUpdate.Set(ixUsed, id, name.name);
      name.setSentTo.insert(lstTargets[0]->GetId());
      ixUsed++;
    }
  }
}
void ServerTimeTrialState::ClearPlayer(int id)
{
  AutoLeaveCS _cs(*m_pCS);
  class PlayerClearer : public TTDataModder<SERVERTT>
  {
  public:
    PlayerClearer(int idClear) : m_idClear(idClear) {};

    virtual bool Do(int iDist, int idThatDidIt, SERVERTT& data) ARTOVERRIDE
    {
      data.setSentTo.erase(m_idClear);
      return true;
    };
  private:
    const int m_idClear;
  };
  PlayerClearer clear(id);
  Apply(&clear);

  // go through all our names and clear them too
  for(unordered_map<int,SENTNAME>::iterator i = mapNames.begin(); i != mapNames.end(); i++)
  {
    const int id = i->first;
    SENTNAME& name = i->second;
    name.setSentTo.erase(id);
  }
}

StateData::StateData(const StateDataConst& constSrc) 
  : StateDataConst(   ((StateData&)constSrc)),
    m_mapLocalsNext(   ((StateData&)constSrc).m_mapLocalsNext),
    m_mapNextPlayerz(   ((StateData&)constSrc).m_mapNextPlayerz),
    m_pServer(   ((StateData&)constSrc).m_pServer),
    m_mapNextPendingChats(   constSrc.m_mapPendingChats),
    m_fNextShutdown(((StateData&)constSrc).m_fNextShutdown)
{
  //printf("my size is: %d\n",sizeof(*this));
  //CASSERT(sizeof(StateData) == 472); // make sure to copy this!

  // let's make sure to eliminate old chats for players that aren't connected anymore
  unordered_set<int> setToKill;
  for(unordered_map<int,queue<CHATUPDATE> >::const_iterator iChatList = m_mapNextPendingChats.begin(); iChatList != m_mapNextPendingChats.end(); iChatList++)
  {
    // if this player is missing, dead, disconnected, whatever, then let's pitch his pending chats.
    if(m_mapNextPlayerz.find(iChatList->first) == m_mapNextPlayerz.end())
    {
      setToKill.insert(iChatList->first);
    }
    else
    {
      ServerPlayerMap::const_iterator iPlayer = m_mapNextPlayerz.find(iChatList->first);
      if(IS_FLAG_SET(iPlayer->second.p->GetActionFlags(), ACTION_FLAG_DEAD | ACTION_FLAG_LOADING))
      {
        setToKill.insert(iChatList->first);
      }
      else
      {
        queue<CHATUPDATE>& qChats = m_mapNextPendingChats.find(iChatList->first)->second;
        while(qChats.size() > 5) qChats.pop(); // make sure we never have too many chats lying around
      }
    }
  }
  for(unordered_set<int>::const_iterator iKill = setToKill.begin(); iKill != setToKill.end(); iKill++)
  {
    m_mapNextPendingChats.erase(*iKill);
  }
}
StateData::StateData(RACEMODE eRaceMode, int cAIs, int iMinAIPower, int iMaxAIPower, int iServerId, int iDelayMinutes,inheritedServer* pServer, ManagedCS* pCS, bool fSinglePlayer, StatsStorePtr pStatsStore, SRTMSource* pSRTM, const vector<MAPDATA>& lstMaps, int iMasterId)
  : StateDataConst(eRaceMode, cAIs, iMinAIPower, iMaxAIPower, iServerId, iDelayMinutes, pServer, pCS, fSinglePlayer,pStatsStore, pSRTM, iMasterId),
  m_pServer(pServer),
  m_fNextShutdown(false)
{
  if(m_eRaceMode == RACEMODE_TIMETRIAL)
  {
    NextMap(NULL,lstMaps,iServerId,0);
    m_ePhysicsMode = REALMODEL_LATE2013_NODRAFT;
    DASSERT(m_iMapId > 0);
      
    pStatsStore->NotifyStartOfRace(&m_iRaceId,
                                    m_eRaceMode, 
                                    m_spMapConst->GetEndDistance().Minus(m_spMapConst->GetStartDistance()),
                                    -1, // timedLengthSeconds - TTs go on forever
                                    m_iMapId, 
                                    m_sfSchedRace.scheduleId,
                                    0, 
                                    0, 
                                    100, 
                                    m_fSinglePlayer, 
                                    m_ePhysicsMode,
                                    -1);

    // let's also load all the past results for this race
    RACERESULTS oldResults;
    m_pStatStore->GetRaceResults(m_iRaceId,&oldResults);
    cout<<"There are "<<oldResults.GetFinisherCount()<<" old results for this race"<<endl;

    for(unsigned int x = 0;x < oldResults.lstResults.size(); x++)
    {
      const PLAYERRESULTPtr& res = oldResults.lstResults[x];
      m_ttState.AddName(res->id,res->strName);
      m_ttState.AddTime(0,res->id,res->flFinishTime);
    }
    m_fAIsAddedYet = true;
  }
}
StateDataConst::StateDataConst(RACEMODE eRaceMode, int cAIs, int iMinAIPower, int iMaxAIPower, int iServerId, int iDelayMinutes,inheritedServer* pServer, ManagedCS* pCS, bool fSinglePlayer, StatsStorePtr pStatsStore, SRTMSource* pSRTM, int iMasterId)
  :
  m_fAIsAddedYet(false), 
  m_tmFirstFinisher(0),
  m_tmFirstHumanFinisher(0),
  m_tmLastHumanFinisher(0),
  m_fSomeFinished(false), 
  m_eGameState(WAITING_FOR_START),
  m_cAIs(cAIs),
  m_iMinAIPower(iMinAIPower),
  m_iMaxAIPower(iMaxAIPower),
  m_tmCountdownLock(0),
  m_eRaceMode(eRaceMode),
  m_ttState(pCS),
  m_iRaceId(-1),
  m_fSinglePlayer(fSinglePlayer),
  m_pStatStore(pStatsStore),
  m_ixMap(0),
  m_pSRTMSource(pSRTM),
  m_tmNow(ArtGetTime()),
  m_fShutdown(false),
  m_cSprintClimbPeopleCount(-1),
  m_iMasterId(iMasterId),
  m_fGamePaused(false)
{
  m_spMapConst = boost::shared_ptr<Map>(new Map());
  m_tmStart = ArtGetTime() + 60000*iDelayMinutes;
  DASSERT(!m_fShutdown);
}
StateDataConst::StateDataConst(const StateData& prevState)
  : m_mapPlayerRestartRequests(prevState.m_mapPlayerRestartRequests),
    m_raceResults(prevState.m_raceResults),
    m_tmFirstFinisher(prevState.m_tmFirstFinisher),
    m_tmFirstHumanFinisher(prevState.m_tmFirstHumanFinisher),
    m_tmLastHumanFinisher(prevState.m_tmLastHumanFinisher),
    m_tmStart(prevState.m_tmStart),
    m_sfSchedRace(prevState.m_sfSchedRace),
    m_eGameState(prevState.m_eGameState),
    m_eRaceMode(prevState.m_eRaceMode),
    m_ttState(prevState.m_ttState),
    m_iRaceId(prevState.m_iRaceId),
    m_fSomeFinished(prevState.m_fSomeFinished),
    m_tmCountdownLock(prevState.m_tmCountdownLock),
    m_mapPendingChats(prevState.GetNextChats()),
    m_iMapId(prevState.m_iMapId),
    m_cAIs(prevState.m_cAIs),
    m_iMaxAIPower(prevState.m_iMaxAIPower),
    m_iMinAIPower(prevState.m_iMinAIPower),
    m_fAIsAddedYet(prevState.m_fAIsAddedYet),
    m_mapStats(prevState.m_mapStats),
    m_fSinglePlayer(prevState.m_fSinglePlayer),
    m_ePhysicsMode(prevState.m_ePhysicsMode),
    m_spMapConst(prevState.GetMap()),
    m_pStatStore(prevState.m_pStatStore),
    m_pSRTMSource(prevState.m_pSRTMSource),
    m_ixMap(prevState.m_ixMap),
    m_tmNow(ArtGetTime()),
    m_fShutdown(prevState.GetNextShutdown()),
    m_cSprintClimbPeopleCount(prevState.m_cSprintClimbPeopleCount),
    m_iMasterId(prevState.m_iMasterId),
    m_fGamePaused(prevState.m_fGamePaused)
{
  // need to copy the updated AI and player positions
  const ServerPlayerMap& mapPlayerz = prevState.GetPlayerz();
  MergePastMap(m_tmNow, mapPlayerz, prevState.m_mapPlayerz, m_mapPlayerz);
  const ServerPlayerMap& mapAIPlayerz = prevState.GetAIPlayerz();
  MergePastMap(m_tmNow, mapAIPlayerz, prevState.m_mapPlayerz, m_mapPlayerz);

  {
    // chats:
    // prevState.m_mapNextPendingChats just got copied into our m_mapPendingChats (therefore, they are equal)
    // however: each queue in prevState.m_mapPendingChats may have had items popped off the front, while the stuff in prevState.m_mapNextPendingChats will only have had stuff added.
    // we need to reconcile the two: we need to pop things off the front of this->m_mapPendingChats until the front item in each queue matches prevState.m_mapPendingChats, and thus reflects chats that did get sent
    for(unordered_map<int,queue<CHATUPDATE> >::iterator i = m_mapPendingChats.begin(); i != m_mapPendingChats.end(); i++)
    {
      queue<CHATUPDATE>& qNeedsFixing = i->second; // remember: m_mapPendingChats' queues have stuff on the front that may have already been sent
      unordered_map<int,queue<CHATUPDATE> >::const_iterator iOld = prevState.GetNextChats().find(i->first);

      
      if(iOld != prevState.m_mapPendingChats.end() && qNeedsFixing.size() > 0)
      {
        const queue<CHATUPDATE>& qOld = iOld->second;
        // this chat-queue existed in the previous state
        const CHATUPDATE* pFrontNeedsFixing = &qNeedsFixing.front();
        const bool fOldQueueIsEmpty = qOld.size() <= 0;
        const bool fOldQueueHeadIsDifferent = !fOldQueueIsEmpty && !!memcmp(pFrontNeedsFixing,&qOld.front(),sizeof(CHATUPDATE));
        while(qNeedsFixing.size() > 0 && ( fOldQueueIsEmpty || fOldQueueHeadIsDifferent ) )
        {
          qNeedsFixing.pop();
          pFrontNeedsFixing = &qNeedsFixing.front();
        }
      }
      else
      {
        // this chat-queue did not exist in the previous state, and so it is impossible that we sent data from it
      }
    }
  }
}

void StateDataConst::MergePastMap(const DWORD tmNow, const ServerPlayerMap& mapPrevReal, const ConstServerPlayerMap& mapPrevConst, ConstServerPlayerMap& mapNextConst)
{
  for(ServerPlayerMap::const_iterator i = mapPrevReal.begin(); i != mapPrevReal.end(); i++)
  {
    const ServerPlayer& sp = i->second;
    const ConstServerPlayerMap::const_iterator iPrevVersion = mapPrevConst.find(i->first);
    if(iPrevVersion != mapPrevConst.end())
    {
      // this guy existed previously
      mapNextConst.insert(std::pair<int,ConstServerPlayer>(i->first,ConstServerPlayer(sp,iPrevVersion->second,tmNow)));
    }
    else
    {
      // we've never seen this guy before
      mapNextConst.insert(std::pair<int,ConstServerPlayer>(i->first,ConstServerPlayer(sp.p,tmNow)));
    }
  }
}

void DoServerTests();

////////////////////
TourServer::TourServer(ICommSocketFactoryPtr pSocketFactory, int iServerId, int iDelayMinutes, const vector<MAPDATA>& lstMaps, StatsStorePtr pStats, SRTMSource* pSRTMSource, int iUDPInPort, int iTCPConnectPort, int cAIs, int iMinAIPower, int iMaxAIPower, RACEMODE eRaceMode, bool fSinglePlayer, int iMasterId) : 
    SimpleServer<TDGGameState,TDGClientState,TDGClientDesc,TDGInitialState,TDGClientToServerSpecial,TDGServerStatus,TourServerSendHandle>(pSocketFactory,PROTOCOL_VERSION,iUDPInPort,iTCPConnectPort) , 
    m_spState(new StateData(eRaceMode, cAIs, iMinAIPower, iMaxAIPower, iServerId, iDelayMinutes, this, &m_cs, fSinglePlayer, pStats, pSRTMSource, lstMaps,iMasterId)),
    m_iServerId(iServerId),
    m_lstMaps(lstMaps),
    m_lCurrentIteration(1),
    m_pStatStore(pStats)
{

  AutoLeaveCS _cs(m_cs);
  m_lstAIs = GrovelAIs();

  m_pSRTMSource = pSRTMSource;
#if 0
  m_pStatStore = new NullStatsStore();
  m_pSRTMSource = NULL;
#endif
  string strErr;
  if(!m_pStatStore->Load(strErr))
  {
    cerr<<"Failed to load stats store - "<<strErr<<endl;
    m_pStatStore = boost::shared_ptr<StatsStore>(new NullStatsStore());
  }

  m_pStatStore->AddAction("Server Starting",GetServerLogName(m_iServerId));

  m_pSchedCheck = boost::shared_ptr<ScheduleChecker>(new ScheduleChecker(m_iServerId,m_pStatStore));

  {
    AutoLeaveCS _cs(m_csTransaction);
    StateDataPtr spState= GetWriteableState();
    spState->NextMap(NULL,m_lstMaps, m_iServerId, eRaceMode == RACEMODE_ROADRACE);
    spState->Init();
    SetState(spState);
  }
  m_hGameThread = boost::thread(_GameThdProc,this);
}
TourServer::~TourServer() 
{
  SetShutdown();
  if(m_hGameThread.get_id() != boost::this_thread::get_id() && m_hGameThread.joinable()) // don't join the game thread if we are the game thread
  {
    cout<<"about to wait for game thread"<<endl;
    this->m_hGameThread.join();
    cout<<"done waiting for game thread"<<endl;
  }
};
boost::shared_ptr<TourServer> TourServer::SharedPtr()
{
    return shared_from_this();
}
void StateData::AddDopers(int cAIs, int min, int max, const vector<AIDLL>& lstAIs)
{
  if(m_eRaceMode == RACEMODE_TIMETRIAL) 
  {
    m_fAIsAddedYet = true;
    return; // no AIs in time-trial mode
  }

  if(m_sfSchedRace.IsValid())
  {
    min = m_sfSchedRace.iAIMinStrength;
    max = m_sfSchedRace.iAIMaxStrength;
    cAIs = m_sfSchedRace.cAIs;
  }
  srand(ArtGetTime());
  //srand(5);
  cout<<"About to select "<<cAIs<<" AIs with watts between "<<min<<" and "<<max<<endl;

  /*
  for(int x = 0; x < lstFrenemies.size(); x++)
  {
    const FRENEMY& fren = lstFrenemies[x];

    // frenemy power levels need to be adjusted to account for map length.
    // for approximation, let's just guess that everyone is doing 35km/h
    const float flEstimatedSpeedMs = 35.0 / 3.6;
    const float flMapLength = m_spMapConst->GetLapLength() * m_spMapConst->GetLaps();
    const float flTimeToCompleteCourse = m_spMapConst->GetLaps() >= 1 ?
                                      (m_spMapConst->GetLapLength() * m_spMapConst->GetLaps()) / flEstimatedSpeedMs :
                                      m_spMapConst->GetTimedLength();

    // we've got the frenemy's FTP, we want to estimate their avg power for this course
    // we got this value out of excel, looking at our "estimated FTP" ratios for a variety of lengths.
    // given an input of "time to complete course", it figures out what fraction of FTP the rider should be able
    // to complete it at.
    float flMultiplier = -0.081*log(flTimeToCompleteCourse) + 1.6671;
    flMultiplier = min(2,flMultiplier); // don't let it get crazy if the course is short

    AddDoper(fren.wattsTarget * flMultiplier, fren.massKg, fren.name, true, lstAIs);
  }
  */

  int cAddFails = 0;
  int cTotalFailures = cAIs*10;
  for(int x = 0;x < cAIs; x++)
  {
    int power = 0;
    int rnd = rand();
    if(min == max) power = min;
    else
    {
      power = rnd % (max-min) + min;
    }
    if(!AddDoper(power,0, "", false, lstAIs))
    {
      cAddFails++;
      if(cAddFails > cTotalFailures)
      {
        // we've failed to add an AI ... for the last time
        break;
      }
      x--;
    }
  }
  cout<<"AIs added"<<endl;
  m_fAIsAddedYet = true;
}
void TourServer::SimpleServer_BuildStartupInfo(STARTUPINFOZ<TDGInitialState>& sfStartup)
{ 
  StateDataConstPtr spState = GetCurState();

  boost::shared_ptr<const IMap> spMap = spState->GetMap();
  const Map* pMap = (const Map*)spMap.get();
  sfStartup.sfInfo = *pMap->GetInitialState();

  sfStartup.sfInfo.eRaceMode = spState->GetRaceMode();
}
void TourServer::SimpleServer_BuildServerStatus(TDGServerStatus* pSS) const
{
  DASSERT(!m_cs.IsOwned());
  StateDataConstPtr spState = GetCurState();
  spState->BuildServerStatus(pSS);

}
void StateDataConst::BuildServerStatus(TDGServerStatus* pSS) const
{
  PLAYERDIST flLastHuman = m_spMapConst->GetEndDistance();
  pSS->cHumans = 0;
  pSS->cAI = 0;
  for(ConstServerPlayerMap::const_iterator i = m_mapPlayerz.begin(); i != m_mapPlayerz.end(); i++)
  {
    if(i->second.p->Player_IsAI())
    {
      pSS->cAI++;
    }
    else
    {
      pSS->cHumans++;
      flLastHuman = min(i->second.p->GetDistance(),flLastHuman);
    }
  }
  if(pSS->cAI == 0 && m_eGameState == WAITING_FOR_START && !m_sfSchedRace.IsValid())
  {
    pSS->cAI = m_cAIs;
  }
  pSS->raceLength = m_spMapConst->GetEndDistance().Minus(m_spMapConst->GetStartDistance());
  pSS->mapStats = m_mapStats;
  pSS->flRaceCompletion = (flLastHuman.Minus(m_spMapConst->GetStartDistance())) / pSS->raceLength;
  pSS->flAvgSlope = (m_spMapConst->GetElevationAtDistance(m_spMapConst->GetEndDistance()) - m_spMapConst->GetElevationAtDistance(m_spMapConst->GetStartDistance())) / pSS->raceLength.flDistPerLap;
  pSS->flClimbing = m_spMapConst->GetClimbing();
  pSS->flMaxGradient = m_spMapConst->GetMaxGradient();
  pSS->eGameMode = m_eRaceMode;

  const int cchMapName = sizeof(pSS->szMapName);
  strncpy(pSS->szMapName,m_spMapConst->GetMapName().c_str(),cchMapName);
  pSS->szMapName[cchMapName-1] = 0;

  for(int x = 0;x < NUMELMS(pSS->rgElevs); x++)
  {
    const float flMapDist = m_spMapConst->GetLapLength();
    const float flPct = (float)x / (float)NUMELMS(pSS->rgElevs);
    const float flPos = flPct*flMapDist;
    pSS->rgElevs[x] = m_spMapConst->GetElevationAtDistance(PLAYERDIST(0,flPos,flMapDist));
  }

  { // sprint/climb
    vector<SprintClimbPointPtr> lstSprintClimb;
    m_spMapConst->GetScoringSources(lstSprintClimb);
    pSS->cSprintClimbs = min((size_t)10,lstSprintClimb.size());
    for(int x = 0;x < pSS->cSprintClimbs; x++)
    {
      lstSprintClimb[x]->GetRaw(&pSS->rgSprintClimbs[x]);

      // we convert this to straight meters because otherwise it gets too complicated figuring out where it goes on the client side
      pSS->rgSprintClimbs[x].flOrigDistance.v = m_spMapConst->GetDistanceOfOrigDist(pSS->rgSprintClimbs[x].flOrigDistance);
    }
  }
}
bool TourServer::SimpleServer_ValidateLogin(TDGClientDesc& cd, TDGConnectionResult* pConnResult) const
{
  // someone is trying to log in.
  if(cd.protocolVersion < PROTOCOL_VERSION)
  {
    pConnResult->eResult = OUTDATEDCLIENT;
    return false;
  }

  int iId = cd.iMasterId;
  char szUsername[sizeof(cd.szUsername)+1];
  memcpy(szUsername,cd.szUsername,sizeof(szUsername));
  szUsername[sizeof(cd.szUsername)] = 0;
  bool fExpired = false;
  if(m_pStatStore->ValidateLogin(szUsername, cd.rgMd5,&iId,&cd.iDefaultTeamId,&fExpired))
  {
    pConnResult->eResult = ::LOGINSUCCESS;
    cd.iMasterId = iId;
    return true;
  }
  pConnResult->eResult = fExpired ? ::ACCOUNTEXPIRED : ::BADPASSWORD;
  return false;
}
bool TourServer::SimpleServer_StartSendHandle(TourServerSendHandle* pHandle, const SimpleServer::ClientData& cdTarget) 
{
  LONG lCurrentIteration = ArtInterlockedExchangeAdd(&m_lCurrentIteration,0);
  StateDataConstPtr spCurState = GetCurState();
  new (pHandle) TourServerSendHandle(spCurState->GetGameState() == WAITING_FOR_START, lCurrentIteration,cdTarget.lstIds, spCurState);

  // make sure that one of cdTarget.lstIds is in the list of all players.  If not, that means the target player isn't connected yet
  bool fFoundTarget = false;
  for(unsigned int x = 0; x < pHandle->m_lstAllPlayers.size(); x++)
  {
    if(pHandle->m_lstAllPlayers[x].p->GetId() == cdTarget.lstIds[0])
    {
      fFoundTarget = true;
      break;
    }
  }
  if(!fFoundTarget) return false;
  
  for(unsigned int x = 0; x < pHandle->m_lstAllPlayers.size() ;x++)
  {
    // precache the "time since last sent" for the xth player to cdTarget.lstIds[0].  This is much faster than looking it up every sort
    pHandle->m_lstAllPlayers[x].CacheSendTimeTo(POSITION_UPDATE,cdTarget.lstIds[0],spCurState->GetTmNow());
  }

  const int msWait = PickPlayers(pHandle->m_lstAllPlayers, pHandle->m_lstPlayerOrder, spCurState->GetTmNow(), POSITION_UPDATE,pHandle->m_lstCentral, spCurState->GetMap());
  if(msWait > 0) 
  {
    return false; // not ready to send anything
  }

  DASSERT(pHandle->lIteration == lCurrentIteration); // making sure this in-place construction stuff works...
  return true;
}
bool TourServer::SimpleServer_IsHandleCurrent(const TourServerSendHandle& handle) const
{
  if(handle.fDone)
    return false;

  LONG lCurrentIteration = ArtInterlockedExchangeAdd(&m_lCurrentIteration,0);
  return lCurrentIteration <= handle.lIteration;
}
  
int TourServer::SimpleServer_BuildGameState(TourServerSendHandle& sendHandle, const SimpleServer::ClientData& cdTarget, TDGGameState* pTGS, bool* pfPlayerDead) const
{
  StateDataConstPtr spGameState = sendHandle.spState ? sendHandle.spState : GetCurState();
  return spGameState->BuildGameState(sendHandle,cdTarget,pTGS,pfPlayerDead);
};

int StateDataConst::BuildGameState(TourServerSendHandle& handle, const inheritedServer::ClientData& cdTarget, TDGGameState* pTGS, bool* pfPlayerDead) const
{
  int ixPlayer = 0;
  if(cdTarget.lstIds.size() <= 0) 
  {
    *pfPlayerDead = true;
    cout<<"Bad ID list.  Maybe have crashed"<<endl;
    DASSERT(FALSE);
    return 0;
  }

  bool fChatWaiting = false;
  queue<CHATUPDATE>* pPendingChats = NULL;
  { // checking chat state
    for(int x = 0;x < cdTarget.lstIds.size(); x++)
    {
      unordered_map<int,queue<CHATUPDATE> >::iterator i = m_mapPendingChats.find(cdTarget.lstIds[x]);
      if(i != m_mapPendingChats.end())
      {
        pPendingChats = &i->second;
        fChatWaiting = pPendingChats && pPendingChats->size() > 0 && cdTarget.fValid;
        if(fChatWaiting)
        {
          break;
          //cout<<"c";
          //cout<<ArtGetCurrentThreadId()<<" theres a chat waiting for "<<cdTarget.lstIds[0]<<", queue is "<<(void*)pPendingChats<<" with "<<pPendingChats->size()<<" chats"<<endl;
        }
      }
    }
  }

  pTGS->lIteration = handle.lIteration;
  if(fChatWaiting)
  {
    pTGS->eType = CHAT_UPDATE;
  }
  else if(cdTarget.cTGSBuilds % 50 == 0)
  {
    // every 100 updates, send a player name update
    pTGS->eType = NAME_UPDATE;
  }
  else if(cdTarget.cTGSBuilds % 50 == 10 && 
        ((m_fSomeFinished && m_eRaceMode == RACEMODE_ROADRACE) ||
        (m_eRaceMode == RACEMODE_TIMETRIAL)))
  {
    // if we have finishers, then send a results update every 50 cycles too
    pTGS->eType = RESULT_UPDATE;
  }
  else if(cdTarget.cTGSBuilds % 50 == 20)
  {
    pTGS->eType = SYNC_UPDATE;
  }
  else if((cdTarget.cTGSBuilds % 50 == 30 || cdTarget.cTGSBuilds % 50 == 32 || cdTarget.cTGSBuilds % 50 == 34) && m_eRaceMode == RACEMODE_TIMETRIAL)
  {
    pTGS->eType = TT_UPDATE;
  }
  else
  {
    if(IsDelayed())
    {
      // 20% of the time, send a player update
      pTGS->eType = POSITION_UPDATE;
    }
    else
    {
      pTGS->eType = POSITION_UPDATE;
    }
  }

  vector<ConstServerPlayer>& _lstAllPlayers = handle.m_lstAllPlayers;
  

  // now that we've picked our update type, let's pick the MAX_PLAYERS recipients that will be included in it!
  if(cdTarget.lstIds.size() <= 0)
  {
    DASSERT(FALSE);
    *pfPlayerDead = true;
    return 0;
  }
  if(_lstAllPlayers.size() <= 0)
  {
    *pfPlayerDead = true;
  }

  switch(pTGS->eType)
  {
  default:
    DASSERT(FALSE);
    break;
  case CHAT_UPDATE:
  {
    if(pPendingChats)
    {
      pTGS->chatUpdate = pPendingChats->front();
      pPendingChats->pop();
      //cout<<"thread "<<ArtGetCurrentThreadId()<<" popping off chats for player id "<<cdTarget.lstIds[0]<<".  "<<pPendingChats->size()<<" left"<<endl;
    }
    else
    {
      pTGS->chatUpdate = CHATUPDATE();
    }
    break;
  }
  case TT_UPDATE:
  {
    // gotta build a TT update
    vector<IConstPlayerPtrConst> lstTargets;
    for(unsigned int x = 0;x < cdTarget.lstIds.size(); x++)
    {
      ConstServerPlayerMap::const_iterator i = m_mapPlayerz.find(cdTarget.lstIds[x]);
      if(i != m_mapPlayerz.end())
      {
        lstTargets.push_back(i->second.p);
      }
    }
    if(lstTargets.size() > 0)
    {
      pTGS->ttupdate.Init();
      const int ixUsed = m_ttState.BuildTTUpdate(lstTargets, pTGS->ttupdate);
      if(ixUsed <= 0)
      {
        // they didn't use any slots in the TT update.  We must need to start sending obscure names!
        pTGS->eType = NAME_UPDATE;
        pTGS->nameUpdate.Init();
        m_ttState.BuildObscureNameUpdate(lstTargets,pTGS->nameUpdate);
      }
    }
    break;
  }
  case SYNC_UPDATE:
    pTGS->syncUpdate.Init();
    break;
  case POSITION_UPDATE:
  {
    pTGS->posUpdate.ClearData();

    if(m_eGameState == WAITING_FOR_START)
    {
      pTGS->posUpdate.iTimeUntilStart = this->m_tmStart - m_tmNow;
      pTGS->posUpdate.iSprintClimbPeopleCount = -1;
    }
    else
    {
      ConstServerPlayerMap::const_iterator i = m_mapPlayerz.find(cdTarget.lstIds[0]);
      DWORD tmStart = m_tmStart;
      if(i != m_mapPlayerz.end() && m_eRaceMode == RACEMODE_TIMETRIAL)
      {
        tmStart = 1000*i->second.p->GetStartTime();
      }
      pTGS->posUpdate.iTimeUntilStart = tmStart - m_tmNow;
      pTGS->posUpdate.iSprintClimbPeopleCount = m_cSprintClimbPeopleCount;
    }

    SERVERFLAGS fdwFlags = 0;
    int iPhysics = (int)m_ePhysicsMode;
    if(iPhysics & 1) fdwFlags |= SF_NORMALIZEDPHYSICS;
    if(iPhysics & 2) fdwFlags |= SF_PHYSICSBIT2;
    if(iPhysics & 4) fdwFlags |= SF_PHYSICSBIT3;
    if(iPhysics & 8) fdwFlags |= SF_PHYSICSBIT4;
    if(m_tmCountdownLock && m_eGameState == WAITING_FOR_START) fdwFlags |= SF_PAUSEDCOUNTDOWN;
    if(m_fGamePaused) fdwFlags |= SF_PAUSEDGAME;

    pTGS->posUpdate.SetServerFlags(fdwFlags);
    for(unsigned int x = 0;x < MAX_PLAYERS; x++)
    {
      pTGS->posUpdate.SetPlayerStats(x,INVALID_PLAYER_ID,0);
    }
    const int idTo = cdTarget.lstIds[0];
    for(unsigned int x = handle.rgixSendPos[pTGS->eType];x < handle.m_lstPlayerOrder.size();x++)
    {
      ConstServerPlayer& p = _lstAllPlayers[handle.m_lstPlayerOrder[x]];
      if(p.p && ixPlayer < MAX_PLAYERS)
      {
        pTGS->posUpdate.SetPlayerStats(ixPlayer,p.p->GetId(),p.p.get());
        for(unsigned int j = 0; j < cdTarget.lstIds.size(); j++) // update that we've sent this data to all the players at this client
        {
          p.RecordLastSend(POSITION_UPDATE,cdTarget.lstIds[j],m_tmNow);
        }
        handle.rgixSendPos[pTGS->eType] = x;
        ixPlayer++;
      }
    }
    if(ixPlayer <= 0)
    {
      // we're all out of players to send data about
      DASSERT(!handle.fDone);
      handle.fDone = true;
      return 50;
    }
    else if(ixPlayer < MAX_PLAYERS)
    {
      // we're mostly out of players to send data about.
      handle.fDone = true;
    }
    break;
  }
  case RESULT_UPDATE:
  {
    for(int x = 0;x < MAX_PLAYERS;x++) pTGS->resultUpdate.rgPlayerIds[x] = INVALID_PLAYER_ID;
      
    for(unsigned int x = handle.rgixSendPos[pTGS->eType];x < handle.m_lstPlayerOrder.size();x++)
    {
      ConstServerPlayer& p = _lstAllPlayers[handle.m_lstPlayerOrder[x]];
      if(p.p && ixPlayer < MAX_PLAYERS)
      {
        pTGS->resultUpdate.rgPlayerIds[ixPlayer] = p.p->GetId();
        pTGS->resultUpdate.rgflPlayerTime[ixPlayer] = p.p->GetFinishTime();
        for(unsigned int j = 0;j < cdTarget.lstIds.size(); j++)
        {
          p.RecordLastSend(pTGS->eType,cdTarget.lstIds[j],m_tmNow);
        }
        ixPlayer++;
        handle.rgixSendPos[pTGS->eType] = x;
      }
    }
    break;
  }
  case NAME_UPDATE:
  {
    pTGS->nameUpdate.Init();

    for(unsigned int x = handle.rgixSendPos[pTGS->eType];x < handle.m_lstPlayerOrder.size(); x++)
    {
      ConstServerPlayer& p = _lstAllPlayers[handle.m_lstPlayerOrder[x]];
      if(p.p && ixPlayer < MAX_PLAYERS)
      {
        pTGS->nameUpdate.Set(ixPlayer, p.p->GetId(), p.p->GetName());
          
        for(unsigned int j = 0;j < cdTarget.lstIds.size(); j++)
        {
          p.RecordLastSend(pTGS->eType,cdTarget.lstIds[j],m_tmNow);
        }
        ixPlayer++;
        handle.rgixSendPos[pTGS->eType] = x;
      }
    }
    break;
  }
  }

  pTGS->checksum = TDGGameState::GetChecksum(pTGS);

  return 0;
}

void StateData::NextMap(SCHEDULEDRACE* pNextRace, const vector<MAPDATA>& lstMaps, const int iServerId, int iForceMinutes)
{
  boost::shared_ptr<Map> spNewMap(new Map());
  cout<<"new map ptr is "<<spNewMap.get()<<endl;
  const DWORD tmNow = GetSecondsSince1970GMT();
  if(pNextRace && pNextRace->tmStart > tmNow)
  {
    // ooo, we've got a scheduled race!
    if(spNewMap->LoadFromDB(m_pSRTMSource,m_pStatStore.get(),pNextRace->iMapId,pNextRace->iKm,pNextRace->laps, pNextRace->timedLengthSeconds, pNextRace->iStartPercent,pNextRace->iEndPercent, m_eRaceMode == RACEMODE_ROADRACE))
    {
      int secondsAhead = iForceMinutes >= 0 ? iForceMinutes*60 : pNextRace->tmStart - tmNow;
      m_tmStart = 1000 * secondsAhead + m_tmNow;
        
      cout<<"Loaded "<<spNewMap->GetMapFile()<<" for next map which will start in "<<secondsAhead<<" seconds"<<endl;
      cout<<"min AI: "<<pNextRace->iAIMinStrength<<"W max AI: "<<pNextRace->iAIMaxStrength<<"W"<<endl;
      // loaded the map
      m_pStatStore->AddMap(&m_iMapId, &m_mapStats, spNewMap.get());
      m_sfSchedRace = *pNextRace;
      m_iMapId = m_sfSchedRace.iMapId;
      m_tmStart = m_tmNow + secondsAhead*1000;
      m_ePhysicsMode = pNextRace->ePhysicsMode;

      m_spMapConst = spNewMap;
      return;
    }
  }
  m_ePhysicsMode = GetDefaultPhysicsMode();

  m_sfSchedRace = SCHEDULEDRACE(); // we aren't running a scheduled race
  m_tmStart = m_tmNow + (iForceMinutes >= 0 ? iForceMinutes*60000 : 60000);
  // either we didn't have a scheduled race, or failed to load it.
  cout<<"Loading next map from map list"<<endl;
  int cFailures = 0;
  while(true)
  {
    m_ixMap++;
    if(m_ixMap >= (int)lstMaps.size())
    {
      
      m_ixMap = 0;
    }
    cout<<"current map is index "<<m_ixMap<<" map list is "<<lstMaps.size()<<" big"<<endl;
    const MAPDATA& map = lstMaps[m_ixMap];
    if(map.id >= 0)
    {
      if(spNewMap->LoadFromDB(m_pSRTMSource,this->m_pStatStore.get(),map.id,map.iDesiredKm,map.laps, map.timedLengthSeconds,map.iPctStart,map.iPctEnd, m_eRaceMode == RACEMODE_ROADRACE))
      {
        cout<<ArtGetCurrentThreadId()<<": "<<"New db map is: "<<spNewMap->GetMapName()<<endl;
        m_pStatStore->AddMap(&m_iMapId,&m_mapStats,spNewMap.get());
        m_sfSchedRace.iMapId = map.id;
        m_sfSchedRace.iKm = map.iDesiredKm;
        m_sfSchedRace.iStartPercent = map.iPctStart;
        m_sfSchedRace.iEndPercent = map.iPctEnd;
        m_sfSchedRace.tmStart = ::GetSecondsSince1970GMT() + 5*60;
        m_sfSchedRace.serverId = iServerId;
        m_sfSchedRace.strRaceName = "Single Player Ride";
        m_sfSchedRace.cAIs = this->m_cAIs;
        m_sfSchedRace.iAIMaxStrength = this->m_iMaxAIPower;
        m_sfSchedRace.iAIMinStrength = this->m_iMinAIPower;
        m_sfSchedRace.timedLengthSeconds = map.timedLengthSeconds;
        m_sfSchedRace.laps = map.laps;
        DASSERT(m_sfSchedRace.IsValid());
        m_iMapId = map.id;
        m_spMapConst = spNewMap;
        break;
      }
    }
    else if(cFailures >= 10 && spNewMap->LoadFromDB(m_pSRTMSource,this->m_pStatStore.get(),1,16,1,-1,0,100, m_eRaceMode == RACEMODE_ROADRACE))
    {
      // we tried to load 10 maps.  we failed.  just load cape spear
      cout<<ArtGetCurrentThreadId()<<": "<<"New failure-caused map is: "<<spNewMap->GetMapName()<<endl;
      m_pStatStore->AddMap(&m_iMapId,&m_mapStats,spNewMap.get());
      m_iMapId = 1;
        m_spMapConst = spNewMap;
      break;
    }
    else if(map.strFile.length() > 0 && spNewMap->LoadFromGPX(m_pSRTMSource, map.strFile, -1,map.iDesiredKm,map.laps,map.timedLengthSeconds,0,100, m_eRaceMode == RACEMODE_ROADRACE))
    {
      cout<<ArtGetCurrentThreadId()<<": "<<"New map is: "<<spNewMap->GetMapName()<<endl;
      m_pStatStore->AddMap(&m_iMapId,&m_mapStats,spNewMap.get());
        m_spMapConst = spNewMap;
      break;
    }
    else
    {
      spNewMap.reset();
      cerr<<"Failed to load "<<map.strFile.c_str()<<"."<<endl;
      cFailures++;
      ArtSleep(500);
    }
  }
}
int StateDataConst::GetActiveHumanCount(int iExcludeMasterId) const
{
  cout<<"checking for active humans"<<endl;
  int cHumans = 0;
  for(ConstServerPlayerMap::const_iterator i = m_mapPlayerz.begin(); i != m_mapPlayerz.end(); i++)
  {
    ConstServerPlayer p = i->second;

    if(p.p && 
      (IS_FLAG_SET(p.p->GetActionFlags(),::ACTION_FLAG_DEAD | ::ACTION_FLAG_DOOMEDAI | ::ACTION_FLAG_GHOST | ::ACTION_FLAG_IDLE | ::ACTION_FLAG_SPECTATOR) || p.p->Player_IsAI()))
    {
      // buddy is dead, doomed, a ghost, idle, an AI, or a spectator
      cout<<"Active human check: "<<p.p->GetName()<<" is dead, doomedai, ghost, idle, or a spectator"<<endl;
    }
    else if(p.p && p.p->GetMasterId() == iExcludeMasterId)
    {
      // buddy is an alias from the person who is interested in our active human count.  So he doesn't count.
      cout<<"Active human check: "<<p.p->GetName()<<" is from masterid "<<iExcludeMasterId<<endl;
    }
    else
    {
      cHumans++;
      cout<<p.p->GetName()<<" is around with flags "<<p.p->GetActionFlags()<<".  Human count = "<<cHumans<<endl;
    }
  }
  return cHumans;
}

void TourServer::SimpleServer_NotifyNewPlayer(const ClientData& sfData)
{
  DASSERT(!m_cs.IsOwned());

  stringstream ssLogData;
  {
    AutoLeaveCS _cs(m_csTransaction);
    StateDataPtr spData = GetWriteableState();
    spData->AddNewPlayer(sfData, m_pSchedCheck.get(), m_iServerId, ssLogData);
    SetState(spData);
  }
  // after all this, let's log what we've got
  m_pStatStore->AddAction(ssLogData.str(),GetServerLogName(m_iServerId));
  
}
void StateData::AddNewPlayer(const inheritedServer::ClientData& sfData, ScheduleChecker* pSchedCheck, int iServerId, stringstream& ss)
{
  // oh boy, a new player
  {
    for(unsigned int x = 0;x < sfData.lstIds.size(); x++)
    {
      ss<<sfData.clientDesc.GetName(x)<<" has joined.";

      cout<<sfData.clientDesc.GetName(x)<<" ("<<sfData.clientDesc.iMasterId<<") has joined the game"<<endl;
      DASSERT(sfData.lstIds[0] > 0);

      if(sfData.clientDesc.mapReq.IsValid() && m_eRaceMode == RACEMODE_ROADRACE)
      {
        ss<<" has a map request for "<<sfData.clientDesc.mapReq.laps<<" x "<<sfData.clientDesc.mapReq.iMeters<<"m on mapid "<<sfData.clientDesc.mapReq.iMapId<<"!";
        // if no-one else is connected, then restart
        if(GetActiveHumanCount(sfData.clientDesc.iMasterId) <= 0)
        {
          SCHEDULEDRACE sr(sfData.clientDesc.mapReq,sfData.clientDesc.iMasterId,iServerId, m_pStatStore.get());
          if(m_sfSchedRace.IsValid() && sr.iKm == m_sfSchedRace.iKm && sr.iMapId == m_sfSchedRace.iMapId && abs((int)sr.tmStart - (int)m_sfSchedRace.tmStart) < 300 && sr.laps == m_sfSchedRace.laps && sr.timedLengthSeconds == m_sfSchedRace.timedLengthSeconds)
          {
            ss<<" request rejected.  too similar.";
          }
          else
          {
            ss<<" request accepted.";
            pSchedCheck->AddScheduledRace(sr);
          }
        }
        else
        {
          ss<<" map request skipped.  Humans around.";
        }
      }
      else
      {
        ss<<" had no mapreq.";
      }
        
      float flMassKg = (float)sfData.clientDesc.rgWeights[x] / 10.0f;
      flMassKg = max(40.0f,flMassKg);

      // let's see if this guy was playing recently.  if so, stick him where his corpse is
      ServerPlayer player;
      ServerPlayerMap::const_iterator iPlayer = m_mapNextPlayerz.find(sfData.lstIds[x]);
      if(iPlayer != m_mapNextPlayerz.end())
      {
        ss<<" revived "<<sfData.clientDesc.GetName(x)<<".";
          
        player = iPlayer->second;
        player.p->SetActionFlags(0,ACTION_FLAG_IDLE | ACTION_FLAG_DEAD);
        bool fWasSpectator = false;
        if(sfData.clientDesc.rgPlayerDevices[x] == SPECTATING)
        {
          player.p->SetActionFlags(ACTION_FLAG_SPECTATOR, ACTION_FLAG_SPECTATOR);
          ss<<" is spectating.";
        }
        else
        {
          if(IS_FLAG_SET(player.p->GetActionFlags(),ACTION_FLAG_SPECTATOR))
          {
            fWasSpectator = true;
            cout<<player.p->GetName()<<" was a spectator but is now real"<<endl;
          }
          player.p->SetActionFlags(0,ACTION_FLAG_SPECTATOR);
        }
          

        const PLAYERDIST oldCorpsePos = player.p->GetDistance();

        ((Player*)player.p.get())->InitPos(m_tmNow, PLAYERDIST(0,0,m_spMapConst->GetLapLength()),player.p->GetDistance(),0,0,flMassKg,m_pStatStore.get(),0, sfData.clientDesc.rgPlayerDevices[x],0); // we need to do this first to ensure that the player has loaded his historic data
        ss<<" has historic power "<<player.p->GetBestStats().GetOriginalHistoric()<<"W/kg. ";

        if(m_eRaceMode == RACEMODE_TIMETRIAL)
        {
          ss<<" is doing tt.";
          ((Player*)player.p.get())->InitPos(m_tmNow, m_spMapConst->GetStartDistance(),m_spMapConst->GetStartDistance(),0,0,flMassKg,m_pStatStore.get(),TIMETRIAL_STARTTIME_OFFSET, sfData.clientDesc.rgPlayerDevices[x],0);
        }
        else
        {

          const PLAYERDIST flPlaceSpot = fWasSpectator ? GetPlacementPosition((Player*)player.p.get(), ss) : oldCorpsePos; // where does this guy get placed?
          const PLAYERDIST flRecordStartPos = fWasSpectator ? flPlaceSpot : player.p->GetStartDistance(); // where do we say this player started in the DB?
            
          ss<<" is doing race. Placed at "<<flPlaceSpot<<"m.";
          ((Player*)player.p.get())->InitPos(m_tmNow, flRecordStartPos,flPlaceSpot,0,0,flMassKg,m_pStatStore.get(),0, sfData.clientDesc.rgPlayerDevices[x],0);
        }
      }
      else
      {
        ss<<" is new player.";
        player = ServerPlayer(boost::shared_ptr<Player>(new Player(m_tmNow, sfData.clientDesc.iMasterId, sfData.clientDesc.iDefaultTeamId, sfData.clientDesc.GetName(x),m_spMapConst, sfData.clientAddr->ToDWORD(),AISELECTION())));
        if(m_eRaceMode == RACEMODE_TIMETRIAL)
        {
          ss<<" is doing tt.";
          ((Player*)player.p.get())->InitPos(m_tmNow, m_spMapConst->GetStartDistance(),m_spMapConst->GetStartDistance(),0,0,flMassKg, m_pStatStore.get(),TIMETRIAL_STARTTIME_OFFSET, sfData.clientDesc.rgPlayerDevices[x],0); // in the future, this could be something like "dead last current player -500m", so that new connections would start at the end of the race

          // let's also check to see if we can generate a ghost rider for this dooood
          IPlayer* pGhost = 0;
          m_pStatStore->LoadTTGhost(player.p,m_iRaceId,&pGhost);
          if(pGhost)
          {
            // holy crap, we loaded a ghost!
            m_mapLocalsNext.insert(std::pair<int,ServerPlayer>(pGhost->GetId(),IPlayerPtr(pGhost)));
          }
        }
        else
        {
          ((Player*)player.p.get())->InitPos(m_tmNow, m_spMapConst->GetStartDistance(),m_spMapConst->GetStartDistance(),0,0,flMassKg,m_pStatStore.get(),0, sfData.clientDesc.rgPlayerDevices[x],0); // we need to do this first to ensure that the player has loaded his historic data

          ss<<" has historic power "<<player.p->GetBestStats().GetOriginalHistoric()<<"W/kg. ";
          const PLAYERDIST flPlace = GetPlacementPosition((Player*)player.p.get(), ss);
          DASSERT(flPlace.IsValid());
          ss<<" is racing.  Placed at "<<flPlace<<"m.";
          ((Player*)player.p.get())->InitPos(m_tmNow, flPlace,flPlace,0,0,flMassKg, m_pStatStore.get(),0, sfData.clientDesc.rgPlayerDevices[x], 0); // in the future, this could be something like "dead last current player -500m", so that new connections would start at the end of the race
        }
      }
      m_ttState.AddName(player.p->GetId(),player.p->GetName());

      {
        char szNotify[500];
        _snprintf(szNotify,sizeof(szNotify),"%s has joined",sfData.clientDesc.GetName(x).c_str());
        if(m_ePhysicsMode == ::FTPHANDICAPPED_2013 || m_ePhysicsMode == ::FTPHANDICAPPED_NODRAFT_2013)
        {
          char szAddition[200];
          snprintf(szAddition,sizeof(szAddition), " (FTP: %2.2f W/kg)",player.p->GetBestStats().GetOriginalHistoric());
          strcat(szNotify,szAddition);
        }
        SendServerChat(szNotify,SendToEveryone());
      }

      if(sfData.clientDesc.rgPlayerDevices[x] == SPECTATING)
      {
        cout<<sfData.clientDesc.GetName(x)<<" is spectating"<<endl;
        player.p->SetActionFlags(ACTION_FLAG_SPECTATOR, ACTION_FLAG_SPECTATOR);
      }

      AddPlayer(player, x==0);
    }
  }
}

void TourServer::SimpleServer_NotifyReconnectedPlayer(const ClientData& sfData)
{
  DASSERT(!m_cs.IsOwned());
  stringstream ssLogData;
  {
    AutoLeaveCS _cs(m_csTransaction);
    StateDataPtr spData = GetWriteableState();

    if(!spData->NotifyReconnection(sfData,ssLogData))
    {
      SimpleServer_NotifyNewPlayer(sfData);
    }
      
    SetState(spData);
  }
  m_pStatStore->AddAction(ssLogData.str(),GetServerLogName(m_iServerId));
}
bool StateData::NotifyReconnection(const inheritedServer::ClientData& sfData, stringstream& ssLogData)
{
  bool fFailed = false;
  for(unsigned int x = 0;x < sfData.lstIds.size(); x++)
  {
    ServerPlayer player;
    ServerPlayerMap::const_iterator i = m_mapNextPlayerz.find(sfData.lstIds[x]);
    if(i != m_mapNextPlayerz.end()) player = i->second;
      
    if(player.p)
    {
      {
        char szNotify[200];
        _snprintf(szNotify,sizeof(szNotify),"%s has reconnected", player.p->GetName().c_str());
        SendServerChat(szNotify,SendToEveryone());
      }

      ssLogData<<player.p->GetName()<<" has reconnected the normal way"<<endl;
      cout<<"Player "<<player.p->GetName()<<" has reconnected"<<endl;
      player.p->SetActionFlags(0,ACTION_FLAG_DEAD);
    }
    else
    {
      cout<<"Player "<<sfData.lstIds[x]<<" has tried to reconnect, but we don't remember them"<<endl;
      fFailed = true;
      break;
    }
  }
  return !fFailed;
}
void TourServer::SimpleServer_NotifyDeadPlayer(const ClientData& sfData)
{
  DASSERT(!m_cs.IsOwned());
  stringstream ssLogData;
  {
    AutoLeaveCS _cs(m_csTransaction);
    StateDataPtr spData = GetWriteableState();
    spData->NotifyDeadPlayer(sfData,ssLogData);
    SetState(spData);
  }
  m_pStatStore->AddAction(ssLogData.str(),GetServerLogName(m_iServerId));
}
void StateData::NotifyDeadPlayer(const inheritedServer::ClientData& sfData, stringstream& ssLogData)
{
  ssLogData<<"Missing player "<<sfData.clientDesc.GetName(0)<<".  Counting as dead";

  cout<<sfData.clientDesc.GetName(0)<<" and cohorts hasn't been heard from.  Cut his power."<<endl;
  for(unsigned int x = 0;x < sfData.lstIds.size(); x++)
  {
    m_ttState.ClearPlayer(sfData.lstIds[x]);
        
    if(m_mapNextPlayerz.find(sfData.lstIds[x]) != m_mapNextPlayerz.end())
    {
      ServerPlayer p = m_mapNextPlayerz[sfData.lstIds[x]];
      ((Player*)p.p.get())->SetPower((3*p.p->GetAveragePower())/4,m_tmNow);
      p.p->SetActionFlags(ACTION_FLAG_DEAD,ACTION_FLAG_DEAD);
      p.SetLastDataTime(m_tmNow); // we'll prune this guy for good if he hasn't returned in 5 minutes

      char szChat[300];
      snprintf(szChat,sizeof(szChat),"The server has lost contact with %s",p.p->GetName().c_str());
      SendServerChat(szChat,SendToEveryone());
    }
  }

}
void TourServer::SimpleServer_NotifyClientState(const ClientData& sfClient, const TDGClientState& sfData)
{
  DASSERT(ArtGetCurrentThreadId() != g_gameThdId);

  QueuePlayerUpdate(sfClient,sfData);
}
void TourServer::QueuePlayerUpdate(const ClientData& sfClient, const TDGClientState& sfData)
{
  AutoLeaveCS _cs(m_csQueue);
  m_qUpdates.push(QUEUEDUPDATE(sfData));
}
void TourServer::QueuePlayerHRCadUpdate(const ClientData& sfClient, const C2S_HRMCADUPDATE_DATA& sfData)
{
  AutoLeaveCS _cs(m_csQueue);
  m_qUpdates.push(QUEUEDUPDATE(sfData));
}


void StateData::NotifyClientState(const QUEUEDUPDATE& sfData)
{
  DASSERT(ArtGetCurrentThreadId() == g_gameThdId);
  if(sfData.fIsState)
  {
    // first, verify that this is a good input.
    float powerproduct = 1;
    for(int x = 0;x < min(MAX_LOCAL_PLAYERS,sfData.m_state.cLocalPlayers); x++)
    {
      powerproduct *= sfData.m_state.rgPlayerPowers[x];
    }
    if(powerproduct != sfData.m_state.powerProduct)
    {
      //cout<<"Bad client data from "<<sfClient.addr.sin_addr.S_un.S_addr<<endl;
      return;
    }
    
    for(int x = 0;x < min(MAX_LOCAL_PLAYERS,sfData.m_state.cLocalPlayers); x++)
    {
      if(m_mapNextPlayerz.find(sfData.m_state.rgPlayerIds[x]) != m_mapNextPlayerz.end())
      {
        ServerPlayer pPlayer = m_mapNextPlayerz[sfData.m_state.rgPlayerIds[x]];
        DASSERT(pPlayer.p); // we had better have found one!
        if(pPlayer.p)
        {
          pPlayer.p->SetActionFlags(sfData.m_state.rgfdwActionFlags[x], ACTION_FLAG_ALL_FROMCLIENT);
          pPlayer.p->SetActionFlags(0,ACTION_FLAG_DEAD); // this guy clearly isn't dead
          
          ((Player*)pPlayer.p.get())->SetPower(sfData.m_state.rgPlayerPowers[x],m_tmNow);
          pPlayer.SetLastDataTime(m_tmNow);
        }
      }
    }
  }
  else if(!sfData.fIsState)
  {
    // heartrate/cadence update
    for(int x = 0;x < min(sfData.m_hrcad.cLocalPlayers,MAX_LOCAL_PLAYERS); x++)
    {
      if(m_mapNextPlayerz.find(sfData.m_hrcad.rgPlayerIds[x]) != m_mapNextPlayerz.end())
      {
        ServerPlayer pPlayer = m_mapNextPlayerz[sfData.m_hrcad.rgPlayerIds[x]];
        DASSERT(pPlayer.p); // we had better have found one!
        if(pPlayer.p)
        {
          pPlayer.p->SetHR(m_tmNow,sfData.m_hrcad.rgHR[x]);
          pPlayer.p->SetCadence(m_tmNow,sfData.m_hrcad.rgCadence[x]);
        }
      }
    }
  }
}
void TourServer::SimpleServer_NotifyC2SData(const inheritedServer::ClientData& src, const TDGClientToServerSpecial& c2s)
{
  DASSERT(!m_cs.IsOwned());

  if(c2s.eType == C2S_PLAYERUPDATE)
  {
    // player update data is frequent enough that we need to special-case it into our queue rather than getting a writeable state
    QueuePlayerUpdate(src,c2s.state);
  }
  else if(c2s.eType == C2S_HRMCADUPDATE)
  {
    QueuePlayerHRCadUpdate(src,c2s.hrcad);
  }
  else
  {
    stringstream ssLogData;
    {
      AutoLeaveCS _cs(m_csTransaction);
      StateDataPtr spData = GetWriteableState();
      spData->NotifyC2SData(src,c2s, m_iServerId, m_lstAIs, m_lstMaps, ssLogData);
      SetState(spData);
    }
    if(ssLogData.str().length() > 0)
    {
      m_pStatStore->AddAction(ssLogData.str().c_str(),GetServerLogName(m_iServerId));
    }
  }

}
void StateData::NotifyC2SData(const inheritedServer::ClientData& src, const TDGClientToServerSpecial& c2s, int iServerId, const vector<AIDLL>& lstAIs, const vector<MAPDATA>& lstMaps, stringstream& ssLogData)
{
  // a client has sent us something.  Probably a chat
  if(c2s.eType == C2S_CHAT_UPDATE)
  {
    ServerPlayerMap::const_iterator i = m_mapNextPlayerz.find(src.lstIds[0]);
    if(i != m_mapNextPlayerz.end())
    {
      ServerPlayer pPlayer = i->second;
        
      DASSERT(pPlayer.p);
      if(pPlayer.p)
      {
        cout<<pPlayer.p->GetName()<<" said '"<<c2s.chat.szChat<<"'"<<endl;
        // they want to chat to us!  this means we'll have to queue the chat update to send back to the other players
      
        for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++)
        {
          if(IS_FLAG_SET(i->second.p->GetActionFlags(),ACTION_FLAG_LOCATION_MAIN_PLAYER))
          {
            m_mapNextPendingChats[i->first].push(c2s.chat);
            cout<<i->second.p->GetName()<<" has "<<m_mapNextPendingChats[i->first].size()<<" pending chats"<<endl;
          }
        }

        ssLogData<<pPlayer.p->GetName().c_str()<<" chatted "<<c2s.chat.szChat;
      }
    }
      
  }
  else if(c2s.eType == C2S_PLAYERUPDATE)
  {
    // a player update over TCP!  Well, I never!
    DASSERT(FALSE); // this should get stuck into the update queue rather than handled here
    NotifyClientState(c2s.state);
  }
  else if(c2s.eType == C2S_CHANGEAI)
  {
    if(m_sfSchedRace.IsValid())
    {
      char szString[100];
      _snprintf(szString,sizeof(szString),"You can't add or remove AIs on a scheduled race.");
      SendServerChat(szString,SendToEveryone());
    }
    else if(m_eGameState == WAITING_FOR_START)
    {
      int cChange = c2s.data;
      
      if(c2s.data < 0)
      {
        cout<<src.clientDesc.GetName(0).c_str()<<" requested that we kill "<<-c2s.data<<" ais"<<endl;
        cChange = max(c2s.data,-10); // don't kill more than 10 at once
        cChange = min((int)m_mapLocalsNext.size(),-cChange); // don't kill more AIs than we have
        

        const int cToKill = cChange;
        int cKilled = 0;
        vector<ServerPlayer> lstChoices;
        for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) lstChoices.push_back(i->second);

        for(unsigned int x = 0;x < lstChoices.size(); x++) // filter out all the already-doomed AIs
        {
          if(lstChoices[x].p->GetActionFlags() & ACTION_FLAG_DOOMEDAI)
          {
            lstChoices[x] = lstChoices[lstChoices.size()-1];
            lstChoices.pop_back();
            x--;
          }
        }
        while(cKilled < cToKill && lstChoices.size() > 0) // kill AIs
        {
          const int r = rand() % lstChoices.size();
          lstChoices[r].p->SetActionFlags(ACTION_FLAG_DOOMEDAI,ACTION_FLAG_DOOMEDAI);
          lstChoices[r] = lstChoices[lstChoices.size()-1];
          lstChoices.pop_back();
          cKilled++;
        }
        char szString[100];
        _snprintf(szString,sizeof(szString),"%s killed %d AIs",src.clientDesc.GetName(0).c_str(), cToKill);
        SendServerChat(szString,SendToEveryone());
      }
      else if(c2s.data > 0)
      {
        cout<<src.clientDesc.GetName(0).c_str()<<" requested that we add "<<c2s.data<<" ais"<<endl;
        cChange = min(cChange,10); // don't add more than 10 at once
        
        vector<ServerPlayer> lstChoices;
        for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) lstChoices.push_back(i->second);

        int cCurrentAICount = 0;
        for(unsigned int x = 0;x < lstChoices.size(); x++)
        {
          if(lstChoices[x].p->GetActionFlags() & ACTION_FLAG_DOOMEDAI)
          {

          }
          else
          {
            cCurrentAICount++;
          }
        }
        cChange = min(cChange,80 - cCurrentAICount); // don't let them add more than a certain # of players
        
        char szString[100];
        _snprintf(szString,sizeof(szString),"%s added %d AIs",src.clientDesc.GetName(0).c_str(), cChange);
        SendServerChat(szString,SendToEveryone());

        float flMinPlayerPower = m_iMaxAIPower;
        float flMaxPlayerPower = m_iMinAIPower;
        cout<<"picking AI strengths"<<endl;
        for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++)
        {
          const ServerPlayer pPlayer = i->second;
          DASSERT(!pPlayer.p->Player_IsAI());

          const DWORD fdwDeadMask = ACTION_FLAG_DEAD | ACTION_FLAG_IDLE | ACTION_FLAG_GHOST;
          if(!(pPlayer.p->GetActionFlags() & fdwDeadMask)) // don't use players that are idle, dead, or ghosts to calculate AI strengths
          {
            bool fFaked=false;
            const float flHistoric = pPlayer.p->GetBestStats().GetHistoricPowerWkg(&fFaked);
            cout<<"Historic power for "<<pPlayer.p->GetName()<<" is "<<flHistoric<<"W/kg (fFaked="<<fFaked<<")"<<endl;
            flMinPlayerPower = min(flHistoric*pPlayer.p->GetMassKg(),flMinPlayerPower);
            flMaxPlayerPower = max(flHistoric*pPlayer.p->GetMassKg(),flMaxPlayerPower);
          }
        }
        cout<<"done picking AI strengths"<<endl;
        if(m_mapNextPlayerz.size() > 0)
        {
          AddDopers(cChange, flMinPlayerPower*0.80, flMaxPlayerPower*1.15, lstAIs);
        }
        else
        {
          // nobody is here, so just use the defaults.
          AddDopers(cChange, m_iMinAIPower, m_iMaxAIPower, lstAIs);
        }
      }
    }
  }
  else if(c2s.eType == C2S_RESTART_REQUEST)
  {
    if(m_sfSchedRace.IsValid())
    {
      SendServerChat("You can't restart a scheduled race!",SendToEveryone());
    }
    else if(m_eRaceMode == RACEMODE_TIMETRIAL)
    {
      // this means we should restart all the players from this client
      if(m_eGameState == RACING)
      {
        char szString[100];
        _snprintf(szString,sizeof(szString),"Timetrial: Restarting all the riders with %s",src.clientDesc.GetName(0).c_str());
        ssLogData<<szString;
        SendServerChat(szString,SendToEveryone());
          
        for(unsigned int x = 0;x < src.lstIds.size(); x++)
        {
          ServerPlayerMap::iterator i = m_mapNextPlayerz.find(src.lstIds[x]);
          if(i != m_mapNextPlayerz.end())
          {
            ServerPlayer pPlayer = i->second;
             
            
            for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) 
            {
              if(IsGhostPair(pPlayer.p.get(),i->second.p.get()))
              {
                // get rid of this guy's ghost
                m_mapLocalsNext.erase(i->first);
              }
            }
            const float flNow = ((float)m_tmNow)/1000.0f;

            // if we haven't already, we should probably credit this guy with the meters he/she rode, since they might be pretty substantial
            if(pPlayer.p->GetFinishTime() <= 0)
            {
              vector<LAPDATA> lstLaps;
              pPlayer.p->GetLapTimes(lstLaps);
              vector<SPRINTCLIMBDATA> lstScores;
              pPlayer.p->GetSprintClimbPoints(lstScores);
              PLAYERRESULTPtr res(new PLAYERRESULT(lstLaps, pPlayer.p->GetId(),-1,flNow - pPlayer.p->GetStartTime(),pPlayer.p->GetDistance().Minus(pPlayer.p->GetStartDistance()),pPlayer.p->GetAveragePower(),pPlayer.p->GetName(),pPlayer.p->GetDistance(),pPlayer.p->GetMassKg(),pPlayer.p->GetIP(),RACEEND_RESTART,pPlayer.p->GetReplayOffset(),pPlayer.p->GetPowerHistory(),pPlayer.p->Player_IsAI(),pPlayer.p->GetPowerType(),pPlayer.p->GetPowerSubType(),lstScores, pPlayer.p->GetTeamNumber()));
              int idRaceEntry = 0;
              DWORD tmRecordTime=0;
              m_pStatStore->StoreRaceResult(m_iRaceId,m_iMapId,RACEEND_RESTART,res,&idRaceEntry,&tmRecordTime);
            }

            // let's see if we've got a ghost for this guy...
            IPlayer* pGhost = 0;
            m_pStatStore->LoadTTGhost(pPlayer.p,m_iRaceId,&pGhost);
            if(pGhost)
            {
              m_mapLocalsNext.insert(std::pair<int,ServerPlayer>(pGhost->GetId(),ServerPlayer(IPlayerPtr(pGhost))));
            }
            ((Player*)pPlayer.p.get())->InitPos(m_tmNow, m_spMapConst->GetStartDistance(),m_spMapConst->GetStartDistance(),0,0,pPlayer.p->GetMassKg(), m_pStatStore.get(),TIMETRIAL_STARTTIME_OFFSET, src.clientDesc.rgPlayerDevices[x],0); // in the future, this could be something like "dead last current player -500m", so that new connections would start at the end of the race
          }
        }
      }
    }
    else
    {
      char szString[100];
      _snprintf(szString,sizeof(szString),"%s requested a restart",src.clientDesc.GetName(0).c_str());
      SendServerChat(szString,SendToEveryone());
      
      for(unsigned int x = 0;x < src.lstIds.size(); x++)
      {
        ServerPlayerMap::iterator i = m_mapNextPlayerz.find(src.lstIds[x]);
        if(i != m_mapNextPlayerz.end())
        {
          ServerPlayer pPlayer = i->second;
          this->m_mapPlayerRestartRequests[pPlayer.p->GetId()] = m_tmNow;
        }
      }
      int cVotes = 0;
      if(CheckForRestart(&cVotes))
      {
        snprintf(szString,sizeof(szString),"Restarting: %d/%d players voted for restart",cVotes,m_mapNextPlayerz.size());
        ssLogData<<szString;
        // enough players have voted for a restart.
        Restart(RACEEND_RESTART, lstMaps, iServerId);
      }
      SendServerChat(szString,SendToEveryone());
    }
  }
  else if(c2s.eType == C2S_PHYSICS_CHANGE)
  {
    if(m_eGameState == WAITING_FOR_START)
    {
      m_ePhysicsMode = (PHYSICSMODE)c2s.data;
      if(m_ePhysicsMode < PHYSICSMODE_FIRST || m_ePhysicsMode >= PHYSICSMODE_LAST) m_ePhysicsMode = PHYSICSMODE_FIRST;
 
      char szString[100];
      snprintf(szString,sizeof(szString),"%s changed server physics to %s mode (switch with ctrl-w)",src.clientDesc.GetName(0).c_str(), GetPhysicsModeString(m_ePhysicsMode));
      SendServerChat(szString,SendToEveryone());
    }
  }
  else if(c2s.eType == C2S_GAME_PAUSE)
  {
    if(m_fSinglePlayer)
    {
      if(c2s.data)
      {
        SendServerChat("Game Paused (ctrl-e to resume)",SendToEveryone());
        m_fGamePaused = true;
      }
      else
      {
        SendServerChat("Game Paused (ctrl-p to pause again)",SendToEveryone());
        m_fGamePaused = false;
      }
    }
    else
    {
      // haha, you aren't pausing an online server...
    }
  }
  else if(c2s.eType == C2S_COUNTDOWN_PAUSE)
  {
    // c2s.data = 1 --> they want to pause
    // c2s.data = 0 --> they want to resume
    bool fWaitingForStart = m_eGameState == WAITING_FOR_START && // we're waiting for race start
                            (!m_sfSchedRace.IsValid() || c2s.data == 0); // and we aren't a scheduled race OR they're trying to resume
    if(fWaitingForStart ||
       m_fSinglePlayer)
    {
      char szString[100];
      if(c2s.data) // they're pausing
      {
        if(iServerId >= 30 && iServerId < 36 || m_fSinglePlayer) // are we the kind of server that permits pausing?
        {
          snprintf(szString,sizeof(szString),"%s paused the countdown (resume with ctrl-e)",src.clientDesc.GetName(0).c_str());
          m_tmCountdownLock = m_tmStart - m_tmNow;
        }
        else
        {
          snprintf(szString,sizeof(szString),"You can't pause on a scheduled server");
        }
      }
      else
      {
        snprintf(szString,sizeof(szString),"%s resumed the countdown (pause with ctrl-p)",src.clientDesc.GetName(0).c_str());
        m_tmCountdownLock = 0;
      }
      SendServerChat(szString,SendToEveryone());
    }
    else if(m_sfSchedRace.IsValid())
    {
      SendServerChat("You can't pause/resume the countdown for a scheduled race!",SendToEveryone());
    }
  }
  else if(c2s.eType == C2S_STARTNOW)
  {
    if(m_sfSchedRace.IsValid() && m_sfSchedRace.scheduleId >= 0)
    {
      SendServerChat("You can't force-start a scheduled race",SendToEveryone());
    }
    else if(m_eGameState == WAITING_FOR_START)
    {
      char szString[100];
      snprintf(szString,sizeof(szString),"%s forced the game to start",src.clientDesc.GetName(0).c_str());
      m_tmCountdownLock = 0;

      if(IsDebuggerPresent())
      {
        m_tmStart = min(m_tmStart,m_tmNow + 5000); // if they're a long way from the start, then move it to 60 seconds
      }
      else
      {
        m_tmStart = min(m_tmStart,m_tmNow + 60000); // if they're a long way from the start, then move it to 60 seconds
      }

      SendServerChat(szString,SendToEveryone());
    }
  }
}
void StateData::ApplyClientStateQueue(queue<QUEUEDUPDATE>& qUpdates)
{
  DASSERT(g_gameThdId == ArtGetCurrentThreadId()); // only do this on the game thread, k?
  while(qUpdates.size() > 0)
  {
    const QUEUEDUPDATE& update = qUpdates.front();
    NotifyClientState(update);
    qUpdates.pop();
  }
}

void TourServer::SimpleServer_AssignIds(const TDGClientDesc& cd, int* prgIds)
{
  StateDataConstPtr spState = this->GetCurState();
  for(int x = 0; x < min(MAX_LOCAL_PLAYERS,cd.cLocalPlayers); x++)
  {
    string strName = cd.GetName(x);
    prgIds[x] = m_pStatStore->GetPlayerId(cd.iMasterId, cd.iDefaultTeamId, strName,false,spState->GetMap().get(),NULL,NULL,NULL,GetSecondsSince1970GMT());
  }
}


void StateData::Restart(RACEENDTYPE eRestartType, const vector<MAPDATA>& lstMaps, const int iServerId, SCHEDULEDRACE* pNextRace)
{
  m_fGamePaused = false;
  if(this->m_fSinglePlayer)
  {
    switch(eRestartType)
    {
    case RACEEND_RESTART: // someone voted to restart
    case RACEEND_FINISH: // someone crossed the finish line
      break; // let the restart happen
    case RACEEND_DISCONNECT: // buddy disconnected
    case RACEEND_NOHUMANS: // buddy disconnected
    case RACEEND_SCHEDULE: // a scheduled race came up
      return; // screw you buddy, no restarting here!
    }
  }
  cout<<"Restarting..."<<endl;
  if(m_eRaceMode == RACEMODE_TIMETRIAL)
  {
    cout<<"someone tried to restart a time trial server.  What idiots."<<endl;
    return;
  }
  if(pNextRace && pNextRace->IsValid() && m_sfSchedRace.IsValid() && pNextRace->IsSame(m_sfSchedRace))
  {
    cout<<"Bugs!  trying to restart using a scheduled race that we're already running!"<<endl;
    return;
  }
  //DASSERT(boost::thread::id(m_cs.locking_thread_id == boost::this_thread::get_id());

  vector<ServerPlayer> lstToUpdate;
  BuildAllPlayerList(lstToUpdate);
    
    
  unordered_map<int,IPlayerPtrConst> mapPlayers;
  {
    for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++) { mapPlayers[i->first] = i->second.p; }
    for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) { mapPlayers[i->first] = i->second.p; }
  }
  
  const bool fTimedMode = IsTimedMode(m_spMapConst->GetLaps(),m_spMapConst->GetTimedLength());
  if(m_eGameState == RACING) // only save results if we were actually racing
  {
    if(m_eRaceMode == RACEMODE_TIMETRIAL)
    {// no results to save: a time trial saves its results continuously
    }
    else
    { 
      DASSERT(m_eRaceMode == RACEMODE_ROADRACE);
      vector<IConstPlayerPtrConst> lstFinishOrder;
      for(unsigned int x = 0;x < lstToUpdate.size(); x++)
      {
        lstFinishOrder.push_back(lstToUpdate[x].p);
      }
      PlayerRankCompare<StateData> prc(this);
      sort(lstFinishOrder.begin(),lstFinishOrder.end(),prc);
      const float flNow = ((float)m_tmNow)/1000.0f;

      for(unsigned int x = 0;x < lstFinishOrder.size(); x++)
      {
        if(lstFinishOrder[x]->GetFinishTime() <= 0)
        {
          // this guy didn't finish, so he won't be in the race results
          IConstPlayerPtrConst pPlayer = lstFinishOrder[x];
          vector<LAPDATA> lstLaps;
          pPlayer->GetLapTimes(lstLaps);
          vector<SPRINTCLIMBDATA> lstScores;
          pPlayer->GetSprintClimbPoints(lstScores);

          float flTimeRidden = flNow - pPlayer->GetStartTime();
          if(fTimedMode)
          {
            flTimeRidden = flTimeRidden > ((float)m_spMapConst->GetTimedLength()) * 0.98f ? 
                                 m_spMapConst->GetTimedLength() : // if we were there for 98% of the time, just say we were there for all of it
                                 flTimeRidden; // if we weren't there for 98% of the time, say how long we were there
          }
          PLAYERRESULT result(lstLaps,
                              pPlayer->GetId(),
                              m_raceResults.GetFinisherCount() + 1,
                              flTimeRidden,
                              pPlayer->GetDistance().Minus(pPlayer->GetStartDistance()),
                              pPlayer->GetAveragePower(),
                              pPlayer->GetName(),
                              pPlayer->GetDistance(),
                              pPlayer->GetMassKg(),
                              pPlayer->GetIP(),
                              RACEEND_RESTART,
                              pPlayer->GetReplayOffset(),
                              pPlayer->GetPowerHistory(),
                              pPlayer->Player_IsAI(),
                              pPlayer->GetPowerType(),
                              pPlayer->GetPowerSubType(),
                              lstScores,
                              pPlayer->GetTeamNumber());

          {
            m_raceResults.AddFinisher(result, mapPlayers, m_spMapConst->GetEndDistance().Minus(m_spMapConst->GetStartDistance()));
          }
        }
      }
    }
  }

  // store statistics for each player
  if(m_raceResults.GetFinisherCount() > 0)  
  {
    const int startpct = m_sfSchedRace.IsValid() ? m_sfSchedRace.iStartPercent : 0;
    const int endpct = m_sfSchedRace.IsValid() ? m_sfSchedRace.iEndPercent : 100;

    m_raceResults.SetInfo(m_eRaceMode,m_sfSchedRace.timedLengthSeconds,m_iMapId,m_sfSchedRace.scheduleId,startpct,endpct,m_fSinglePlayer,m_ePhysicsMode);

    new ThreadedRaceResultSaver(m_iMasterId, m_spMapConst, m_raceResults, m_pStatStore, eRestartType);
    
  }

  m_raceResults.Clear();
    
  if(!m_fSinglePlayer)
  {
    NextMap(pNextRace,lstMaps, iServerId);
    m_pServer->DisconnectAll();

    m_tmFirstFinisher = 0;
    m_tmFirstHumanFinisher = 0;
    m_tmLastHumanFinisher = 0;
    m_eGameState = WAITING_FOR_START;

    this->m_mapNextPlayerz.clear();
    this->m_mapNextPendingChats.clear();
    this->m_mapLocalsNext.clear();
    m_fAIsAddedYet = false;
  }
  else
  {
    SetShutdown(); // we're done here.
  }
}


// figures out where to put a player, taking into account their power level and the historic power levels of the other players in the race
PLAYERDIST StateDataConst::GetPlacementPosition(const IConstPlayer* pPlayer, stringstream& ssLogData) const
{
  vector<ConstServerPlayer> lstPlayers;
  BuildAllPlayerListConst(lstPlayers);
  if(lstPlayers.size() <= 0) return m_spMapConst->GetStartDistance();

  // first, let's see if there's a human with a similar power/weight ratio
  PLAYERDIST flPlacePosHuman(0,0,0);
  PLAYERDIST flPlacePosAI(0,0,0);
  float flWKgDiffHuman = 100; // if we find someone with a better wkg diff than this, we'll use them
  float flWKgDiffAI = 100;

  IConstPlayerPtrConst pPlacedNearAI;
  IConstPlayerPtrConst pPlacedNearHuman;

  bool fFaked=false;
  const float flNewGuyPowerWKg = pPlayer->GetBestStatsConst().GetHistoricPowerWkg(&fFaked);
  for(unsigned int x = 0; x < lstPlayers.size(); x++)
  {
    IConstPlayerPtrConst p = lstPlayers[x].p;
    if(p->GetDistance() >= m_spMapConst->GetEndDistance())
    {
      // don't place him near a finished guy.  that's dumb.
      continue;
    }
    if(p->GetActionFlags() & (ACTION_FLAG_IDLE | ACTION_FLAG_DEAD))
    {
      // don't place him near a guy that is gone.
      continue;
    }

    // so here's the idea: we need to figure out the current rider (from lstPlayers) who most-closely approximates pPlayer in terms of power
    // if there's a human, we will prefer dropping pPlayer by the human over dropping them
    const PERSONBESTSTATS& person = p->GetBestStatsConst();
    bool fFaked=false;
    const float flThisGuyPowerWKg = person.GetHistoricPowerWkg(&fFaked);
    const float flDiffWKg = abs(flThisGuyPowerWKg - flNewGuyPowerWKg);
    if(p->Player_IsAI())
    {
      if(flDiffWKg < flWKgDiffAI)
      {
        flWKgDiffAI = flDiffWKg;
        flPlacePosAI = p->GetDistance();
        pPlacedNearAI = p;
      }
    }
    else
    {
      if(flDiffWKg < flWKgDiffHuman)
      {
        flWKgDiffHuman = flDiffWKg;
        flPlacePosHuman = p->GetDistance();
        pPlacedNearHuman = p;
      }
    }
  }

  PLAYERDIST flRet = m_spMapConst->GetStartDistance();
  IConstPlayerPtrConst pPlacedNear;

  if(flPlacePosHuman.IsValid() && flPlacePosHuman >= flRet)
  {
    flRet = flPlacePosHuman;
    pPlacedNear = pPlacedNearHuman;
  }
  else if(flPlacePosAI.IsValid() && flPlacePosAI >= flRet)
  {
    flRet = flPlacePosAI;
    pPlacedNear = pPlacedNearAI;
  }
  DASSERT(flRet.IsValid());

  { // log why they got placed where they did
    if(pPlacedNear)
    {
      char szText[300];
      bool fFaked=false;
      snprintf(szText,sizeof(szText),"Placed new guy %s near %s because avgpower %3.2f matched %3.2f",pPlayer->GetName().c_str(),pPlacedNear->GetName().c_str(),pPlayer->GetBestStatsConst().GetHistoricPowerWkg(&fFaked),pPlacedNear->GetBestStatsConst().GetHistoricPowerWkg(&fFaked));
      ssLogData<<szText;
    }
  }
  return flRet;
}


template<class TCrit>
void StateData::SendServerChat(string str,const TCrit& crit)
{
  for(int x = 0;x < (int)str.size(); x++)
  {
    int iChar = (int)str[x];
    if(iChar >= 127 || iChar < 0) str[x] = '_'; // making safe non-ASCII characters
  }
  CHATUPDATE cu;
  cu.idFrom = 0;
  strncpy(cu.szChat,str.c_str(),sizeof(cu.szChat));
  GetSystemTimeAsFileTime(&cu.tmSent);
    
  for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++)
  {
    if(crit.ShouldSendTo(i->second.p.get()))
    {
      m_mapNextPendingChats[i->first].push(cu);
    }
  }
}

// decides for a given update, what players will be included in it
// returns 0 if we should send an update for these players immediately
// returns >0 if we should wait <return> milliseconds before sending an update
bool StateDataConst::IsDelayed() const
{
  return m_tmNow < m_tmStart;
}
int StateDataConst::GetTimeUntilStart()
{
  return m_tmStart - m_tmNow;
}
bool StateData::AddLocal(ServerPlayer sfPlayer)
{
  for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++)
  {
    if(i->second.p->GetId() == sfPlayer.p->GetId())
    {
      return false;
    }
  }
  m_mapLocalsNext.insert(std::pair<int,ServerPlayer>(sfPlayer.p->GetId(),sfPlayer));
  return true;
}
void StateData::AddPlayer(ServerPlayer sfPlayer, bool fIsMainPlayer)
{
  sfPlayer.p->SetActionFlags(fIsMainPlayer ? ACTION_FLAG_LOCATION_MAIN_PLAYER : 0,ACTION_FLAG_LOCATION_MAIN_PLAYER);
  m_mapNextPlayerz[sfPlayer.p->GetId()] = sfPlayer;
}
bool StateDataConst::CheckForRestart(int* piVotes) const
{
  // if more than half of players have voted to restart in the last 30 seconds, then restart
  int cVotes = 0;
  for(ConstServerPlayerMap::const_iterator i = m_mapPlayerz.begin(); i != m_mapPlayerz.end(); i++)
  {
    unordered_map<int,DWORD>::const_iterator iStats = m_mapPlayerRestartRequests.find(i->second.p->GetId());
    if(iStats != m_mapPlayerRestartRequests.end())
    {
      if(m_tmNow - iStats->second < 30000)
      {
        cVotes++;
      }
    }
  }
  *piVotes = cVotes;
  return cVotes >= (int)m_mapPlayerz.size()/2;
}


void StateData::DoGameTick(int* pmsWait, ScheduleChecker* pSchedCheck, const vector<AIDLL>& lstDLLs, DWORD* ptmLastUpdate, const vector<MAPDATA>& lstMaps, const int iServerId)
{
  static int counter = 0;
  counter++;
  switch(m_eGameState)
  {
  case UNKNOWN:
  case WAITING_FOR_START:
  {
    // not much to do here.  players don't move.  just check to see if we've advanced to the game start
    *pmsWait = 100;
    if(counter % 100 == 0)
    {
      cout<<this->GetTimeUntilStart()<<"ms until start"<<endl;
      SCHEDULEDRACE sfRace;

      if(GetSecondsSince1970GMT() > m_sfSchedRace.tmStart + 600)
      {
        // it's been at least 10 minutes since the scheduled start of this race.  so clear it.
        m_sfSchedRace = SCHEDULEDRACE();
        DASSERT(!m_sfSchedRace.IsValid());
      }
      if(pSchedCheck->GetNextScheduledRace(&sfRace) &&  // there is a scheduled race coming up
        (   !m_sfSchedRace.IsValid() || // and our current scheduled race isn't valid
            sfRace.tmStart < m_sfSchedRace.tmStart))  // or the new race is earlier than our current scheduled race
      {
        if(sfRace.IsValid() && sfRace.tmStart > GetSecondsSince1970GMT())
        {
          // we have a scheduled race coming up, and it is sooner than our current race (or our current race is not a scheduled race)
          Restart(RACEEND_SCHEDULE,lstMaps,iServerId, &sfRace);
        }
      }
    }
        
    int cReadyHumanPlayers = 0;
    for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++)
    {
      if(!IS_FLAG_SET(i->second.p->GetActionFlags(),ACTION_FLAG_LOADING | ACTION_FLAG_DEAD) && !i->second.p->Player_IsAI())
      {
        cReadyHumanPlayers++;
      }
    }
    if(cReadyHumanPlayers <= 0)
    {
      // no humans, no race start (saves CPU time)
      m_tmStart = max(m_tmNow + 60000,m_tmStart);
    }
    else
    {
      if(!m_fAIsAddedYet)
      {
        DASSERT(m_mapLocalsNext.size() <= 0);
        float flMinPlayerPower = m_iMaxAIPower;
        float flMaxPlayerPower = m_iMinAIPower;
        cout<<"picking AI strengths.  startmin "<<flMinPlayerPower<<" startmax "<<flMaxPlayerPower<<endl;

        if(!m_sfSchedRace.IsValid())
        {
          // only do it based on the player if we don't have a scheduled race telling us what to do
          for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++)
          {
            const ServerPlayer pPlayer = i->second;
            DASSERT(!pPlayer.p->Player_IsAI());

            if(pPlayer.p->GetActionFlags() & ACTION_FLAG_SPECTATOR || 
              pPlayer.p->GetActionFlags() & ACTION_FLAG_DEAD)
            {
              // spectators and dead people shouldn't affect the average AI strength
            }
            else
            {
              bool fFaked=false;
              const float flHistoric = pPlayer.p->GetBestStats().GetHistoricPowerWkg(&fFaked);
              cout<<"Historic power for "<<pPlayer.p->GetName()<<" (faked="<<fFaked<<") is "<<flHistoric<<"W/kg"<<endl;
              flMinPlayerPower = min(flHistoric*pPlayer.p->GetMassKg(),flMinPlayerPower);
              flMaxPlayerPower = max(flHistoric*pPlayer.p->GetMassKg(),flMaxPlayerPower);
            }
          }
        }
        else
        {
        }
        if(flMaxPlayerPower < flMinPlayerPower)
        {
          float tmp = flMaxPlayerPower;
          flMaxPlayerPower = flMinPlayerPower;
          flMinPlayerPower = tmp;
        }
        if(m_mapNextPlayerz.size() > 0 && !m_sfSchedRace.IsValid())
        {
          AddDopers(m_cAIs, flMinPlayerPower*0.80, flMaxPlayerPower*1.15,lstDLLs);
        }
        else if(m_sfSchedRace.IsValid())
        {
          AddDopers(m_sfSchedRace.cAIs, m_sfSchedRace.iAIMinStrength, m_sfSchedRace.iAIMaxStrength,lstDLLs);
        }
        else
        {
          // nobody is here, so just use the defaults.
          AddDopers(m_cAIs, m_iMinAIPower, m_iMaxAIPower,lstDLLs);
        }
        DASSERT(m_fAIsAddedYet);
      }

      if(m_tmCountdownLock)
      {
        m_tmStart = m_tmNow + m_tmCountdownLock;
      }
    }
    if(m_tmNow > m_tmStart)
    {
      unordered_set<int> setToKill;
      for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++)
      {
        // now that we're starting, kill the AIs that people requested dead
        if(i->second.p->GetActionFlags() & ACTION_FLAG_DOOMEDAI)
        {
          setToKill.insert(i->first);
        }
      }
      for(unordered_set<int>::const_iterator i = setToKill.begin(); i != setToKill.end(); i++)
      {
        m_mapLocalsNext.erase(*i);
      }

      vector<ServerPlayer> lstToUpdate;
      BuildAllPlayerList(lstToUpdate);

      {
        vector<SprintClimbPointPtr> lstSprintClimbs;
        m_spMapConst->GetScoringSources(lstSprintClimbs);
        for(unsigned int ixPoint = 0; ixPoint < lstSprintClimbs.size(); ixPoint++)
        {
          lstSprintClimbs[ixPoint]->SetPeopleCount(lstToUpdate.size());
        }
        m_cSprintClimbPeopleCount = lstToUpdate.size();
      }

      if(m_eRaceMode == RACEMODE_ROADRACE)
      {
        // in a road race, we claim that people are starting at GetStartDistance()
        const float flMinLane = -ROAD_WIDTH/2;
        const float flMaxLane = ROAD_WIDTH/2;
        float flLane = flMinLane;
            
        PLAYERDIST flDistance = m_spMapConst->GetStartDistance();
        for(unsigned int x = 0;x < lstToUpdate.size();x++)
        {
          ((PlayerBase*)lstToUpdate[x].p.get())->SetFinishTime(-1);
          ((PlayerBase*)lstToUpdate[x].p.get())->InitPos(m_tmNow, m_spMapConst->GetStartDistance(),flDistance,flLane,10,lstToUpdate[x].p->GetMassKg(),m_pStatStore.get(),0,lstToUpdate[x].p->GetPowerType(),lstToUpdate[x].p->GetPowerSubType());

          flLane += CYCLIST_WIDTH;
          if(flLane >= flMaxLane)
          {
            flLane = flMinLane;
            flDistance.AddMeters(CYCLIST_LENGTH);
          }
        }
      }
      else if(m_eRaceMode == RACEMODE_TIMETRIAL)
      {
        // everyone starts at GetStartDistance() for a timetrial
        for(unsigned int x = 0;x < lstToUpdate.size();x++)
        {
          ((PlayerBase*)lstToUpdate[x].p.get())->SetFinishTime(-1);
          ((PlayerBase*)lstToUpdate[x].p.get())->InitPos(m_tmNow, m_spMapConst->GetStartDistance(),m_spMapConst->GetStartDistance(),0,10,lstToUpdate[x].p->GetMassKg(),m_pStatStore.get(),0, lstToUpdate[x].p->GetPowerType(),lstToUpdate[x].p->GetPowerSubType());
        }
      }
          
      stringstream ss;
      ss<<"Starting race on "<<m_spMapConst->GetMapName()<<". "<<cReadyHumanPlayers<<" humans, "<<lstToUpdate.size()<<" riders total. physics: "<<GetPhysicsModeString(m_ePhysicsMode)<<endl;
      m_pStatStore->AddAction(ss.str(),GetServerLogName(iServerId));

      *ptmLastUpdate = m_tmNow;
      m_eGameState = RACING;
      m_fSomeFinished = false; // surely nobody has finished yet...

    }
    break;
  }
  case RACING:
    {
      // go through all the players, apply physics.
      // the server threads will grab the data via SimpleServer_BuildGameState and send it to the clients
      vector<IPlayerPtr> lstToUpdate;
          
      bool fAnyActiveHumans = false;
      const bool fAnyConnectedHumans = m_mapNextPlayerz.size() > 0;
      bool fAllHumansFinished = true;
      lstToUpdate.reserve(m_mapNextPlayerz.size() + m_mapLocalsNext.size());
      for(ServerPlayerMap::iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++) 
      {
        lstToUpdate.push_back(i->second.p);

        DoIdlenessCheck(i->second, m_tmNow);
            

        if(i->second.p->GetFinishTime() <= 0 && // if this guy isn't finished...
            !(i->second.p->GetActionFlags() & (ACTION_FLAG_DEAD | ACTION_FLAG_SPECTATOR)) // and he's not dead or a spectator (if a player is disconnected, we don't want to wait for them... sorry buddy
            )
        {
          fAllHumansFinished = false;
        }
        if(!(i->second.p->GetActionFlags() & (ACTION_FLAG_DEAD | ACTION_FLAG_SPECTATOR)))
        {
          fAnyActiveHumans = true;
        }
      }
      fAllHumansFinished &= fAnyActiveHumans;

      if(fAllHumansFinished && m_tmLastHumanFinisher == 0)
      {
        m_tmLastHumanFinisher = m_tmNow;
      }
          
      // first, let's do a little trim of the AI list in case there are ghosts in here
      for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++)
      {
        if(i->second.p->GetId() & GHOST_BIT)
        {
          // this guy is a ghost.  Let's make sure that his main player is still around...
          const int idMain = i->second.p->GetId() & (~GHOST_BIT);
          ServerPlayerMap::const_iterator i = m_mapNextPlayerz.find(idMain);
          if(i == m_mapNextPlayerz.end())
          {
            // this is a ghost whose main player is gone.  boohoo. kill the ghost (yay smart pointers!)
            m_mapLocalsNext.erase(i->first);
            i = m_mapLocalsNext.begin(); // start over
          }
        }
      }
      
      for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++)
      {
        lstToUpdate.push_back(i->second.p);
      }

      if(IsDebuggerPresent())
      {
        // debug-only: let's make sure we don't have two copies of anyone in lstToUpdate
        unordered_set<int> setIdsPresent;
        for(unsigned int x = 0;x < lstToUpdate.size(); x++)
        {
          DASSERT(setIdsPresent.find(lstToUpdate[x]->GetId()) == setIdsPresent.end());
          setIdsPresent.insert(lstToUpdate[x]->GetId());
        }
      }
          
      PlayerDraftCompare<StateData> pdc(this);
      sort(lstToUpdate.begin(),lstToUpdate.end(), pdc);
      vector<IConstPlayerPtrConst> lstToUpdateDraftConst;
      vector<int> lstToUpdateIxMe; // the indices into lstToUpdateDraftConst for each item in lstToUpdate
      lstToUpdateIxMe.reserve(lstToUpdate.size());
      lstToUpdateDraftConst.reserve(lstToUpdate.size());
      for(unsigned int x = 0;x < lstToUpdate.size(); x++)
      {
        const IConstPlayer* pPlayer = lstToUpdate[x].get();
        int ixMe = -1;
        if(pPlayer->GetFinishTime() >= 0) 
        {
          // finished players don't factor into drafting calculations
        }
        else if(IS_FLAG_SET(pPlayer->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS)) 
        {
          // spectators/dead/idle players don't factor into drafting calculations
        }
        else
        {
          ixMe = lstToUpdateDraftConst.size();
          lstToUpdateDraftConst.push_back(lstToUpdate[x]->GetConstCopy(m_tmNow));
        }
        lstToUpdateIxMe.push_back(ixMe);
      }
      DASSERT(lstToUpdateIxMe.size() == lstToUpdate.size()); // since this is a mapping from lstToUpdate indices to lstToUpdateDraftConst indices, it must have an entry for every thing in lstToUpdate

      DWORD msDiff = m_tmNow - (*ptmLastUpdate);
      const float dt = m_fGamePaused ? ((float)(msDiff))/1000000.0f : ((float)(msDiff))/1000.0f;
          
      DraftMap mapDrafting;
      switch(m_eRaceMode)
      {
      case RACEMODE_ROADRACE:
        BuildDraftingMap(lstToUpdateDraftConst, mapDrafting);
        break;
      case RACEMODE_WORKOUT:
      case RACEMODE_TIMETRIAL:
        // don't need the drafting map in timetrial mode
        break;
      }
          
      const DWORD tmFinishTime = m_tmNow - m_tmStart;
      const float flElapsedTime = (float)tmFinishTime/1000.0f;

      PLAYERDIST flFurthest = m_spMapConst->GetStartDistance();
      PLAYERDIST flLeastFar = m_spMapConst->GetEndDistance();
      for(unsigned int x = 0;x < lstToUpdate.size();x++)
      {
        if(IS_FLAG_SET(lstToUpdate[x]->GetActionFlags(),ACTION_FLAG_SPECTATOR))
        {
          continue; // spectators don't need physics updates or checks for race finishes
        }

        flFurthest = max(flFurthest,lstToUpdate[x]->GetDistance());
        flLeastFar = min(flLeastFar,lstToUpdate[x]->GetDistance());

        const float flTTTime = (((float)m_tmNow)/1000.0f) - lstToUpdate[x]->GetStartTime();
        if(lstToUpdate[x]->GetFinishTime() <= 0)
        {
          const int iOldSegment = (int)(lstToUpdate[x]->GetDistance().ToMeters() / TimeTrialState<float>::s_minInterval);
          if(lstToUpdate[x]->GetStartTime()*1000.0f > m_tmNow)
          {
            // this guy hasn't started yet...
          }
          else if(lstToUpdate[x]->GetFinishTime() >= 0)
          {
            // this guy is already done, and so doesn't need anything done
          }
          else if(IS_FLAG_SET(lstToUpdate[x]->GetActionFlags(),ACTION_FLAG_IGNOREFORPHYSICS))
          {
            // this guy is a spectator or something, and so doesn't need anything done
          }
          else
          {
            SPRINTCLIMBDATA scoredPoints;
            std::string strScoredDesc;
            lstToUpdate[x]->Tick(flElapsedTime,lstToUpdateIxMe[x],m_ePhysicsMode, m_eRaceMode, lstToUpdateDraftConst, mapDrafting, dt,&scoredPoints,&strScoredDesc, m_tmNow);
            if(!lstToUpdate[x]->Player_IsAI() && scoredPoints.flPoints > 0)
            {
              char szChat[300];
              snprintf(szChat,sizeof(szChat),"%s scored %3.1f points for %s",lstToUpdate[x]->GetName().c_str(),scoredPoints.flPoints,strScoredDesc.c_str());
              SendServerChat(szChat,SendToPlayer(lstToUpdate[x]->GetId()));
            }
            CheckWattsPBs(lstToUpdate[x]);
          }
          const int iNewSegment = (int)(lstToUpdate[x]->GetDistance().ToMeters() / TimeTrialState<float>::s_minInterval);
          if(iNewSegment != iOldSegment && m_eRaceMode == RACEMODE_TIMETRIAL)
          {
            m_ttState.AddTime(iNewSegment*TimeTrialState<float>::s_minInterval,lstToUpdate[x]->GetId(),SERVERTT(flTTTime));
          }
        }
        else
        {
          // this guy has a > 0 finish time, so his distance had better be beyond the map's end distance...
          DASSERT(lstToUpdate[x]->GetDistance() >= m_spMapConst->GetEndDistance() || lstToUpdate[x]->GetActionFlags() & ACTION_FLAG_GHOST);
        }
        if(lstToUpdate[x]->GetDistance() >= m_spMapConst->GetEndDistance() && lstToUpdate[x]->GetFinishTime() <= 0)
        {
          if(m_eRaceMode == RACEMODE_TIMETRIAL)
          {
            if((lstToUpdate[x]->GetActionFlags() & ACTION_FLAG_GHOST) == 0)
            {
              m_ttState.AddTime(0,lstToUpdate[x]->GetId(),flTTTime);
              // non-ghost TIME-TRIAL FINISHER
              char szChat[200];
              char szTime[200];
              ::FormatTimeMinutesSecondsMs(flTTTime,szTime,sizeof(szTime));
              _snprintf(szChat,sizeof(szChat),"%s has laid down a time of %s",lstToUpdate[x]->GetName().c_str(), szTime);
              SendServerChat(szChat,SendToEveryone());

              // now we have to send this guy's finish time off to the DB
                
              IPlayerPtrConst pPlayer = lstToUpdate[x];

              DASSERT(pPlayer->GetStartDistance() <= m_spMapConst->GetStartDistance()); // since this is a TT, this guy had better have finished the damn race!
              DASSERT(m_iRaceId > 0);
              vector<LAPDATA> lstLaps;
              pPlayer->GetLapTimes(lstLaps);
              vector<SPRINTCLIMBDATA> lstScores;
              pPlayer->GetSprintClimbPoints(lstScores);
              PLAYERRESULTPtr result(new PLAYERRESULT(lstLaps,
                                  pPlayer->GetId(), 
                                    -1,
                                    flTTTime,m_spMapConst->GetEndDistance().Minus(pPlayer->GetStartDistance()),
                                    pPlayer->GetAveragePower(),
                                    pPlayer->GetName(), 
                                    m_spMapConst->GetEndDistance(),
                                    pPlayer->GetMassKg(),
                                    pPlayer->GetIP(),
                                    RACEEND_FINISH,
                                    pPlayer->GetReplayOffset(),
                                    pPlayer->GetPowerHistory(),
                                    pPlayer->Player_IsAI(),
                                    pPlayer->GetPowerType(),
                                    pPlayer->GetPowerSubType(),
                                    lstScores,
                                    pPlayer->GetTeamNumber()));
              if(result->flDistanceRidden > PLAYERDIST(0,0,result->flDistanceRidden.flDistPerLap))
              {
                ThreadedResultsSaver* pSaver = new ThreadedResultsSaver(m_iMasterId,m_pStatStore, m_iMapId, m_iRaceId, result);
                ((PlayerBase*)lstToUpdate[x].get())->SetFinishTime(flTTTime);
              }
              else
              {
                cout<<"didn't save results for "<<pPlayer->GetId()<<" because distance ridden was "<<result->flDistanceRidden<<endl;
              }
            }
          }
          else
          {
            DASSERT(m_eRaceMode == RACEMODE_ROADRACE);
            if(m_sfSchedRace.IsValid() && IsTimedMode(m_sfSchedRace.laps,m_sfSchedRace.timedLengthSeconds))
            {
              // someone went past the "finish line" for a timed race.  extend the finish line!
              cout<<"Someone just finished the "<<m_spMapConst->GetEndDistance().iCurrentLap<<"th lap.  Let's extend this timed race"<<endl;
              ((Map*)m_spMapConst.get())->OneMoreLap();
            }
            else
            {
              // ROAD RACE FINISHER
              if(m_tmFirstFinisher == 0)
              {
                // first finisher!
                m_tmFirstFinisher = m_tmNow;
                
                char szChat[200];
                _snprintf(szChat,sizeof(szChat),"%s has won the race.", lstToUpdate[x]->GetName().c_str());
                SendServerChat(szChat,SendToEveryone());
              }
              if(m_tmFirstHumanFinisher == 0 && !lstToUpdate[x]->Player_IsAI())
              {
                m_tmFirstHumanFinisher = m_tmNow;

                char szChat[200];
                _snprintf(szChat,sizeof(szChat),"%s is the first human finisher.", lstToUpdate[x]->GetName().c_str());
                SendServerChat(szChat,SendToEveryone());
              }
              // this guy finished! (and hasn't already finished)
              ((PlayerBase*)lstToUpdate[x].get())->SetFinishTime(flElapsedTime);

              {
                IPlayerPtrConst pPlayer = lstToUpdate[x];
                vector<LAPDATA> lstLaps;
                pPlayer->GetLapTimes(lstLaps);
                vector<SPRINTCLIMBDATA> lstScores;
                pPlayer->GetSprintClimbPoints(lstScores);
                PLAYERRESULT result(lstLaps,
                                    pPlayer->GetId(), 
                                    m_raceResults.GetFinisherCount() + 1,
                                    flElapsedTime,pPlayer->GetDistance().Minus(pPlayer->GetStartDistance()),
                                    pPlayer->GetAveragePower(),
                                    pPlayer->GetName(), 
                                    pPlayer->GetDistance(),
                                    pPlayer->GetMassKg(),
                                    pPlayer->GetIP(),
                                    RACEEND_FINISH,
                                    pPlayer->GetReplayOffset(),
                                    pPlayer->GetPowerHistory(),
                                    pPlayer->Player_IsAI(),
                                    pPlayer->GetPowerType(),
                                    pPlayer->GetPowerSubType(),
                                    lstScores,
                                    pPlayer->GetTeamNumber());
                if(result.flDistanceRidden > PLAYERDIST(0,0,result.flDistanceRidden.flDistPerLap))
                {
                  unordered_map<int,IPlayerPtrConst> mapPlayers;
                  for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++) { mapPlayers[i->first] = i->second.p; }
                  for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) { mapPlayers[i->first] = i->second.p; }

                  m_raceResults.AddFinisher(result, mapPlayers, m_spMapConst->GetEndDistance().Minus(m_spMapConst->GetStartDistance()));
                }
                else
                {
                  cout<<"Didn't add result for player "<<pPlayer->GetId()<<" because distanceridden was negative"<<endl;
                }
              }
              m_fSomeFinished = true; // put a flag in so that we start sending finish times
            } // end of road-race for distance
          }
        }
      }
          
      static unsigned int lastUpdate = ArtGetTime();
      if(m_eRaceMode == RACEMODE_ROADRACE && m_tmNow > lastUpdate + 150000)
      {
        // it has been 5 minutes since our last status update
        stringstream ss;
        ss<<"Race update: slowest: "<<flLeastFar<<"m, fastest: "<<flFurthest<<"m humans connected: "<<m_mapNextPlayerz.size();
        m_pStatStore->AddAction(ss.str(),GetServerLogName(iServerId));
        lastUpdate = m_tmNow; 
      }

      SCHEDULEDRACE sfSchedRace;
      if(fAnyActiveHumans && fAllHumansFinished && m_tmNow - m_tmLastHumanFinisher > 60000 && m_eRaceMode != RACEMODE_TIMETRIAL)
      {
        m_pStatStore->AddAction("Restarting: All active humans have been done for 60s",GetServerLogName(iServerId));
        Restart(RACEEND_FINISH,lstMaps,iServerId);
      }
      else if(m_sfSchedRace.IsValid() && IsTimedMode(m_sfSchedRace.laps, m_sfSchedRace.timedLengthSeconds) && flElapsedTime > m_sfSchedRace.timedLengthSeconds)
      {
        // race is over!
        char szText[200];
        snprintf(szText,sizeof(szText),"Timed-length race (%ds) is over",m_sfSchedRace.timedLengthSeconds);
        m_pStatStore->AddAction(szText,GetServerLogName(iServerId));
        Restart(RACEEND_FINISH,lstMaps,iServerId);
      }
      else if(!fAnyConnectedHumans &&  // if there isn't anyone around...
              m_eRaceMode != RACEMODE_TIMETRIAL &&  // and we're not a TT server (TT server should never reset)
              !m_fSinglePlayer) // and we're not in singleplayer (no point resetting in single player)
      {
        m_pStatStore->AddAction("Restarting: Non-TT mode and all humans done",GetServerLogName(iServerId));
        Restart(RACEEND_NOHUMANS,lstMaps,iServerId);
      }
      else if(pSchedCheck->GetNextScheduledRace(&sfSchedRace) && // there is a scheduled race coming up
              sfSchedRace.IsValid() &&                           // and it's real
              (sfSchedRace.tmStart - GetSecondsSince1970GMT())<600 && // and it starts in less than 10 minutes
              sfSchedRace.tmStart > ::GetSecondsSince1970GMT() &&  // and it starts in the future
              !m_fSinglePlayer) // and we're not in single-player mode (no point ever resetting in single-player)
      {
        // ok, there's a scheduled race request.  Let's make sure we're not currently RUNNING this particular scheduled race, then restart
        if(!m_sfSchedRace.IsSame(sfSchedRace))
        {
          char szBlah[300];
          snprintf(szBlah,sizeof(szBlah),"Restarting: Scheduled race (mapid %d for %dkm) within 10 minutes",sfSchedRace.iMapId,sfSchedRace.iKm);
          m_pStatStore->AddAction(szBlah,GetServerLogName(iServerId));
          cout<<"There's a scheduled race coming up within 10 minutes, and the only people on the server are the people who scheduled it.  Restarting."<<endl;
          Restart(::RACEEND_SCHEDULE,lstMaps,iServerId,&sfSchedRace);
        }
      }
      (*ptmLastUpdate) = m_tmNow;

    }
    break;
  case RESULTS:
    break;
  }

  {
    DWORD tm = m_tmNow - m_tmStart;
    // let's go through the player list and prune any dead guys who haven't come back after 5 minutes
    vector<int> lstToKill;
    for(ServerPlayerMap::iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++)
    {
      if(m_tmNow - i->second.GetLastDataTime() > 5*60*1000 &&
        ((i->second.p->GetActionFlags() & ACTION_FLAG_DEAD) || (i->second.p->GetActionFlags() & ACTION_FLAG_IDLE)) )
      {
        stringstream ss;
        ss<<"Been 5 minutes since "<<i->second.p->GetName()<<" went missing. Disconnecting.";
        m_pStatStore->AddAction(ss.str(),GetServerLogName(iServerId));

        cout<<"Server totally killing player "<<i->second.p->GetName()<<endl;
        lstToKill.push_back(i->first);
      }
    }
    for(unsigned int x = 0; x < lstToKill.size(); x++)
    {
      ServerPlayer pPlayer = m_mapNextPlayerz[lstToKill[x]];

      if(m_tmNow < m_tmStart)
      {
        // haven't started the race yet, so this guy doesn't have a time...
        // but we still want to save his result, since he might have replay data.
        tm = 0;
      }
      // if she/he hasn't been recorded as a finisher already...
      if(!m_raceResults.IsFinisher(pPlayer.p->GetId()))
      {
        vector<LAPDATA> lstLaps;
        pPlayer.p->GetLapTimes(lstLaps);
        vector<SPRINTCLIMBDATA> lstScores;
        pPlayer.p->GetSprintClimbPoints(lstScores);
        PLAYERRESULT result(lstLaps,
                            pPlayer.p->GetId(), 
                            -1,
                            (float)tm/1000.0f,
                            pPlayer.p->GetDistance().Minus(pPlayer.p->GetStartDistance()),
                            pPlayer.p->GetAveragePower(),
                            pPlayer.p->GetName(), 
                            pPlayer.p->GetDistance(),
                            pPlayer.p->GetMassKg(),
                            pPlayer.p->GetIP(),
                            RACEEND_DISCONNECT,
                            pPlayer.p->GetReplayOffset(),
                            pPlayer.p->GetPowerHistory(),
                            pPlayer.p->Player_IsAI(),
                            pPlayer.p->GetPowerType(),
                            pPlayer.p->GetPowerSubType(),
                            lstScores,
                            pPlayer.p->GetTeamNumber());

        if(result.flDistanceRidden > PLAYERDIST(0,0,result.flDistanceRidden.flDistPerLap))
        {
          unordered_map<int,IPlayerPtrConst> mapPlayers;
          for(ServerPlayerMap::const_iterator i = m_mapNextPlayerz.begin(); i != m_mapNextPlayerz.end(); i++) { mapPlayers[i->first] = i->second.p; }
          for(ServerPlayerMap::const_iterator i = m_mapLocalsNext.begin(); i != m_mapLocalsNext.end(); i++) { mapPlayers[i->first] = i->second.p; }

          m_raceResults.AddFinisher(result, mapPlayers, m_spMapConst->GetEndDistance().Minus(m_spMapConst->GetStartDistance()));
        }
        else
        {
          cout<<"Didn't add result for player "<<pPlayer.p->GetId()<<" because distance ridden was "<<result.flDistanceRidden<<endl;
        }
      }
      m_mapNextPlayerz.erase(lstToKill[x]);
      m_mapNextPendingChats.erase(lstToKill[x]);
      m_mapPlayerRestartRequests.erase(lstToKill[x]);
      pPlayer.p.reset();
    }
  }
}

void EXPECT_T(bool f);
void DoServerTests()
{
  boost::shared_ptr<const IMap> spMap(new Map());
  Map* pMap = (Map*)spMap.get();
  pMap->LoadFromSine(10, 2, -1);

  ManagedCS m_cs;
  StatsStorePtr pNull(new NullStatsStore());
  vector<MAPDATA> lstMaps;
  lstMaps.push_back(MAPDATA(1,16,1,-1,0,100));

  IPlayerPtr spPlayer(new Player(ArtGetTime(),0,-1,"Player",spMap,0,AISELECTION()));
  const int iPlayerId = spPlayer->GetId();

  // test 1: Build a StateData, de-queue a chat from it, then make sure a StateData constructed from it shows that chat as gone
  StateData sd1(RACEMODE_ROADRACE,0,100,400,12,0,NULL,&m_cs,false,pNull,NULL,lstMaps,-1);
  sd1.AddPlayer(ServerPlayer(spPlayer),true);
  EXPECT_T(sd1.m_mapNextPlayerz.size() == 1);
  EXPECT_T(sd1.m_mapPlayerz.size() == 0); // since we haven't had a cycle for the new guy to filter into the "current" player list, there shouldn't be anything yet

  sd1.SendServerChat("blah",SendToEveryone());
  EXPECT_T(sd1.m_mapNextPendingChats.size() == 1);
  EXPECT_T(sd1.m_mapNextPendingChats[iPlayerId].size() == 1);
  EXPECT_T(sd1.m_mapPendingChats.size() == 0); // haven't had a cycle yet
  EXPECT_T(sd1.m_mapNextPlayerz.size() == 1); // the player should also be in the next list

  StateData sd2(sd1); // create a 2nd StateData from the first
  EXPECT_T(sd2.m_mapPendingChats.size() == 1); // since we queued it previous cycle with the SendServerChat call
  EXPECT_T(sd2.m_mapNextPendingChats.size() == 1); // we haven't dequeued yet
  EXPECT_T(sd2.m_mapNextPendingChats[iPlayerId].size() == 1); // we haven't dequeued yet
  sd2.m_mapNextPendingChats[iPlayerId].pop(); // pops the chat off the queue, to simulate us sending it
  EXPECT_T(sd2.m_mapNextPendingChats[iPlayerId].size() == 0); // we just popped it!
  EXPECT_T(sd2.m_mapPlayerz.size() == 1); // the player should have filtered into the active player list by now

  StateData sd3(sd2);
  EXPECT_T(sd3.m_mapPendingChats[iPlayerId].size() == 0); // the "current" chat list should reflect the "next" chat list now
  EXPECT_T(sd3.m_mapNextPendingChats[iPlayerId].size() == 0); // nothing should be here anymore, since we popped it last time
}