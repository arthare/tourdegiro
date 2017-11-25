#pragma once
#include "CommStructs.h"

extern float ASSUMED_AVERAGE_POWER; // for players where we don't have knowledge of their historic power, just assume 150

using namespace std;

class IPlayer;

// how did the race end?
enum RACEENDTYPE
{
  RACEEND_RESTART, // someone voted to restart
  RACEEND_FINISH, // someone crossed the finish line
  RACEEND_DISCONNECT, // buddy disconnected
  RACEEND_NOHUMANS, // buddy disconnected
  RACEEND_SCHEDULE, // a scheduled race came up
};

struct RACEINFO
{
  int id;
  int iMapId;
  float flLength;
  int iStartTime;
  string strMapName;
};

// in/out struct for specifying maps to be loaded and the result once they are processed
struct PROCESSMAP
{
  PROCESSMAP() : iOwnerId(-1),iMapId(-1),id(-1) {};

  std::string strFile;
  std::string strUserName; // what name did the user give this map?
  int iOwnerId;
  int iMapId;
  int id;
};

class StatsStore;

extern int FAKEWORKOUTMAP; // the id of the "workout" map

struct SCHEDULEDRACE
{
  SCHEDULEDRACE() :
    scheduleId(-1),
    serverId(-1),
    ownerId(-1),
    fdwPermittedDevices(0),
    tmStart(-1),
    fRepeat(false),
    iKm(0),
    cAIs(0),
    iAIMinStrength(150),
    iAIMaxStrength(250),
    iStartPercent(0),
    iEndPercent(100),
    iMapId(-1),
    laps(-1),
    ePhysicsMode(GetDefaultPhysicsMode()),
    timedLengthSeconds(-1)
  {

  }
  SCHEDULEDRACE(const MAPREQUEST& mapReq, int iMasterId, int iServerId, StatsStore* pStats);

  bool IsSame(const SCHEDULEDRACE& other) const
  {
    return laps == other.laps && timedLengthSeconds == other.timedLengthSeconds && iMapId == other.iMapId && iKm == other.iKm && tmStart == other.tmStart && scheduleId == other.scheduleId && serverId == other.serverId && cAIs == other.cAIs && iStartPercent == other.iStartPercent && iEndPercent == other.iEndPercent;
  }
  static unsigned int BuildPermittedDevicesFlags(bool fPermitCT, bool fPermitPM, bool fPermitSC, bool fPermitCH, bool fPermitKickr)
  {
    return (fPermitPM ? (1<<ANT_POWER) : 0) |
           (fPermitCT ? (1<<COMPUTRAINER) : 0) |
           (fPermitSC ? (1<<ANT_SPEED) : 0) |
           (fPermitCH ? (1<<CHEATING) : 0) |
           (fPermitKickr ? (1<<WAHOO_KICKR) : 0);
  }
  bool DoesPermitDevice(POWERTYPE eType) const
  {
    return ((fdwPermittedDevices & (1<<eType)) != 0) || eType == SPECTATING;
  }
  bool IsValid() const
  {
    return serverId > 0 && iMapId >= 1 && strRaceName.length() > 0 && iKm > 0 && (IsTimedMode(laps,timedLengthSeconds) ^ IsLappingMode(laps,timedLengthSeconds));
  }
  int scheduleId; // what is the scheduleId for this? (lets the server distinguish between events)
  int ownerId; // what is the masterid of the guy that owns this? (lets the server know who is allowed force-starting or delaying a race)
  int serverId; // what serverport should run this?  if a server gets a SCHEDULEDRACE object whose serverId doesn't match, then
  int iKm; // what is the length of a single lap?
  unsigned int fdwPermittedDevices;
  DWORD tmStart; // in unix time
  string strRaceName; // friendly name: like "TdF Stage 3 2013" - used to send to 
  string strMapName; // name of the map
  bool fRepeat;
  int cAIs;
  int iAIMinStrength;
  int iAIMaxStrength;
  int iStartPercent; // value between 0 and 100 indicating the %-of-map-distance that the user wants their race to start at.  must be less than iEndPercent
  int iEndPercent; // value between 0 and 100 indicating the %-of-map-distance that the user wants their race to end at.  must be greater than iStartPercent
  int iMapId;
  PHYSICSMODE ePhysicsMode;
  int laps; // -1: 1 lap or timed mode.
  int timedLengthSeconds; // -1: non-timed mode.  >0: timed mode for that many seconds
};


