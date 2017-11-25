#pragma once
#include "TDGInterface.h"

// a "local player" is a description of how a player would like their viewport built.
class TourDeGiroLocalPlayer
{
public:
  virtual RECTF GetViewport() const = 0; // where does this guy's viewport go?
  virtual int GetViewingId() const = 0; // who is this guy viewing? (aka what userid).  note that it is possible for a player to be viewing someone other than themselves
  virtual int GetLocalId() const = 0; // what is a unique value I can identify this guy by?
  virtual float GetR() const = 0; // color: 0-1,
  virtual float GetG() const = 0; // color: 0-1
  virtual float GetB() const = 0; // color: 0-1
  virtual float GetAveragePower() const = 0;
  virtual POWERTYPE GetPowerType() const = 0;
  virtual CAMERASTYLE GetCameraStyle() const = 0;
};

extern const int LOCAL_SERVER_ID;

bool operator < (const unordered_set<int>&, const unordered_set<int>&);

extern std::string strVidData;
struct PELETONINFO
{
  PELETONINFO(const float flDistPerLap) : fContainsLocalPlayer(false),fContainsHumanPlayer(false),cHumans(0),m_flSlowestFinisher(1e30f),m_flFastestFinisher(-1e30f),flPos(0,0,flDistPerLap) {};

  PLAYERDIST flPos;
  const unordered_set<int>& GetPlayerSet() const {return setPlayerIds;}
  void AddPlayer(int id, bool fIsLocal, bool fIsHuman, float flFinishTime, const PLAYERDIST& playerDist)
  {
    setPlayerIds.insert(id);
    fContainsLocalPlayer |= fIsLocal;
    fContainsHumanPlayer |= fIsHuman;
    if(fIsHuman) cHumans++;
    if(flFinishTime > 0)
    {
      m_flSlowestFinisher = m_flSlowestFinisher > 0 ? max(m_flSlowestFinisher,flFinishTime) : flFinishTime;
      m_flFastestFinisher = m_flFastestFinisher > 0 ? min(m_flFastestFinisher,flFinishTime) : flFinishTime;
    }
  }
  bool HasFinishers() const
  {
    return m_flSlowestFinisher > 0 && m_flFastestFinisher > 0;
  }
  float GetFastestFinishTime() const
  {
    DASSERT(HasFinishers());
    return m_flFastestFinisher;
  }
  float GetSlowestFinishTime() const
  {
    DASSERT(HasFinishers());
    return m_flSlowestFinisher;
  }
  bool HasPlayer(int id) const
  {
    return setPlayerIds.find(id) != setPlayerIds.end();
  }
  bool ContainsLocalPlayer() const
  {
    return fContainsLocalPlayer;
  }
  bool ContainsHuman() const
  {
    return fContainsHumanPlayer;
  }
  int GetPlayerCount() const
  {
    return setPlayerIds.size();
  }
  int GetHumanCount() const
  {
    return cHumans;
  }
  void Init(const PLAYERDIST& flPos)
  {
    setPlayerIds.clear();
    this->flPos = flPos;
    fContainsLocalPlayer = false;
    fContainsHumanPlayer = false;
    m_flFastestFinisher = -1;
    m_flSlowestFinisher = -1;
    cHumans=0;
  }
  bool operator < (const PELETONINFO& p) const
  {
    bool fRet = false;
    if(setPlayerIds < p.setPlayerIds) 
    {
      fRet = true;
      goto doneexit;
    }
    else if(p.setPlayerIds < setPlayerIds)
    {
      fRet = false;
      goto doneexit;
    }

    if(fContainsLocalPlayer < p.fContainsLocalPlayer) 
    {
      fRet = true;
      goto doneexit;
    }
    else if(fContainsLocalPlayer > p.fContainsLocalPlayer)
    {
      fRet = false;
      goto doneexit;
    }
    else if(fContainsHumanPlayer < p.fContainsHumanPlayer)
    {
      fRet = true;
    }
    else if(fContainsHumanPlayer > p.fContainsHumanPlayer)
    {
      fRet = false;
    }
    else if(m_flFastestFinisher < p.m_flFastestFinisher)
    {
      fRet = true;
    }
    else if(m_flFastestFinisher > p.m_flFastestFinisher)
    {
      fRet = false;
    }
    else if(m_flSlowestFinisher < p.m_flSlowestFinisher)
    {
      fRet = true;
    }
    else if(m_flSlowestFinisher > p.m_flSlowestFinisher)
    {
      fRet = false;
    }
doneexit:
    //cout<<"peletoncompare "<<this<<(fRet ? "<" : ">")<<&p<<endl;
    return fRet;
  }
private:
  bool fContainsLocalPlayer;
  bool fContainsHumanPlayer;
  float m_flFastestFinisher;
  float m_flSlowestFinisher;
  unordered_set<int> setPlayerIds; // who is in this group?
  int cHumans;
};
typedef boost::shared_ptr<PELETONINFO> PELETONINFOPtr;
typedef boost::shared_ptr<const PELETONINFO> PELETONINFOConstPtr;
bool operator < (const PELETONINFOConstPtr& p1, const PELETONINFOConstPtr& p2);

