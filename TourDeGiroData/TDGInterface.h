#pragma once
#include <string>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <stdio.h>
#include <list>
#include <algorithm> // std::max
#include <math.h> // pow()

class IPlayer;
class IConstPlayer;

#pragma warning(disable:4996)
#pragma warning(disable:4005)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

//#define LOCALDB // forces the server components to use 127.0.0.1 for mysql

#ifdef _WIN32
#define ARTOVERRIDE override
#else
#define ARTOVERRIDE
#endif

#ifndef ArtMax
#define ArtMax(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef ArtMin
#define ArtMin(x,y) ((x) > (y) ? (y) : (x))
#endif

enum SPEEDPREF
{
  SPEEDPREF_FIRST,
  METRIC = SPEEDPREF_FIRST,
  IMPERIAL,

  SPEEDPREF_COUNT,
};
struct TRAINER_PARAMS
{
  TRAINER_PARAMS()
  {
    a = 0;
    b = (5.244820f);
    c = 0;
    d = 0.01968f;
  }
  float a,b,c,d; // coefficients for Watts = a + bx + cx^2 + dx^3
};

enum POWERTYPE
{
  POWERTYPE_START = 1,
  ANT_POWER = POWERTYPE_START,
  ANT_SPEED = 2,
  COMPUTRAINER = 3,
  CHEATING = 4,
  SPECTATING = 5,
  AI_POWER = 6,
  WAHOO_KICKR = 7,
  POWERCURVE=8,
  ANT_SPEEDONLY=9,
  COMPUTRAINER_ERG=10,
  ANT_FEC=11,
  PT_UNKNOWN,
};

bool IsComputrainer(POWERTYPE eType);


// interface for a load-controller.  Will be fed back into golden cheetah
class ITrainerLoadController
{
public:
  virtual void LoadController_SetSlope(float slopeInPercent, float windFraction) = 0;
  virtual void LoadController_SetParams(float cda, float crr, float massKg, float rho) = 0;
};

template<class type>
struct TRAINERDATA
{
  type eType;
  std::string strName;

  TRAINERDATA()
  {
  }
  TRAINERDATA(type num, std::string name) : eType(num),strName(name) {};

  bool operator < (const TRAINERDATA<type>& td) const
  {
    return strName < td.strName;
  }
};

struct DRAFTINGINFO
{
  bool fIsSameTeam; // is this guy drafting anyone on his team?
};

typedef boost::shared_ptr<const IPlayer> IPlayerPtrConst;
typedef boost::shared_ptr<IPlayer> IPlayerPtr;
typedef boost::shared_ptr<IConstPlayer> IConstPlayerPtr;
typedef boost::shared_ptr<const IConstPlayer> IConstPlayerPtrConst;

typedef std::map<IConstPlayerPtrConst, DRAFTINGINFO> DraftMap;
enum RACEMODE
{
  RACEMODE_ROADRACE=0,
  RACEMODE_TIMETRIAL=1,
  RACEMODE_WORKOUT=2,
};

struct PLAYERDIST
{
  PLAYERDIST(int laps, float flDistInLap, float flDistPerLap);

  bool operator < (const PLAYERDIST& dist) const;
  bool operator > (const PLAYERDIST& dist) const;
  bool operator >= (const PLAYERDIST& dist) const;
  bool operator <= (const PLAYERDIST& dist) const;
  bool operator != (const PLAYERDIST& dist) const;
  float operator / (const PLAYERDIST& dist) const;
  const PLAYERDIST& operator = (const PLAYERDIST& src);
  bool IsValid() const
  {
    return iCurrentLap >= 0 && flDistPerLap > 0;
  }
  float ToMeters() const
  {
    return flDistPerLap*iCurrentLap + flDistance;
  }
  void AddMeters(float flMeters);
  PLAYERDIST CalcAddMeters(float dist) const
  {
    PLAYERDIST ret = *this;
    ret.AddMeters(dist);
    return ret;
  }
  PLAYERDIST operator - () const
  {
    PLAYERDIST ret = *this;
    ret.AddMeters(-ToMeters()*2);
    return ret;
  }
  PLAYERDIST Minus (const PLAYERDIST& other) const
  {
    // on a 10km circuit:
    // (5 laps, 2000m) minus (2 laps, 1000) is (3 laps, 1000m)
    // (5 laps, 2000m) minus (2 laps, 2500m) is (2 laps, 9500m)
    float flDist = flDistance - other.flDistance;
    int laps = iCurrentLap - other.iCurrentLap;
    while(flDist < 0)
    {
      flDist += flDistPerLap;
      laps--;
    }
    while(flDist > flDistPerLap)
    {
      flDist -= flDistPerLap;
      laps++;
    }
    return PLAYERDIST(laps, flDist, flDistPerLap);
  }


