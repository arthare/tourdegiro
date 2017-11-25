#include "stdafx.h"

#include "WinDef.h"

#include "game.h" // includes the main game client definitions from GameLib


#include "ArtNet.h"
#include "ArtTools.h"

#include <iostream>
#include <boost/iostreams/device/null.hpp>

using namespace std;

class ConsolePainter : public TourDeGiroPainter
{
public:
  ConsolePainter(LockingMap& map) : m_lastMapId(map.GetMapId())
  {
    // we should use the map object here to cache a bunch of info about how we're going to draw it.  We don't want to store the map itself though.
  };

  virtual bool Painter_Do3DSetup()
  {
    // we're a console window!  We don't have any 3d setup to do!
    // if we WERE a 3D window, we'd probably want to initialize our art assets
    return true;
  }
  virtual void Painter_Init(const TDGPAINTERINITPARAMS& params, const vector<const TourDeGiroLocalPlayer*>& lstLocals) ARTOVERRIDE
  {
    m_params = params;
    m_lstLocals = lstLocals;

    m_params.pGameLogic->Doer_DoAction(DOER_ACTION::DOER_STARTINGLOAD);
    m_params.pGameLogic->Doer_DoAction(DOER_ACTION::DOER_DONELOAD);
  }
  virtual void Painter_Go()
  {
    float dt = 0.1;
    float t = 0;
    while(true)
    {
      t += dt;
      
      // get the game logic to tell us about the current state
      TDGFRAMEPARAMS params;
      m_params.pGameLogic->Doer_Do(dt, params);

      PaintState(params);
    }
  }
  virtual bool Painter_IsQuit() const
  {
    return false;
  }

private:
  void PaintState(const TDGFRAMEPARAMS& params)
  {
    // check to see if the map changed.  If it has, we'll probably have to put up a load screen and rebuild our map data (if this were a 3D renderer)
    if(params.pCurrentMap->GetMapId() != m_lastMapId)
    {
      ChangeMap(params.pCurrentMap);
    }
    else
    {
      // same map as last time!  Let's draw the current state of the race.

      switch(params.eGameState)
      {
      case WAITING_FOR_START:
      {
        ClearConsole();
        float flTimeToStart = (float)params.msToStart / 1000.0f;

        char szTimeBuf[255];
        ::FormatTimeMinutesSecondsMs(flTimeToStart, szTimeBuf, sizeof(szTimeBuf));
        cout<<"Time until start: "<<szTimeBuf<<endl;
        break;
      }
      case RACING:
        // let's paint the state of the race!
        ClearConsole();
        for(int x = 0;x < params.lstPlayers.size(); x++)
        {
          IPlayerPtrConst spPlayer = params.lstPlayers[x];
          string strName = spPlayer->GetName();
          float distance = spPlayer->GetDistance().ToMeters();
          float speed = spPlayer->GetSpeed() * 3.6;
          printf("%20s %5dm %2.1fkm/h\n", strName.c_str(), (int)distance, speed);
        }

        break;
      case RESULTS:
        break;
      }
    }
  }
  void ChangeMap(const Map* pNewMap)
  {
    // if you're doing a 3D renderer, you should probably display a load screen, build your terrain, place scenery, etc based on this new map that just showed up.

    m_lastMapId = pNewMap->GetMapId();
  }
private:
  static void ClearConsole()
  {
#ifdef _WINDOWS
    system("cls");
#else
    // clear your console in your favourite OSX/posix-like way
#endif
  }
private:
  int m_lastMapId;
  TDGPAINTERINITPARAMS m_params;
  vector<const TourDeGiroLocalPlayer*> m_lstLocals;
};

class ConsoleLocalPlayer : public TourDeGiroLocalPlayer
{
public:
  virtual RECTF GetViewport() const ARTOVERRIDE
  {
    RECTF ret;
    ret.left = ret.top = 0;
    ret.right = ret.bottom = 1;
    return ret;
  }
  virtual int GetViewingId() const ARTOVERRIDE
  {
    return 0;
  }
  virtual int GetLocalId() const
  {
    return 0;
  }
  virtual float GetR() const
  {
    return 1;
  }
  virtual float GetG() const
  {
    return 1;
  }
  virtual float GetB() const
  {
    return 1;
  }
  virtual float GetAveragePower() const
  {
    return 100;
  }
  virtual POWERTYPE GetPowerType() const
  {
    return ANT_POWER;
  }
  virtual CAMERASTYLE GetCameraStyle() const
  {
    return CAMSTYLE_1STPERSON;
  }
};


class ConsolePlayerSetupData : public PlayerSetupData
{
public:
  ConsolePlayerSetupData(const POWERTYPE powerType, const float flMassKg, const string& strName) : m_powerType(powerType), m_flMassKg(flMassKg), m_strName(strName)
  {

  }
  virtual bool isEnabled() const
  {
    return true;
  }
  virtual string getName() const
  {
    return m_strName;
  }
  virtual float getMassKg() const
  {
    return m_flMassKg;
  }
  virtual POWERTYPE getPowerType() const
  {
    return m_powerType;
  }

