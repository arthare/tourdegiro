#pragma once

#include "SimpleServer.h"

using namespace std;

struct MAPDATA
{
  MAPDATA(const string& strFile, int iKm, int laps) : strFile(strFile),id(-1),iPctStart(0),iPctEnd(100),iDesiredKm(iKm),laps(laps),timedLengthSeconds(-1)
  {
  }
  MAPDATA(int id, int iKm, int laps, int timedLengthSeconds, int iPctStart, int iPctEnd) : strFile(""),id(id),iPctStart(iPctStart),iPctEnd(iPctEnd),iDesiredKm(iKm),laps(laps),timedLengthSeconds(timedLengthSeconds)
  {
    DASSERT(IsLappingMode(laps,timedLengthSeconds) ^ IsTimedMode(laps,timedLengthSeconds)); // either we're doing a timed race or we're doing a finite-lap race.  Can't do both
  }
  string strFile;
  int id; // either this or strFile will be present
  
  int iPctStart;
  int iPctEnd;

  int iDesiredKm;
  int laps;
  int timedLengthSeconds;
};

#define TIMETRIAL_STARTTIME_OFFSET 90000 // so when a guy joins, he'll always get 10 seconds to hop back on his bike

DWORD WINAPI _GameThdProc(LPVOID pv);

// keeps track of when this guy last sent data to/from other players
class ServerPlayerData
{
private:
  ServerPlayerData(const ServerPlayerData& other); // NO COPYING
  ServerPlayerData& operator == (const ServerPlayerData& other); // NO COPYING
public:
  ServerPlayerData() : m_tmLastData(ArtGetTime()) {}
  virtual ~ServerPlayerData() {};
  
  int GetTimeSinceLastSend(S2C_UPDATETYPE eType, int idTo, DWORD tmNow) const
  {
    AutoLeaveCS _cs(m_cs);
    const std::unordered_map<int,DWORD>& map = m_mapDataTimes[eType];
    const std::unordered_map<int,DWORD>::const_iterator i = map.find(idTo);
    if(i != map.end())
    {
      return tmNow - i->second;
    }
    return 0x7fffffff;
  }
  void RecordLastSend(S2C_UPDATETYPE eType, int idTo, DWORD tmNow)
  {
    AutoLeaveCS _cs(m_cs);
    std::unordered_map<int,DWORD>& map = m_mapDataTimes[eType];
    map[idTo] = tmNow;
  }

  virtual DWORD GetLastDataTime() const {return m_tmLastData;}
  virtual void SetLastDataTime(DWORD tmLastData) {m_tmLastData = tmLastData;}

private:
  mutable ManagedCS m_cs;
  DWORD m_tmLastData;
  std::unordered_map<int,DWORD> m_mapDataTimes[UPDATETYPE_COUNT];
};

class ServerPlayer
{
public:
  ServerPlayer() 
  {
    spData = boost::shared_ptr<ServerPlayerData>(new ServerPlayerData());
  }
  ServerPlayer(IPlayerPtr pPlayer) : p(pPlayer)
  {
    spData = boost::shared_ptr<ServerPlayerData>(new ServerPlayerData());
  }
  int GetTimeSinceLastSend(S2C_UPDATETYPE eType, int idTo, DWORD tmNow) const {return spData->GetTimeSinceLastSend(eType,idTo,tmNow);}
  int GetCachedTime(S2C_UPDATETYPE eType, int idTo, DWORD tmNow) const
  {
    if(eType == m_eCachedType && m_idCached == idTo)
    {
      return m_tmCachedTimeSince;
    }
    return GetTimeSinceLastSend(eType,idTo,tmNow);
  }
  void RecordLastSend(S2C_UPDATETYPE eType, int idTo, DWORD tmNow)  { spData->RecordLastSend(eType,idTo,tmNow);}
  DWORD GetLastDataTime() const {return spData->GetLastDataTime();}
  void SetLastDataTime(DWORD tmLastData) const {spData->SetLastDataTime(tmLastData);}
  void CacheSendTimeTo(S2C_UPDATETYPE eType, int idTo, DWORD tmNow)
  {
    m_eCachedType = eType;
    m_idCached = idTo;
    m_tmCachedTimeSince = GetTimeSinceLastSend(eType,idTo,tmNow);
  }
  IPlayerPtr p;
private:
  mutable boost::shared_ptr<ServerPlayerData> spData; // the actual when-sent data recordings

