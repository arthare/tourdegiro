#pragma once
#include "Tools.h"

using namespace std;

#define SERVERSHIFT 30
enum SERVER
{
  SERVER_FIRST = SERVERSHIFT,
  SERVER_FLAT = SERVER_FIRST,
  SERVER_HILLY,
  SERVER_MOUNTAIN,
  SERVER_TIMETRIAL,
  SERVER_LONGRIDE,
  SERVER_SPRINT,

  SERVER_LAST,
};

// 10->11: adding protocol version, weight and team number to clientdesc and the player data structs
// 11->12: the big 2.0: multilap, sprint/hill points, etc
const static int PROTOCOL_VERSION = 12;

//const static int LOGIN_UDP_IN_PORT = 63900; // march6 version
//const static int LOGIN_TCP_CONNECT_PORT = 63901; // march6 version
const static int LOGIN_UDP_IN_PORT = 63902; // june version
const static int LOGIN_TCP_CONNECT_PORT = 63903; // june version

const static int SERVER_UDP_IN_PORT = 63940;
const static int SERVER_TCP_CONNECT_PORT = 63941;

const int MAX_PLAYER_NAME = 16;
const static int MAX_PLAYERS = 8;

const int AI_MASTER_ID = 1;
const int AI_DEFAULT_TEAM = 0;

// we don't ever want to send an update to a player about another player sooner than this delay.  Example: rider barack obama doesn't want to hear about mitt romney more than once every 30ms.
// this is primarily for bandwidth saving - my initial test with a linode server put the single-player bandwidth at like 160kb/s, and all the pings were at 0ms.  I want that more like 4kb/s and 33ms.
const int MINIMUM_UPDATE_DELAY = 45; 


const float MAX_LANECHANGE_SPEED = 1.5; // how fast are cyclists allowed to move laterally (meters per second)
const float ROAD_WIDTH = 3.5f; // left side of the road, in meters
const float ROAD_HEIGHT = 1.0f; // how much higher is the road than the surrounding environment?
const float MAX_SPEED = 100.0f; // the maximum speed we will permit to occur.

const float CYCLIST_LENGTH = 2.0f;
const float CYCLIST_WIDTH = 0.4f;
const float CYCLIST_HEIGHT = 2.0f;

const static unsigned int PLAYERFLAG_HUMAN =    0x01;
const static unsigned int PLAYERFLAG_REMOVE =   0x02;
const static unsigned int PLAYERFLAG_GHOST =    0x04;
const static unsigned int PLAYERFLAG_SPECTATOR =0x08;
const static unsigned int PLAYERFLAG_FRENEMY =  0x10;


enum LOGINRESULT
{
  LOGINSUCCESS=0,
  PLAYERNOTFOUND=1, // player account not created
  ACCOUNTEXPIRED=2, // out of days to play
  BADPASSWORD=3, // password not accepted
  REASONUNKNOWN=4, // rejected, but we don't know why
  CONNECTIONFAILURE=5,
  OUTDATEDCLIENT=6,
  SERVERTOOBUSY=7, // too many people are trying to log in at once
  SERVERFULL=8, // too many people are trying to log in at once
  //ANTPLUSPM_NOT_ALLOWED=7,
  //ANTPLUSSC_NOT_ALLOWED=8,
  //COMPUTRAINER_NOT_ALLOWED=9,
  //CHEATER_NOT_ALLOWED=10,
  //KICKR_NOT_ALLOWED=11,
};




class SERVERKEY
{
public:
  SERVERKEY(ICommSocketAddressPtr pAddr) : dwAddr(pAddr->ToDWORD()),dwPort(pAddr->get_local_port()) {}
  DWORD dwAddr;
  DWORD dwPort;

  SERVERKEY() : dwAddr(0),dwPort(0) {};

