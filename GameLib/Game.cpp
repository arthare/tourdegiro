#include "stdafx.h"
#include "Game.h"
#include "../ServerLib/ThreadedResultsSaver.h"
#include "../TourDeGiroData/PainterIFace.h"


#ifndef _MSC_VER
#include <CoreServices/CoreServices.h> // for screensaver prevention
#endif

const char* g_pszOverrideTarget = 0;

float g_flTotalError = 0;
float g_flLaneError = 0;
int g_cTotalError = 0;
int g_idMonitor = 0;

Map* g_map = NULL;
TDGGameClient* g_pGameClient = NULL;
NameDB* g_pNameDB = NULL;

#define PI 3.14159

Vector2D GetMapDir(const PLAYERDIST& flDist, const IMap* pMap)
{
  float flX,flY;
  pMap->GetDirectionAtDistance(flDist,&flX,&flY);
  return V2D(flX,flY);
}

float PLAYERDATA::GetFinishTime() const
{
  return g_pGameClient->GetFinishTime(id);
}

DWORD DoPlayerBackup(LPVOID pvData);
class AutobackupData
{
public:
  AutobackupData(bool fTrainerMode, DWORD tmNow, const char* pszBase, IConstPlayerDataPtrConst pPlayer) : m_fTrainerMode(fTrainerMode),m_lstData(pPlayer->GetPowerHistory()),m_strBase(pszBase), m_pConstCopy(pPlayer) 
  {
    m_thd = boost::thread(DoPlayerBackup,this);
  };
  virtual ~AutobackupData() {};


  const vector<RECORDEDDATA> m_lstData;
  IConstPlayerDataPtrConst m_pConstCopy;
  boost::thread m_thd;
  string m_strBase;
  const bool m_fTrainerMode;
};

DWORD DoPlayerBackup(LPVOID pvData)
{
  AutobackupData* pData = (AutobackupData*)pvData;

  // first, we gotta build the path:
  // g_qos.strSaveDest/<playername>/year-month-day-hour-minute.<format>
  SYSTEMTIME st;
  ArtGetLocalTime(&st);

  bool fOverwrite = false;
  
  TCHAR pathmark = '\\';
#ifdef _MSC_VER
  pathmark = '\\';
#else
  pathmark = '/';
#endif
  
  wstringstream ss;
  
  string strNameA(pData->m_pConstCopy->GetName());
  wstring strName(strNameA.begin(),strNameA.end());
  
  wstring strBase(pData->m_strBase.begin(),pData->m_strBase.end());
  
  if(pData->m_strBase.length() <= 0)
  {
    // use the default path
    ss<<g_qos.strSaveDest<<pathmark<<strName<<pathmark<<st.wYear<<" - "<<st.wMonth<<" - "<<st.wDay<<" "<<st.wHour<<"h "<<st.wMinute<<"m "<<strBase;
  }
  else
  {
    fOverwrite = true;
    // use the "custom" base path they requested
    
    ss<<g_qos.strSaveDest<<pathmark<<strName<<pathmark<<st.wYear<<" - "<<st.wMonth<<" - "<<st.wDay<<" "<<st.wHour<<"h "<<st.wMinute<<"m "<<strBase;
  }

  ArtCreateDirectories(ss.str().c_str());

  wcout<<"We want to save to "<<ss.str().c_str()<<endl;
  WriteCSVFile(ss.str().c_str(), fOverwrite, pData->m_fTrainerMode, pData->m_lstData);
  WritePWXFile(ss.str().c_str(), fOverwrite, pData->m_fTrainerMode, pData->m_pConstCopy, pData->m_lstData);
  WriteTCXFile(ss.str().c_str(), fOverwrite, pData->m_fTrainerMode, pData->m_pConstCopy, pData->m_lstData);

  delete pData;

  return 0;
}

void SavePlayerData(bool fTrainerMode, const DWORD tmNow, const char* pszBase, IConstPlayerDataPtrConst pPlayer)
{
  if(!pszBase) pszBase = "";
  AutobackupData* pData = new AutobackupData(fTrainerMode, tmNow, pszBase, pPlayer); // this will create a thread and dump the data on that thread, then delete the data object
}