  float flDistance;
  int iCurrentLap;
  float flDistPerLap;
};
PLAYERDIST operator * (const float fl, const PLAYERDIST& dist);

std::ostream& operator << (std::ostream& stream, const PLAYERDIST& dist);

// a single point of race-replay data
struct RECORDEDDATA
{
  RECORDEDDATA(unsigned short power,const PLAYERDIST& dist,float flTime,float flElev,float flSpeed,unsigned short cadence, unsigned short hr,float lane)
    : power(power),
    dist(dist),
    flTime(flTime),
    flElev(flElev),
    flSpeed(flSpeed),
    cadence(cadence),
    hr(hr),
    flLane(lane),
    flStrength(1.0f)
  {}

  bool operator != (const RECORDEDDATA& other) const
  {
    return power != other.power ||
           cadence != other.cadence || 
           hr != other.hr;
  }
  unsigned short power;
  PLAYERDIST dist; // m since this player joined
  float flTime; // seconds since race started
  float flElev; // m
  float flSpeed; // m/s
  unsigned short cadence; // rpm
  unsigned short hr; // bpm
  float flLane;
  float flStrength; // not saved to DB: the strength of the workout when this point was recorded.  Defaults to 1
};

enum STATID
{
  //STATID_COMBATIVENESS_DEFUNCT=0, // how often this player beats players who put out more W/kg than they do
  STATID_DRAFTPCT=1, // % of time this player drafted
  STATID_10S_AVG=2,
  STATID_30S_AVG=3,
  STATID_60S_AVG=4,
  STATID_2MIN_AVG=5,
  STATID_5MIN_AVG=6,
  STATID_10MIN_AVG=7,
  STATID_20MIN_AVG=8,
  STATID_30MIN_AVG=9,
  STATID_45MIN_AVG=10,
  STATID_60MIN_AVG=11,
  STATID_120MIN_AVG=12,
  STATID_TIMEAVG_RESERVED1=13,
  STATID_TIMEAVG_RESERVED2=14,
  STATID_TIMEAVG_RESERVED3=15,
  STATID_TIMEAVG_RESERVED4=16,
  STATID_TIMEAVG_RESERVED5=17,
  STATID_TIMEAVG_RESERVED6=18,
  STATID_TIMEAVG_RESERVED7=19,
  STATID_TIMEAVG_RESERVED8=20,
  STATID_WORKOUT_ACCURACY=21, // the accuracy that a user rode a workout at
  //STATID_WORKOUT_ACCURACY_FULLSTRENGTH=22, // if a user rides a workout at full strength and never deviates, their accuracy goes here too.  Note: deprecated
  STATID_WORKOUT_STRENGTH=23, // the time-weighted mean of the strength they rode the workout at

  STATID_COUNT,
};
int StatIdToSeconds(const STATID id);
const char* StatIdToDesc(const STATID id);

class TimeAverager
{
public:
  TimeAverager(float flTimeSpanSeconds) : m_flSpanTarget(flTimeSpanSeconds),m_flBestSum(0),m_flBestDtSum(0),m_flCurDtSum(0),m_flBestAverageStartTime(0),m_flCurSum(0),m_fFull(false) {};
  
  float GetSpan() const {return m_flSpanTarget;}
  void AddData(float flTime, float flPower)
  {
    if(m_qData.size() > 0 && 
        (flTime - m_qData.back().flTime < 0.5))
    {
      // don't update too fast!
      return;
    }
    TIMEDATA tNewData;
    tNewData.flTime = flTime;
    tNewData.flPower = flPower;
    tNewData.dt = m_qData.size() > 0 ? 
                        flTime - m_qData.back().flTime : 0;

    m_qData.push_back(tNewData);

    m_flCurSum += tNewData.dt * tNewData.flPower;
    m_flCurDtSum += tNewData.dt;
    // pop all the old data off the front
    while(m_qData.size() > 0 && 
          m_flCurDtSum >= m_flSpanTarget)
    {
      TIMEDATA tOldData = m_qData.front();
      m_qData.pop_front();
      m_flCurSum -= tOldData.dt*tOldData.flPower;
      m_flCurDtSum -= tOldData.dt;
      m_fFull = true;
    }
    
    if(m_qData.size() > 0 && // we have data
      m_flCurSum > m_flBestSum &&  // that data is better than our previous best
      m_flCurDtSum <= m_flSpanTarget) // and it doesn't span more time than our goal
    {
      m_flBestSum = m_flCurSum;
      m_flBestDtSum = m_flCurDtSum;
      m_flBestAverageStartTime = m_qData.front().flTime;
    }
  }
  bool IsFull() const {return m_fFull;}
  float GetBestAverage() const {return m_flBestSum / (m_flSpanTarget > m_flBestDtSum ? m_flSpanTarget : m_flBestDtSum);}
private:
  struct TIMEDATA
  {
    float flTime;
    float flPower;
    float dt;
  };
  bool m_fFull;
  std::list<TIMEDATA> m_qData;
  float m_flCurSum;
  float m_flCurDtSum;
  float m_flBestSum;
  float m_flBestDtSum;
  float m_flBestAverageStartTime;
  float m_flSpanTarget;
};
typedef boost::shared_ptr<TimeAverager> TimeAveragerPtr;