  bool operator < (const SERVERKEY& other) const
  {
    LONGLONG* pllThis = (LONGLONG*)&dwAddr;
    LONGLONG* pllOther = (LONGLONG*)&other.dwAddr;
    return *pllThis < *pllOther;
  }
  bool operator == (const SERVERKEY& other) const
  {
    return dwAddr == other.dwAddr && other.dwPort == dwPort;
  }
  friend std::ostream& operator<< (std::ostream &out, const SERVERKEY &cPoint);
};

namespace std
{
    template<>
    struct hash<SERVERKEY>
    {
        size_t operator () (const SERVERKEY& uid) const
        {
            return uid.dwAddr ^ uid.dwPort;
        }
    };
}

std::ostream& operator<< (std::ostream &out, const SERVERKEY &cPoint);

typedef unsigned int SERVERFLAGS;

const SERVERFLAGS SF_NORMALIZEDPHYSICS = 0x1;
const SERVERFLAGS SF_PAUSEDCOUNTDOWN   = 0x2;
const SERVERFLAGS SF_PHYSICSBIT2       = 0x4; // the 2nd bit of the physics mode.  physicsmode = SF_NORMALIZEDPHYSICS | (SF_PHYSICSBIT2<<1) | (BIT3<<2) | (BIT4<<3)
const SERVERFLAGS SF_PHYSICSBIT3       = 0x8; // the 3rd bit of the physics mode.  physicsmode = SF_NORMALIZEDPHYSICS | (SF_PHYSICSBIT2<<1) | (BIT3<<2) | (BIT4<<3)
const SERVERFLAGS SF_PHYSICSBIT4       = 0x10; // the 4th bit of the physics mode.  physicsmode = SF_NORMALIZEDPHYSICS | (SF_PHYSICSBIT2<<1) | (BIT3<<2) | (BIT4<<3)
const SERVERFLAGS SF_PAUSEDGAME        = 0x20; // game is paused!
 

class Player;
struct POSITIONUPDATE
{
  friend class TDGGameClient;

  int iTimeUntilStart;
  int iSprintClimbPeopleCount;