class NameDB
{
public:
  virtual std::string GetName(int id) const = 0;
  virtual unordered_map<int,std::string> GetNameList() const = 0;
};

struct ROADHEIGHT
{
  ROADHEIGHT(float d, float e) : flDist(d),flElev(e) {};

  float flDist;
  float flElev;
};
class RoadHeightDB
{
public:
  virtual float GetRoadHeight(float flDist) const = 0;
  virtual void SetRoadHeights(const std::vector<ROADHEIGHT>& m_lstRoadElev) = 0;
};

// comes from the TourDeGiroFrameDoer (aka game logic) to the frame painter.  Includes everything the painter needs to know about painting
struct TDGFRAMEPARAMS
{
  const Map* pCurrentMap;
  SERVERFLAGS fdwServerFlags;
  PHYSICSMODE ePhysicsMode;
  TDG_GAMESTATE eGameState;
  int msToStart;
  vector<IPlayerPtrConst> lstPlayers; // all the cyclists.  Each one needs to get drawn
  vector<const TourDeGiroLocalPlayer*> lstViewports; // all the local players - each one must have a viewport
  vector<PELETONINFOConstPtr> lstGroups;
  vector<CHATUPDATE> lstChats;
  NameDB* pNameDB;
  RACEMODE eRaceMode;
  const TimeTrialState<float>* pTTState;
};


// these are things that can be triggered in the TourDeGiroPainter implementor (perhaps by its UI), that it can signal to the logic portion
enum DOER_ACTION
{
  DOER_RESTART,
  DOER_STARTNOW,
  DOER_MOREAI,
  DOER_LESSAI,
  DOER_CHANGEWEIGHTMODE,
  DOER_PAUSECOUNT,
  DOER_RESUMECOUNT,
  DOER_MOVELEFT,
  DOER_MOVERIGHT,
  DOER_NOMOVE,
  DOER_STARTINGLOAD,
  DOER_DONELOAD,
  DOER_NODOWNHILL, // they've requested to turn on "no downhill" mode
};

enum GAMEMODE
{
  GAMEMODE_SINGLEPLAYER,
  GAMEMODE_MULTIPLAYER,

  GAMEMODE_COUNT,
};

enum MAPTYPE
{
  MAPTYPE_MAP,
};

// The "Frame Doer" is the best-named and most important part of the painter<->gamelogic.  
// Each frame, the painter calls Doer_Do() as well as potentially actions if they've been triggered in the UI.
// Doer_Do() will run a physics step, talk to the server, and populate the TDGFRAMEPARAMS with data about the current state of the game.
// The TourDeGiroPainter will then use the contents of TDGFRAMEPARAMS to do all of its drawing tasks
class TourDeGiroFrameDoer
{
public:
  virtual ManagedCS* Doer_GetLock() = 0;
  virtual void Doer_Do(float dt, TDGFRAMEPARAMS& frameData) = 0;
  virtual void Doer_Quit() = 0;