// represents the historic best stats of a given player
struct PERSONBESTSTATS
{
  PERSONBESTSTATS() : flOriginalHistoric(4.5)
  {
    Reset();
  }
  void Reset()
  {
    memset(this,0,sizeof(*this));
    flOriginalHistoric = 4.0;
  }
  void SetMax(int iStatId, float flValue)
  {
    if(iStatId >= STATID_10S_AVG && iStatId <= STATID_120MIN_AVG)
    {
      rgMaxes[iStatId] = flValue;
      rgfMaxSet[iStatId] = true;
    }
  }
  void SetBestLap(float flValue)
  {
    bestlap = flValue;
  }
  void SetOriginalHistoric(float flVal) // call this after loading the original data
  {
    flOriginalHistoric = flVal;
  }
  float GetOriginalHistoric() const // call this after loading the original data
  {
    if(flOriginalHistoric < 1.0) return 1.0f;
    if(flOriginalHistoric >= 10.0f) return 10.0f;
    return flOriginalHistoric;
  }
  float GetMax(STATID id) const
  {
    return rgMaxes[id];
  }
  bool IsStatSet(STATID id) const
  {
    return rgfMaxSet[id];
  }
  float GetHistoricPowerWkg(bool* pfIsFake) const
  {
    // note: multipliers for FTP retrieved with this SQL statement:
    /*"select 
        person.name, 
        person.masterid,
	    raceentry.powertype,
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=11 and raceentry.personid=person.id),
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=10 and raceentry.personid=person.id),
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=9 and raceentry.personid=person.id),
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=8 and raceentry.personid=person.id),
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=7 and raceentry.personid=person.id),
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=6 and raceentry.personid=person.id),
	    (select max(statdata.value) from statdata where statdata.raceentryid=raceentry.id and statdata.statid=5 and raceentry.personid=person.id)
    from
        statdata,
        raceentry,
        person
    where
        statdata.raceentryid = raceentry.id
            and raceentry.powertype != 5 // no spectators
            and raceentry.powertype != 4 // no cheaters
            and raceentry.personid = person.id
		    and person.masterid!=9 // no art
		    and person.masterid!=0 // no eric
		    and person.name not like "Player 1" // no newbies
		    and person.name not like "Player" // no newbies
		    and person.isai=0 // no AI
		    and statdata.statid=11 // make sure they have 60min times
		    and raceentry.powertype is not null
    group by person.id"*/


    const static float flRatios[] = 
    {
      0,
      0,
      0, // 10s
      0, // 30s
      0, // 60s
      0.776f, // 2min
      0.835f, // 5min
      0.879f, // 10min
      0.912f, // 20min
      0.938f, // 30min
      0.968f, // 45min
      1, // 60min
    };
    
    float flFTP = 0;
    for(int x = STATID_5MIN_AVG; x <= STATID_60MIN_AVG; x++)
    {
      if(IsStatSet((STATID)x))
      {
        flFTP = ArtMax(GetMax((STATID)x)*flRatios[x],flFTP);
      }
    }
    if(flFTP > 0)
    {
      return flFTP;
    }
    *pfIsFake=true;
    return flOriginalHistoric; // make sure that this guy gets overestimated for now...
  }
private:
  //float rgMins[STATID_COUNT];
  bool  rgfMaxSet[STATID_COUNT];
  float rgMaxes[STATID_COUNT];
  float flOriginalHistoric; // stores what their best FTP was upon loading from the DB.  Since they might change their FTP mid-race, we need some stability here
  float bestlap;
  //float flAvg[STATID_COUNT];
};

