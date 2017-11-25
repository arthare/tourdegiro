#pragma once

#include "commstructs.h"

class SRTMSource;
class ArtSimpleSpline;
using namespace std;

#ifndef PI
#define PI 3.14159
#endif

class Map;
class IHeightMap
{
public:
  virtual void Init(const Map& src, const vector<SRTMCELL>& lstSRTM, int widthCells, int heightCells, bool fInitSub, const LATLON& llTopLeft, const LATLON& llBotRight) = 0;

  virtual float GetElevation(float lat, float lon) const = 0;
  virtual float GetElevation_Fast(int x, int y) const = 0;

  virtual int GetXCellCount() const = 0;
  virtual int GetYCellCount() const = 0;
  virtual void GetCellCorners(int x, int y, LATLON* pllTL, LATLON* pllTR, LATLON* pllBL, LATLON* pllBR, float* elevTL, float* elevTR, float* elevBL, float* elevBR) const = 0;
};

class InterpHeightMap : public IHeightMap
{
public:
  virtual ~InterpHeightMap() {};
  
  virtual void Init(const Map& src, const vector<SRTMCELL>& lstSRTM, int widthCells, int heightCells, bool fInitSub, const LATLON& llTopLeft, const LATLON& llBotRight) ARTOVERRIDE;
  
  virtual float GetElevation(float lat, float lon) const ARTOVERRIDE;
  virtual float GetElevation_Fast(int x, int y) const ARTOVERRIDE;

  virtual int GetXCellCount() const ARTOVERRIDE
  {
    return CELL_COUNT;
  }
  virtual int GetYCellCount() const ARTOVERRIDE
  {
    return CELL_COUNT;
  }
  virtual void GetCellCorners(int x, int y, LATLON* pllTL, LATLON* pllTR, LATLON* pllBL, LATLON* pllBR, float* elevTL, float* elevTR, float* elevBL, float* elevBR) const
  {
    const float flPctX = (float)x / (float)(CELL_COUNT);
    const float flPctY = (float)y / (float)(CELL_COUNT);
    const float flPctNextX = (float)(x+1) / (float)(CELL_COUNT);
    const float flPctNextY = (float)(y+1) / (float)(CELL_COUNT);

    const float flSpanX = llBR.flLat - llTL.flLat;
    const float flSpanY = llBR.flLon - llTL.flLon;

    const float flLat = flPctX*flSpanX + llTL.flLat;
    const float flLon = flPctY*flSpanY + llTL.flLon;
    const float flLatNext = flPctNextX*flSpanX + llTL.flLat;
    const float flLonNext = flPctNextY*flSpanY + llTL.flLon;

    if(pllTL) { pllTL->flLat = flLat;     pllTL->flLon = flLon;}
    if(pllTR) { pllTR->flLat = flLatNext; pllTR->flLon = flLon;}
    if(pllBL) { pllBL->flLat = flLat;     pllBL->flLon = flLonNext;}
    if(pllBR) { pllBR->flLat = flLatNext;     pllBR->flLon = flLonNext;}

    if(elevTL) *elevTL = GetElevation(flLat,flLon);
    if(elevTR) *elevTR = GetElevation(flLatNext,flLon);
    if(elevBL) *elevBL = GetElevation(flLat,flLonNext);
    if(elevBR) *elevBR = GetElevation(flLatNext,flLonNext);
  }
private:
  const static int CELL_COUNT = 128;

  vector<SRTMCELL> m_lstSRTM;
  unsigned int m_ixSRTMEnd; // where do the SRTM points end in our list?
  LATLON llTL;
  LATLON llBR;
};