  S2C_UPDATETYPE m_eCachedType;
  int m_idCached;
  int m_tmCachedTimeSince;
};

// this will spawn a "checker thread" that will keep an eye on when this server may need to switch to a different map, race distance, and settings
class ScheduleChecker
{
public:
  ScheduleChecker(const int iServerId, boost::shared_ptr<StatsStore> pStats);
  ~ScheduleChecker();

  void ThreadProc();
  void Shutdown();

  bool AddScheduledRace(const SCHEDULEDRACE& sr); // returns true if the race was successfully added
  bool GetNextScheduledRace(SCHEDULEDRACE* pOut);
private:
private:
  ManagedCS m_cs;
  bool m_fShutdown;
  const int m_iServerId;
  StatsStorePtr m_pStats;
  boost::thread m_hThread;
  bool m_fChangedSinceLastCheck;

  // the upcoming race that we've found.  must be accessed within critical section
  SCHEDULEDRACE m_upcoming;
  
};


// stores a client update.  Used to store some other metadata too, but I realized I didn't need it
struct QUEUEDUPDATE
{
  QUEUEDUPDATE(const TDGClientState& state) : fIsState(true), m_state(state) {};
  QUEUEDUPDATE(const C2S_HRMCADUPDATE_DATA& state) : fIsState(false), m_hrcad(state) {};

  bool fIsState;
  TDGClientState m_state;
  C2S_HRMCADUPDATE_DATA m_hrcad;
};

struct SERVERTT
{
  SERVERTT(float flTime) : flTime(flTime) {};

  float flTime;
  unordered_set<int> setSentTo;
};
class ServerTimeTrialState : public TimeTrialState<SERVERTT>
{
public:
  ServerTimeTrialState(ManagedCS* pCS) : TimeTrialState<SERVERTT>(pCS) {};
  virtual ~ServerTimeTrialState() {};

  void AddName(int id, const std::string& strName)
  {
    AutoLeaveCS _cs(*m_pCS);
    SENTNAME name;
    name.name = strName;
    mapNames.insert(std::pair<int,SENTNAME>(id,name));
  }
  int BuildTTUpdate(const vector<IConstPlayerPtrConst>& lstTargets, TIMETRIALUPDATE& ttUpdate); // figure out what we should put in the timetrial update.  returns how many slots it used.  if it used zero slots, we'll switch to doing name updates for obscure players
  void BuildObscureNameUpdate(const vector<IConstPlayerPtrConst>& lstTargets, PLAYERNAMEUPDATE& nameUpdate);
  void ClearPlayer(int id); // removes a player from the "sent lists".  Note: DOES NOT remove the player from the result lists
private:
  struct SENTNAME
  {
    string name;
    unordered_set<int> setSentTo;
  };
  unordered_map<int,SENTNAME> mapNames; // mapping from player IDs to their name.  The name struct keeps track of what Ids have been told about a given name
};

class SendToEveryone
{
public:
  bool ShouldSendTo(const IPlayer* p) const
  {
    return !IS_FLAG_SET(p->GetActionFlags(), ::ACTION_FLAG_DEAD | ACTION_FLAG_GHOST) && IS_FLAG_SET(p->GetActionFlags(),ACTION_FLAG_LOCATION_MAIN_PLAYER);
  }
};