  float GetLane(int ix) const
  {
    return ((float)(rgsPlayerLane[ix] / 65535.0f) * (ROAD_WIDTH*2)) - ROAD_WIDTH;
  }
  float GetSpeed(int ix) const
  {
    return (((float)rgsPlayerSpeed[ix]) / 65535.0f) * MAX_SPEED;
  }
  unsigned short GetPower(int ix) const
  {
    return rgsPlayerPower[ix];
  }
  void SetPlayerStats(int ix, int id, const IConstPlayer* pPlayer);
  unsigned int GetPlayerId(int ix) const
  {
    return rgPlayerIds[ix];
  }
  float GetPosition(int ix) const
  {
    return rgPlayerPos[ix];
  }
  void SetDead(int ixPlayer)
  {
    rgPlayerFlags[ixPlayer] |= PLAYERFLAG_REMOVE;
  }
  void SetGhost(int ixPlayer)
  {
    rgPlayerFlags[ixPlayer] |= PLAYERFLAG_GHOST;
  }
  void SetSpectator(int ixPlayer)
  {
    rgPlayerFlags[ixPlayer] |= PLAYERFLAG_SPECTATOR;
  }
  bool IsGhost(int ixPlayer) const
  {
    return !!(rgPlayerFlags[ixPlayer] & PLAYERFLAG_GHOST);
  }
  bool IsDead(int ixPlayer) const
  {
    return !!(rgPlayerFlags[ixPlayer] & PLAYERFLAG_REMOVE);
  }
  bool IsSpectator(int ixPlayer) const
  {
    return !!(rgPlayerFlags[ixPlayer] & PLAYERFLAG_SPECTATOR);
  }
  void ClearData()
  {
    for(int x = 0;x < MAX_PLAYERS;x++)
    {
      rgPlayerIds[x] = INVALID_PLAYER_ID;
      rgsPlayerPower[x] = 0;
      rgsPlayerSpeed[x] = 0;
      rgsPlayerLane[x] = 0; 
      rgPlayerFlags[x] = 0;
    }
  }
  bool IsHuman(int ix) const {return rgPlayerFlags[ix] & PLAYERFLAG_HUMAN;}
  bool IsFrenemy(int ix) const {return rgPlayerFlags[ix] & PLAYERFLAG_FRENEMY;}
  float GetWeight(int ix) const {return ((float)rgPlayerWeights[ix]) / 10.0;}
  SERVERFLAGS GetServerFlags() const {return serverFlags;}
  void SetServerFlags(SERVERFLAGS eFlags) {serverFlags = eFlags;}
  int GetLap(int ix) const {return rgPlayerLaps[ix];}
private:
  void SetLane(int ix, float flLane) 
  {
    DASSERT(flLane >= -ROAD_WIDTH && flLane <= ROAD_WIDTH);
    rgsPlayerLane[ix] = (unsigned short)(65535*((flLane + ROAD_WIDTH) / (ROAD_WIDTH*2)));
  }
  void SetSpeed(int ix, float flSpeed)
  {
    DASSERT(flSpeed <= 100.0f);
    rgsPlayerSpeed[ix] = (unsigned short)(65535*(flSpeed / 100.0f));
  }
  void SetPower(int ix, unsigned short power)
  {
    rgsPlayerPower[ix] = power;
  }
private:
  // these are private since they have to be converted from their small representations into floating-point on the receiving side
  unsigned short rgsPlayerLane[MAX_PLAYERS]; // 0 -> left.  65535 -> right
  unsigned short rgsPlayerSpeed[MAX_PLAYERS]; // 100m/s = 65536
  unsigned short rgsPlayerPower[MAX_PLAYERS]; // straight conversion to power
  unsigned int rgPlayerIds[MAX_PLAYERS]; // an array of the player ids referred to in this update.  the nth rgPlayerPos/Speed/Power values should be applied to the player with id rgPlayerIds[n]
  float rgPlayerPos[MAX_PLAYERS];
  unsigned int rgPlayerFlags[MAX_PLAYERS]; // player flags
  unsigned short rgPlayerWeights[MAX_PLAYERS]; // weight in 1/10th kg.  Ex: 650 -> 65kg
  unsigned char rgPlayerLaps[MAX_PLAYERS]; // lap count for each player
  SERVERFLAGS serverFlags;
};


// sent during the game so that clients know what each player's name is
struct PLAYERNAMEUPDATE
{
  const static int NAME_COUNT=MAX_PLAYERS;
  unsigned int  rgPlayerIds[NAME_COUNT];
  char rgszPlayerNames[NAME_COUNT][MAX_PLAYER_NAME];
  void Init()
  {
    for(unsigned int x = 0;x < NAME_COUNT; x++)
    {
      rgPlayerIds[x] = INVALID_PLAYER_ID;
      rgszPlayerNames[x][0] = 0;
    }
  }
  std::string GetName(int x) const
  {
    char szName[MAX_PLAYER_NAME+1];
    memcpy(szName,rgszPlayerNames[x],MAX_PLAYER_NAME);
    szName[MAX_PLAYER_NAME] = 0;
    return szName;
  }
  void Set(const int ix, const int id, const std::string& strName)
  {
    DASSERT(ix >= 0 && ix < NAME_COUNT);
    rgPlayerIds[ix] = id;
    strncpy(rgszPlayerNames[ix],strName.c_str(),sizeof(rgszPlayerNames[ix]));
  }
};

struct RESULTUPDATE
{
  unsigned int  rgPlayerIds[MAX_PLAYERS];
  float rgflPlayerTime[MAX_PLAYERS]; // negative numbers: player isn't done yet
};

struct SCHEDULEDUPDATE
{
  int iSchedId; // id of this scheduled item
  int iKm; // how long is each lap of the ride
  int laps; // how many laps is the ride?
  int timedLengthSeconds;
  int iServerId; // what server is it running on? (so we know where to connect to)
  unsigned int tmStartGMT; // the unix time (in GMT time) that the race will be starting
  char szName[64]; // name of the race
  char szMapName[64]; // name of the map
  int iAIMinStrength;
  int iAIMaxStrength;