void TDGGameClient::SimpleClient_BuildDesc(TDGClientDesc* pDesc) const
{
  pDesc->eConnType = CONNTYPE_PLAYGAME;
  pDesc->cLocalPlayers = m_lstLocals.size();
  pDesc->iMasterId = this->m_iMasterId;
  for(int x = 0;x < m_lstLocals.size(); x++)
  {
    pDesc->SetName(x,m_lstLocals[x].strPlayerName);
    pDesc->rgWeights[x] = (unsigned short)m_lstLocals[x].flMassKg*10;
    pDesc->rgPlayerDevices[x] = m_lstLocals[x].GetPowerType();
  }

  if(m_mapReq.IsValid(NULL))
  {
    pDesc->mapReq.iMeters = m_mapReq.iMeters;
    pDesc->mapReq.laps = m_mapReq.laps;
    pDesc->mapReq.iSecondsToDelay = m_mapReq.iSecondsToDelay;
    pDesc->mapReq.iMapId = m_mapReq.iMapId;
    pDesc->mapReq.iAIMinStrength = m_mapReq.iAIMinStrength;
    pDesc->mapReq.iAIMaxStrength = m_mapReq.iAIMaxStrength;
    pDesc->mapReq.cAIs = m_mapReq.cAIs;
    pDesc->mapReq.cPercentStart = m_mapReq.cPercentStart;
    pDesc->mapReq.cPercentEnd = m_mapReq.cPercentEnd;
    pDesc->mapReq.timedLengthSeconds = m_mapReq.timedLengthSeconds;
  }
}
void TDGGameClient::SimpleClient_BuildState(TDGClientState* pState) const
{ 
  AutoLeaveCS _cs(m_cs);
  pState->cLocalPlayers = m_lstLocals.size();
  float powerProduct = 1;
  for(int x = 0;x < m_lstLocals.size(); x++)
  {
    pState->rgPlayerIds[x] = m_lstLocals[x].idLocal;

    if(m_lstLocals[x].m_ePowerType != SPECTATING)
    {
      if(m_lstLocals[x].m_ePowerType == CHEATING)
      {
        int powerToSend = (m_lstLocals[x].m_sLastPower + ((rand() % 20) - 10));
        powerToSend = max(0, (int)powerToSend);
        pState->rgPlayerPowers[x] = powerToSend;
      }
      else
      {
        pState->rgPlayerPowers[x] = m_lstLocals[x].m_sLastPower;
      }
    }
    else
    {
      // spectators need to send their current road position to the server
      pState->rgPlayerPowers[x] = (unsigned short)m_lstLocals[x].flRoadPos/10;
    }
    powerProduct *= pState->rgPlayerPowers[x];
    pState->rgfdwActionFlags[x] = 0;

    if(x == 0)
    {
      if(m_fMoveLeft) pState->rgfdwActionFlags[x] |= ACTION_FLAG_LEFT;
      if(m_fMoveRight) pState->rgfdwActionFlags[x] |= ACTION_FLAG_RIGHT;
    }

    DASSERT(pState->rgPlayerPowers[x] >= 0 && pState->rgPlayerPowers[x] < 65000);
  }
  pState->powerProduct = powerProduct; // checksum

  if(m_fGraphicsLoading)
  {
    for(int x = 0;x < MAX_LOCAL_PLAYERS; x++)
    {
      pState->rgfdwActionFlags[x] |= ACTION_FLAG_LOADING;
    }
  }
}
void TDGGameClient::SimpleClient_SetStartupInfo(const STARTUPINFOZ<TDGInitialState>& info)
{
  unordered_set<int> setAssigned;
  for(int x = 0;x < m_lstLocals.size(); x++)
  {
    cout<<"I've been assigned id "<<info.rgIds[x]<<" for "<<m_lstLocals[x].strPlayerName<<endl;
    DASSERT(setAssigned.find(info.rgIds[x]) == setAssigned.end());
    setAssigned.insert(info.rgIds[x]);
  }
  AutoLeaveCS _cs(m_cs);
  
  for(int x = 0;x < m_lstLocals.size(); x++)
  {
    m_lstLocals[x].idLocal = m_lstLocals[x].idViewing = info.rgIds[x];
    this->m_mapPlayerData[m_lstLocals[x].idLocal].strName = m_lstLocals[x].strPlayerName;
  }

  if(info.guid == GetConnectionGuid() && m_map.IsValid() && m_map.GetMapName().compare(info.sfInfo.szMapName) == 0)
  {
    // if this was a successful reconnection, we don't need to rebuild the heightmap
  }
  else
  {
    m_map.BuildFromInitState(info.sfInfo,true, 0);
    { // save CSV data for all local players
      for(unsigned int x = 0;x < m_lstLocals.size(); x++)
      {
        unordered_map<int,boost::shared_ptr<PLAYERDATA> >::const_iterator i = m_mapPlayers.find(m_lstLocals[x].idLocal);
        if(i != m_mapPlayers.end())
        {
          // found the local guy's player
          ::SavePlayerData(false, 0,NULL,i->second->GetConstDataCopy(0,true));
        }
      }
    }

    m_eRaceMode = info.sfInfo.eRaceMode;
    m_mapPlayers.clear();
    this->m_mapPlayerData.clear();
    this->m_mapFinishTimes.clear();
  }
  if(m_map.IsValid())
  {
    // hooray!
    // we need to tell the painter about this new map, but we're on a non-UI thread...
  }
  else
  {
    exit(0);
  }
  cout<<"Map name is "<<m_map.GetMapName().c_str()<<endl;
}
void TDGGameClient::DoRaceSetup()
{
  for(ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.begin(); i != m_mapPlayers.end(); i++)
  {
    i->second->flDrawLane = 0;
    i->second->flDrawPos = m_map.GetStartDistance();
    i->second->vDrawDir = ::GetMapDir(m_map.GetStartDistance(),&m_map);
  }

  for(unsigned int x = 0;x < m_lstLocals.size(); x++)
  {
    m_lstLocals[x].ResetPowerAvg();
  }
}