// the serverplayers lets us cache a bit of data about a given player along with their pointer
// the ConstServerPlayer holds a deep-copied player pointer so that the networking threads can read from it and not worry about the game thread changing the player as they're working with it
class ConstServerPlayer
{
public:
  ConstServerPlayer() 
  {
    spData = boost::shared_ptr<ServerPlayerData>(new ServerPlayerData());
  }

  // building a fresh ConstServerPlayer (aka a new guy just joined, we don't have to merge a changed ServerPlayer with an old ConstServerPlayer)
  ConstServerPlayer(IConstPlayerPtrConst pSrc, unsigned int tmNow) : p(pSrc->GetConstCopy(tmNow))
  {
    spData = boost::shared_ptr<ServerPlayerData>(new ServerPlayerData());
  }
  // after making a CurrentStateData current, (by calling TourServer::SetData), the new player positions (from the writeable player positions) will be combined with the old ConstServerPlayer data to produce the new pile of ConstServerPlayer
  ConstServerPlayer(const ServerPlayer& lastGuy, const ConstServerPlayer& lastConstGuy, unsigned int tmNow) : p(lastGuy.p->GetConstCopy(tmNow))
  {
    DASSERT(lastGuy.p->GetId() == lastConstGuy.p->GetId());
    this->spData = lastConstGuy.spData;
    this->m_tmLastData = lastGuy.GetLastDataTime();
  }
  int GetTimeSinceLastSend(S2C_UPDATETYPE eType, int idTo, DWORD tmNow) const {return spData->GetTimeSinceLastSend(eType,idTo,tmNow);}
  void RecordLastSend(S2C_UPDATETYPE eType, int idTo, DWORD tmNow)  { spData->RecordLastSend(eType,idTo,tmNow);}
  void CacheSendTimeTo(S2C_UPDATETYPE eType, int idTo, DWORD tmNow)
  {
    m_eCachedType = eType;
    m_idCached = idTo;
    m_tmCachedTimeSince = GetTimeSinceLastSend(eType,idTo,tmNow);
  }
  int GetCachedTime(S2C_UPDATETYPE eType, int idTo, DWORD tmNow) const
  {
    if(eType == m_eCachedType && m_idCached == idTo)
    {
      return m_tmCachedTimeSince;
    }
    return GetTimeSinceLastSend(eType,idTo,tmNow);
  }
  IConstPlayerPtrConst p;
private:
  mutable boost::shared_ptr<ServerPlayerData> spData; // the actual when-sent data recordings
   
  S2C_UPDATETYPE m_eCachedType;
  int m_idCached;
  int m_tmCachedTimeSince;
  
  DWORD m_tmLastData;
};
typedef unordered_map<int,ConstServerPlayer> ConstServerPlayerMap;
typedef unordered_map<int,ServerPlayer> ServerPlayerMap;

struct TourServerSendHandle;

// so here's the idea: the entire server state will be stored in a StateDataConst.
// the game thread will grab the current StateDataConst, do a tick on it, and then replace the StateDataConst.
// this means that the global server-lock critical section will be held for only microseconds, rather than for the entire "readstate->calculate->writestate" process as it was before
typedef SimpleServer<TDGGameState,TDGClientState,TDGClientDesc, TDGInitialState, TDGClientToServerSpecial,TDGServerStatus, TourServerSendHandle> inheritedServer;
class StateData;

class StateDataConst
{
  friend void DoServerTests();
public:
  StateDataConst(RACEMODE eRaceMode, int cAIs, int iMinAIPower, int iMaxAIPower, int iServerId, int iDelayMinutes,inheritedServer* pServer, ManagedCS* pCS, bool fSinglePlayer, StatsStorePtr pStatsStore, SRTMSource* pSRTM, int iMasterId);
  StateDataConst(const StateData& constSrc);
  virtual ~StateDataConst() {};
public:
  virtual bool Init() = 0; // not implemented, just to make sure that this class is abstract.  This will always be a full CurrentStateData, but we won't always pass it around like that

