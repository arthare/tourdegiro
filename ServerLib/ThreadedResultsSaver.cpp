#include "stdafx.h"
#include "ThreadedResultsSaver.h"

void _DoSave(ThreadedRaceResultSaver* p)
{
  p->DoSave();
}

LONG ThreadedRaceResultSaver::m_cSaversActive = 0;

ThreadedRaceResultSaver::ThreadedRaceResultSaver(int iMasterId, boost::shared_ptr<Map> spMap, const RACERESULTS& raceResults, boost::shared_ptr<StatsStore> spStatsStore, RACEENDTYPE eRestartType) 
  : m_iMasterId(iMasterId),
    m_raceResults(raceResults),
    m_spMap(spMap),
    m_spStatsStore(spStatsStore),
    m_eRestartType(eRestartType)
{
  ArtInterlockedIncrement(&m_cSaversActive);
  m_thd = boost::thread(_DoSave, this);
};
ThreadedRaceResultSaver::~ThreadedRaceResultSaver()
{
  ArtInterlockedDecrement(&m_cSaversActive);
}
bool IsStillSavingResults(LONG* pcToGo)
{
  *pcToGo = ArtInterlockedExchangeAdd(&ThreadedRaceResultSaver::m_cSaversActive,0) > 0;
  return (*pcToGo) > 0;
}
void ThreadedRaceResultSaver::DoSave()
{
  const PLAYERDIST flRaceDist = m_spMap->GetEndDistance().Minus(m_spMap->GetStartDistance());

  float flAISum = 0;
  int cAICount = 0;
  for(int x = 0;x < m_raceResults.GetFinisherCount(); x++)
  {
    const PLAYERRESULTPtr& res = m_raceResults.lstResults[x];
    if(res->fIsAI && res->flDistanceRidden >= flRaceDist)
    {
      flAISum += res->flFinishTime;
      cAICount++;
    }
  }
  const float flRaceAvgTime = cAICount > 0 ? flAISum / (float)cAICount : 0;

  // we've got at least one finisher, so let's store results!
  int iCurrentRaceId = 0;

  RACEMODE eRaceMode;
  int timedLengthSeconds=0;
  int iMapId=0;
  int iSchedId=0;
  int startpct=0;
  int endpct=100;
  bool fSinglePlayer=false;
  PHYSICSMODE ePhysicsMode;
  m_raceResults.GetInfo(&eRaceMode,&timedLengthSeconds,&iMapId,&iSchedId,&startpct,&endpct,&fSinglePlayer,&ePhysicsMode);

  m_spStatsStore->NotifyStartOfRace(&iCurrentRaceId,eRaceMode,flRaceDist,timedLengthSeconds,iMapId, iSchedId,flRaceAvgTime, startpct,endpct, fSinglePlayer, ePhysicsMode,-1);
  cout<<"restarting: raceid = "<<iCurrentRaceId<<endl;
  DASSERT(iCurrentRaceId > 0);

  unordered_map<int,int> mapRaceEntryIds;
  unordered_map<int,DWORD> mapRecordTimes; // we may need these to authorize race-replay saving
  m_spStatsStore->NotifyEndOfRace(iCurrentRaceId,iMapId,m_eRestartType,m_raceResults,mapRaceEntryIds, mapRecordTimes);


  for(vector<PLAYERRESULTPtr>::const_iterator i = m_raceResults.lstResults.begin(); i != m_raceResults.lstResults.end(); i++)
  {
    if(mapRaceEntryIds.find((*i)->id) != mapRaceEntryIds.end() && mapRecordTimes.find((*i)->id) != mapRecordTimes.end())
    {
      RACEREPLAYAUTH auth;
      auth.iMasterId = m_iMasterId;
      auth.strMd5 = m_strMd5;
      auth.recordtime = mapRecordTimes[(*i)->id];

      m_spStatsStore->StoreRaceReplay(mapRaceEntryIds[(*i)->id],(*i)->fIsAI,&auth,&(*i)->m_lstHistory);
    }
  }


  // all done!
  delete this;
}