enum PHYSICSMODE
{
  REALMODEL_2013 = 0,                     // real-world physics.  Fixed CDA and CRR       0000
  PHYSICSMODE_FIRST = 1,                  // the first physics mode we'll use (aka everything numerically before this is deprecated) 0000
  WEIGHTHANDICAPPED_2013 = 1,             // everyone has the same weight (80kg)          0001
  REALMODEL_LATE2013 = 2,                 // varied CDA based on weight                   0010
  REALMODEL_LATE2013_NODRAFT = 3,         // same as late2013, but no drafting            0011
  FTPHANDICAPPED_2013 = 4,                // everyone's power is adjusted based on their FTP.  100% FTP -> 300W 0100
  FTPHANDICAPPED_NODRAFT_2013 = 5,        // same as FTPHANDICAPPED, but no drafting
  TRAININGMODE = 6,
  PHYSICSMODE_LAST,
};
bool IsDraftingMode(PHYSICSMODE ePhysics); // returns whether a given physics mode requires us to perform drafting calculations

const char* GetPhysicsModeString(PHYSICSMODE eMode);
const char* GetPhysicsModeStringShort(PHYSICSMODE eMode);
PHYSICSMODE GetDefaultPhysicsMode();

const int INVALID_PLAYER_ID = 65535;
const int GHOST_BIT = (1<<31); // if a player ID has this set, it is a ghost

bool IsLappingMode(int laps, int timedLength);
bool IsTimedMode(int laps, int timedLength);

struct LAPDATA
{
  LAPDATA(const float time, const float cumkj, const float tmStart, const int posAtStart) : time(time),cumkj(cumkj),tmStart(tmStart),posAtStart(posAtStart) {};

  LAPDATA(const LAPDATA& other) : time(other.time),cumkj(other.cumkj),tmStart(other.tmStart),posAtStart(other.posAtStart) {};
  float time;
  float cumkj; // how many KJ has the rider burned up to this point?
  float tmStart; // start time of the lap, in seconds since race started
  int posAtStart;
};

enum SCORETYPE
{
  SCORE_NONE=0,
  SCORE_SPRINT=1,
  SCORE_CLIMB=2,

  SCORE_COUNT,
};

// a representation of how far into a map a mappoint was AT THE MAP'S ORIGINAL UNCUT SCALE AND SHAPE
// baked into a struct to avoid all-too-easy "assigned an origdist as a normal map distance" bugs
struct ORIGDIST
{
  float v;

  ORIGDIST operator - (const ORIGDIST& other) const
  {
    ORIGDIST ret;
    ret.v = v - other.v;
    return ret;
  } 
  bool operator > (const ORIGDIST& other) const {return v > other.v;}
  bool operator < (const ORIGDIST& other) const {return v < other.v;}
  bool operator <= (const ORIGDIST& other) const {return v <= other.v;}
  bool operator >= (const ORIGDIST& other) const {return v >= other.v;}
};
ORIGDIST operator * (const float& f, const ORIGDIST& o);
ORIGDIST operator + (const ORIGDIST& o1, const ORIGDIST& o2);

struct SPRINTCLIMBDATA_RAW // for transmitting SPRINTCLIMBDATA stuff
{
  ORIGDIST flOrigDistance; // where in the lap (in original meters) is this scoring point at?  values 0-1
  ORIGDIST flOrigLeadInDist; // how far is the leadin for this scoring point? (in original meters.  To compute leadin location, you'll want GetDistanceOfOrigDist(flOrigDistance-flOrigLeadInDist))
  float flMaxPoints; // how many points is 1st place worth? (point handouts will drop off in a 1/x^2 manner, so that halfway through the field is worth 1/4 the points)
  SCORETYPE eScoreType; // what type of scoring thingy is it?
  char szName[24]; // what's the name of the thingy?
  bool fIsFinish; // is this a finish-lap-only point?
};
struct SPRINTCLIMBDATA
{
  SPRINTCLIMBDATA() : eType(SCORE_NONE),flPoints(0),lap(-1),flDistance(0),flRise(0) {};
  SPRINTCLIMBDATA(SCORETYPE eType, float flPoints, int lap,float time, int meters, float flDistance, float flRise) : eType(eType),flPoints(flPoints),lap(lap),time(time),meters(meters),flDistance(flDistance),flRise(flRise) {}

  SCORETYPE eType; // what kind of points did we get?
  float flPoints; // how many points did we get?
  int lap; // which lap were these points awarded on?
  float time; // how long did it take (in seconds) to complete this segment from leadin to finish?
  int meters; // how long (ridden distance) was this segment?
  float flDistance; // where (ridden distance) was the finish line for this segment?
  float flRise; // how high (meters) was the elevation difference on this segment?
};
// represents an intermediate sprint or climb.
// in a multi-lap environment, the scoringSource must keep separate track of each lap.
// example: if the scoring source is at 500m, GetPointsEarned should give different results for (just crossed on lap 0) and (just crossed on lap 1)
class SprintClimbPoint
{
public:
  // returns a nonzero value if a rider earned points on this ScoringSource
  virtual bool GetPointsEarned(const ORIGDIST& flLastDist, const ORIGDIST& flThisDist, int ixCurLap, bool fLastLap, SPRINTCLIMBDATA* pData, int idPlayer, float flCurRaceTime) = 0;