void TDGGameClient::InitPlayer(int iPlayerId)
{
  // create player.  initialize player
  m_mapPlayers[iPlayerId]->flDrawLane = m_mapPlayers[iPlayerId]->flLane;
  m_mapPlayers[iPlayerId]->flDrawPos = m_mapPlayers[iPlayerId]->flPos;
  m_mapPlayers[iPlayerId]->flJoulesSpent = 0;
  m_mapPlayers[iPlayerId]->flTimeSum = 0;
  m_mapPlayerData[iPlayerId] = PLAYEREXTRADATA();
  LOCALPLAYERDATA* pLocal = NULL;
  for(unsigned int x = 0;x < m_lstLocals.size(); x++)
  {
    if(m_lstLocals[x].idLocal == iPlayerId)
    {
      pLocal = &m_lstLocals[x];
    }
  }
  if(pLocal)
  {
    // if this player is actually a local player, then we want to initialize his color to the local guy's colours
    m_mapPlayerData[iPlayerId].strName = pLocal->strPlayerName;
  }
  else
  {
    m_mapPlayerData[iPlayerId] = PLAYEREXTRADATA();
  }
}
bool TDGGameClient::SimpleClient_NotifyGameState(const TDGGameState& tgs)
{
  AutoLeaveCS _cs(m_csNetQueue);
  int iChecksum = TDGGameState::GetChecksum(&tgs);
  DASSERT(iChecksum == tgs.checksum);
  m_qNetData.push(tgs);

  return true;
}
void TDGGameClient::ApplyNetData()
{
  //DASSERT(m_cs.IsOwnedByCurrentThread());
  while(true)
  {
    TDGGameState tgs;
    {
      AutoLeaveCS _cs(m_csNetQueue);
      if(m_qNetData.size() <= 0) break;
      tgs = m_qNetData.front();
      m_qNetData.pop();
    }
    if(!ApplySingleGameState(tgs))
    {
      m_fShouldReconnect = true;
    }
  }
}
bool TDGGameClient::ApplySingleGameState(const TDGGameState& tgs)
{
  //DASSERT(m_cs.IsOwnedByCurrentThread());

  int iChecksum = TDGGameState::GetChecksum(&tgs);
  if(iChecksum != tgs.checksum)
  {
    DASSERT(FALSE); // historically, this has meant something is fucked up.  FIX IT.
    cout<<"Failed checksum for update type "<<tgs.eType<<endl;
    return false;
  }

  if(tgs.eType == CHAT_UPDATE)
  {
    // someone wants to chat to us!
    CHATUPDATE cu = tgs.chatUpdate;
    for(int x = 0;x < sizeof(tgs.chatUpdate.szChat); x++)
    {
      int iChar = (int)cu.szChat[x];
      if(iChar >= 127 || iChar < 0) cu.szChat[x] = '_'; // making safe non-ASCII characters
    }
    m_stkReceivedChats.push(cu);
  }
  else if(tgs.eType == TT_UPDATE)
  {
    for(unsigned int x = 0;x < tgs.ttupdate.TT_PLAYER_COUNT; x++)
    {
      const int id = tgs.ttupdate.ids[x];
      if(id != INVALID_PLAYER_ID)
      {
        char szTime[100];
        ::FormatTimeMinutesSecondsMs(tgs.ttupdate.times[x],szTime,sizeof(szTime));
        cout<<GetName(id).c_str()<<" was "<<szTime<<" at dist "<<tgs.ttupdate.segment[x] * TimeTrialState<float>::s_minInterval<<endl;
      }
    }
    m_ttManager.AddNetState(tgs.ttupdate);
  }
  else if(tgs.eType == NAME_UPDATE)
  {
    for(int x = 0;x < MAX_PLAYERS;x++)
    {
      if(tgs.nameUpdate.rgPlayerIds[x] != INVALID_PLAYER_ID)
      {
        PLAYEREXTRADATA& ped = m_mapPlayerData[tgs.nameUpdate.rgPlayerIds[x]];

        const int cch = sizeof(tgs.nameUpdate.rgszPlayerNames[x])+1;
        char szMaxName[cch];
        memcpy(szMaxName,tgs.nameUpdate.rgszPlayerNames[x],cch-1);
        szMaxName[cch-1] = 0;
        ped.strName = szMaxName;
      }
    }
  }
  else if(tgs.eType == RESULT_UPDATE)
  {
    // results!  someone must have finished!
    for(int x = 0; x < MAX_PLAYERS;x++)
    {
      if(tgs.resultUpdate.rgPlayerIds[x] != INVALID_PLAYER_ID)
      {
        ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.find(tgs.resultUpdate.rgPlayerIds[x]);
        if(i != m_mapPlayers.end())
        {
          // this is a player that actually exists!
          m_mapFinishTimes[tgs.resultUpdate.rgPlayerIds[x]] = tgs.resultUpdate.rgflPlayerTime[x];
        }
        m_ttManager.AddTime(0,tgs.resultUpdate.rgPlayerIds[x],tgs.resultUpdate.rgflPlayerTime[x]);
      }
    }
  }
  else if(tgs.eType == POSITION_UPDATE)
  {
    int physicsBit1 = !!(tgs.posUpdate.GetServerFlags() & SF_NORMALIZEDPHYSICS);
    int physicsBit2 = !!(tgs.posUpdate.GetServerFlags() & SF_PHYSICSBIT2);
    int physicsBit3 = !!(tgs.posUpdate.GetServerFlags() & SF_PHYSICSBIT3);
    int physicsBit4 = !!(tgs.posUpdate.GetServerFlags() & SF_PHYSICSBIT4);
    m_ePhysicsMode = (PHYSICSMODE)(physicsBit1 | (physicsBit2<<1) | (physicsBit3<<2) | (physicsBit4<<3));
    m_fGamePaused = !!(tgs.posUpdate.GetServerFlags() & SF_PAUSEDGAME);
    m_fServerCountdownPaused = !!(tgs.posUpdate.GetServerFlags() & SF_PAUSEDCOUNTDOWN);

    const bool fWasRacing = m_eGameState == RACING;
    m_eGameState = tgs.posUpdate.iTimeUntilStart > 0 ? WAITING_FOR_START : RACING;

    { // updating peoplecounts for the scoring sources
      vector<SprintClimbPointPtr> lstSources;
      m_map.GetScoringSources(lstSources);
      for(unsigned int x = 0;x < lstSources.size(); x++)
      {
        SprintClimbPointPtr pSource = lstSources[x];
        pSource->SetPeopleCount(tgs.posUpdate.iSprintClimbPeopleCount);
      }
    }

    if(m_eGameState == RACING && !fWasRacing)
    {
      m_tmFirstFrameInGame = ArtGetTime();
      DoRaceSetup();
    }
    else if(m_eGameState == WAITING_FOR_START && fWasRacing)
    {
      m_mapPlayers.clear();
      m_mapPlayerData.clear();
      this->m_mapFinishTimes.clear();
    }
    m_tmGameStart = ArtGetTime() + tgs.posUpdate.iTimeUntilStart;
    DWORD tmNow = ArtGetTime();
    for(int x = 0; x < MAX_PLAYERS;x++)
    {
      const int iPlayerId = tgs.posUpdate.GetPlayerId(x);
      if(iPlayerId != INVALID_PLAYER_ID && IsValidPositionSlot(tgs.posUpdate,x))
      {
        if(tgs.posUpdate.IsDead(x))
        {
          m_mapPlayers.erase(iPlayerId);
          m_mapPlayerData.erase(iPlayerId);
        }
        else if(m_mapPlayerData.find(iPlayerId) == m_mapPlayerData.end() || m_mapPlayers.find(iPlayerId) == m_mapPlayers.end())
        {
          m_mapPlayers[iPlayerId] = boost::shared_ptr<PLAYERDATA>(new PLAYERDATA(&m_map));
          m_mapPlayers[iPlayerId]->BuildFromTGS(tgs,x);
          InitPlayer(iPlayerId);
          DASSERT(m_mapPlayers[iPlayerId]->id == iPlayerId);
        }
        else
        {
          // this is a valid player update for a player that already existed
          m_mapPlayers[iPlayerId]->BuildFromTGS(tgs,x);
          DASSERT(m_mapPlayers[iPlayerId]->id == iPlayerId);
        }
      }
    }

  }
  return true;
}
bool TDGGameClient::SimpleClient_GetSpecialData(TDGClientToServerSpecial* pData)
{
  static DWORD tmLastPlayerUpdateOverTCP = 0;
  static DWORD tmLastHRCadUpdateOverTCP = 0;
  DWORD tmNow = ArtGetTime();

  if(m_qPendingChats.size() > 0)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = C2S_CHAT_UPDATE;
    pData->chat.idFrom = m_lstLocals[0].idLocal;
    GetSystemTimeAsFileTime(&pData->chat.tmSent);

    const std::string strData = m_qPendingChats.front();
    _snprintf(pData->chat.szChat,sizeof(pData->chat.szChat),"%s",strData.c_str());
    m_qPendingChats.pop();
    return true;
  }
  else if(tmNow - tmLastPlayerUpdateOverTCP > 125)
  {
    AutoLeaveCS _cs(m_cs); // need to grab this before netqueue.  Otherwise, BuildState grabs it and we have  AB-BA deadlock
    AutoLeaveCS _cs2(m_csNetQueue);
    // it has been 500ms since our last player update over TCP
    pData->eType = C2S_PLAYERUPDATE;
    SimpleClient_BuildState(&pData->state);
    tmLastPlayerUpdateOverTCP = tmNow;
    return true;
  }
  else if(m_fRequestingRestart)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = C2S_RESTART_REQUEST;
    m_fRequestingRestart = false;
    return true;
  }
  else if(m_fRequestingAICull || m_fRequestingAIBirth)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = C2S_CHANGEAI;
    pData->data = m_fRequestingAICull ? -10 : 10;
    m_fRequestingAICull = m_fRequestingAIBirth = false;
    return true;
  }
  else if(m_fRequestingPhysicsChange)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = C2S_PHYSICS_CHANGE;
    int iNextPhysics = ((int)m_ePhysicsMode)+1;
    if(iNextPhysics >= PHYSICSMODE_LAST) iNextPhysics = PHYSICSMODE_FIRST;

    pData->data = iNextPhysics;

    m_fRequestingPhysicsChange = false;
    return true;
  }
  else if(m_fRequestingCountdownPause)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = this->m_eGameState == RACING ? C2S_GAME_PAUSE : C2S_COUNTDOWN_PAUSE;
    pData->data = true;
    m_fRequestingCountdownPause = false;
    return true;
  }
  else if(m_fRequestingCountdownResume)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = this->m_eGameState == RACING ? C2S_GAME_PAUSE : C2S_COUNTDOWN_PAUSE;
    pData->data = false;
    m_fRequestingCountdownResume = false;
    return true;
  }
  else if(m_fRequestingStartNow)
  {
    AutoLeaveCS _cs(m_csNetQueue);
    pData->eType = C2S_STARTNOW;
    pData->data = 0;
    m_fRequestingStartNow = false;
    return true;
  }
  else if(tmNow - tmLastHRCadUpdateOverTCP > 750)
  {
    AutoLeaveCS _cs(m_cs); // need to grab this before netqueue.  Otherwise, BuildState grabs it and we have  AB-BA deadlock
    AutoLeaveCS _cs2(m_csNetQueue);

    memset(&pData->hrcad,0,sizeof(pData->hrcad));
    pData->eType = C2S_HRMCADUPDATE;
    pData->hrcad.cLocalPlayers = m_lstLocals.size();

    bool fAnyNonZero = false;
    for(int x = 0;x < this->m_lstLocals.size(); x++)
    {
      pData->hrcad.rgPlayerIds[x] = m_lstLocals[x].idLocal;
      ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.find(pData->hrcad.rgPlayerIds[x]);
      if(i != m_mapPlayers.end())
      {
        // we found the guy that this cadence represents!
        int HR = i->second->GetHR(tmNow);
        int cadence = i->second->GetCadence(tmNow);
        fAnyNonZero |= (HR != 0 || cadence != 0);
        pData->hrcad.rgHR[x] = HR;
        pData->hrcad.rgCadence[x] = cadence;
      }
    }
    tmLastHRCadUpdateOverTCP = tmNow;
    return fAnyNonZero;
  }
  return false;
}
void TDGGameClient::SendInternalChat(const char* pszChat)
{
  CHATUPDATE cu;
  strncpy(cu.szChat,pszChat,sizeof(cu.szChat));
  cu.idFrom = 0;
  GetSystemTimeAsFileTime(&cu.tmSent);
  if(cu.szChat[0])
  {
    m_stkReceivedChats.push(cu);
  }
}

