
#include "stdafx.h"
#include "StatsStore.h"
#include <set>
using namespace std;

int FAKEWORKOUTMAP = 21006; // this is the id in the DB of the fake workout map

SCHEDULEDRACE::SCHEDULEDRACE(const MAPREQUEST& mapReq, int iMasterId, int iServerId, StatsStore* pStats) :
  scheduleId(-1),
  ownerId(iMasterId),
  serverId(iServerId),
  iKm(mapReq.iMeters/1000),
  fdwPermittedDevices(0xffffffff),
  tmStart(::GetSecondsSince1970GMT()+mapReq.iSecondsToDelay),
  fRepeat(false),
  cAIs(mapReq.cAIs),
  iAIMinStrength(mapReq.iAIMinStrength),
  iAIMaxStrength(mapReq.iAIMaxStrength),
  iStartPercent(mapReq.cPercentStart),
  iEndPercent(mapReq.cPercentEnd),
  iMapId(mapReq.iMapId),
  ePhysicsMode(GetDefaultPhysicsMode()),
  laps(mapReq.laps),
  timedLengthSeconds(mapReq.timedLengthSeconds)
{
  string strMapFileName;
  int iMapOwner = 0; // we don't care who owns the map at this point
  MAPCAP mapCap=0;
  pStats->GetMapData(mapReq.iMapId,&strMapName,&strMapFileName,&iMapOwner,&mapCap);

  cout<<"Build a scheduledrace for "<<strMapName<<"/"<<strMapFileName<<endl;
  strRaceName = "A map request";
}


PLAYERRESULT::PLAYERRESULT(const vector<LAPDATA>& lstLapTimes, int id,int iFinishPos,float flFinishTime,const PLAYERDIST& flDistanceRidden,float flAvgPower,const string& strName,const PLAYERDIST& flFinishDistance,float flWeight,unsigned int ip,RACEENDTYPE eReason, float flReplayOffset, const std::vector<RECORDEDDATA>& lstHistory, bool fIsAI, POWERTYPE ePowerType, int iPowerSubType, const std::vector<SPRINTCLIMBDATA>& lstScoreData, int teamId)
  :lstLapTimes(lstLapTimes),
    id(id),
    iFinishPos(iFinishPos),
    flFinishTime(flFinishTime),
    flDistanceRidden(flDistanceRidden),
    flAvgPower(flAvgPower),
    strName(strName),
    flFinishDistance(flFinishDistance),
    flWeight(flWeight),
    ip(ip),
    eReason(eReason),
    flReplayOffset(flReplayOffset),
    m_lstHistory(lstHistory),
    fIsAI(fIsAI),
    ePowerType(ePowerType),
    iPowerSubType(iPowerSubType),
    lstScoreData(lstScoreData),
    teamId(teamId)
{
  cout<<"Constructed playerresult for "<<id<<" who rode "<<flDistanceRidden<<" and ended for reason "<<eReason<<" and had "<<m_lstHistory.size()<<" samples of history"<<endl;
}

typedef boost::shared_ptr<TimeAverager> TimeAveragerPtr;
void PLAYERRESULT::BuildAvgPowers() // gets this result to self-analyze and insert needed stats
{
  {
    set<TimeAveragerPtr > setTimers;
    if(!fIsAI)
    {
      for(int x = STATID_10S_AVG; x <= STATID_120MIN_AVG; x++)
      {
        setTimers.insert(TimeAveragerPtr(new TimeAverager(StatIdToSeconds((STATID)x))));
      }
    }
    for(unsigned int x = 0;x < m_lstHistory.size(); x++)
    {
      for(ARTTYPE set<TimeAveragerPtr >::const_iterator i = setTimers.begin(); i != setTimers.end(); i++)
      {
        TimeAveragerPtr p = *i;
        p->AddData(m_lstHistory[x].flTime,m_lstHistory[x].power);
      }
    }


    for(set<TimeAveragerPtr >::const_iterator i = setTimers.begin(); i != setTimers.end(); i++)
    {
      TimeAveragerPtr p = *i;
      if(p->IsFull())
      {
        // this time averager has actual data for us!
        STATID eStatId = (STATID)-1;
        switch((int)(p->GetSpan()+0.5))
        {
        case 10: eStatId = STATID_10S_AVG; break;
        case 30: eStatId = STATID_30S_AVG; break;
        case 60: eStatId = STATID_60S_AVG; break;
        case 120: eStatId = STATID_2MIN_AVG; break;
        case 300: eStatId = STATID_5MIN_AVG; break;
        case 600: eStatId = STATID_10MIN_AVG; break;
        case 1200: eStatId = STATID_20MIN_AVG; break;
        case 1800: eStatId = STATID_30MIN_AVG; break;
        case 2700: eStatId = STATID_45MIN_AVG; break;
        case 3600: eStatId = STATID_60MIN_AVG; break;
        case 7200: eStatId = STATID_120MIN_AVG; break;
        }
        if(eStatId >= STATID_10S_AVG && eStatId <= STATID_TIMEAVG_RESERVED8)
        {
          cout<<"player "<<id<<" had avg power "<<p->GetBestAverage()<<"W for stat "<<eStatId<<endl;
          SetStat(eStatId, p->GetBestAverage());
        }
      }
    }
  } // done building average powers
}