  void BuildServerStatus(TDGServerStatus* pSS) const;

  // accessors:
  void BuildStartupInfo(STARTUPINFOZ<TDGInitialState>& sfStartup) const;
  bool ValidateLogin(TDGClientDesc& cd, TDGConnectionResult* pConnResult) const;
  int BuildGameState(TourServerSendHandle& handle, const inheritedServer::ClientData& cdTarget, TDGGameState* pTGS, bool* pfPlayerDead) const;
  int GetTimeUntilStart(DWORD tmNow) const;
  void BuildAllPlayerListConst(vector<ConstServerPlayer>& lstPlayers) const;
  void BuildCentralPlayerListConst(const vector<int>& lstIds, const vector<ConstServerPlayer>& lstPlayers, vector<ConstServerPlayer>& lstOutput);
  // decides for a given update, what players will be included in it
  // returns 0 if we should send an update for these players immediately
  // returns >0 if we should wait <return> milliseconds before sending an update
  int PickPlayers(vector<ConstServerPlayer>& lstPlayers, vector<unsigned int>& lstPlayerOrder, DWORD tmNow, S2C_UPDATETYPE eType, const vector<ConstServerPlayer>& lstCentral) const;

  RACEMODE GetRaceMode() const {return m_eRaceMode;};
  
  boost::shared_ptr<Map> GetMap() const {return m_spMapConst;}
  bool IsDelayed() const;
  bool GetShutdown() const {return m_fShutdown;}
  int GetPlayerCount() const {return m_mapPlayerz.size();}
  DWORD GetTmNow() const {return m_tmNow;}
  TDG_GAMESTATE GetGameState() const {return m_eGameState;}
protected:
  int GetActiveHumanCount(int iExcludeMasterId) const;
  bool CheckForRestart(int* piVotes) const;
  PLAYERDIST GetPlacementPosition(const IConstPlayer* pPlayer, stringstream& ssLogData) const;
  int GetTimeUntilStart();

  // merges the previous-state "real" players (mapPrevReal) with the previous-state "const" players (mapPrevConst) into the next-state "const" players
  static void MergePastMap(const DWORD tmNow, const ServerPlayerMap& mapPrevReal, const ConstServerPlayerMap& mapPrevConst, ConstServerPlayerMap& mapNextConst);
private:
  // these are private so that the basic StateData functions can't directly access them
  ConstServerPlayerMap m_mapPlayerz; // map from player IDs to players
protected:
  unordered_map<int,DWORD> m_mapPlayerRestartRequests; // when was the last restart request from a player?
  // race result data - cleared on each race start, populated as people cross the line
  RACERESULTS m_raceResults;
    
  DWORD m_tmFirstFinisher;
  DWORD m_tmFirstHumanFinisher;
  DWORD m_tmLastHumanFinisher;
  
  DWORD m_tmStart; // the time (in ArtGetTime units) until we're going to start the race
  SCHEDULEDRACE m_sfSchedRace; // metadata about the race that we're currently running
  TDG_GAMESTATE m_eGameState;
  RACEMODE      m_eRaceMode; // when we are RACING, what kind of race are we racing?

  mutable ServerTimeTrialState m_ttState; // keeps track of all our TT results, along with who we've told about them.  mutable because we need to mod it to keep track of what data we've sent to whom
  int m_iRaceId; // our race ID.  Used for TT modes
  
  bool m_fSomeFinished;
  DWORD m_tmCountdownLock; // if zero: no countdown lock.  If nonzero: lock the countdown at ArtGetTime() + m_tmCountdownLock

  // map from player IDs to chats that need to be sent to them.  When an incoming chat arrives, it gets stuck in every single thing here.
  // in case of multilocal play, the first player ID at that location is used as the index
  mutable unordered_map<int,queue<CHATUPDATE> > m_mapPendingChats; 
  