  void Set(const int _iSchedId,
            const int _iKm,
            const int _laps,
            const int _iServerId,
            const unsigned int _tmStartGMT,
            const char* _szName,
            const char* _szMapName,
            const int _iAIMinStrength,
            const int _iAIMaxStrength,
            const int _timedLengthSeconds
            )
  {
    iSchedId = _iSchedId;
    iKm = _iKm;
    laps = _laps;
    iServerId = _iServerId;
    tmStartGMT = _tmStartGMT;

    const int cchName=sizeof(szName);
    strncpy(szName,_szName,cchName);
    szName[cchName-1] = 0;

    const int cchMapName = sizeof(szMapName);
    strncpy(szMapName,_szMapName,sizeof(szMapName));
    szMapName[cchMapName-1]=0;
    
    iAIMinStrength = _iAIMinStrength;
    iAIMaxStrength = _iAIMaxStrength;
    timedLengthSeconds = _timedLengthSeconds;
  }

  void Init()
  {
    iSchedId = 0;
    iKm = 0;
    iServerId = 0;
    tmStartGMT = 0;
    szName[0] = 0;
    szMapName[0] = 0;
    iAIMinStrength = 100;
    iAIMaxStrength = 300;
    laps = 1;
    timedLengthSeconds = 0;
  }
  bool IsValid() const
  {
    bool fRet = iServerId >= 18 && iKm > 0 && szName[0] && szMapName[0] && (laps > 0 || timedLengthSeconds >= 60);
    return fRet;
  }

  std::string ToString() const
  {
    SYSTEMTIME st = SecondsSince1970ToSYSTEMTIME(tmStartGMT, true);
    char szTime[100];

    const char* pszSuffix = "am";
    if(st.wHour > 12)
    {
      st.wHour -=12;
      pszSuffix = "pm";
    }

    char szStarted[200];
    DWORD tmNow = ::GetSecondsSince1970GMT();
    const int tmUntilStart = tmStartGMT - tmNow;
    szStarted[0] = 0;
    if(tmNow > tmStartGMT)
    {
      // this race started in the past
      snprintf(szStarted,sizeof(szStarted),"(STARTED)");
    }
    else if(tmUntilStart < 5400)
    {
      // race is starting within an hour
      snprintf(szStarted,sizeof(szStarted),"(%d mins)",tmUntilStart/60);
    }
    else if(tmUntilStart < 24*3600)
    {
      int hours = tmUntilStart / 3600;
      int minutes = (tmUntilStart % 3600) / 60;
      snprintf(szStarted,sizeof(szStarted),"(%dh %dm)",hours,minutes);
    }

    snprintf(szTime,sizeof(szTime),"%02d/%02d/%04d %d:%02d%s",st.wMonth,st.wDay,st.wYear,st.wHour,st.wMinute,pszSuffix);

    char szDist[100];
    ::FormatDistance(szDist,sizeof(szDist),iKm * 1000.0,1,GetSpeedPref());
    char szName[200];

    if(IsTimedMode(laps,timedLengthSeconds))
    {
      snprintf(szName,sizeof(szName),"%s %s: %d minutes lapping %s on %s: \"%s\"",szStarted, szTime, timedLengthSeconds/60, szDist,szMapName,this->szName);
    }
    else
    {
      snprintf(szName,sizeof(szName),"%s %s: %d x %s on %s: \"%s\"",szStarted, szTime, laps, szDist,szMapName,this->szName);
    }
    return szName;
  }
};

extern const char* SYNC_STRING;
struct SYNCUPDATE
{
  void Init()
  {
    memset(szGarbage,0,sizeof(szGarbage));
    strncpy(szSync,SYNC_STRING,sizeof(szSync));
    DASSERT(strlen(szSync) == strlen(SYNC_STRING));
  }
  char szGarbage[60];
  char szSync[32]; // a 32-character string.  If we start failing checksums, we will use this to see if maybe we're out of sync.
};