struct SDL_Surface;
// a quadtree that, once computed, determines whether a given point is within a given range to a road
class DistanceDeterminer
{
  enum RANGERESULT
  {
    RANGE_DEFINRANGE, // this square knows that it is definitely in range to the road
    RANGE_DEFOUTRANGE_CLOSE, // this square knows that it is definitely out of range to the road because it is too close
    RANGE_DEFOUTRANGE_FAR, // this square knows that it is definitely out of range to the road because it is too far
    RANGE_MIXED, // this square doesn't know if it is in range or not (check children
  };
public:
  DistanceDeterminer(RECTF rcDims, int ixFirst, int ixLast) : m_rcDims(rcDims),m_ixFirst(ixFirst),m_ixLast(ixLast)
  {
    DASSERT(ixFirst <= ixLast);
    for(int x = 0;x < NUMELMS(m_children); x++)
    {
      m_children[x] = 0;
    }
    m_eRangeResult = RANGE_MIXED;
  }
  ~DistanceDeterminer()
  {
    for(unsigned x = 0;x < NUMELMS(m_children); x++)
    {
      if(m_children[x]) delete m_children[x];
    }
  }
  bool NeedRebuild(float flLow, float flHigh, const Map& map) const
  {
    MAPDATA newMap(flLow,flHigh,map);
    return !(newMap == m_curMap);
  }
  void Precompute(float flLow, float flHigh, const Map& map)
  {
    RANGERESULT rg[] = {RANGE_MIXED,RANGE_MIXED,RANGE_MIXED,RANGE_MIXED};
    RANGERESULT* prg[] = {&rg[0],&rg[1],&rg[2],&rg[3]};
    Precompute(flLow,flHigh,map,prg);
    m_curMap = MAPDATA(flLow,flHigh, map);
  }
  //void DrawOnSDL(SDL_Surface* pSurface, const MAPBOUNDS& bounds); // implementation commented out in TourDeGiroServer.cpp
  void Precompute(float flLow, float flHigh, const Map& map, RANGERESULT** rgPredone);
  
  bool IsInRangeToRoad(float flLat, float flLon) const;
private:

  // ixScanStart: where should all our children start looking through the map at?
  // ixScanEnd:
  void BuildChildren(float flLow, float flHigh, const Map& map, RANGERESULT* rgMyResults, int ixScanStart, int ixScanEnd);
private:
  DistanceDeterminer* m_children[4]; // top left, top right, bot left, bot right
  RECTF m_rcDims;
  RANGERESULT m_eRangeResult;
  int m_ixFirst,m_ixLast;

  struct MAPDATA
  {
    MAPDATA();
    MAPDATA(float flLow, float flHigh, const Map& map);
    bool operator == (const MAPDATA& other) const;

    MAPBOUNDS mb;
    float flLen;
    float flLow;
    float flHigh;
  };
  MAPDATA m_curMap;
};

class GPXPointSource : public LatLonSource
{
public:
  GPXPointSource(const std::string& path) : m_path(path) {};
  virtual ~GPXPointSource() {};

  virtual bool GetLatLonPoints(vector<MAPPOINT>& lstPoints, string& strMapName, int iStartPct, int iEndPct, int* piOwnerId, MAPCAP* pMapCap) ARTOVERRIDE;
private:
  const std::string m_path;
};

float DistanceInMeters(float flLat1,float flLon1, float flLat2, float flLon2);
class StatsStore;

class Map : public IMap
{
public:
  Map();
  Map(const TDGInitialState& initState);
  virtual ~Map() 
  {
  };

  boost::shared_ptr<TDGInitialState> GetInitialState() const {return m_tdgInitState;};

  void ComputeSwitchbacks();
  virtual bool LoadFromGPX(SRTMSource* pSRTMSource, const string& pszFile, int iOwnerId, int iMaxKm, int laps, int timedLengthSeconds, int iStartPercent, int iEndPercent, bool fPermitSprintClimbs);
  virtual bool LoadFromDB(SRTMSource* pSRTMSource, StatsStore* pStatsStore, const int iMapId, int iMaxKm, int laps, int timedLengthSeconds, int iStartPercent, int iEndPercent, bool fPermitSprintClimbs);
  virtual bool LoadFromSine(int iMaxKm, int laps, int timedLengthSeconds);
  bool IsValid() const;
  