//ofstream speedlog;
//float g_flCTSpeed = 0;
void TDGGameClient::SetPower(int playerIndex, int deviceId, int sPower)
{
  if(m_cs.Try()) // don't block the delivery thread if the game is busy doing something else...
  {
    if(sPower < 0)
    {
      // means the device manager is trying to tell us something
      string strPlayerName;
      if(playerIndex >=0 && playerIndex < m_lstLocals.size())
      {
        LOCALPLAYERDATA& local = m_lstLocals[playerIndex];
        strPlayerName = this->GetPlayerName(local.GetLocalId());
        local.m_fLastPowerWasError = true;
      }
      dLastPower = 0;
      char szChat[2000];
      switch(sPower)
      {
      case POWERRECV_NEEDCALIB:         snprintf(szChat,sizeof(szChat),"%s's power device needs calibration",strPlayerName.c_str()); break;
      case POWERRECV_PEDALFASTER:       snprintf(szChat,sizeof(szChat),"%s: pedal faster to calibrate",strPlayerName.c_str()); break;
      case POWERRECV_COASTDOWN:         snprintf(szChat,sizeof(szChat),"%s: coast to calibrate",strPlayerName.c_str()); break;
      case POWERRECV_PRESSBUTTON:       snprintf(szChat,sizeof(szChat),"%s: push CT-F3 to finish calibration",strPlayerName.c_str()); break;
      case POWERRECV_DISCONNECTED:      snprintf(szChat,sizeof(szChat),"%s: power device has gone missing.  Searching...",strPlayerName.c_str()); break;
      }
      SendInternalChat(szChat);
    }
    else
    {
      dLastPower = sPower;
      if(playerIndex >=0 && playerIndex < m_lstLocals.size())
      {
        LOCALPLAYERDATA& local = m_lstLocals[playerIndex];
        
        if(local.m_fLastPowerWasError)
        {
          char szChat[200];
          snprintf(szChat,sizeof(szChat),"%s's power device is back!",local.strPlayerName.c_str());
          SendInternalChat(szChat);
          local.m_fLastPowerWasError = false;
        }
        bool fRacing = false;
        const float flFinishTime = GetFinishTime(local.idLocal);
        fRacing = flFinishTime < 0 && m_eGameState == RACING;
        m_lstLocals[playerIndex].AddPower(sPower,fRacing);

      }
    }
    m_cs.unlock();
  }
}
void TDGGameClient::SetCadence(int playerIndex, int deviceId, unsigned short cadence)
{
  AutoLeaveCS _cs(m_cs);
  if(playerIndex >=0 && playerIndex < m_lstLocals.size())
  {
    const LOCALPLAYERDATA& local = m_lstLocals[playerIndex];

    bool fRacing = false;
    const float flFinishTime = GetFinishTime(local.idLocal);
    fRacing = flFinishTime < 0 && m_eGameState == RACING;
    int idPlayer = m_lstLocals[playerIndex].idLocal;

    unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.find(idPlayer);
    if(i != m_mapPlayers.end())
    {
      // we found the guy that this cadence represents!
      i->second->SetCadence(ArtGetTime(),cadence);
    }
  }
}
void TDGGameClient::SetHR(int playerIndex, int deviceId, unsigned short hr)
{
  AutoLeaveCS _cs(m_cs);
  if(playerIndex >=0 && playerIndex < m_lstLocals.size())
  {
    const LOCALPLAYERDATA& local = m_lstLocals[playerIndex];

    bool fRacing = false;
    const float flFinishTime = GetFinishTime(local.idLocal);
    fRacing = flFinishTime < 0 && m_eGameState == RACING;
    int idPlayer = m_lstLocals[playerIndex].idLocal;

    ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.find(idPlayer);
    if(i != m_mapPlayers.end())
    {
      // we found the guy that this cadence represents!
      i->second->SetHR(ArtGetTime(),hr);
    }
  }
}