  // authorization data for single player race replay saving
  const int m_iMasterId;

  int m_iMapId;
  int m_cAIs;
  int m_iMaxAIPower;
  int m_iMinAIPower;
  bool m_fAIsAddedYet;
  MAPSTATS m_mapStats;
  bool m_fSinglePlayer;
  bool m_fGamePaused;
  int m_cSprintClimbPeopleCount; // what did we use for our sprint/climb people count this map?

  PHYSICSMODE m_ePhysicsMode;

  boost::shared_ptr<StatsStore> m_pStatStore;
  SRTMSource* m_pSRTMSource;
  boost::shared_ptr<Map> m_spMapConst;
  int m_ixMap;

  friend class StateData;

  const DWORD m_tmNow; // set upon construction
  bool m_fShutdown;
};
typedef boost::shared_ptr<StateDataConst> StateDataConstPtr;

class StateData : public StateDataConst
{
public:
  StateData(const StateDataConst& prevData); // copying from previous state, getting ready for more calculations in DoGameTick
  StateData(RACEMODE eRaceMode, int cAIs, int iMinAIPower, int iMaxAIPower, int iServerId, int iDelayMinutes,inheritedServer* pServer, ManagedCS* pCS, bool fSinglePlayer, StatsStorePtr pStatsStore, SRTMSource* pSRTMSource, const vector<MAPDATA>& lstMaps, int iMasterId);
  virtual ~StateData() 
  {
  };

  void DoGameTick(int* pmsWait, ScheduleChecker* pSchedCheck, const vector<AIDLL>& lstDLLs, DWORD* ptmLastUpdate, const vector<MAPDATA>& lstMaps, const int iServerId);
  void NextMap(SCHEDULEDRACE* pNextRace, const vector<MAPDATA>& lstMaps, const int iServerId, int iForceMinutes=-1);
  bool Init() {return true;};
  
  void AddDopers(int cAIs, int min, int max, const vector<AIDLL>& lstAIs);
  void AddNewPlayer(const inheritedServer::ClientData& sfData, ScheduleChecker* pSchedCheck, int iServerId, stringstream& ssLogData);

  // responses to network input
  bool NotifyReconnection(const inheritedServer::ClientData& data, stringstream& ssLogData);
  void NotifyDeadPlayer(const inheritedServer::ClientData& sfData, stringstream& ssLogData);
  void NotifyClientState(const QUEUEDUPDATE& sfData);
  void NotifyC2SData(const inheritedServer::ClientData& src, const TDGClientToServerSpecial& c2s, int iServerId, const vector<AIDLL>& lstAIs, const vector<MAPDATA>& lstMaps, stringstream& ssLogData);
  
  void ApplyClientStateQueue(queue<QUEUEDUPDATE>& qUpdates);

  const ServerPlayerMap& GetPlayerz() const {return m_mapNextPlayerz;};
  const ServerPlayerMap& GetAIPlayerz() const {return m_mapLocalsNext;};
  const unordered_map<int,queue<CHATUPDATE> >& GetNextChats() const {return m_mapNextPendingChats;}

  bool GetNextShutdown() const {return m_fNextShutdown;}

  void Cleanup(const vector<MAPDATA>& lstMaps, int iServerId); // called when we want to shut the server down and save data
  void SetShutdown()
  {
    m_fNextShutdown = true;
  }
private:
  friend void DoServerTests();

  bool AddDoper(const int iPower, const int massKg, const std::string& name, bool fIsFrenemy, const vector<AIDLL>& lstAIs);
  bool AddLocal(ServerPlayer sfPlayer);
  void DoIdlenessCheck(const ServerPlayer& p, DWORD tmNow);
  template<class TCrit>
  void SendServerChat(string str,const TCrit& crit);
  void CheckWattsPBs(IPlayerPtr pPlayer);
  void AddPlayer(ServerPlayer sfPlayer, bool fIsMainPlayer);
  void Restart(RACEENDTYPE eRestartType, const vector<MAPDATA>& lstMaps, int iServerId, SCHEDULEDRACE* pNextRace = NULL);
  