struct CHATUPDATE
{
  int idFrom; // player ID that sent it
  FILETIME tmSent; // when did they send it?
  char szChat[128]; // what did they say?
};

struct TIMETRIALUPDATE
{
  const static int TT_PLAYER_COUNT = 16;
  unsigned short segment[TT_PLAYER_COUNT]; // 65535 = finish point.  Otherwise, this indicates how many multiples of TimeTrialState::s_minInterval that the nth time represents
  float times[TT_PLAYER_COUNT]; // what time did they do?
  int ids[TT_PLAYER_COUNT]; // who did each time?

  void Init()
  {
    for(int x = 0;x < TT_PLAYER_COUNT; x++)
    {
      ids[x] = INVALID_PLAYER_ID;
    }
  }
};

enum TDG_GAMESTATE
{
  UNKNOWN,
  WAITING_FOR_START,
  RACING,
  RESULTS,
};

enum S2C_UPDATETYPE
{
  POSITION_UPDATE, // server telling clients where everyone is and how fast they're going.
  NAME_UPDATE, // server telling clients player ID->name mappings
  RESULT_UPDATE, // server telling us player's finishing times
  SYNC_UPDATE,
  CHAT_UPDATE,
  SCHEDULEDSERVER_UPDATE, // login server telling the loginclient about a scheduled server opportunity
  TT_UPDATE,

  UPDATETYPE_COUNT,
};


struct TDGGameState
{
  S2C_UPDATETYPE eType;
  long lIteration;
  union
  {
    POSITIONUPDATE posUpdate;
    PLAYERNAMEUPDATE nameUpdate;
    RESULTUPDATE resultUpdate;
    SYNCUPDATE syncUpdate;
    CHATUPDATE chatUpdate;
    SCHEDULEDUPDATE schedUpdate; // description of a scheduled server
    TIMETRIALUPDATE ttupdate;
  };
  int checksum;
  static int GetChecksum(const TDGGameState* in);
};


// THESE FLAGS COME FROM THE CLIENT or are generated by the server for internal use.  IF YOU WANT TO SEND A FLAG, USE PLAYERFLAGS
const static int ACTION_FLAG_LEFT =           0x1;
const static int ACTION_FLAG_RIGHT =          0x2;
const static int ACTION_FLAG_DEAD =           0x4; // the player isn't currently connected (internal server flag, though I guess a cheater client could send it...)
const static int ACTION_FLAG_IDLE =           0x8; // the player isn't currently connected (internal server flag, though I guess a cheater client could send it...)
const static int ACTION_FLAG_DOOMEDAI =      0x10; // this is a player that is doomed, once we tell the other players about it
const static int ACTION_FLAG_GHOST =         0x20;
const static int ACTION_FLAG_SPECTATOR =     0x40; // this player is a spectator
const static int ACTION_FLAG_LOADING =       0x80; // whether this client is loading
const static int ACTION_FLAG_LOCATION_MAIN_PLAYER = 0x100; // this play is the "main" player at their location.  So if there's 3 riders there, this guy is player #1/3.

const static int ACTION_FLAG_ALL_FROMCLIENT =      ACTION_FLAG_LEFT | ACTION_FLAG_RIGHT | ACTION_FLAG_LOADING;
const static int ACTION_FLAG_IGNOREFORPHYSICS = ACTION_FLAG_SPECTATOR | ACTION_FLAG_GHOST;

const static int ACTION_FLAG_MOVES = ACTION_FLAG_LEFT | ACTION_FLAG_RIGHT;

const static int MAX_LOCAL_PLAYERS = 8;