enum SCENERYTYPE
{
  SCENERY_CITY,
  SCENERY_CITYBUILDING,
  SCENERY_FIELD,
  SCENERY_FOREST,
  SCENERY_GENERAL,
  SCENERY_LAKE,

  SCENERY_UNKNOWN,
};

struct LANDMARK
{
  LANDMARK() : mapid(-1),eType(SCENERY_UNKNOWN),lat1(0),lat2(0),param3(0),param4(0),param5(0),param6(0),param7(0),param8(0),lon1(0),lon2(0),str1(""),str2("")
  {

  }

  void SetCity(int mapid, float flLat, float flLon, float flRad, string strName)
  {
    this->mapid = mapid;
    eType = SCENERY_CITY;
    lat1 = flLat;
    lon1 = flLon;
    param3 = flRad;
    str1 = strName;
  }
  void SetLake(int mapid, float flLat, float flLon, float flRad, float flElev)
  {
    this->mapid = mapid;
    eType = SCENERY_LAKE;
    lat1 = flLat;
    lon1 = flLon;
    param3 = flRad;
    param4 = flElev;
  }

  int mapid;
  SCENERYTYPE eType;
  float lat1;
  float lat2;
  float param3;
  float param4;
  int param5;
  int param6;
  int param7;
  int param8;
  float lon1;
  float lon2;
  string str1;
  string str2;
};

struct PLAYERRESULT
{
  PLAYERRESULT(const vector<LAPDATA>& lstLapTimes, int id,int iFinishPos,float flFinishTime,const PLAYERDIST& flDistanceRidden,float flAvgPower,const string& strName,const PLAYERDIST& flFinishDistance,float flWeight,unsigned int ip,RACEENDTYPE eReason, float flReplayOffset, const std::vector<RECORDEDDATA>& lstHistory, bool fIsAI, POWERTYPE ePowerType, int iPowerSubType, const std::vector<SPRINTCLIMBDATA>& lstScoreData, int teamId);
  void SetStat(STATID eStat, float value)
  {
    m_mapStatValues[eStat] = value;
  }
  vector<LAPDATA> lstLapTimes;
  int id;
  int iFinishPos;
  float flFinishTime; // note: this will be positive even if they didn't finish
  PLAYERDIST flDistanceRidden; // how far they rode in this race
  float flAvgPower;
  string strName;
  PLAYERDIST flFinishDistance; // where they were upon finishing
  float flWeight;
  unsigned int ip; // what was this guy's IP?
  RACEENDTYPE eReason; // how did this player finish?  finish, restart, disconect, etc
  float flReplayOffset;
  std::vector<RECORDEDDATA> m_lstHistory;
  map<STATID,float> m_mapStatValues;
  bool fIsAI;
  POWERTYPE ePowerType;
  int iPowerSubType;
  std::vector<SPRINTCLIMBDATA> lstScoreData; // what points did this player score?
  int teamId; // what team was this guy riding on when he did that?

  void BuildAvgPowers(); // gets this result to self-analyze and insert needed stats
};
typedef boost::shared_ptr<PLAYERRESULT> PLAYERRESULTPtr;

struct PLAYERINFO
{
  int id;
  int tmLastSeen;
  string strName;
  float flAvgPower;
};

struct RACERESULTS
{
  RACERESULTS() : m_fSetOnce(false) {};

  vector<PLAYERRESULTPtr > lstResults;