  void BuildAllPlayerList(vector<ServerPlayer>& lstPlayers);
protected:
  ServerPlayerMap m_mapLocalsNext; // AI players (next update)
  ServerPlayerMap m_mapNextPlayerz; // map from playerIDs to the next frame's player data
  
  unordered_map<int,queue<CHATUPDATE> > m_mapNextPendingChats;

  inheritedServer* m_pServer; // lets us force the server to disconnect

  bool m_fNextShutdown; // do we want to shut down?

};
typedef boost::shared_ptr<StateData> StateDataPtr;

struct TourServerSendHandle
{
public:
  TourServerSendHandle() : lIteration(-1),fDone(false) {};
  TourServerSendHandle(bool fShuffle, LONG lIteration, const vector<int>& lstTargetIds, StateDataConstPtr spState) : lIteration(lIteration),spState(spState),fDone(false)
  {
    spState->BuildAllPlayerListConst(m_lstAllPlayers);
    spState->BuildCentralPlayerListConst(lstTargetIds, m_lstAllPlayers, m_lstCentral);

    m_lstPlayerOrder.resize(m_lstAllPlayers.size());
    for(unsigned int x = 0; x < m_lstPlayerOrder.size(); x++)
    {
      m_lstPlayerOrder[x] = x;
    }
    if(fShuffle)
    {
      std::random_shuffle(m_lstPlayerOrder.begin(),m_lstPlayerOrder.end());
    }
    memset(rgixSendPos,0,sizeof(rgixSendPos));
  };
  virtual ~TourServerSendHandle()
  {
  }
  const StateDataConstPtr spState;
  const LONG lIteration; // what game-state iteration did this one come from?

  unsigned int rgixSendPos[UPDATETYPE_COUNT]; // what position in m_lstPlayerOrder is a given send-type up to?
  bool fDone;
  vector<ConstServerPlayer> m_lstAllPlayers;
  vector<ConstServerPlayer> m_lstCentral; // what players are targeted by this sendhandle?
  vector<unsigned int> m_lstPlayerOrder; // a pre-allocated list of indices to use for picking players from m_lstAllPlayers
};

typedef SimpleServer<TDGGameState,TDGClientState,TDGClientDesc, TDGInitialState, TDGClientToServerSpecial,TDGServerStatus,TourServerSendHandle> TourServer_inherited;
class TourServer : public TourServer_inherited, public boost::enable_shared_from_this<TourServer>
{
public:
  TourServer(ICommSocketFactoryPtr pSocketFactory, int iServerId, int iDelayMinutes, const vector<MAPDATA>& lstMaps, StatsStorePtr pStats, SRTMSource* pSRTMSource, int iUDPInPort, int iTCPConnectPort, int cAIs, int iMinAIPower, int iMaxAIPower, RACEMODE eRaceMode, bool fSinglePlayer, int iMasterId);
  virtual ~TourServer() ;
  boost::shared_ptr<TourServer> SharedPtr();

  int GetMasterId() const; // used for authorization of race replays

  void SetShutdown();
protected:
  virtual void SimpleServer_BuildStartupInfo(STARTUPINFOZ<TDGInitialState>& sfStartup) ARTOVERRIDE;
  virtual void SimpleServer_BuildServerStatus(TDGServerStatus* pSS) const ARTOVERRIDE;
  virtual bool SimpleServer_ValidateLogin(TDGClientDesc& cd, TDGConnectionResult* pConnResult) const ARTOVERRIDE;