  // returns the description of this scoring source
  virtual std::string GetDescription(int ixLap) = 0;

  // tells the scoring source how many people to use for scoring calculations.  This will be called at the start of the race, once the server knows how many starters there are
  virtual void SetPeopleCount(int cPeople) = 0;

  // tells the scoring source the maximum number of points for it to hand out.  This will be called after the map builds itself and figures out weights for each thing.
  // for example: if the map wants to hand out 33.333pts total for climbing, then it needs to weight that
  virtual void SetMaxPoints(float flPoints) = 0;

  // gets the key description of this thing (position, points, type, and name) for network transmission
  virtual void GetRaw(SPRINTCLIMBDATA_RAW* pRaw) const = 0;

  // whether this is a "last-lap-only" point
  virtual bool IsFinishMode() const = 0;

  // how many points is this thing going to hand out for the next guy that crosses?
  virtual float GetNextPoints(int iLap) const = 0;
};
typedef boost::shared_ptr<SprintClimbPoint> SprintClimbPointPtr;

struct AISELECTION
{
  AISELECTION() : ixDLL(-1),ixAI(-1) {}
  AISELECTION(int i, int j) : ixDLL(i),ixAI(j) {}
  bool operator == (const AISELECTION& other) const {return ixDLL == other.ixDLL && ixAI == other.ixAI;}
  int ixDLL;
  int ixAI;
};

enum TARGETTYPE
{
  TARGET_FTP_PERCENT=0,
  TARGET_WATTS=1,
  TARGET_FREESPIN=2,
};
struct WORKOUTTARGET
{
  WORKOUTTARGET() : startTime(0),length(1),eType(TARGET_FTP_PERCENT),flValue(1.0f),flStrength(1.0f) {};

  WORKOUTTARGET(int startTime, int length, TARGETTYPE eType, float flValue) : startTime(startTime),length(length), eType(eType),flValue(flValue),flStrength(1.0f) {};

  TARGETTYPE GetType() const {return eType;}
  int GetLengthSeconds() const {return length;}
  int GetStartTime() const {return startTime;}

  // flWKg - the player that'll be reading this's FTP
  // flStrength - their current workout strength
  std::string GetDescription(float flFTPW, float flStrength) const; // returns "Hold 50% FTP", "Hold 200W", "Spin freely"
  std::string GetShortDescription() const; // returns "FTP", "Watts", "Spin"
  void SetStrength(float fl) {flStrength = fl;}
  float GetWatts(float flFTPW) const
  {
    switch(eType)
    {
    case TARGET_FTP_PERCENT: // flValue represents a percentage of FTP to hit
      return flFTPW*flValue*flStrength;
    case TARGET_WATTS:
      return flValue*flStrength;
    case TARGET_FREESPIN:
      return 0; // they can do whatever they want
    }
    return 0;
  }
private:
  int startTime;
  int length;
  TARGETTYPE eType;
  float flValue;
  float flStrength; // usually 1.0, but can be different
};

class IWorkout
{
public:
  virtual bool GetTargetAtTime(float flTime, float flStrength, WORKOUTTARGET* pTarget, float* pflTimeUntilNext) const = 0;
  virtual int GetWorkoutId() const = 0;
  virtual int GetPlanId() const = 0;
  virtual int GetLengthSeconds() const = 0;

  // returns a descriptive line about this workout.  Usually generated by the website
  virtual std::string GetWebLine(int line) const = 0;
  // returns the workout segment that'll have the highest wattage.
  virtual void GetMaxTarget(WORKOUTTARGET* pTarget, float flFTPW) const = 0;
};
typedef boost::shared_ptr<const IWorkout> IWorkoutPtrConst;
IWorkoutPtrConst CreateWorkout(const int iPlanId, const int iWorkoutId, const std::vector<WORKOUTTARGET>& lstSteps, const std::vector<std::string>& lstWebLines);
float GetWorkoutAccuracy(const std::vector<RECORDEDDATA>& lstOldData, IWorkoutPtrConst spWorkout, float flFTPW);

