#pragma once

class ThreadedRaceResultSaver
{
public:
  ThreadedRaceResultSaver(int iMasterId, boost::shared_ptr<Map> spMap, const RACERESULTS& raceResults, boost::shared_ptr<StatsStore> spStatsStore, RACEENDTYPE eRestartType);
  virtual ~ThreadedRaceResultSaver();

  void DoSave();
  static LONG m_cSaversActive; // counts how many of these are active, so that we can warn the user before they quit
private:

  const int m_iMasterId;
  const string m_strMd5;

  boost::thread m_thd;

  const boost::shared_ptr<Map> m_spMap;
  const RACERESULTS m_raceResults;
  const RACEENDTYPE m_eRestartType;
  boost::shared_ptr<StatsStore> m_spStatsStore;
};

bool IsStillSavingResults(LONG* pcToGo);