  bool IsFinisher(int id)
  {
    for(unsigned int x = 0; x < lstResults.size(); x++)
    {
      if(lstResults[x]->id == id) return true;
    }
    return false;
  }
  void Clear()
  {
    m_fSetOnce = false;
    lstResults.clear();
  }
  void AddFinisher(const PLAYERRESULT& result)
  {
    lstResults.push_back(PLAYERRESULTPtr(new PLAYERRESULT(result)));
  }
  void AddFinisher(PLAYERRESULT& result, const unordered_map<int,IPlayerPtrConst>& mapPlayers, const PLAYERDIST& flRaceDist)
  {
    unordered_map<int,IPlayerPtrConst>::const_iterator i = mapPlayers.find(result.id);
    if(i != mapPlayers.end())
    {
      IPlayerPtrConst pPlayer = i->second;
      BuildStat(result,pPlayer, flRaceDist, mapPlayers);
    }
    result.BuildAvgPowers();

    lstResults.push_back(PLAYERRESULTPtr(new PLAYERRESULT(result)));
  }
  void BuildStat(PLAYERRESULT& result, IPlayerPtrConst pPlayer, const PLAYERDIST& flRaceDist, const unordered_map<int,IPlayerPtrConst>& mapPlayers)
  {
    float flDraftPct = 0;
    if(pPlayer->GetStat(STATID_DRAFTPCT,&flDraftPct))
    {
      result.SetStat(STATID_DRAFTPCT,flDraftPct);
    }
  }
  int GetFinisherCount() const 
  {
    int c = 0;
    for(unsigned int x = 0;x < lstResults.size(); x++)
    {
      if(lstResults[x]->iFinishPos >= 0)
      {
        c++;
      }
    }
    return c;
  }

  void SetInfo(const RACEMODE eRaceMode, const int timedLengthSeconds, const int iMapId, const int iSchedId, const int startpct, const int endpct, const bool fSinglePlayer, PHYSICSMODE ePhysicsMode)
  {
    m_eRaceMode = eRaceMode;
    m_timedLengthSeconds = timedLengthSeconds;
    m_iMapId = iMapId;
    m_iSchedId = iSchedId;
    m_startpct = startpct;
    m_endpct = endpct;
    m_fSinglePlayer = fSinglePlayer;
    m_ePhysicsMode = ePhysicsMode;

    m_fSetOnce = true;
  }
  void GetInfo(RACEMODE* eRaceMode, int* timedLengthSeconds, int* iMapId, int* iSchedId, int* startpct, int* endpct, bool* pfSinglePlayer, PHYSICSMODE* ePhysicsMode) const
  {
    DASSERT(m_fSetOnce); // we better have set data once since the last clear!
    *eRaceMode = m_eRaceMode;
    *timedLengthSeconds = m_timedLengthSeconds;
    *iMapId = m_iMapId;
    *iSchedId = m_iSchedId;
    *startpct = m_startpct;
    *endpct = m_endpct;
    *pfSinglePlayer = m_fSinglePlayer;
    *ePhysicsMode = m_ePhysicsMode;
  }
private:
  bool m_fSetOnce;
  RACEMODE m_eRaceMode;
  int m_timedLengthSeconds;
  int m_iMapId;
  int m_iSchedId;
  int m_startpct;
  int m_endpct;
  bool m_fSinglePlayer;
  PHYSICSMODE m_ePhysicsMode;
};

struct SRTMCELL;

// SRTMSource: If you've got a database of elevation points, you can make more-realistic maps by implementing this interface and passing it to your server on startup.
//             In TdG's heyday, the 3D renderer would take these into account when building the terrain.
//             Tip: Named after the Shuttle Radar Topography Mission, in which the space shuttle developed a very consistent, very high-quality elevation map that is now available for free.
//             Tip: Storing the entire set of SRTM data takes a LOT of space, so TdG only ever used it on very popular maps.  There may be better way forward.
class SRTMSource
{
public:
  virtual void GetPoints(float flMinLat, float flMinLon, float flMaxLat, float flMaxLon, int limit, std::vector<SRTMCELL>& lstSRTM) = 0;
};


struct STATSSTORECREDS
{
  std::string strDest;
  std::string strUser;
  std::string strPass;
  std::string strSchema;
};

struct RACEREPLAYAUTH
{
  int iMasterId; // the masterid of the guy doing the uploading
  DWORD recordtime; // the record time for the raceentry we're trying to save data for
  string strMd5; // user's password hash
};

class StatsStore
{
public:
  virtual bool Load(string& strErr) = 0; // loads from this stat store's local file
  
  // data access

  // GetPlayerId: Given a masterId and a name string, this will return: stats, account parameters (stealth mode), team numbers, and most importantly the rider's DB ID.
  // ex: GetPlayerId(masterId=1, strName="art") might return id 1234
  // ex: GetPlayerId(masterId=1, strName="eric") might return 4321
  // ex: GetPlayerId(masterId=2, strName="art") might return 3141.  It's the same rider name,but different account id.
  // iDefaultTeamId: if this person doesn't exist and thus already have a teamID, what team to put the person on?
  virtual int GetPlayerId(int iMasterId, int iDefaultTeamId, string strName, bool fIsAI, const IMap* pMapToRideOn, PERSONBESTSTATS* pflHistoricAvgPower, bool* pfIsStealth, int* piTeamNumber, int iSecondsSince1970) = 0;