// just the data/workout-related portions of a player.  These are things that a training player and a racing player both share
class IConstPlayerData;
typedef boost::shared_ptr<IConstPlayerData> IConstPlayerDataPtr;
typedef boost::shared_ptr<const IConstPlayerData> IConstPlayerDataPtrConst;
class IConstPlayerData
{
public:
  virtual ~IConstPlayerData() {};
  
  virtual std::string GetName() const = 0;
  virtual unsigned short GetPower() const = 0;
  virtual unsigned short GetCadence(unsigned int tmNow) const = 0;
  virtual unsigned short GetHR(unsigned int tmNow) const = 0;
  virtual int GetMasterId() const = 0;
  virtual int GetId() const = 0;
  virtual float GetMassKg() const = 0;
  virtual float GetAveragePower() const {return 0;}; // returns the average power from all our Tick() calls.
  virtual const float GetEnergySpent() const = 0;
  virtual const std::vector<RECORDEDDATA>& GetPowerHistory() const = 0;
  virtual POWERTYPE GetPowerType() const = 0;
  virtual int GetPowerSubType() const = 0;
  virtual const float GetTimeRidden() const = 0;
  virtual const PERSONBESTSTATS& GetBestStatsConst() const = 0;

  virtual IConstPlayerDataPtrConst GetConstDataCopy(unsigned int tmNow, bool fCopyData) const;
};

// all the racing-related portions of a player
class IConstPlayer : public IConstPlayerData
{
public:
  virtual ~IConstPlayer() {};
  
  virtual float GetFinishTime() const = 0;
  virtual float GetLane() const = 0;
  virtual const PLAYERDIST& GetDistance() const = 0;
  virtual float GetSpeed() const = 0;
  virtual float GetLastDraft() const = 0;
  virtual float GetLastDraftNewtons() const {return 0;}
  virtual bool Player_IsAI() const = 0;
  virtual bool Player_IsFrenemy() const = 0; // not quite a human, not quite a worthless AI
  virtual float GetStartTime() const = 0; // returns when this player started, in seconds since 1970.
  virtual const PLAYERDIST GetStartDistance() const = 0; // returns where the player started on the map
  virtual unsigned int GetIP() const {return 0;}; // returns the average power from all our Tick() calls.
  virtual unsigned int GetTimeAtDistance(const PLAYERDIST& flDist) const = 0; // returns when (in ArtGetTime time) this player was at a given distance
  virtual unsigned int GetTeamNumber() const = 0; // what team is this guy on?
  virtual unsigned int GetActionFlags() const = 0;
  virtual const AISELECTION& GetAIType() const = 0;
  virtual float GetReplayOffset() const {return 0;}
  virtual bool GetStat(STATID eStat, float* pflValue) const = 0; // returns true if the player has relevant data for the stat
  virtual const float GetRunningPower(STATID id) const = 0;
  virtual bool IsStealth() const {return false;}
  virtual void GetLapTimes(std::vector<LAPDATA>& lstTimes) const = 0;
  virtual void GetSprintClimbPoints(std::vector<SPRINTCLIMBDATA>& lstPoints) const {};

  virtual IConstPlayerPtrConst GetConstCopy(unsigned int tmNow) const;
};
class IPlayer : public IConstPlayer
{
public:
  virtual ~IPlayer() {};
  virtual void SetCadence(unsigned int tmNow, unsigned short cadence) = 0;
  virtual void SetHR(unsigned int tmNow, unsigned short hr) = 0;
  virtual void Tick(float flRaceTime, int ixMe, PHYSICSMODE ePhysics, RACEMODE eRaceMode, const std::vector<IConstPlayerPtrConst>& lst, const DraftMap& mapWhoDraftingWho, float dt,SPRINTCLIMBDATA* pPointsScored, std::string* pstrPointsDesc, const unsigned int tmNow) = 0;
  virtual void SetActionFlags(unsigned int flags, unsigned int mask) = 0;
  virtual PERSONBESTSTATS& GetBestStats() = 0;
  const static float DEFAULT_CRR;
  const static float DEFAULT_MASS;
};

struct LATLON
{
  LATLON() : flLat(0),flLon(0) {}
  LATLON(float lat,float lon) : flLat(lat),flLon(lon) {};
  float flLat;
  float flLon;
};

struct MAPBOUNDS
{
  float flCenterLat;
  float flCenterLon;
  float flCenterElev;
  float flMinLat;
  float flMaxLat;
  float flMinLon;
  float flMaxLon;
  float flMinElev;
  float flMaxElev;
};

struct ELEVCOLOR
{
  float r,g,b;
};

