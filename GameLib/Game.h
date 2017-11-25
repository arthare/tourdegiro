#pragma once

#include "GameShared.h"
#include "PlayerSetup.h"
#include "../TourDegiroCommon/Player.h"
#include "../TourDeGiroData/PainterIFace.h"
#include "LocalStatsStore.h"
#include <fstream>

using namespace std;

class TDGGameClient;
extern TDGGameClient* g_pGameClient; // used to get finish times in PLAYERDATA::GetFinishTime

extern float g_flSpeedCadenceLastSpeed;
extern float g_flLastPowerMeter;

extern NameDB* g_pNameDB; // = 0

struct PLAYERDATA : public PlayerBase
{
  PLAYERDATA(const IMap* pMap)
    : PlayerBase(AISELECTION()),flPos(0,0,pMap->GetLapLength()),flDrawPos(0,0,pMap->GetLapLength()),distZero(0,0,pMap->GetLapLength())
  {
    tmLastUpdated = 0;
    fIsHuman = false;
    flMassKg = Player::DEFAULT_MASS; // until we find out otherwise, use DEFAULT_MASS
    flTimeSum = 0;
    flJoulesSpent = 0;
    SetHR(ArtGetTime(),30);
  }
  void AddTickData(float flNow, const IMap* pMap)
  {
    const DWORD tmNow = ArtGetTime();
    RECORDEDDATA rec((unsigned short)GetPower(),GetDistance().Minus(pMap->GetStartDistance()),flNow,pMap->GetElevationAtDistance(GetDistance()),GetSpeed(),GetCadence(tmNow),GetHR(tmNow),GetLane());
    UpdatePowerTrace(tmNow,rec);
  }
  void BuildFromTGS(const TDGGameState& state, int ixPlayer)
  {
    // note: ixPlayer is not a player ID, but rather the index of the player in the state we should build from
    id = state.posUpdate.GetPlayerId(ixPlayer);
    const PLAYERDIST flLastPos = flPos;

    flPos.flDistance = state.posUpdate.GetPosition(ixPlayer);
    flPos.iCurrentLap = state.posUpdate.GetLap(ixPlayer);
    if(flPos.flDistance < flLastPos.flDistance - 500)
    {
      flDrawPos = flPos; // make sure that if there's a big positional error that we reset properly
    }
    flSpeed = state.posUpdate.GetSpeed(ixPlayer);
    flPower = state.posUpdate.GetPower(ixPlayer);
    //g_flLaneError += (state.posUpdate.GetLane(ixPlayer) - flLane);
    flLane = state.posUpdate.GetLane(ixPlayer);
    fIsHuman = state.posUpdate.IsHuman(ixPlayer);
    fIsFrenemy = state.posUpdate.IsFrenemy(ixPlayer);
    tmLastUpdated = ArtGetTime();
    flMassKg = state.posUpdate.GetWeight(ixPlayer);
    
    SetActionFlags(state.posUpdate.IsGhost(ixPlayer) ? ACTION_FLAG_GHOST : 0, ACTION_FLAG_GHOST);
    SetActionFlags(state.posUpdate.IsSpectator(ixPlayer) ? ACTION_FLAG_SPECTATOR : 0, ACTION_FLAG_SPECTATOR);
    DASSERT(flMassKg > 0);
    DASSERT(!IsNaN(flPos.flDistance));
    DASSERT(!IsNaN(flSpeed));
    DASSERT(!IsNaN(flPower));
  }
  int id;
  PLAYERDIST flPos;
  float flSpeed;
  float flPower;
  float flLane;
  float flDrawLane; // where we should draw this player as being.  It'll be a bit innacurate (they'll already be in their new lane), but it'll smooth things out nicely
  PLAYERDIST flDrawPos; // see above
  Vector2D vDrawDir; // see above
  float flLastDraft; // updated from DoPhysics, rather than TDGGameState
  float flLastDraftNewtons;
  float flMassKg;
  DWORD tmLastUpdated;
  PLAYERDIST distZero;

  float flJoulesSpent;
  float flTimeSum;

  bool fIsHuman;
  bool fIsFrenemy;