  // returns a hash of this map's input data, so we can know if we need to rebuild it or not
  DWORD GetStateHash() const;

  void OneMoreLap(); // extends the length of this map by 1 lap

  const boost::shared_ptr<IHeightMap> GetHeightMap() const {return m_heightMap;}
  // note: for 3d vector stuff...
  // lat -> x coord (m_v[0])
  // lon -> z coord (m_v[2])
  // elev -> y coord (m_v[1])
  // for 2d vector stuff:
  // lat -> x coord (m_v[0])
  // lon -> y coord (m_v[1])
  string GetMapName() const {return m_strMapName;}
  string GetMapFile() const {return m_strMapFile;}
  MAPBOUNDS GetMapBounds() const; // returns the center point of the map (aka (max-min)/2 in each dimension)
  void GetDirectionAtDistance(const PLAYERDIST& flDistance, float* pflXDir, float* pflYDir) const ARTOVERRIDE;
  float GetElevationAtDistance(const PLAYERDIST& flDistance) const ARTOVERRIDE; // returns our elevation at a given distance
  ORIGDIST GetOrigDistAtDistance(float flDist) const ARTOVERRIDE;
  float GetDistanceOfOrigDist(const ORIGDIST& dist) const ARTOVERRIDE;

  float GetMaxSpeedAtDistance(const PLAYERDIST& flDistance) const ARTOVERRIDE;
  float GetRadiusAtDistance(const PLAYERDIST& flDistance) const ARTOVERRIDE;

  virtual int GetTimedLength() const {return m_timedLengthSeconds;}
  virtual int GetLaps() const ARTOVERRIDE;
  virtual float GetLapLength() const ARTOVERRIDE;

  
  virtual void GetScoringSources(std::vector<SprintClimbPointPtr>& lstSources) const ARTOVERRIDE;

  // IRoadRouteSupplier
  virtual PLAYERDIST GetRoadRouteStartDistance() const ARTOVERRIDE {return PLAYERDIST(0,0,GetLapLength());}
  virtual PLAYERDIST GetRoadRouteEndDistance() const ARTOVERRIDE {return PLAYERDIST(1,0,GetLapLength());}
  virtual float GetRoadRouteElevationAtDistance(const PLAYERDIST& flPos) const ARTOVERRIDE {return GetElevationAtDistance(flPos);}
  virtual LATLON GetLatLonAtDistance(const PLAYERDIST& flDistance) const ARTOVERRIDE;
  

  float GetSlopeAtDistance(const PLAYERDIST& flDistance) const ARTOVERRIDE; // returns our elevation at a given distance
  float GetNaturalDistance() const ARTOVERRIDE;
  PLAYERDIST GetEndDistance() const ARTOVERRIDE 
  {
    return PLAYERDIST(IsTimedMode(m_cLaps,this->m_timedLengthSeconds) ? max(1,m_cLaps+2) : m_cLaps,0,GetLapLength());
  } // returns how long this map is
  PLAYERDIST GetStartDistance() const ARTOVERRIDE {return PLAYERDIST(0,0,GetLapLength());}
  float GetClimbing() const; // returns how many meters of climbing there is on this map
  float GetMaxGradient() const;
  void RescaleMap(float flTargetMeters, vector<SRTMCELL>& lstSRTM);
  int GetOwnerId() const ARTOVERRIDE {return m_iOwnerId;}
  float GetRhoAtDistance(float flDistance) const ARTOVERRIDE
  {
    // at some point we'll calculate altitude and have this change, but for now...
    return 1.204f; // wikipedia says this is air at 20 degrees C
  }
  virtual void GetColor(float flDistance,ELEVCOLOR * pclrTop,ELEVCOLOR * pclrBottom) const ARTOVERRIDE
  {
    pclrTop->r = 0.5;
    pclrTop->g = 1;
    pclrTop->b = 0.1;
    pclrBottom->r = 0;
    pclrBottom->g = 0.3;
    pclrBottom->b = 0;
  }
  virtual int GetMapId() const ARTOVERRIDE
  {
    return m_iMapId;
  }
  void SetMapId(int iMapId)
  {
    m_iMapId = iMapId;
  }
  const vector<MAPPOINT>& GetAllPoints() const {return m_lstPoints;}