  // lol md5 hashes for validating logins.  Given a username and a password hash, this will return the masterid of a given rider.
  virtual bool ValidateLogin(const char* pszUsername, const MD5& pszMd5, int* pidMaster, int* piDefaultTeamId, bool* pfExpired) = 0;

  // loads a "ghost" of a past TimeTrial effort, and returns it as a fake AI player whose position at any given time is equivalent to the ghost's position.  Totally optional to implement, obviously.
  virtual void LoadTTGhost(IPlayerPtrConst pOwner, int raceid, IPlayer** ppOut) {};

  // TT mode: gets past race results.  Used to populate the "past riders" box.
  virtual void GetRaceResults(int iRaceId, RACERESULTS* pResults) {};

  // Gets the actual route for a given map ID.  In the November-2017 OSS release, this isn't used, but I highly recommend someone implement a Sqlite or web-back-end implementation.
  virtual bool GetMapRoute(int iMapId, vector<MAPPOINT>& lstPoints, int iStartPct, int iEndPct) = 0;

  // Gets map name, original filename, ownerID (the account ID of the person that uploaded it), and some flags about how we should treat the map
  virtual void GetMapData(int iMapId, std::string* pstrMapName, std::string* pstrMapFilename, int* piMapOwner, MAPCAP* pMapCap) = 0;

  // asks what this server's next scheduled race is.  OSS Note: While TdG was a business, scheduled races would get slotted on a given server.  So you might have "Jimbob's 40km Tuesday Ride" scheduled for serverid 36 at 7pm.
  // This function would ask "Hi Database, I'm serverID 36.  Do I need to switch to a specific map at this particular time?".  If it was getting close to 7pm and the current binary was serverId 36, the answer would be yes, indicated
  // by the contents of the SCHEDULEDRACE object.
  virtual void GetNextScheduledRace(int iServerId, SCHEDULEDRACE* pRace) = 0;

  // data modification:
  // NotifyStartOfRace: Tells the database that a race has started.
  //                    Returns: piRaceId - a database ID for the race that just started, so we can associate other data with it later.
  //                    Takes: A variety of bits of info about the race.
  virtual void NotifyStartOfRace(int* piRaceId, RACEMODE eRaceMode, const PLAYERDIST& rideDist, int timedLengthSeconds, int iMapId, int iSchedId, float flAvgAITime, int startpct, int endpct, bool fSinglePlayer, PHYSICSMODE ePhysics, int iWorkoutId) = 0;

  // AddMap:
  //      Tells the database to give us an ID for the map described by the IMap interface we pass in.
  //      Returns: piMapId, a database ID for the current map.
  virtual void AddMap(int* piMapId, MAPSTATS* pflAverageRaceTime, const IMap* map) = 0;
  
  // NotifyEndOfRace
  // iRaceId: the race id that just ended
  // iMapId: the map it ran on (redundant)
  // eEndType: how the race ended (restart, humans quitting, actual finishing)
  // lstFinishOrder: all the results
  // mapEntryIds: an out parameter that tells you the raceentry ids for each player.  Maps person.id -> raceentry.id
  virtual void NotifyEndOfRace(int iRaceId, int iMapId, RACEENDTYPE eEndType, const RACERESULTS& lstFinishOrder, unordered_map<int,int>& mapEntryIds, unordered_map<int,DWORD>& mapEntryRecordTimes) = 0; // tells the stats store that the race has ended.

  // StoreRaceResult - store's a particular rider's race result
  // iRaceId: Your raceID that you retrieved from NotifyStartOfRace()
  // iMapId: The map you're riding on (you got this from calling AddMap())
  // eEndType: Why'd this player's race end? Did it finish? Did it disconnect?  Did the server restart?
  // result: Tell the database about this player's race effort
  virtual void StoreRaceResult(int iRaceId, int iMapId, RACEENDTYPE eEndType, const PLAYERRESULTPtr& result, int* pidRaceEntry, DWORD* ptmRecordTime) = 0;