  virtual float GetFinishTime() const ARTOVERRIDE;
  virtual std::string GetName() const ARTOVERRIDE 
  {
    if(g_pNameDB)
    {
      return g_pNameDB->GetName(this->id);
    }
    return "";
  }
  virtual float GetLane() const ARTOVERRIDE {return flDrawLane;}
  virtual const PLAYERDIST& GetDistance() const ARTOVERRIDE {return flDrawPos;}
  virtual float GetSpeed() const ARTOVERRIDE {return flSpeed;}
  virtual unsigned short GetPower() const ARTOVERRIDE {return flPower;}
  virtual float GetLastDraft() const ARTOVERRIDE {return flLastDraft;}
  virtual float GetLastDraftNewtons() const ARTOVERRIDE {return flLastDraftNewtons;}
  virtual int GetMasterId() const ARTOVERRIDE {return -1;}
  virtual int GetId() const ARTOVERRIDE {return id;}
  virtual float GetMassKg() const ARTOVERRIDE {return flMassKg;}
  virtual bool Player_IsAI() const ARTOVERRIDE {return !fIsHuman;}
  virtual bool Player_IsFrenemy() const ARTOVERRIDE {return fIsFrenemy;}

  virtual float GetStartTime() const ARTOVERRIDE {return 0;}
  
  virtual const PLAYERDIST GetStartDistance() const ARTOVERRIDE {return PLAYERDIST(0,0,flPos.flDistPerLap);}
  virtual void GetLapTimes(std::vector<LAPDATA> & lstLapTimes) const {};
  virtual void Tick(float flRaceTime, int ixMe, PHYSICSMODE ePhysicsMode, RACEMODE eRaceMode, const vector<IConstPlayerPtrConst>& lst, const DraftMap& mapDraftingInfo, float dt, SPRINTCLIMBDATA* pScore, std::string* pstrScoreDesc, const unsigned int tmNow) ARTOVERRIDE
  {
    PlayerBase::Tick(dt);
  }
  void UpdateHistory(unsigned int tmNow)
  {
    PlayerBase::UpdateHistory(tmNow);
  }
  virtual const float GetEnergySpent() const
  {
    return flJoulesSpent;
  }
  virtual const float GetTimeRidden() const
  {
    return flTimeSum;
  }
  virtual float GetAveragePower() const ARTOVERRIDE
  {
    if(flTimeSum > 0)
    {
      return flJoulesSpent / flTimeSum;
    }
    return 0;
  }
  virtual PERSONBESTSTATS& GetBestStats() ARTOVERRIDE {    return m_stats;  }
  virtual const PERSONBESTSTATS& GetBestStatsConst() const ARTOVERRIDE  {    return m_stats;  }
  PERSONBESTSTATS m_stats;
  virtual const float GetRunningPower(STATID id) const
  {
    return 0;
  }
};

struct PLAYEREXTRADATA
{
  PLAYEREXTRADATA() : strName("") {};

  string strName;
};

enum ROADSTAGE
{
  ROADSTAGE_FIRST,
  LEFTDIRT = ROADSTAGE_FIRST,
  ROAD,
  RIGHTDIRT,


  ROADSTAGE_COUNT
};

// part of the TDGSetupState: represents your player configuration: mass, name, etc
class PlayerSetupData
{
public:
  virtual bool isEnabled() const = 0;
  virtual string getName() const = 0;
  virtual float getMassKg() const = 0;
  virtual POWERTYPE getPowerType() const = 0;

  // what kind of camera does this person prefer?  The renderer should make a best-effort at implementing the selected camera style.
  virtual CAMERASTYLE getCameraStyle() const = 0; 
};

// a representation of how you want the game set up.
// tells the game logic:
// -your account ID
// -what server you want to connect to
// -etc
class TDGSetupState
{
public:
  virtual int GetMasterId() const = 0; // returns 
  virtual string GetTarget() const = 0; // returns the IP or DNS name of the server we want to connect to

  // returns the setup data for the ixPlayerth player who is playing locally today.
  virtual int GetPlayerCount() const = 0;
  virtual const PlayerSetupData* GetPlayer(int ixPlayer) const = 0;
};