bool NullStatsStore::Load(string& strErr)
{
  return true;
}
  
// data access
int NullStatsStore::GetPlayerId(int iMasterId, int iDefaultTeamId, string strName, bool fIsAI, const IMap* pMapToRideOn, PERSONBESTSTATS* pflHistoricAvgPower, bool* pfIsStealth, int* piTeamNumber, int iSecondsSince1970)
{
  if(m_mapPlayers.find(strName) != m_mapPlayers.end())
  {
    return m_mapPlayers[strName];
  }
  m_iNextPlayer++;
  m_mapPlayers[strName] = m_iNextPlayer;
  if(piTeamNumber)
  {
    *piTeamNumber = iDefaultTeamId;
  }
  return m_iNextPlayer;
}
void NullStatsStore::GetNextScheduledRace(int iServerId,SCHEDULEDRACE * pRace)
{
  pRace->serverId =0;
  pRace->iMapId = 0;
  pRace->strRaceName = "";
  pRace->iKm = 0;
}
bool NullStatsStore::GetMapRoute(int iMapId, vector<MAPPOINT>& lstPoints, int iStartPct, int iEndPct)
{
  DASSERT(FALSE);
  return false;
}
void NullStatsStore::GetMapData(int iMapId, std::string* pstrMapName, std::string* pstrMapFilename, int* piMapOwner, MAPCAP* pMapCap)
{
  *pstrMapName = "Fake Map";
  *pstrMapFilename = "8km-upanddown.gpx";
  *pMapCap = 0;
}
// data modification:
void NullStatsStore::NotifyStartOfRace(int* piRaceId, RACEMODE eRaceMode, const PLAYERDIST& mapDist, int timedLengthSeconds, int iMapId, int iSchedId, float flAvgAITime, int startpct, int endpct, bool fSinglePlayer, PHYSICSMODE ePhysics, int iWorkoutId)
{
  *piRaceId = ++m_iRaceId;
}
void NullStatsStore::AddMap(int* piMapId, MAPSTATS* pMapStats, const IMap* map)
{
  const float flLength = map->GetLapLength()*map->GetLaps();
  pMapStats->flAverageRaceTime = flLength / 10; // just assume 10m/s
  *piMapId = ++m_iMapId;
}
void NullStatsStore::CheckTrialStart(const IPlayer* pGuy)
{

}
void NullStatsStore::NotifyEndOfRace(int iRaceId, int iMapId, RACEENDTYPE eEndType, const RACERESULTS& lstFinishOrder, unordered_map<int,int>& mapEntryIds, unordered_map<int,DWORD>& mapRecordTimes)
{
}
void NullStatsStore::StoreRaceResult(int iRaceId, int iMapId, RACEENDTYPE eEndType, const PLAYERRESULTPtr& result, int* pidRaceEntry, DWORD* ptmRecordTime)
{
}
bool NullStatsStore::ValidateLogin(const char* pszUsername, const MD5& pszMd5, int* pidMaster, int* piDefaultTeamId, bool* pfExpired)
{
  static int s = 0;
  s++;
  *pidMaster = s;
  *piDefaultTeamId = 2;

  return true;
}
void NullStatsStore::StoreRaceReplay(int raceEntryId, bool fIsAI, const RACEREPLAYAUTH* auth, const vector<RECORDEDDATA>* plstHistory)
{
  //cout<<"Storing something with "<<plstHistory->size()<<" history points"<<endl;
}
void NullStatsStore::RecordStat(int raceEntryId, float value, STATID eStatId, float flWeightTime, float flDistWeight)
{
}
void NullStatsStore::AddAction(std::string strAction, std::string source)
{
  cout<<"Action: "<<strAction<<endl;
}