  // StoreRaceReplay - store's the second-by-second wattage/position/etc data.  Little-known fact: TdG's website had a hidden capability to view full replays of races.  But it was super-ugly because at the time Art sucked at both graphics AND web programming.
  virtual void StoreRaceReplay(int raceEntryId, bool fIsAI, const RACEREPLAYAUTH* auth, const vector<RECORDEDDATA>* plstHistory) = 0;

  // RecordStat: records a particular statistic about this player's race.
  //    raceEntryId: The raceEntryId you got from StoreRaceResult()
  //    value: The value for this statistic
  //    eStatId: what statistic are we recording?
  //    flWeightTime: I forget
  //    flDistWeight: I forget
  virtual void RecordStat(int raceEntryId, float value, STATID eStatId, float flWeightTime, float flDistWeight) = 0;

  // the server dumps data to an in-DB log.  We used this to keep tabs on server health and race status, but may be less useful in the OSS future.
  virtual void AddAction(std::string strAction, std::string source) = 0;
};
typedef boost::shared_ptr<StatsStore> StatsStorePtr;

class NullSRTMSource : public SRTMSource
{
  virtual void GetPoints(float flMinLat, float flMinLon, float flMaxLat, float flMaxLon, int limit, std::vector<SRTMCELL>& lstSRTM)
  {
  }
};

class NullStatsStore : public StatsStore
{
public:
  NullStatsStore() : m_iMapId(1),m_iRaceId(1),m_iNextPlayer(1) {};
  virtual ~NullStatsStore() {};

  virtual bool Load(string& strErr) ARTOVERRIDE;
  
  // data access
  virtual int GetPlayerId(int iMasterId, int iDefaultTeamId, string strName, bool fIsAI, const IMap* pMapToRideOn, PERSONBESTSTATS* pflHistoricAvgPower, bool* pfIsStealth, int* piTeamNumber, int iSecondsSince1970) ARTOVERRIDE;
  virtual void GetNextScheduledRace(int iServerId,SCHEDULEDRACE * pRace) ARTOVERRIDE;
  virtual bool GetMapRoute(int iMapId, vector<MAPPOINT>& lstPoints, int iStartPct, int iEndPct) ARTOVERRIDE;
  virtual void GetMapData(int iMapId, std::string* pstrMapName, std::string* pstrMapFilename, int* piMapOwner, MAPCAP* pMapCap) ARTOVERRIDE;
  virtual void NotifyStartOfRace(int* piRaceId, RACEMODE eRaceMode, const PLAYERDIST& mapDist, int timedLengthSeconds, int iMapId, int iSchedId, float flAvgAITime, int startpct, int endpct, bool fSinglePlayer, PHYSICSMODE ePhysics, int iWorkoutId) ARTOVERRIDE;
  virtual void AddMap(int* piMapId, MAPSTATS* pMapStats, const IMap* map) ARTOVERRIDE;
  virtual void CheckTrialStart(const IPlayer* pGuy);
  virtual void NotifyEndOfRace(int iRaceId, int iMapId, RACEENDTYPE eEndType, const RACERESULTS& lstFinishOrder, unordered_map<int,int>& mapEntryIds, unordered_map<int,DWORD>& mapRecordTimes) ARTOVERRIDE;
  virtual void StoreRaceResult(int iRaceId, int iMapId, RACEENDTYPE eEndType, const PLAYERRESULTPtr& result, int* pidRaceEntry, DWORD* ptmRecordTime) ARTOVERRIDE;
  virtual bool ValidateLogin(const char* pszUsername, const MD5& pszMd5, int* pidMaster, int* piDefaultTeamId, bool* pfExpired) ARTOVERRIDE;
  virtual void StoreRaceReplay(int raceEntryId, bool fIsAI, const RACEREPLAYAUTH* auth, const vector<RECORDEDDATA>* plstHistory) ARTOVERRIDE;
  virtual void RecordStat(int raceEntryId, float value, STATID eStatId, float flWeightTime, float flDistWeight);
  virtual void AddAction(std::string strAction, std::string source) ARTOVERRIDE;
private:
  int m_iMapId;
  int m_iRaceId;
  int m_iNextPlayer;
  map<string,int> m_mapPlayers; // string->id mappings
};

bool CreateMySQLStatStore(const STATSSTORECREDS& dbData, StatsStore** ppStats, SRTMSource** ppSRTM);