  // what kind of camera does this person prefer?  The renderer should make a best-effort at implementing the selected camera style.
  virtual CAMERASTYLE getCameraStyle() const
  {
    return CAMSTYLE_1STPERSON;
  }

private:
  const POWERTYPE m_powerType;
  const float m_flMassKg;
  const string m_strName;
};

class ConsoleTDGSetupState : public TDGSetupState
{
public:
  ConsoleTDGSetupState(const string& strTargetServer) : m_strTargetServer(strTargetServer) {};
  virtual ~ConsoleTDGSetupState() {};

  virtual int GetMasterId() const ARTOVERRIDE
  {
    return 0;
  }
  virtual string GetTarget() const ARTOVERRIDE
  {
    return m_strTargetServer;
  }

  void AddPlayer(boost::shared_ptr<PlayerSetupData> spPlayer)
  {
    m_lstPlayers.push_back(spPlayer);
  }

  virtual int GetPlayerCount() const ARTOVERRIDE
  {
    return m_lstPlayers.size();
  }
  // returns the setup data for the ixPlayerth player who is playing locally today.
  virtual const PlayerSetupData* GetPlayer(int ixPlayer) const ARTOVERRIDE
  {
    return m_lstPlayers[ixPlayer].get();
  }

private:
  string m_strTargetServer;
  vector<boost::shared_ptr<PlayerSetupData> > m_lstPlayers;
};



// need to override these so that TDGGameClient can create and destroy the painter
TourDeGiroPainter* CreatePainter(LockingMap& map, void* context)
{
  return new ConsolePainter(map);
}
void FreePainter(TourDeGiroPainter* painter)
{
  // this sucka is allocated in the global space, so no freeing needed
  ConsolePainter* pActualPainter = (ConsolePainter*)painter;
  delete pActualPainter;
}

void GameEntryPoint(const string& strMap, int mapLengthKm, int laps, int cAIs, int minWatts, int maxWatts, RACEMODE eRaceMode, int accountId)
{
  // use the boost sockets factory (alternative is pipe sockets factory, which is obviously only good for local play)
  ICommSocketFactoryPtr spFactory(CreateBoostSockets());

  // since this is a basic example, we will run the server locally.  In a real world setup, the server would be running somewhere else
  boost::shared_ptr<TourServer> spServer; // this needs to be refcounted (aka in a shared_ptr), or else all kinds of things get angry at us later
  {
    // set up our map list.  In this simple example, it's only one map.
    vector<MAPDATA> maps;
    maps.push_back(MAPDATA(strMap, mapLengthKm, laps));

    // Set up our database.  In this case, it's the null stats store that doesn't do anything.  In TdG's prime, we used a MySQL Stats Store.
    // In the Golden Cheetah future, we'd probably want to make a GoldenCheetahStatsStore.
    StatsStorePtr spNullStats(new NullStatsStore());
    spServer = boost::shared_ptr<TourServer>(new TourServer(spFactory, 0, 1, maps, spNullStats, 0, SERVER_UDP_IN_PORT, SERVER_TCP_CONNECT_PORT, cAIs, minWatts, maxWatts, eRaceMode, true, accountId));
    spServer->Init();
  }
  Sleep(2500);
  
  {
    TDGGameClient gameClient(spFactory);

    ConsoleTDGSetupState gameSetup("127.0.0.1");
    boost::shared_ptr<PlayerSetupData> spSetupData = boost::shared_ptr<PlayerSetupData>(new ConsolePlayerSetupData(POWERTYPE::CHEATING, 80, "Test"));
    gameSetup.AddPlayer(spSetupData);
  
    TDGConnectionResult eResult;
    if(gameClient.DoConnect(&gameSetup, &eResult))
    {
      gameClient.GameLoop();
    }
  }
}

void GetSettingsFromUI(string* pstrMapName, int* piMapLength, int* pcLaps, int* pcAIs, int* pMinAIWatts, int* pMaxAIWatts, int* piAccountId)
{
  // maybe your user signs in in your UI, and so they learn their account ID:
  *piAccountId = 5;

  // then they pick how many AIs they want to ride against and how strong they are
  *pcAIs = 35;
  *pMinAIWatts = 100;
  *pMaxAIWatts = 250;

  // then they choose how long they want to ride
  *pstrMapName = "test.gpx";
  *pcLaps = 2;
  *piMapLength = 5; // note that this is how long each LAP is.
}

int main(void)
{
  string strMapName;
  int mapLenKm = 0;
  int cLaps = 0;
  int cAIs = 0;
  int minAIWatts = 0;
  int maxAIWatts = 0;
  int accountId = 0;
  GetSettingsFromUI(&strMapName, &mapLenKm, &cLaps, &cAIs, &minAIWatts, &maxAIWatts, &accountId);

  GameEntryPoint(strMapName, 
    /*stretch/shrink map to km*/mapLenKm, 
    /*laps */cLaps,
    /* # of AI opponents */ cAIs, 
    /* minimum AI wattage */minAIWatts, 
    /* maximum AI wattage */maxAIWatts, 
    /* race/TT/other */RACEMODE_ROADRACE, 
    /* accountID */ accountId);
}