  virtual void Doer_DoAction(DOER_ACTION eAction) = 0;
  virtual void Doer_AddChat(const string& strChat) = 0;
};



struct TDGPAINTERINITPARAMS
{
  TDGPAINTERINITPARAMS() {};

  TourDeGiroFrameDoer* pGameLogic; // gets called once per frame.  Updates physics, applies game state, and whatnot
};


// this is implemented by anything that wants to paint a representation of a TdG race.
// It gets passed a TourDeGiroFrameDoer (aka the game logic) via the TDGPAINTERINITPARAMS.
// It is up to the 
class TourDeGiroPainter
{
public:
  virtual bool Painter_Do3DSetup() = 0;
  virtual void Painter_Init(const TDGPAINTERINITPARAMS& params, const vector<const TourDeGiroLocalPlayer*>& lstLocals) = 0;
  virtual void Painter_Go() = 0; // enters the game loop.  Physics logic can be done in your TourDeGiroFrameDoer in the Doer_Do function
  virtual bool Painter_IsQuit() const = 0; // whether the user wants to fully quit everything
};
class BaseApplication;

// pContext is a pointer to data for your particular painter.  Try to keep it simple, like a window handle or something.
TourDeGiroPainter* CreatePainter(LockingMap& map, void* pContext);
void FreePainter(TourDeGiroPainter* pPainter);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// setup stuff


class MapSelection
{
public:
  MapSelection(int id, string strName) : id(id),strName(strName) {};
  int id;
  string strName;
};
class TourDeGiroMapRequest
{
public:
  TourDeGiroMapRequest()
  {
    iMeters = 20000;
    iSecondsToDelay = 5*60;
    iMapId = 0;
    cAIs = 55;
    cPercentStart = 0;
    cPercentEnd = 100;
    iAIMinStrength = 100;
    iAIMaxStrength = 200;
    laps = 1;
    timedLengthSeconds = -1;
  }
  bool IsValid(const char** ppszError) const
  {
    const char* pszWaste = NULL;
    if(!ppszError) ppszError = &pszWaste;

    if(iMeters < 1000)
    {
      *ppszError = "Length too short (less than 1km)";
      return false;
    }
    if(iSecondsToDelay < 60)
    {
      *ppszError = "Delay too short (must be at least 1 minute to allow you time to load the map)";
      return false;
    }
    if(iMapId <= 0)
    {
      *ppszError = "You must select a map.";
      return false;
    }
    if(cAIs < 0)
    {
      *ppszError = "You must have a nonnegative number of AIs (I hope no-one actually ever sees this string)";
      return false;
    }
    if(cPercentStart < 0 || cPercentStart >= 100)
    {
      *ppszError = "The start point must be between 0 and 99% of the map's length";
      return false;
    }
    if(cPercentEnd < 1 || cPercentEnd > 100)
    {
      *ppszError = "The end point must be between 1 and 100% of the map's length";
      return false;
    }
    if(cPercentEnd <= cPercentStart)
    {
      *ppszError = "The end point must be after the start point";
      return false;
    }
    if(!IsLappingMode(laps,timedLengthSeconds) && !IsTimedMode(laps,timedLengthSeconds))
    {
      *ppszError = "You need to either ride more than one lap or enter a # of minutes to ride";
      return false;
    }
    if(laps > 0 && timedLengthSeconds > 0)
    {
      DASSERT(FALSE);
      *ppszError = "You can't have a capped number of laps and a count of minutes to ride";
      return false;
    }
    *ppszError = NULL;
    return true;
  }
  int iMeters; // how long is the ride?
  int iSecondsToDelay; // how long do you want to delay before the race starts?
  int iMapId;
  int iAIMinStrength;
  int iAIMaxStrength;
  int cAIs;
  int laps;
  int cPercentStart; // percent (0-100) along the map that they want to start at
  int cPercentEnd; // percent (0-100) along the map that they want to end at (must be bigger than cPercentStart)
  int timedLengthSeconds; // how long should this ride be?
};