struct TDGClientState
{
  int cLocalPlayers;
  unsigned int rgPlayerIds[MAX_LOCAL_PLAYERS];
  unsigned short rgPlayerPowers[MAX_LOCAL_PLAYERS]; // note: if a given player is a spectator, then rgPlayerPowers = (their current look point / 10)
  unsigned int rgfdwActionFlags[MAX_LOCAL_PLAYERS];
  float powerProduct; // the product of multiplying all the ACTIVE powers together - serves as a checksum
};

enum C2S_UPDATETYPE
{
  C2S_CHAT_UPDATE=0, // client wants to send us some sweet chitty chatty
  C2S_RESTART_REQUEST=1, // client wants to restart
  C2S_PHYSICS_CHANGE=2, // client wants to change the server physics from normalized to true weights (or vice versa)
  C2S_COUNTDOWN_PAUSE=3, // client wants the countdown to pause or resume
  C2S_CHANGEAI=4, // client wants to add or remove AI players
  C2S_STARTNOW=5, // client wants to start now
  C2S_CHANGEMAP=6, // id indicates the map they want to switch to
  C2S_PLAYERUPDATE=7, // a full player update, sent via TCP
  C2S_HRMCADUPDATE=8, // a heartrate/cadence update, sent via TCP
  C2S_GAME_PAUSE=9, // request a pause of the game
};

struct C2S_CHANGEMAP_DATA
{
  int idMap; // which map do they want?
  int iKmLength; // how long of a ride do they want on it?
};

struct C2S_HRMCADUPDATE_DATA
{
  int cLocalPlayers;
  unsigned int rgPlayerIds[MAX_LOCAL_PLAYERS];
  unsigned short rgHR[MAX_LOCAL_PLAYERS];
  unsigned short rgCadence[MAX_LOCAL_PLAYERS];
};

struct TDGClientToServerSpecial // any kind of must-send data like chats or player updates (quitting, voting, etc) must go here
{
  C2S_UPDATETYPE eType;
  union
  {
    CHATUPDATE chat;
    C2S_CHANGEMAP_DATA map;
    int data; // some random data (used for physics change or countdown pausing requests)
    TDGClientState state; // for C2S_PLAYERUPDATE (the client will send player updates via TCP @ 2hz
    C2S_HRMCADUPDATE_DATA hrcad;
  };
};

// since we don't trust the client, we don't want it sending its own position or speed - those physics calcs will occur on the server


struct MAPSTATS
{
  float flAverageRaceTime;

  char szReservered[50];
};

struct TDGServerStatus
{
  TDGServerStatus() : raceLength(0,0,0)
  {
    protocolVersion = PROTOCOL_VERSION;
    cHumans = -1;
    cAI = -1;
    flRaceCompletion = -1;
    flAvgSlope = 0;
    szMapName[0] = 0;
    cSprintClimbs=0;
  }
  bool IsValid()
  {
    return flRaceCompletion > 0 && raceLength.iCurrentLap > 0 && raceLength.flDistPerLap > 0 && cAI >= 0 && cHumans >= 0;
  }
  int protocolVersion;
  int cHumans;
  int cAI;
  MAPSTATS mapStats;
  PLAYERDIST raceLength;
  float flRaceCompletion;
  float rgElevs[100]; // an evenly-spaced set of elevation samples of the first lap
  float flAvgSlope;
  RACEMODE eGameMode;
  char szMapName[100];
  float flClimbing;
  float flMaxGradient;
  int cSprintClimbs;
  SPRINTCLIMBDATA_RAW rgSprintClimbs[10]; // note: the embedded ORIGDISTs here won't actually be original distances.  They will have already been converted to "as ridden" distances

  // 200->196: art: added flAvgSlope
  // 196->192: art: added protocolVersion
  // 192->188: art: added eGameMode
  // 188->88: art: added szMapName
  // 88->80: art: added flClimbing and flMaxGradient
  char szReserved[80]; 
};

enum CONNECTIONTYPE
{
  CONNTYPE_QUERYSTATUS,
  CONNTYPE_PLAYGAME,
};

struct MD5
{
  MD5() {}
  MD5(const char* str);
  unsigned char szMD5[16];
};