void TDGGameClient::Doer_Do(float dt, TDGFRAMEPARAMS& frameData)
{
  static DWORD tmDisplayCheck = 0;

  if(this->m_fGamePaused)
  {
    dt /= 1000.0f;
  }

#ifdef _WIN32
  if(ArtGetTime() - tmDisplayCheck > 30000)
  {
    DWORD result = SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
    DASSERT(result != 0);
    tmDisplayCheck = ArtGetTime();
  }
#else
  if(ArtGetTime() - tmDisplayCheck > 30000)
  {
    UpdateSystemActivity(UsrActivity);
  }
#endif
  const DWORD tmNow = ArtGetTime();

  {
    AutoLeaveCS _cs(m_cs);
      
    ApplyNetData();
    
    CheckForDeadPlayers();

    vector<IConstPlayerPtrConst> lstDraftPlayers;
    for(ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.begin(); i != m_mapPlayers.end(); i++)
    {
      IPlayerPtrConst sp(i->second);

      if(IS_FLAG_SET(sp->GetActionFlags(),ACTION_FLAG_SPECTATOR) && !IsLocalId(sp->GetId()))
        continue;
      if(IS_FLAG_SET(sp->GetActionFlags(),ACTION_FLAG_DEAD)) 
        continue;
      if(IS_FLAG_SET(sp->GetActionFlags(),::ACTION_FLAG_DOOMEDAI)) 
        continue;
      frameData.lstPlayers.push_back(sp);
      if(IS_FLAG_SET(sp->GetActionFlags(),::ACTION_FLAG_IGNOREFORPHYSICS)) // spectators don't count for drafting
        continue;
      if(sp->GetFinishTime() >= 0) // finished players don't count for drafting
        continue;
      lstDraftPlayers.push_back(sp);
    }

    for(unsigned int x = 0;x < m_lstLocals.size(); x++)
    {
      if(  (!DoesPlayerExist(m_lstLocals[x].idViewing) || (m_lstLocals[x].idViewing != m_lstLocals[x].idLocal && DoesPlayerExist(m_lstLocals[x].idLocal))) && frameData.lstPlayers.size() > 0)
      {
        // let's see if their local target is available...
        if(DoesPlayerExist(m_lstLocals[x].idLocal))
        {
          m_lstLocals[x].idViewing = m_lstLocals[x].idLocal;
        }
        else
        {
          m_lstLocals[x].idViewing = frameData.lstPlayers[0]->GetId();
        }
      }
      frameData.lstViewports.push_back(&m_lstLocals[x]);
    }
    frameData.pCurrentMap = &this->m_map;
    frameData.eRaceMode = this->m_eRaceMode;
    frameData.pNameDB = this;
    frameData.eGameState = m_eGameState;
    frameData.msToStart = (int)m_tmGameStart - (int)ArtGetTime();
    frameData.fdwServerFlags = (this->m_fServerCountdownPaused ? SF_PAUSEDCOUNTDOWN : 0);
    frameData.fdwServerFlags |= (this->m_fGamePaused ? SF_PAUSEDGAME : 0);
    frameData.ePhysicsMode = m_ePhysicsMode;
    frameData.pTTState = &m_ttManager;

    { // building chat list
      frameData.lstChats.clear();
      stack<CHATUPDATE> stkLocal = m_stkReceivedChats;
      while(stkLocal.size())
      {
        const CHATUPDATE& cu = stkLocal.top();
        frameData.lstChats.push_back(cu);
        stkLocal.pop();
      }
    }
    

    if(m_eGameState == RACING)
    {
      UpdateGameState(lstDraftPlayers);
      PlayerRankCompare<TDGGameClient> prc(this);
      sort(frameData.lstPlayers.begin(),frameData.lstPlayers.end(),prc);
      BuildPeletonList(frameData.lstPlayers,frameData.lstGroups);
      
      for(unsigned int x = 0;x < m_lstLocals.size(); x++)
      {
        unordered_map<int,boost::shared_ptr<PLAYERDATA> >::const_iterator i = m_mapPlayers.find(m_lstLocals[x].idLocal);
        if(i != m_mapPlayers.end() && tmNow - m_lstLocals[x].m_tmLastBackup > 120000)
        {
          SavePlayerData(false, tmNow,"Backup",i->second->GetConstDataCopy(tmNow,true));
          m_lstLocals[x].m_tmLastBackup = tmNow;
        }
      }
    }
    else
    {
      PlayerRankCompare<TDGGameClient> prc(this);
      sort(frameData.lstPlayers.begin(),frameData.lstPlayers.end(),prc);
      BuildPeletonList(frameData.lstPlayers,frameData.lstGroups);
    }
  }
}

void TDGGameClient::Doer_DoAction(DOER_ACTION eAction)
{
  switch( eAction)
  {
  case DOER_RESTART:
    m_fRequestingRestart = true;
    break;
  case DOER_STARTNOW:
    m_fRequestingStartNow = true;
    break;
  case DOER_MOREAI:
    m_fRequestingAIBirth = true;
    break;
  case DOER_LESSAI:
    m_fRequestingAICull = true;
    break;
  case DOER_CHANGEWEIGHTMODE:
    m_fRequestingPhysicsChange = true;
    break;
  case DOER_PAUSECOUNT:
    m_fRequestingCountdownPause = true;
    break;
  case DOER_RESUMECOUNT:
    m_fRequestingCountdownResume = true;
    break;
  case DOER_MOVELEFT:
    m_fMoveLeft = true;
    break;
  case DOER_MOVERIGHT:
    m_fMoveRight = true;
    break;
  case DOER_NOMOVE:
    m_fMoveLeft = m_fMoveRight = false;
    break;
  case DOER_STARTINGLOAD:
    m_fGraphicsLoading = true;
    break;
  case DOER_DONELOAD:
    m_fGraphicsLoading = false;
    break;
  case DOER_NODOWNHILL:
    static DWORD tmLast = 0;
    const DWORD tmNow = ArtGetTime();
    if(tmNow - tmLast > 2000)
    {
      g_qos.fNoDownhillMode = !g_qos.fNoDownhillMode;
      if(g_qos.fNoDownhillMode)
      {
        SendInternalChat("No-downhill mode turned ON (smart trainers won't do downhills)");
      }
      else
      {
        SendInternalChat("No-downhill mode turned off (smart trainers will do downhills)");
      }
      tmLast = tmNow;
    }
    break;
  }
}
void TDGGameClient::Doer_AddChat(const string& strChat)
{
  m_qPendingChats.push(strChat);
}

void BuildLocalPlayers(IPowerSourceReceiver* pPowerReceiver, const TDGSetupState& setupState, vector<LOCALPLAYERDATA>& lstLocals)
{
  lstLocals.clear();

  int ixPlayer = 0;
  for (int x = 0; x < 8 && x < setupState.GetPlayerCount(); x++)
  {
    // time to figure out what they've actually selected...
    const PlayerSetupData* pPlayer = setupState.GetPlayer(x);
    if (pPlayer->isEnabled())
    {
      LOCALPLAYERDATA lpd(pPlayer->getName(), pPlayer->getMassKg(), pPlayer->getPowerType(), pPlayer->getCameraStyle());
      switch (pPlayer->getPowerType())
      {
      case POWERCURVE:
      case COMPUTRAINER_ERG:
      case COMPUTRAINER:
      case WAHOO_KICKR:
      case ANT_FEC:
      case ANT_POWER:
      case ANT_SPEED:
      case ANT_SPEEDONLY:
      {
        //antHandler.ChangePowerReceiver(pPlayer->m_ixPlayer, ixPlayer, pPowerReceiver);
        //lpd.pLoadController = antHandler.GetLoadController(ixPlayer);
        break;
      }
      case SPECTATING:
      {
        lpd.m_sLastPower = 0;
        break;
      }
      case CHEATING:
      {
        //antHandler.ChangePowerReceiver(pPlayer->m_ixPlayer, ixPlayer, pPowerReceiver); // they might have a HRM...
        //lpd.m_sLastPower = pPlayer->cheatingWatts + rand()%20 - 10;
        break;
      }
      }

      lstLocals.push_back(lpd);
      ixPlayer++;
    }
    else
    {
    }
  }
}