  // checks to see if the specced lat/lon is between rangeLow and rangeHigh from a road - used to place scenery not to close and not too far from the road
  bool IsInRangeToRoad(float lat, float lon, float rangeLow, float rangeHigh, int* pixFirst, int* pixLast, bool* pfTooClose) const;
  bool BuildFromInitState(const TDGInitialState& initState, bool fBuildHeightmap, float flNaturalDistance);
  static int m_cRangeCalls;
  static int m_cPointsChecked;

  static void Test();
private:
  bool ProcessLoadedMap(SRTMSource* pSRTMSource, int iScaleKm, const MAPCAP mapCap);
  void ComputeMapBounds();
  void BuildInitialState(TDGInitialState* initState, const vector<SRTMCELL>& lstSRTM) const;
  void GetPoints(const float flDistance, MAPPOINT* p1, MAPPOINT* p2) const;
  void GetOrigPoints(const ORIGDIST& dist, MAPPOINT* p1, MAPPOINT* p2) const;
  void InitSlopes();
  void InitCornerRadii();
  void BuildFakeSprintClimbs();
  int GetIndexOfDistance(int x, int max, float flSkipTo) const;
  friend class DistanceDeterminer;

  // this is private on PURPOSE.  You shouldn't commonly be making copies of the map.  They're big heavy objects, and there should be only
  // be one source of truth for the current map state.
  Map(const Map& map)
  {
    m_lstPoints = map.m_lstPoints;
    m_strMapName = map.m_strMapName;
    m_strMapFile = map.m_strMapFile;
    m_flNaturalDistance = map.m_flNaturalDistance;
    m_iOwnerId = map.m_iOwnerId;
    m_iMapId = map.m_iMapId;
    m_cLaps = map.m_cLaps;
    m_timedLengthSeconds = map.m_timedLengthSeconds;
    m_lstScoring = map.m_lstScoring;
    m_pSpline = map.m_pSpline;
    // note: on-purpose not copying heightmap.

    m_mb = map.m_mb;
    m_tdgInitState = map.m_tdgInitState;
    m_mapCap = map.m_mapCap;

    DASSERT(GetStateHash() == map.GetStateHash());
  }
protected:
  vector<MAPPOINT> m_lstPoints;
  string m_strMapName;
  string m_strMapFile;
  boost::shared_ptr<IHeightMap> m_heightMap;
  float m_flNaturalDistance;
  int m_iOwnerId;
  int m_iMapId;
  int m_cLaps; // how many laps are we doing of this map?
  int m_timedLengthSeconds; // how long are we riding?  note: if this is > 0, then m_cLaps MUST be < 0
  MAPCAP m_mapCap;

  MAPBOUNDS m_mb;
  boost::shared_ptr<TDGInitialState> m_tdgInitState;

  vector<SprintClimbPointPtr> m_lstScoring;

  boost::shared_ptr<ArtSimpleSpline> m_pSpline;

};

class LockingMap : public Map
{
public:
  LockingMap(ManagedCS* pcs) : m_pcs(pcs) {}
  void Lock() {m_pcs->lock();}
  void Unlock() {m_pcs->unlock();}
private:
  ManagedCS* m_pcs;
};

class AutoMapLocker
{
public:
  AutoMapLocker(LockingMap* pMap) : m_pMap(pMap)
  {
    pMap->Lock();
  }
  ~AutoMapLocker()
  {
    m_pMap->Unlock();
  }
private:
  LockingMap* m_pMap;
};

void DumpMap(const Map& map, ostream& out);