struct MAPREQUEST
{
  bool IsValid() const
  {
    if(iMeters < 1000)
    {
      return false;
    }
    if(iSecondsToDelay < 60)
    {
      return false;
    }
    if(iMapId <= 0)
    {
      return false;
    }
    if(cAIs < 0)
    {
      return false;
    }
    if(cPercentStart < 0 || cPercentStart >= 100)
    {
      return false;
    }
    if(cPercentEnd < 1 || cPercentEnd > 100)
    {
      return false;
    }
    if(cPercentEnd <= cPercentStart)
    {
      return false;
    }
    if(!IsLappingMode(laps,timedLengthSeconds) && !IsTimedMode(laps,timedLengthSeconds))
    {
      DASSERT(FALSE);
      return false;
    }
    if(timedLengthSeconds > 0 && laps > 0)
    {
      DASSERT(FALSE);
      return false;
    }
    return true;
  }

  int iMeters; // how long is the ride?
  int iSecondsToDelay; // how long do you want to delay before the race starts?
  int iMapId;
  int iAIMinStrength;
  int iAIMaxStrength;
  int cAIs;
  int cPercentStart; // percent (0-100) along the map that they want to start at
  int cPercentEnd; // percent (0-100) along the map that they want to end at (must be bigger than cPercentStart)
  int laps;
  int timedLengthSeconds;
};

struct TDGClientDesc
{
  TDGClientDesc()
  {
    protocolVersion = PROTOCOL_VERSION;
    iMasterId = 0;
    szUsername[0] = 0;
    for(int x = 0;x < MAX_LOCAL_PLAYERS;x++)
    {
      szName[x][0] = 0;
    }
  }
  int protocolVersion;
  CONNECTIONTYPE eConnType;
  int cLocalPlayers;
  int iMasterId;
  int iDefaultTeamId;
  char szUsername[16]; // the username for this guy
  MD5 rgMd5; // the md5 of this guy's password
private:
  char szName[MAX_LOCAL_PLAYERS][MAX_PLAYER_NAME];
public:
  unsigned short rgWeights[MAX_LOCAL_PLAYERS]; // each player's weight in 1/10kg.  650 -> 65kg
  POWERTYPE rgPlayerDevices[MAX_LOCAL_PLAYERS]; // what is everyone using?
  
  std::string GetName(int ixPlayer) const
  {
    char szTemp[17];
    memcpy(szTemp,this->szName[ixPlayer],sizeof(szTemp));
    szTemp[16] = 0;
    return std::string(szTemp);
  }
  void SetName(int ixPlayer, const std::string& str)
  {
#define tempmin(a,b) ((a < b) ? a : b)
    memcpy(szName[ixPlayer],str.c_str(),tempmin(sizeof(szName[ixPlayer]),str.length()));
#undef tempmin
    if(str.length() < sizeof(szName[ixPlayer]))
    {
      szName[ixPlayer][str.length()] = 0;
    }
  }
  boost::uuids::uuid guid; // on initial connection, set this to GUID_NULL.  On reconnects, set it to the GUID that the server assigned to us in the TDGInitialState response
  MAPREQUEST mapReq;

  // may 26: added MAPREQUEST
  char szExtra[200 - sizeof(MAPREQUEST)]; // reserved for future use
};

struct SRTMCELL
{
  float flLat;
  float flLon;
  float flElev;
};


struct TDGInitialState
{
  TDGInitialState()
  {
    szMapName[0] = 0;
    cPointsUsed = 0;
    cSRTMPoints = 0;
    cLaps = 1;
  }
  const static int NUM_MAP_POINTS = 4000;
  const static int NUM_SCORE_POINTS = 50;
  int cPointsUsed; // how many points did we use?
  char szMapName[64]; // name of the map (ex: "Muskoka 70.3")
  float rgLat[NUM_MAP_POINTS]; // latitude (in meters right of the average position on the map)
  float rgLon[NUM_MAP_POINTS]; // longitude (in meters south of the average position on the map)
  float rgDistance[NUM_MAP_POINTS]; // distance in meters
  float rgElevation[NUM_MAP_POINTS]; // elevation in meters
  ORIGDIST rgOrigDist[NUM_MAP_POINTS]; // original distance in meters (how far along this was in the raw map.  Used for aligning roadside stuff like sprint/climb points)