class TDGGameClient : public SimpleClient<TDGGameState,TDGClientState,TDGClientDesc,TDGInitialState,::TDGClientToServerSpecial,TDGServerStatus>, 
                      public IPowerSourceReceiver,
                      public TourDeGiroFrameDoer,
                      public NameDB
{
  const static int PROTOCOL_VERSION = 1;
public:
  TDGGameClient(ICommSocketFactoryPtr pSocketFactory) 
    : SimpleClient(pSocketFactory, PROTOCOL_VERSION,SERVER_UDP_IN_PORT, SERVER_TCP_CONNECT_PORT), 
    m_eGameState(UNKNOWN),
    m_tmGameStart(0),
    m_fRequestingRestart(false),
    m_ixHighlightedLocal(0),
    m_fQuit(false),
    m_fRequestingPhysicsChange(false),
    m_fRequestingCountdownPause(false),
    m_fRequestingCountdownResume(false),
    m_fRequestingAIBirth(false),
    m_fRequestingAICull(false),
    m_fServerCountdownPaused(false),
    m_fRequestingStartNow(false),
    m_map(&m_cs),
    m_eRaceMode(RACEMODE_ROADRACE),
    m_ttManager(&m_cs),
    m_fGraphicsLoading(true),
    m_fMoveLeft(false),
    m_fMoveRight(false),
    m_iMasterId(-1),
    m_fShouldReconnect(false),
    m_fGamePaused(false)
  {
    g_pGameClient = this;
    ::g_pNameDB = this;
    memset(rgLastKeyState,0,sizeof(rgLastKeyState));
  }
  virtual ~TDGGameClient()
  {

  }
  void ClearLocalPlayers()
  {
    m_lstLocals.clear();
  }
  virtual std::string GetName(int id) const ARTOVERRIDE
  {
    unordered_map<int,PLAYEREXTRADATA>::const_iterator i = m_mapPlayerData.find(id);
    if(i != m_mapPlayerData.end())
    {
      return i->second.strName;
    }
    if(id == 0)
    {
      return "Server";
    }
    return "";
  }
  virtual unordered_map<int,std::string> GetNameList() const ARTOVERRIDE
  {
    // nobody calls this function
    return unordered_map<int,std::string>();
  }


  // SimpleClient overrides
  virtual void SimpleClient_BuildDesc(TDGClientDesc* pDesc) const ARTOVERRIDE;
  virtual void SimpleClient_BuildState(TDGClientState* pState) const ARTOVERRIDE;
  virtual void SimpleClient_SetStartupInfo(const STARTUPINFOZ<TDGInitialState>& info) ARTOVERRIDE;
  virtual bool SimpleClient_NotifyGameState(const TDGGameState& tgs) ARTOVERRIDE;
  virtual bool SimpleClient_GetSpecialData(TDGClientToServerSpecial* pData) ARTOVERRIDE; // fill up pData and return true if you have any special data to send to the server
  virtual bool SimpleClient_ShouldReconnect() ARTOVERRIDE
  {
    bool fRet = m_fShouldReconnect;
    m_fShouldReconnect = false;
    return fRet;
  }
  // IPowerSourceReceiver
  virtual void SetPower(int playerIndex, int deviceId, int sPower) ARTOVERRIDE; // sets the local player's most recent power
  virtual void SetCadence(int playerIndex, int deviceId, unsigned short cadence) ARTOVERRIDE;
  virtual void SetHR(int playerIndex, int deviceId, unsigned short hr) ARTOVERRIDE;

  void GameLoop();

  // TourDeGiroFrameDoer
  virtual ManagedCS* Doer_GetLock() override {return &m_cs;}
  virtual void Doer_Do(float dt, TDGFRAMEPARAMS& frameData) ARTOVERRIDE;
  virtual void Doer_Quit() ARTOVERRIDE;
  virtual void Doer_DoAction(DOER_ACTION eAction) ARTOVERRIDE;
  virtual void Doer_AddChat(const string& strChat) ARTOVERRIDE;

  void DoSelfTestThread(LPVOID pv);

  bool DoConnect(TDGSetupState* pSetupState, TDGConnectionResult* pResult);
private:
  void SendInternalChat(const char* pszChat);
  string GetMd5(TDGSetupState* pSetupState) const;
  void UpdateGameState(const vector<IConstPlayerPtrConst>& lstPlayers); // does a predictive update of m_mapPlayers so that the client looks smooth - also figures out the grouping for this frame
  float GetLastPower(int ixLocal) const;
  string GetPlayerName(int id) const;
  float GetFinishTime(int id) const;
  bool IsLocalId(int id) const;
  bool DoesPlayerExist(int id) const
  {
    return this->m_mapPlayers.find(id) != m_mapPlayers.end();
  }
  void CheckForDeadPlayers();
  void DoRaceSetup(); // setup once we move to racing mode
  void BuildPeletonList(const vector<IPlayerPtrConst>& lstPlayers, vector<PELETONINFOConstPtr>& lstGroups) const;
  void BuildViewports(const RECT& rcGameArea, SIZE sScreen);
  void InitPlayer(int iPlayerId); // does initialization of a player when we need to set him up the first time
  virtual void SetSpeed(int playerIndex, int deviceId, float flSpeedMetersPerSecond) ARTOVERRIDE
  {
    static DWORD tmLastSpeed = 0;
    static double dKJ = 0;
    const DWORD tmNow = ArtGetTime();

    tmLastSpeed = tmNow;
  }
  const LOCALPLAYERDATA GetLocalData(int iPlayerId) const
  {
    for(unsigned int x = 0; x < m_lstLocals.size(); x++)
    {
      if(m_lstLocals[x].idLocal == iPlayerId)
      {
        return m_lstLocals[x];
      }
    }
    DASSERT(FALSE);
    return m_lstLocals[0];
  }
  friend struct PLAYERDATA;

  // network updating stuff.  Called once per game loop to get all the recent network updates applied
  void ApplyNetData();
  bool ApplySingleGameState(const TDGGameState& tgs);

  bool DoGamePlay();
private:
  mutable ManagedCS m_cs;
  mutable ManagedCS m_csNetQueue; // manages the network queue

  // the queue of networking data.  You must hold m_csNetQueue while interacting with this.
  // how this works: the SimpleClient recv thread will call us with new data.  We stuff it in the queue.
  // then, on the main game loop thread, we dequeue all the stuff from the queue and apply the data to the game data
  // why? because if we just apply the data on the recv thread, the recv thread becomes dependent on our frame rate, which
  // bogs it down, which then bogs the server down, and everything sucks.
  queue<TDGGameState> m_qNetData; 

  // local player data
  int m_ixHighlightedLocal;
  vector<LOCALPLAYERDATA> m_lstLocals;

  // game state stuff: must be protected by m_cs all the time
  TDG_GAMESTATE m_eGameState;
  unordered_map<int,boost::shared_ptr<PLAYERDATA> > m_mapPlayers;
  unordered_map<int,PLAYEREXTRADATA> m_mapPlayerData; // maps from player IDs to player names
  unordered_map<int,float> m_mapFinishTimes;
  LockingMap m_map; // elevation map
  double dLastPower;
  PHYSICSMODE m_ePhysicsMode;
  RACEMODE m_eRaceMode;
  bool m_fServerCountdownPaused;
  bool m_fGamePaused;
  
  // key state stuff
  const static int cLastKeyState=512;
  unsigned char rgLastKeyState[cLastKeyState];
  bool m_fRequestingRestart;
  bool m_fRequestingPhysicsChange;
  bool m_fRequestingCountdownPause;
  bool m_fRequestingCountdownResume;
  bool m_fRequestingAIBirth;
  bool m_fRequestingAICull;
  bool m_fRequestingStartNow;
  bool m_fQuit;
  bool m_fGraphicsLoading;
  bool m_fMoveLeft;
  bool m_fMoveRight;

  // keeps track of our chatting state
  stack<CHATUPDATE> m_stkReceivedChats;
  queue<std::string> m_qPendingChats;

  // username/password data
  TourDeGiroMapRequest m_mapReq;

  // delay state
  DWORD m_tmGameStart;
  DWORD m_tmFirstFrameInGame; // when did we do the first bit of logic involving our current race?  used for getting rid of help menu after 60 seconds

  // whether we should reconnect - this gets set if we detect bad/corrupted data packets
  bool m_fShouldReconnect;

  int m_iMasterId;

  TimeTrialState<float> m_ttManager; // keeps track of all the segment times that we've received
};