bool TDGGameClient::DoConnect(TDGSetupState* pSetupState, TDGConnectionResult* pResult)
{
  TourDeGiroMapRequest myMapRequest;

  this->Disconnect();
  ArtSleep(500);
  m_pSocketFactory = ICommSocketFactoryPtr(CreateBoostSockets());

  m_mapReq = myMapRequest;

  BuildLocalPlayers(this, *pSetupState, m_lstLocals);

  // should be all done local players now, let's try connecting
  m_iMasterId = pSetupState->GetMasterId();
  if(Connect(pSetupState->GetTarget().c_str(),pResult) && pResult->eResult == LOGINSUCCESS)
  {
    return true;
  }
  else if(pResult->eResult == CONNECTIONFAILURE)
  { 
    const TCHAR* pszServerDownUrl = L"http://www.tourdegiro.com/fuckup.php?masterid=%d&serverid=%d&target=%S";
    // that's no good
    TCHAR rgURL[1000];
    _snwprintf(rgURL,NUMCHARS(rgURL), pszServerDownUrl,m_iMasterId,0,pSetupState->GetTarget().c_str());
    vector<char> buf;
    DownloadFileToMemory(rgURL,buf); // tell us about it!
  }
  return false;
}

void TDGGameClient::Doer_Quit()
{
}
string TDGGameClient::GetPlayerName(int id) const
{
  char szName[256];
  szName[0] = 0;
  unordered_map<int,PLAYEREXTRADATA>::const_iterator iData = m_mapPlayerData.find(id);
  if(iData != m_mapPlayerData.end())
  {
    strncpy(szName,iData->second.strName.c_str(), sizeof(szName));
  }
  else
  {
    if(id == 0) return "Server";
    _snprintf(szName,sizeof(szName),"Player #%d",id); 
  }
  return string(szName);
}
float TDGGameClient::GetFinishTime(int id) const 
{
  unordered_map<int,float>::const_iterator iTime = m_mapFinishTimes.find(id);
  if(iTime != m_mapFinishTimes.end())
  {
    return iTime->second;
  }
  else
  {
    return -1;
  }
}