class ElevationSupplier
{
public:
  virtual float GetElevationAtDistance(const PLAYERDIST& flDistance) const = 0;
  virtual PLAYERDIST GetEndDistance() const = 0;
  virtual PLAYERDIST GetStartDistance() const = 0;
  virtual MAPBOUNDS GetMapBounds() const = 0;
  virtual float GetMaxGradient() const = 0;
  virtual float GetSlopeAtDistance(const PLAYERDIST& flDistance) const = 0;
  virtual int GetMapId() const = 0;
  virtual float GetNaturalDistance() const = 0; // how long is the map in the real world?
  virtual ORIGDIST GetOrigDistAtDistance(float flDist) const = 0;
  virtual float GetDistanceOfOrigDist(const ORIGDIST& dist) const = 0;

  virtual void GetColor(float flDistance, ELEVCOLOR* pclrTop, ELEVCOLOR* pclrBottom) const = 0;
};
typedef boost::shared_ptr<const ElevationSupplier> ElevationSupplierPtrConst;
typedef boost::shared_ptr<ElevationSupplier> ElevationSupplierPtr;

// indicates where a road goes.  Used for plotting 2d top-down roadmaps
class IRoadRouteSupplier
{
public:
  virtual PLAYERDIST GetRoadRouteStartDistance() const = 0;
  virtual PLAYERDIST GetRoadRouteEndDistance() const = 0;
  virtual float GetRoadRouteElevationAtDistance(const PLAYERDIST& flPos) const = 0;
  virtual LATLON GetLatLonAtDistance(const PLAYERDIST& flDistance) const = 0;
};

struct MAPPOINT
{
  float flElev;
  float flDistance;
  ORIGDIST flOrigDist;
  float flLat;
  float flLon;
  float flSlope;
  float flRadius;
  float flMaxSpeed; // what is the speed limit for this section?
  float flRawLat;
  float flRawLon; // where did this point come from originally? (on the planet, that is) (this is important for storing stuff in mapPoints)
  float flSplineT; // what value of T for the spline generated this point?
  bool operator < (const float flDist) const
  {
    return this->flDistance < flDist;
  }
  float DistTo(const MAPPOINT& pt) const
  {
    return sqrt(pow(flLat-pt.flLat,2) + pow(flLon-pt.flLon,2));
  }

  static MAPPOINT Blend(const MAPPOINT& ptLeft, const MAPPOINT& ptRight, float flPctLeft)
  {
    MAPPOINT pt;
    pt.flElev = flPctLeft*ptLeft.flElev + (1-flPctLeft)*ptRight.flElev;
    pt.flDistance = flPctLeft*ptLeft.flDistance + (1-flPctLeft)*ptRight.flDistance;
    pt.flOrigDist = flPctLeft*ptLeft.flOrigDist + (1-flPctLeft)*ptRight.flOrigDist;
    pt.flLat = flPctLeft*ptLeft.flLat + (1-flPctLeft)*ptRight.flLat;
    pt.flLon = flPctLeft*ptLeft.flLon + (1-flPctLeft)*ptRight.flLon;
    pt.flSlope = flPctLeft*ptLeft.flSlope + (1-flPctLeft)*ptRight.flSlope;
    pt.flRadius = flPctLeft*ptLeft.flRadius + (1-flPctLeft)*ptRight.flRadius;
    pt.flMaxSpeed = flPctLeft*ptLeft.flMaxSpeed + (1-flPctLeft)*ptRight.flMaxSpeed;
    pt.flRawLat = flPctLeft*ptLeft.flRawLat + (1-flPctLeft)*ptRight.flRawLat;
    pt.flRawLon = flPctLeft*ptLeft.flRawLon + (1-flPctLeft)*ptRight.flRawLon;
    pt.flSplineT = flPctLeft*ptLeft.flSplineT + (1-flPctLeft)*ptRight.flSplineT;
    return pt;
  }
};

// an indication from the DB/GPX/etc about what we are allowed doing to a map
typedef unsigned int MAPCAP;

// note: no-flags-set should behave like TdG 1.0.
static const MAPCAP MAPCAP_NOZIPPER = 0x1; // disable zippering for this map (1.0 always had zippering on)
static const MAPCAP MAPCAP_LOOP = 0x2; // make a fake loop for multilap courses, rather than going start->finish->warp to start
static const MAPCAP MAPCAP_NOSPRINTCLIMB = 0x4; // disable sprints/climbs for this map

class LatLonSource
{
public:
  virtual bool GetLatLonPoints(std::vector<MAPPOINT>& lstPoints, std::string& strMapName, int iStartPct, int iEndPct, int* piOwnerId, MAPCAP* pMapCap) = 0;
};