  RACEMODE eRaceMode;
  // extra elevation data derived from the SRTM mission, used to build a more accurate heightmap
  const static int NUM_SRTM_POINTS = 6000;
  int cSRTMPoints;
  SRTMCELL rgSRTM[NUM_SRTM_POINTS];
  int cLaps;
  int timedLengthSeconds;

  int cScorePoints; // how many score points does this map have?
  SPRINTCLIMBDATA_RAW rgScorePoints[NUM_SCORE_POINTS]; // up to NUM_SCORE_POINTS points available
  MAPCAP mapCap;
  char rgExtra[200]; // padding
};

// validates a given position slot to make sure that we're not getting garbage
bool IsValidPositionSlot(const POSITIONUPDATE& pos,int ixSlot);
std::string MakeIPString(unsigned int ip);


template<class DATASTORED>
class TTDataModder
{
public:
  // a callback to the guys data-changer.
  // iDist: integer dist
  // idThatDidIt: who did the recorded time
  // data: the recorded time
  virtual bool Do(int iDist, int idThatDidIt, DATASTORED& data) = 0; // return true to continue iteration.  false to stop
};

template<class DATASTORED>
class TimeTrialState
{
public:
  TimeTrialState(ManagedCS* pCS) : m_pCS(pCS) {};
  virtual ~TimeTrialState() {};

  void AddTime(int iDist,int idPlayer, const DATASTORED& flTime)
  {
    AutoLeaveCS _cs(*m_pCS);
    DASSERT(iDist % s_minInterval == 0); // we only accept multiples of 500m
    if(iDist % s_minInterval == 0 && ((idPlayer & GHOST_BIT)==0))
    {
      mapResults[iDist].insert(std::pair<int,DATASTORED>(idPlayer,flTime));
    }
  }
  void GetTimesAtDistance(int iDist, unordered_map<int,DATASTORED>& results) const
  {
    if(iDist % s_minInterval == 0)
    {
      ARTTYPE unordered_map<int,unordered_map<int,DATASTORED> >::const_iterator i = mapResults.find(iDist);
      if(i != mapResults.end())
      {
        results = i->second;
      }
    }
  }
  void AddNetState(const TIMETRIALUPDATE& data)
  {
    for(unsigned int x = 0;x < TIMETRIALUPDATE::TT_PLAYER_COUNT; x++)
    {
      if(data.ids[x] != INVALID_PLAYER_ID && ((data.ids[x] & GHOST_BIT)==0))
      {
        const int iDist = data.segment[x]*s_minInterval;
        float flTime = data.times[x];
        mapResults[iDist][data.ids[x]] = DATASTORED(flTime);
      }
    }
  }
  const static int s_minInterval = 500;
protected:
  ManagedCS* m_pCS;
  void Apply(TTDataModder<DATASTORED>* mod)
  {
    for(ARTTYPE unordered_map<int,unordered_map<int,DATASTORED> >::iterator i = mapResults.begin(); i != mapResults.end(); i++)
    {
      const int iDist = i->first;
      unordered_map<int,DATASTORED>& inner = i->second;
      for(ARTTYPE unordered_map<int,DATASTORED>::iterator iInner = inner.begin(); iInner != inner.end(); iInner++)
      {
        const int idWhoDidIt = iInner->first;
        DATASTORED& data = iInner->second;
        if(!mod->Do(iDist,idWhoDidIt, data))
        {
          return;
        }
      }
    }
  }
private:
  unordered_map<int,unordered_map<int,DATASTORED> > mapResults; // maps from a distance to a player->time mapping.  This is how we store all our intermediate times
};