bool TDGGameClient::IsLocalId(int id) const
{
  bool fIsLocal = false;
  for(unsigned int x = 0;x < m_lstLocals.size(); x++)
  {
    if(m_lstLocals[x].idLocal == id)
    {
      return true;
    }
  }
  return false;
}
void TDGGameClient::UpdateGameState(const vector<IConstPlayerPtrConst>& lstPlayers)
{
  static DWORD tmLast = 0;
  DWORD tmNow = ArtGetTime();
  if(tmLast == 0 || tmNow - tmLast > 5000)
  {
    tmLast = ArtGetTime();
    return; // we don't know what dt is or dt is huge, so don't bother doing this update
  }

  const double dt = (double)(tmNow - tmLast) / 1000.0;
  tmLast = tmNow;
  {
    AutoLeaveCS _cs(m_cs);

	  for(int x = 0;x < m_lstLocals.size(); x++)
	  {
		  LOCALPLAYERDATA& local = m_lstLocals[x];
		  ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::const_iterator i = m_mapPlayers.find(local.idLocal);
		  if(i != m_mapPlayers.end())
		  {
        const float rho = m_map.GetRhoAtDistance(i->second->GetDistance().flDistance);
			  float flSlope = m_eGameState == RACING ? m_map.GetSlopeAtDistance(i->second->GetDistance()) : 0;
        if(i->second->GetDistance() >= m_map.GetEndDistance())
        {
          flSlope = -0.02; // downhill slope for finishers - let those dudes cool down
        }
        flSlope = min(0.12f,flSlope);

        float flMinSlope = -0.12f;
        if(g_qos.fNoDownhillMode)
        {
          flMinSlope = 0.0;
        }
        flSlope = max(flMinSlope,flSlope);
		    if(local.pLoadController)
		    {
          // ooo, this guy has a fancy load-controlling trainer!  let's mess with it!
          local.pLoadController->LoadController_SetSlope(flSlope*100, 1 - i->second->GetLastDraft());
          local.pLoadController->LoadController_SetParams(Player::GetCDA(m_ePhysicsMode,80),Player::DEFAULT_CRR,local.flMassKg, rho);
		    }
		  }

      { // let's find this guy's player
        unordered_map<int,boost::shared_ptr<PLAYERDATA> >::const_iterator iPlayer = m_mapPlayers.find(local.idLocal);
        if(iPlayer != m_mapPlayers.end())
        {
          boost::shared_ptr<PLAYERDATA> pd = iPlayer->second;
          if(!pd->Player_IsAI() && IsLocalId(pd->id))
          {
            pd->AddTickData(pd->flTimeSum,&m_map);
          }
        }
      }
	  }

    vector<IConstPlayerPtrConst> lstDraftPlayers;
    for(ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.begin(); i != m_mapPlayers.end(); i++)
    {
      if(IS_FLAG_SET(i->second->GetActionFlags(), ::ACTION_FLAG_IGNOREFORPHYSICS)) continue; // specatator or ghost, ignore for drafting
      if(i->second->GetFinishTime() >= 0) continue; // finished guy, doesn't matter for drafting
      lstDraftPlayers.push_back(i->second->GetConstCopy(tmNow));
    }

    
    PlayerDraftCompare<IPlayerPtrConst> pdc(0);
    sort(lstDraftPlayers.begin(),lstDraftPlayers.end(),pdc);
    
    for(ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.begin(); i != m_mapPlayers.end(); i++)
    {
      if(IS_FLAG_SET(i->second->GetActionFlags(), ::ACTION_FLAG_IGNOREFORPHYSICS)) continue; // specatator or ghost, ignore for physics
      if(i->second->GetFinishTime() >= 0) continue; // finished guy, doesn't matter for physics

      float flNewSpeed = 0;
      float flDistDelta = 0;
      boost::shared_ptr<PLAYERDATA> pd = i->second;

      RunPhysics(m_ePhysicsMode,
                 m_eRaceMode,
                 lstDraftPlayers,
                 i->second.get(),
                 m_map.GetMaxSpeedAtDistance(pd->flPos),
                 m_map.GetSlopeAtDistance(pd->flPos),
                 pd->flPower,
                 pd->GetMassKg(),
                 m_map.GetRhoAtDistance(pd->flPos.flDistance),
                 pd->flSpeed,
                 Player::GetCDA(m_ePhysicsMode,pd->GetMassKg()),
                 Player::DEFAULT_CRR,
                 (float)dt,
                 &flNewSpeed,
                 &flDistDelta,
                 &pd->flLastDraft,
                 &pd->flLastDraftNewtons);

      const int msToStart = -((int)m_tmGameStart - (int)tmNow);
      const float flElapsed = (float)msToStart / 1000.0f;
      SPRINTCLIMBDATA scoreData;
      std::string strScoreDesc;
      pd->Tick(flElapsed, 0, m_ePhysicsMode, m_eRaceMode, vector<IConstPlayerPtrConst>(), DraftMap(), dt, &scoreData,&strScoreDesc,tmNow);
      pd->flSpeed = flNewSpeed;

      const PLAYERDIST distLast = pd->flPos;
      const ORIGDIST origDistLast = m_map.GetOrigDistAtDistance(distLast.flDistance);
      if(pd->flPos <= m_map.GetEndDistance() && this->m_eGameState == RACING)
      {
        pd->flPos.AddMeters(flDistDelta);
      }
      const ORIGDIST origDistNow = m_map.GetOrigDistAtDistance(pd->flPos.flDistance);

      if(pd->GetFinishTime() <= 0)
      {
        pd->flJoulesSpent += pd->flPower*dt;
        pd->flTimeSum += dt;
      }

      DASSERT(!::IsNaN(pd->flPos.flDistance));
      pd->flDrawPos.AddMeters(flDistDelta);
      pd->UpdateHistory(tmNow);
      const float flFinishTime = GetFinishTime(pd->id);
      if(flFinishTime > 0 || 
        (pd->flPos > m_map.GetEndDistance() && IsLappingMode(m_map.GetLaps(),m_map.GetTimedLength()))) // went beyond end of map on a non-timed-race
      {
        pd->flPos = min(pd->flPos,m_map.GetEndDistance());
        pd->flSpeed = 0;
        pd->flDrawPos = pd->flPos;
      }
      else if(pd->flPos > m_map.GetEndDistance() && IsTimedMode(m_map.GetLaps(),m_map.GetTimedLength()))
      {
        m_map.OneMoreLap();
      }

      {
        const double flLaneHoldBack = 0.987; // increase this number to slow down the lane-switch animation
        const float flOldWeight = 1.0 - pow(flLaneHoldBack,dt*1000);
        const float flDrawWeight = pow(flLaneHoldBack,dt*1000);
        pd->flDrawLane = flOldWeight*pd->flLane + flDrawWeight*pd->flDrawLane;
      }
      {
        const double flDirHoldBack = 0.987; // increase this number to slow down the lane-switch animation
        const float flOldWeight = 1.0 - pow(flDirHoldBack,dt*1000);
        const float flDrawWeight = pow(flDirHoldBack,dt*1000);
        DASSERT(!IsNaN(pd->vDrawDir.m_v[0]));

        pd->vDrawDir = GetMapDir(pd->flPos,&m_map)*flOldWeight + pd->vDrawDir*flDrawWeight;
        DASSERT(!IsNaN(pd->vDrawDir.m_v[0]));
      }
      {
        const double flPosHoldBack = 0.9982; // increase this number to slow down the position-switch animation
        const float flOldWeight = 1.0 - pow(flPosHoldBack,dt*1000);
        const float flDrawWeight = pow(flPosHoldBack,dt*1000);
        if(IsNaN(pd->flDrawPos.flDistance))
        {
          DASSERT(!IsNaN(pd->flPos.flDistance));
          pd->flDrawPos = pd->flPos;
        }

        const float flNewDist = flOldWeight*pd->flPos.ToMeters() + flDrawWeight*pd->flDrawPos.ToMeters();
        pd->flDrawPos = PLAYERDIST(0,flNewDist,pd->flPos.flDistPerLap);
      }

      { // update the sprint/climb data approximations (the server is the master here, but we might as well approximate shit)
        vector<SprintClimbPointPtr> lstSources;
        m_map.GetScoringSources(lstSources);
        for(unsigned int x = 0; x < lstSources.size(); x++)
        {
          SPRINTCLIMBDATA score;
          lstSources[x]->GetPointsEarned(origDistLast,origDistNow,distLast.iCurrentLap,pd->flPos.iCurrentLap == m_map.GetLaps()-1,&score,pd->id,flElapsed);
        }
      }
    }
    
  }
}
void TDGGameClient::BuildPeletonList(const vector<IPlayerPtrConst>& lstPlayers, vector<PELETONINFOConstPtr>& lstGroups) const
{
  PELETONINFOPtr curPeleton;
  for(unsigned int x = 0; x < lstPlayers.size(); x++)
  {
    const IPlayer* pCurrent = lstPlayers[x].get();
    if(IS_FLAG_SET(pCurrent->GetActionFlags(),ACTION_FLAG_SPECTATOR) && !IsLocalId(pCurrent->GetId())) 
      continue;
    if(IS_FLAG_SET(pCurrent->GetActionFlags(),ACTION_FLAG_DEAD)) 
      continue;
    if(IS_FLAG_SET(pCurrent->GetActionFlags(),::ACTION_FLAG_GHOST) && !IsLocalId(pCurrent->GetId())) 
      continue;
    if(IS_FLAG_SET(pCurrent->GetActionFlags(),::ACTION_FLAG_DOOMEDAI)) 
      continue;

    if(!curPeleton)
    {
      curPeleton = PELETONINFOPtr(new PELETONINFO(m_map.GetLapLength()));
      curPeleton->Init(pCurrent->GetDistance());
      curPeleton->AddPlayer(pCurrent->GetId(), IsLocalId(pCurrent->GetId()), !pCurrent->Player_IsAI(),pCurrent->GetFinishTime(),pCurrent->GetDistance());
    }
    else
    {
      const IPlayer* pLast = lstPlayers[x-1].get();
      if(pLast->GetDistance().ToMeters() - pCurrent->GetDistance().ToMeters() > 10)
      {
        // pCurrent is too far behind pLast to be getting a draft, so count him as a separate peleton
        lstGroups.push_back(curPeleton);
        curPeleton = PELETONINFOPtr(new PELETONINFO(m_map.GetLapLength()));
        curPeleton->Init(pCurrent->GetDistance());
        curPeleton->AddPlayer(pCurrent->GetId(), IsLocalId(pCurrent->GetId()), !pCurrent->Player_IsAI(),pCurrent->GetFinishTime(),pCurrent->GetDistance());
      }
      else
      {
        // pCurrent is close enough behind pLast to be in pLast's group, so just append him to the current peleton
        curPeleton->AddPlayer(pCurrent->GetId(), IsLocalId(pCurrent->GetId()), !pCurrent->Player_IsAI(),pCurrent->GetFinishTime(),pCurrent->GetDistance());
      }
    }
  }
  if(lstPlayers.size() > 0)
  {
    lstGroups.push_back(curPeleton);
  }
  
#ifdef _DEBUG
  int cPlayersByPeletons = 0;
  for(unsigned int x = 0;x < lstGroups.size(); x++)
  {
    cPlayersByPeletons += lstGroups[x]->GetPlayerCount();
  }
  DASSERT(cPlayersByPeletons == m_mapPlayers.size() && 
          cPlayersByPeletons == lstPlayers.size() && 
          lstPlayers.size() == m_mapPlayers.size());
#endif
}
float TDGGameClient::GetLastPower(int ixLocal) const
{
  return m_lstLocals[ixLocal].m_sLastPower;
}
RECTF BuildViewport(const RECT& rcGameArea, SIZE sScreenSize, int rows, int cols, int player)
{
  const float gameW = rcGameArea.right - rcGameArea.left;
  const float gameH = rcGameArea.bottom - rcGameArea.top;

  const int row = player / cols;
  const int col = player % cols;
  RECT rc;
  rc.left = (col*gameW)/cols + rcGameArea.left;
  rc.right = ((col+1)*gameW)/cols + rcGameArea.left;
  rc.top = (row*gameH)/rows + rcGameArea.top;
  rc.bottom = ((row+1)*gameH)/rows + rcGameArea.top;

  RECTF rcf;
  rcf.left = rc.left / (float)sScreenSize.cx;
  rcf.top = rc.top / (float)sScreenSize.cy;
  rcf.right = (rc.right) / (float)sScreenSize.cx;
  rcf.bottom = (rc.bottom) / (float)sScreenSize.cy;
  return rcf;
}
void TDGGameClient::BuildViewports(const RECT& rcGameArea, SIZE sScreen)
{
  int c = m_lstLocals.size();
  switch(c)
  {
  case 1:
  case 2: // 1/2 players: side by side
    for(int x = 0;x < m_lstLocals.size(); x++)
    {
      m_lstLocals[x].SetViewport(BuildViewport(rcGameArea,sScreen,1,c,x));
    }
    break;
  case 3:
  case 4: // 3/4 players: 4-way split
    for(int x = 0;x < m_lstLocals.size(); x++)
    {
      m_lstLocals[x].SetViewport(BuildViewport(rcGameArea,sScreen,2,2,x));
    }
    break;
  case 5:
  case 6: 
    for(int x = 0;x < m_lstLocals.size(); x++)
    {
      m_lstLocals[x].SetViewport(BuildViewport(rcGameArea,sScreen,2,3,x));
    }
    break;
  case 7:
  case 8:
    for(int x = 0;x < m_lstLocals.size(); x++)
    {
      m_lstLocals[x].SetViewport(BuildViewport(rcGameArea,sScreen,2,4,x));
    }
    break;
  }

}