class IMap : public ElevationSupplier, public IRoadRouteSupplier
{
public:
  virtual std::string GetMapName() const = 0;
  virtual std::string GetMapFile() const = 0;
  virtual void GetDirectionAtDistance(const PLAYERDIST& flDistance, float* flXDir, float* flYDir) const = 0;
  virtual float GetMaxSpeedAtDistance(const PLAYERDIST& flDistance) const = 0;
  virtual float GetRadiusAtDistance(const PLAYERDIST& flDistance) const = 0;
  virtual float GetRhoAtDistance(float flDistance) const = 0;
  virtual float GetNaturalDistance() const = 0; // how long is the map without compression/contraction?
  virtual float GetClimbing() const = 0; // returns how many meters of climbing there is on this map
  virtual const std::vector<MAPPOINT>& GetAllPoints() const = 0;
  virtual int GetOwnerId() const = 0;
  virtual int GetLaps() const = 0;
  virtual int GetTimedLength() const = 0;
  virtual float GetLapLength() const = 0;
  virtual void GetScoringSources(std::vector<SprintClimbPointPtr>& lstSources) const = 0;
};

#ifdef _MSC_VER
#define snprintf _snprintf
#else
#endif

class IAI
{
public:
  IAI(const char* pszName, int iWatts)
  {
    char szName[200];
    snprintf(szName,sizeof(szName),"[ai]%s%d",pszName,iWatts);
    m_strName = szName;
  }
  virtual ~IAI() {};
  
  virtual int GetWatts(const float tmNow, const IPlayer* pMe, const std::vector<IConstPlayerPtrConst>& lstOthers, const IMap* pMap) = 0;
  virtual const char* GetName() const
  {
    return m_strName.c_str();
  }
private:
  std::string m_strName;
};

// interface to set up a logical player
enum CALIBSTATE
{
  CALIB_NEEDED,
  CALIB_PEDALFASTER,
  CALIB_COASTDOWN,
  CALIB_OK,
  CALIB_DISCONNECTED,
  CALIB_PRESSBUTTON,
  CALIB_TIGHTEN,
  CALIB_LOOSEN,
  CALIB_RETRIEVING,

  CALIB_COUNT,
};
enum CAMERASTYLE
{
  CAMSTYLE_1STPERSON,
  CAMSTYLE_3RDPERSON,
};

const static int POWERRECV_NEEDCALIB = -7; // device needs calibration
const static int POWERRECV_PEDALFASTER = -8; // user needs to pedal faster!
const static int POWERRECV_COASTDOWN = -9; // user needs to coast down
const static int POWERRECV_PRESSBUTTON = -10; // user needs to coast down
const static int POWERRECV_DISCONNECTED = -11; // disconnected
const static int POWERRECV_TIGHTEN = -12; // the user needs to tighten their thingy, then recalibrate
const static int POWERRECV_LOOSEN = -13; // the user needs to loosen their thingy, then recalibrate
const static int POWERRECV_CALIB_RETRIEVING = -14; // retrieving calibration information


class IPowerSourceReceiver
{
public:
  virtual void SetPower(int playerIndex, int deviceId, int sPower) = 0;
  virtual void SetCadence(int playerIndex, int deviceId, unsigned short cadence) = 0;
  virtual void SetHR(int playerIndex, int deviceId, unsigned short hr) = 0;
  virtual void SetSpeed(int playerIndex, int deviceId, float flSpeedMetersPerSecond) {};

  virtual void ButtonDown(int playerIndex, int ixKey) {};
  virtual void ButtonUp(int playerIndex, int ixKey, int msHoldTime) {};
};

    
#ifdef _MSC_VER
    typedef int CTPORTTYPE; // on windows, we can just pass around the integer port value
#else
    typedef std::string CTPORTTYPE; // on mac, ports are represented as disgusting strings
#endif
bool CTPort_IsValid(CTPORTTYPE port);
std::string CTPort_ToString(CTPORTTYPE port); // goes to a user-unfriendly string, like "5"
std::string CTPort_ToUserString(CTPORTTYPE port); // goes to a user-friendly string, like "COM5"
CTPORTTYPE CTPort_FromString(std::string str); // for windows: string is a numeric port number, like "5".  For Mac, string is the complete path to the serial device
CTPORTTYPE CTPort_Default(); // returns an "empty" port
    
void FormatDistance(char* psz, int cch, float flMeters, int decimals, SPEEDPREF eUnits, bool fIncludeUnits=true);
void FormatTotalDistance(char* psz, int cch, const PLAYERDIST& dist, int decimals, SPEEDPREF eUnits, bool fIncludeUnits=true);
void FormatCompletedDistance(char* psz, int cch, const PLAYERDIST& dist, int decimals, SPEEDPREF eUnits, bool fIncludeUnits=true);