  virtual bool SimpleServer_StartSendHandle(TourServerSendHandle* pHandle, const SimpleServer::ClientData& cdTarget) ARTOVERRIDE; // set up some data that will be used for calls to SimpleServer_BuildGameState until SimpleServer_IsHandleCurrent returns false
  virtual bool SimpleServer_IsHandleCurrent(const TourServerSendHandle& handle) const ARTOVERRIDE; // checks to see if the SendHandle is still up to date.  If your game state has moved on, then return false and you'll be able to generate a new one
  virtual int SimpleServer_BuildGameState(TourServerSendHandle& sendHandle, const SimpleServer::ClientData& cdTarget, TDGGameState* pTGS, bool* pfPlayerDead) const ARTOVERRIDE;

  virtual void SimpleServer_NotifyNewPlayer(const ClientData& sfData) ARTOVERRIDE;
  virtual void SimpleServer_NotifyReconnectedPlayer(const ClientData& sfData) ARTOVERRIDE;
  virtual void SimpleServer_NotifyDeadPlayer(const ClientData& sfData) ARTOVERRIDE;
  virtual void SimpleServer_NotifyClientState(const ClientData& sfClient, const TDGClientState& sfData) ARTOVERRIDE;
  virtual void SimpleServer_NotifyC2SData(const ClientData& src, const TDGClientToServerSpecial& c2s) ARTOVERRIDE;
  virtual void SimpleServer_AssignIds(const TDGClientDesc& cd, int* prgIds) ARTOVERRIDE;
  DWORD GameThdProc();


  friend DWORD WINAPI _GameThdProc(LPVOID pv);
  friend void DoServerTests();
private:
  StateDataConstPtr GetCurState() const
  {
    AutoLeaveCS _cs(m_csQuick); // need to protect our data.  It is assumed that the caller will have grabbed the overall m_cs at this point
    return m_spState;
  }
  void SetState(StateDataPtr spState)
  {
    AutoLeaveCS _cs(m_csQuick); // need to protect our data.  It is assumed that the caller will have grabbed the overall m_cs at this point
    DASSERT((void*)spState.get() != (void*)this->m_spState.get());
    DASSERT(m_csTransaction.IsOwned());
    ArtInterlockedIncrement(&m_lCurrentIteration);
    m_spState = spState;
  }
  StateDataPtr GetWriteableState()
  {
    DASSERT(m_csTransaction.IsOwned());
    StateDataConstPtr spOld;
    {
      AutoLeaveCS _cs(m_csQuick); // need to protect our data.  It is assumed that the caller will have grabbed the overall m_cs at this point
      spOld = GetCurState();
    }

    // build and return a writeable pointer, built from the current state (which we know is actually a CurrentStateData)
    return StateDataPtr(new StateData((const StateDataConst&)*spOld));
  }

  void QueuePlayerUpdate(const ClientData& sfClient, const TDGClientState& state);
  void QueuePlayerHRCadUpdate(const ClientData& sfClient, const C2S_HRMCADUPDATE_DATA& sfData);
private:
  ManagedCS m_csTransaction;
  mutable ManagedCS m_csQuick; // for GetState/SetState/GetWriteableState, to protect the smart pointer
  
  // map describing the world we're riding on.
  const vector<MAPDATA> m_lstMaps;
  boost::thread m_hGameThread;

  StateDataConstPtr m_spState;
  mutable LONG m_lCurrentIteration;

  mutable ManagedCS m_csQueue; // for QueuePlayerUpdate

  // a queue of incoming player updates, so they can be decoded and responded to on a single thread, rather than every network thread individually trying to write just its own update
  // to the current state.
  queue<QUEUEDUPDATE> m_qUpdates; 

  // game state stuff


  // runs a thread that checks for schedule updates.
  const int       m_iServerId;
  boost::shared_ptr<ScheduleChecker> m_pSchedCheck;

  // stats stuff
  boost::shared_ptr<StatsStore> m_pStatStore;
  SRTMSource* m_pSRTMSource;

  vector<AIDLL> m_lstAIs;
};