void TDGGameClient::CheckForDeadPlayers()
{
  vector<int> lstErase;
  for(ARTTYPE unordered_map<int,boost::shared_ptr<PLAYERDATA> >::iterator i = m_mapPlayers.begin(); i != m_mapPlayers.end(); i++)
  {
    if(ArtGetTime() - i->second->tmLastUpdated > 10000 && i->second->tmLastUpdated != 0)
    {
      if(!IsLocalId(i->second->id))
      {
        // this player hasn't been heard from for a while, and isn't local.  Thus, he is dead.
        cout<<"Haven't heard from "<<i->second->id<<" for a while"<<endl;
        lstErase.push_back(i->first);
        continue;
      }
    }
  }

  for(unsigned int x = 0;x < lstErase.size(); x++)
  {
    m_mapPlayers.erase(lstErase[x]);
  }
}

unordered_set<int> GetPortList();

bool IsSetsSame(const unordered_set<int>& setOld, const unordered_set<int>& setNew, unordered_set<int>& setAdded, unordered_set<int>& setRemoved)
{
  if(setOld.size() <= 0 && setAdded.size() <= 0) return true;

  bool fIsSame = true;
  for(unordered_set<int>::const_iterator iNew = setNew.begin(); iNew != setNew.end(); iNew++)
  {
    if(setOld.find(*iNew) == setOld.end())
    {
      // this item wasn't in the old set, so it is new
      setAdded.insert(*iNew);
      fIsSame = false;
    }
    else
    {
      // item was in the old set, so it isn't new
    }
  }
  for(unordered_set<int>::const_iterator iOld = setOld.begin(); iOld != setOld.end(); iOld++)
  {
    if(setNew.find(*iOld) == setNew.end())
    {
      // this item wasn't in the new set, so it has been removed
      setRemoved.insert(*iOld);
      fIsSame = false;
    }
    else
    {
      // item was in the new set, so it is new
    }
  }
  return fIsSame;
}

struct RECHECKDATA
{
  RECHECKDATA(DWORD tmRecheck, const unordered_set<int>& setAdded, const unordered_set<int>& setRemoved) : setAdded(setAdded),setRemoved(setRemoved),tmRecheck(tmRecheck) {}
  DWORD tmRecheck;
  unordered_set<int> setAdded;
  unordered_set<int> setRemoved;
};

extern float g_flPowermeterWheelSpeed;

bool TDGGameClient::DoGamePlay()
{
  DWORD tmDrawSum = 0;
  int cFrames = 0;


  DWORD tmDisplayCheck = ArtGetTime();
#ifdef _WIN32
  SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_CONTINUOUS);
  SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
#else
  UpdateSystemActivity(UsrActivity);
#endif
  
  bool fContinue = true;

  ICommSocketFactoryPtr pSocketFactory(CreateBoostSockets());

  while(fContinue) // game loop that goes from setup to game
  { // lifespan for hdc/hgl stuff

    IWorkoutPtrConst spWorkoutToDo;

    if(fContinue)
    { // lifespan for main game stuff
      TourDeGiroPainter* pPainter = CreatePainter(m_map, NULL);

      vector<const TourDeGiroLocalPlayer*> lstLocals;
      for(unsigned int x = 0; x < m_lstLocals.size(); x++)
      {
        lstLocals.push_back(&m_lstLocals[x]);
      }
  
      TDGPAINTERINITPARAMS params;
      params.pGameLogic = this;
  
      pPainter->Painter_Do3DSetup();
      pPainter->Painter_Init(params,lstLocals);
      try
      {
        pPainter->Painter_Go();
      }
      catch(...)
      {
        // oh shit, a c++ exception
        fContinue = false;
      }
      if(pPainter->Painter_IsQuit())
      {
        fContinue = false;
      }
      FreePainter(pPainter);
    }

    // we need to fully disconnect
    // they quit the game.  Return them to the setup screen
    Disconnect();
    // disconnected.  Get rid of all our other data

    {
      AutoLeaveCS _cs(m_cs);
      for(unsigned int x = 0;x < m_lstLocals.size(); x++)
      {
        unordered_map<int,boost::shared_ptr<PLAYERDATA> >::const_iterator i = m_mapPlayers.find(m_lstLocals[x].idLocal);
        if(i != m_mapPlayers.end() && i->second)
        {
          // found the player
          SavePlayerData(false, 0, NULL,i->second->GetConstDataCopy(0,true));
        }
      }
    }

    if(!fContinue)
    {
      LONG cToGo=0;
      while(IsStillSavingResults(&cToGo))
      {
        cout<<"Still saving results... ("<<cToGo<<" to go)"<<endl;
        ArtSleep(500);
      }
      exit(0);
    }

    memset(rgLastKeyState,0,sizeof(rgLastKeyState));
    m_lstLocals.clear();
    m_mapPlayers.clear();
    m_mapFinishTimes.clear();
    while(m_qNetData.size()) m_qNetData.pop();
  }

  return true;
}
float g_flSpeedCadenceLastSpeed = 0;

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif
void TDGGameClient::GameLoop()
{
  SIZE sScreen;
#ifdef _WIN32
  sScreen.cx = GetSystemMetrics(SM_CXSCREEN);
  sScreen.cy = GetSystemMetrics(SM_CYSCREEN);
#elif defined(__APPLE__)
  CGDirectDisplayID dispId = CGMainDisplayID();
  sScreen.cx = CGDisplayPixelsWide(dispId);
  sScreen.cy = CGDisplayPixelsHigh(dispId) - 50;
#endif

  if(sScreen.cx >= 1920 && sScreen.cy >= 1080)
  {
    sScreen.cx = 1920;
    sScreen.cy = 1080;
  }

  DoGamePlay();
}