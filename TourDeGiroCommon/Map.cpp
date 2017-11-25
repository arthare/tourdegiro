
#include "stdafx.h"
#include "Map.h"
#include "Tools.h"
#include "CommStructs.h"
#include "StatsStore.h"
#include "SprintClimbImpl.h"

#define assert(x)
#include "ArtSimpleSpline.h"
#include <queue>


using namespace std;

float InterpolateElev(float lat, float lon, const vector<SRTMCELL>& lst, const int ixSRTMEnd);

DistanceDeterminer::MAPDATA::MAPDATA()
{
}
DistanceDeterminer::MAPDATA::MAPDATA(float flLow, float flHigh, const Map& map)
{
  this->flLow = flLow;
  this->flHigh = flHigh;
  mb = map.GetMapBounds();
  flLen = map.GetLapLength();
}
bool DistanceDeterminer::MAPDATA::operator == (const MAPDATA& other) const
{
  return memcmp(&mb,&other.mb,sizeof(mb)) == 0 && flLen == other.flLen && flLow == other.flLow && flHigh == other.flHigh;
}

inline float w(const SRTMCELL& pt, float lat, float lon)
{
  const float dlat = pt.flLat - lat;
  const float dlon = pt.flLon - lon;
  const float denon = dlat*dlat + dlon*dlon;
  if(denon != 0) 
  {
    return 1.0f / (denon*denon);
  }
  else
  {
    return 0;
  }

}

void InterpHeightMap::Init(const Map& src, const vector<SRTMCELL>& lstSRTM, int widthCells, int heightCells, bool fInitSub, const LATLON& llTopLeft, const LATLON& llBotRight)
{
  m_lstSRTM = lstSRTM;

  vector<MAPPOINT> lst = src.GetAllPoints();
  for(unsigned int x = 0;x < lst.size(); x++)
  {
    SRTMCELL srtm;
    srtm.flElev = lst[x].flElev;
    srtm.flLat = lst[x].flLat;
    srtm.flLon = lst[x].flLon;
    m_lstSRTM.push_back(srtm);
  }
  m_ixSRTMEnd = lstSRTM.size();

  llTL = llTopLeft;
  llBR = llBotRight;
}
float InterpHeightMap::GetElevation(float lat, float lon) const
{
  return InterpolateElev(lat,lon,m_lstSRTM,m_ixSRTMEnd);
}
float InterpHeightMap::GetElevation_Fast(int x, int y) const
{
  const float flPctX = (float)x / (float)(CELL_COUNT);
  const float flPctY = (float)y / (float)(CELL_COUNT);

  const float flSpanX = llBR.flLat - llTL.flLat;
  const float flSpanY = llBR.flLon - llTL.flLon;

  const float flLat = flPctX*flSpanX + llTL.flLat;
  const float flLon = flPctY*flSpanY + llTL.flLon;

  return GetElevation(flLat,flLon);
}

// lstPoints must be arranged as: 
//    0..ixSRTMEnd: SRTM cells
//    ixSRTMEnd..end(): map cells
float InterpolateElev(float lat, float lon, const vector<SRTMCELL>& lstPoints, const int ixSRTMEnd)
{
  bool fSave = false;
  if(fSave)
  {
    ofstream out;
    out.open("c:\\temp\\inputs.txt");
    out<<"lat = "<<lat<<endl;
    out<<"lon = "<<lon<<endl;
    out<<"lst = ["<<lstPoints.size()<<" points]"<<endl;
    out<<"srtmEnd = "<<ixSRTMEnd<<endl;
    for(unsigned int x = 0;x < lstPoints.size(); x++)
    {
      out<<"\t"<<lstPoints[x].flLat<<"\t"<<lstPoints[x].flLon<<"\t"<<lstPoints[x].flElev<<endl;
    }
    out.close();
  }
  float flSum = 0;
  const int ixSRTMStep = max(1,ixSRTMEnd/500);
  const int ixRoadStep = max(1,(int)(lstPoints.size() - ixSRTMEnd) / 4000);
  float flBottomSum = 0;
  const int cPoints = lstPoints.size();

  for(int j = 0;j < ixSRTMEnd; j+=ixSRTMStep)
  {
    const SRTMCELL& srtm = lstPoints[j];
    const float wRes = w(srtm,lat,lon);
    flBottomSum += wRes;
    flSum += wRes*srtm.flElev;
  }
  for(int j = ixSRTMEnd; j < cPoints; j+=ixRoadStep)
  {
    const SRTMCELL& srtm = lstPoints[j];
    const float wRes = w(srtm,lat,lon);
    flBottomSum += wRes;
    flSum += wRes*srtm.flElev;
  }
  flSum /= flBottomSum;

  return flSum;
}
Map::Map() : m_flNaturalDistance(0),m_iOwnerId(-1),m_iMapId(-1),m_cLaps(1),m_mapCap(0)
{
  memset(&m_mb,0,sizeof(m_mb));
}

float GetWeight(float flX, float flCenter, float flWidth)
{
  flX -= flCenter;
  if(flX < -flWidth) return 0;
  if(flX > flWidth) return 0;
  if(flX < 0)
  {
    return flX + flWidth;
  }
  else
  {
    return -flX + flWidth;
  }
}

void BuildSmoothMap(const vector<MAPPOINT>& input, vector<MAPPOINT>& output, ArtSimpleSpline* pSplineOut)
{
  if(input.size() <= 1) return;

  pSplineOut->clear();
  pSplineOut->setAutoCalculate(false);
  MAPPOINT ptLastInserted = input.front();
  NonOgreVector vPos(input.front().flLat,input.front().flElev,input.front().flLon);
  pSplineOut->addPoint(STOREDDATA(vPos,&input.front()));
  for(unsigned int x = 1;x < input.size(); x++)
  {
    const MAPPOINT& ptThis = input[x];
    DASSERT(ptThis.flOrigDist.v >= 0 && ptThis.flOrigDist.v <= 200000);

    if(ptThis.flDistance - ptLastInserted.flDistance > 50 || x == input.size()-1)
    {
      NonOgreVector vPos(input[x].flLat,input[x].flElev,input[x].flLon);
      pSplineOut->addPoint(STOREDDATA(vPos,&input[x]));

      DASSERT(ptThis.flOrigDist > ptLastInserted.flOrigDist); // if we have doubled-up origdists, we have trouble
      ptLastInserted = ptThis;
    }
  }

  pSplineOut->recalcTangents();

  const int kPoints = 2000;
  const float flInitialDistance = input.back().flDistance - input.front().flDistance;
  for(int x = 0; x < kPoints; x++)
  {
    const float flPctInMap = (float)x / (float)kPoints;
    const void* pvPrev=0;
    const void* pvNext=0;

    float flPctInSegment=0;
    NonOgreVector vOut = pSplineOut->interpolate(flPctInMap,&flPctInSegment,&pvPrev,&pvNext);

    MAPPOINT* pPrev = (MAPPOINT*)pvPrev;
    MAPPOINT* pNext = (MAPPOINT*)pvNext;
    if(!pPrev)
    {
      // we're at the very start of the map somehow
      pPrev = pNext;
      flPctInSegment = 1;
    }
    else if(!pNext)
    {
      // we're beyond the end of the map somehow
      pNext = pPrev;
      flPctInSegment = 0;
    }


    MAPPOINT ptNew;
    ptNew.flLat = vOut.x;
    ptNew.flLon = vOut.z;
    ptNew.flElev = vOut.y;
    ptNew.flDistance = flInitialDistance*flPctInMap;
    ptNew.flOrigDist = (1-flPctInSegment)*pPrev->flOrigDist + flPctInSegment*pNext->flOrigDist;
    ptNew.flRadius = 0;
    ptNew.flMaxSpeed = 0;
    ptNew.flRawLat = flPctInSegment*pNext->flRawLat + (1-flPctInSegment)*pPrev->flRawLat;
    ptNew.flRawLon = flPctInSegment*pNext->flRawLon + (1-flPctInSegment)*pPrev->flRawLon;
    ptNew.flSplineT = flPctInMap;
    output.push_back(ptNew);
  }
}

Map::Map(const TDGInitialState& initState): m_iOwnerId(-1),m_iMapId(-1),m_cLaps(1),m_mapCap(0)
{
  memset(&m_mb,0,sizeof(m_mb));
  BuildFromInitState(initState,false,0);
}

// rgPrecomputed is the precomputed values for each of our corners
void DistanceDeterminer::Precompute(float flLow, float flHigh, const Map& map, RANGERESULT** rgPrecomputed)
{
  if(m_rcDims.Width() < flLow/2 || m_rcDims.Height() < flLow/2 ||
     (flLow == 0 && m_rcDims.Width() < flHigh))
  {
    // we're too small.  just assume that we're not in range
    m_eRangeResult = RANGE_DEFOUTRANGE_CLOSE;
    return;
  }
  bool fRoadGoesThroughSquare = false;
  const vector<MAPPOINT>& lstMapPoints = map.GetAllPoints();
  int ixScanStart = m_ixFirst == -1 ? 0 : m_ixFirst;
  int ixScanEnd = m_ixLast == -1 ? lstMapPoints.size() : m_ixLast;
  for(int x = ixScanStart; x < ixScanEnd; x++)
  {
    const MAPPOINT& pt = lstMapPoints[x];
    if(m_rcDims.Contains(pt.flLat,pt.flLon))
    {
      fRoadGoesThroughSquare = true;
      break;
    }
    else
    {
      // figure out how far we are from the bounds, then advance that far
      float flXDist = 1e30f;
      float flYDist = 1e30f;
      if(pt.flLat <= m_rcDims.left)
      {
        flXDist = m_rcDims.left - pt.flLat;
      }
      else if(pt.flLat >= m_rcDims.right)
      {
        flXDist = pt.flLat - m_rcDims.right;
      }
      else if(pt.flLat >= m_rcDims.left && pt.flLat <= m_rcDims.right)
      {
        flXDist = 0;
      }
      else
      {
        DASSERT(FALSE); // it's either above, below, or within...
      }
      if(pt.flLon <= m_rcDims.top)
      {
        flYDist = m_rcDims.top - pt.flLon;
      }
      else if(pt.flLon >= m_rcDims.bottom)
      {
        flYDist = pt.flLon - m_rcDims.bottom;
      }
      else if(pt.flLon >= m_rcDims.top && pt.flLon <= m_rcDims.bottom)
      {
        flYDist = 0;
      }
      else
      {
        DASSERT(FALSE); // it's either above, below, or within
      }
      DASSERT(flXDist != 1e30f || flYDist != 1e30f);
      const float flAdvanceAmt = max(flXDist,flYDist); // we can advance at least flAdvanceAmt down the road
      const float flSkipTo = pt.flDistance + flAdvanceAmt;
      x = map.GetIndexOfDistance(x,ixScanEnd,flSkipTo);
    }
  }

  Vector2D lstCorners[4];
  lstCorners[0] = (V2D(m_rcDims.left,m_rcDims.top));
  lstCorners[1] = (V2D(m_rcDims.right,m_rcDims.top));
  lstCorners[2] = (V2D(m_rcDims.left,m_rcDims.bottom));
  lstCorners[3] = (V2D(m_rcDims.right,m_rcDims.bottom));

  int cInRange = 0;
  int cOutRangeClose = 0;
  int cOutRangeFar = 0;
  RANGERESULT rgMyRanges[4];
  for(unsigned int x = 0;x < 4; x++) 
  {
    rgMyRanges[x] = *rgPrecomputed[x]; // we don't know the value for each corner yet
    if(rgMyRanges[x] == RANGE_DEFINRANGE) cInRange++;
    if(rgMyRanges[x] == RANGE_DEFOUTRANGE_CLOSE) cOutRangeClose++;
    if(rgMyRanges[x] == RANGE_DEFOUTRANGE_FAR) cOutRangeFar++;
  }
  int ixNewStart = ixScanEnd;
  int ixNewEnd = ixScanStart;
  
  if(cInRange > 0 && (cOutRangeClose > 0 || cOutRangeFar > 0) ||
     fRoadGoesThroughSquare && (cOutRangeFar > 0 || cOutRangeClose > 0) ||
     cOutRangeFar > 0 && cOutRangeClose > 0)
  {
    // that's all we need to know, we're mixed
    ixNewStart = ixScanStart;
    ixNewEnd = ixScanEnd;
  }
  else
  {
    for(unsigned int x = 0;x < 4; x++)
    {
      if(cInRange > 0 && (cOutRangeFar > 0 || cOutRangeClose > 0))
      {
        // that's all we need to know, we're mixed
        break;
      }
      if(fRoadGoesThroughSquare && (cOutRangeFar > 0 || cOutRangeClose > 0))
      {
        // the road goes through us, and we have one out of range.  we're must be mixed
        break;
      }
      int iComputedFirst = m_ixFirst;
      int iComputedLast = m_ixLast;
      bool fTooClose = false;
      if(*rgPrecomputed[x] == RANGE_DEFINRANGE)
      {
        ixNewStart = min(m_ixFirst,ixNewStart);
        ixNewEnd = max(m_ixLast,ixNewEnd); // since we don't know what bounds came with this precomputation, we must assume the worst
        *rgPrecomputed[x] = rgMyRanges[x] = RANGE_DEFINRANGE;
        cInRange++;
      }
      else if((*rgPrecomputed[x] == RANGE_MIXED && map.IsInRangeToRoad(lstCorners[x].m_v[0],lstCorners[x].m_v[1],flLow,flHigh,&iComputedFirst,&iComputedLast,&fTooClose)))
      {
        ixNewEnd = max(iComputedLast,ixNewEnd);
        ixNewStart = min(iComputedFirst,ixNewStart);
        *rgPrecomputed[x] = rgMyRanges[x] = RANGE_DEFINRANGE;
        cInRange++;
      }
      else
      {
        if(iComputedLast == -1) iComputedLast = ixScanEnd;
        if(iComputedFirst == -1) iComputedFirst = ixScanStart;
        ixNewEnd = max(iComputedLast,ixNewEnd);
        ixNewStart = min(iComputedFirst,ixNewStart);
        *rgPrecomputed[x] = rgMyRanges[x] = fTooClose ? RANGE_DEFOUTRANGE_CLOSE : RANGE_DEFOUTRANGE_FAR;
        if(fTooClose) cOutRangeClose++;
        else cOutRangeFar++;
      }
    }
  }
  DASSERT(ixNewStart<=ixNewEnd);
  //m_ixFirst = ixNewStart;
  //m_ixLast = ixNewEnd;
  cInRange = 0;
  cOutRangeFar = 0;
  cOutRangeClose = 0;
  for(unsigned int x = 0;x < 4; x++) 
  {
    if(rgMyRanges[x] == RANGE_DEFINRANGE) cInRange++;
    if(rgMyRanges[x] == RANGE_DEFOUTRANGE_FAR) cOutRangeFar++;
    if(rgMyRanges[x] == RANGE_DEFOUTRANGE_CLOSE) cOutRangeClose++;
  }
  if(fRoadGoesThroughSquare)
  {
    // the road goes through our square.
    if(cOutRangeClose == 0 && cOutRangeFar == 0)
    {
      // all 4 of our points are in range to the road
      if(flLow == 0)
      {
        // the caller doesn't care about too-short distance (and since we contain the road, then we need to worry about too-short distances
        m_eRangeResult = RANGE_DEFINRANGE;
      }
      else
      {
        // all of our points are in range, but the road is in the middle.
        BuildChildren(flLow, flHigh,map, rgMyRanges, m_ixFirst, m_ixLast);
      }
    }
    else if(cOutRangeFar > 0 || cOutRangeClose > 0)
    {
      // at least one of our corners are too far from the road.  But we know we contain it, so we must be mixed
      BuildChildren(flLow,flHigh,map, rgMyRanges, m_ixFirst, m_ixLast);
    }
    else
    {
      DASSERT(FALSE); // should never happen.
      m_eRangeResult = RANGE_DEFOUTRANGE_FAR;
    }
  }
  else
  {
    // our square does not contain the road
    if(cInRange >= NUMELMS(lstCorners))
    {
      // all 4 of our points are in the correct range to the road
      m_eRangeResult = RANGE_DEFINRANGE;
    }
    else if(cOutRangeFar >= NUMELMS(lstCorners))
    {
      // all 4 of our points are out of range to the road
      m_eRangeResult = RANGE_DEFOUTRANGE_FAR;
    }
    else if(cOutRangeClose >= NUMELMS(lstCorners))
    {
      m_eRangeResult = RANGE_DEFOUTRANGE_CLOSE;
    }
    else
    {
      // we have a mixture of in-range and out-of-range points, so we gotta make babies
      BuildChildren(flLow,flHigh,map, rgMyRanges, m_ixFirst, m_ixLast);
    }
  }
}
void DistanceDeterminer::BuildChildren(float flLow, float flHigh, const Map& map, RANGERESULT* rgMyRanges, int ixFirst, int ixLast)
{
  RANGERESULT rgCorners[9];
  rgCorners[0] = rgMyRanges[0];       rgCorners[1] = RANGE_MIXED;       rgCorners[2] = rgMyRanges[1];
  rgCorners[3] = RANGE_MIXED;         rgCorners[4] = RANGE_MIXED;       rgCorners[5] = RANGE_MIXED;
  rgCorners[6] = rgMyRanges[2];       rgCorners[7] = RANGE_MIXED;       rgCorners[8] = rgMyRanges[8];

  m_eRangeResult = RANGE_MIXED;
  RANGERESULT* rgTopLeft[] = {&rgCorners[0],&rgCorners[1],&rgCorners[3],&rgCorners[4]};
  m_children[0] = new DistanceDeterminer(RECTF(m_rcDims.left,m_rcDims.top,m_rcDims.CenterX(),m_rcDims.CenterY()), ixFirst, ixLast);
  m_children[0]->Precompute(flLow,flHigh,map,rgTopLeft);
  // top right
  RANGERESULT* rgTopRight[] = {&rgCorners[1],&rgCorners[2],&rgCorners[4],&rgCorners[5]};
  m_children[1] = new DistanceDeterminer(RECTF(m_rcDims.CenterX(),m_rcDims.top,m_rcDims.right,m_rcDims.CenterY()), ixFirst, ixLast);
  m_children[1]->Precompute(flLow,flHigh,map,rgTopRight);
  // bottom left
  RANGERESULT* rgBotLeft[] = {&rgCorners[3],&rgCorners[4],&rgCorners[6],&rgCorners[7]};
  m_children[2] = new DistanceDeterminer(RECTF(m_rcDims.left,m_rcDims.CenterY(),m_rcDims.CenterX(),m_rcDims.bottom), ixFirst, ixLast);
  m_children[2]->Precompute(flLow,flHigh,map, rgBotLeft);
  // bottom right
  RANGERESULT* rgBotRight[] = {&rgCorners[4],&rgCorners[5],&rgCorners[7],&rgCorners[8]};
  m_children[3] = new DistanceDeterminer(RECTF(m_rcDims.CenterX(),m_rcDims.CenterY(),m_rcDims.right,m_rcDims.bottom), ixFirst, ixLast);
  m_children[3]->Precompute(flLow,flHigh,map, rgBotRight);
}
bool DistanceDeterminer::IsInRangeToRoad(float flLat, float flLon) const
{
  switch(m_eRangeResult)
  {
  case RANGE_DEFINRANGE: return true;
  case RANGE_DEFOUTRANGE_FAR: return false;
  case RANGE_DEFOUTRANGE_CLOSE: return false;
  case RANGE_MIXED:
  {
    if(flLat > m_rcDims.CenterX())
    {
      // right side
      if(flLon > m_rcDims.CenterY())
      {
        // bottom right
        return m_children[3] ? m_children[3]->IsInRangeToRoad(flLat,flLon) : false;
      }
      else
      {
        // top right
        return m_children[1] ? m_children[1]->IsInRangeToRoad(flLat,flLon) : false;
      }
    }
    else
    {
      // left side
      if(flLon > m_rcDims.CenterY())
      {
        // left bottom
        return m_children[2] ? m_children[2]->IsInRangeToRoad(flLat,flLon) : false;
      }
      else
      {
        return m_children[0] ? m_children[0]->IsInRangeToRoad(flLat,flLon) : false;
      }
    }
  }
  }
  DASSERT(FALSE);
  return false;
}

bool Map::BuildFromInitState(const TDGInitialState& initState, bool fBuildHeightmap, float flNaturalDistance)
{
  m_mapCap = initState.mapCap;
  m_timedLengthSeconds = initState.timedLengthSeconds;
  m_flNaturalDistance = flNaturalDistance;
  this->m_cLaps = initState.cLaps;

  m_tdgInitState = boost::shared_ptr<TDGInitialState>(new TDGInitialState(initState));
  if(initState.cPointsUsed <= 0) return false;

  vector<MAPPOINT> lstPoints;

  const int cch = sizeof(initState.szMapName);
  char szMapNameSafe[cch];
  strncpy(szMapNameSafe, initState.szMapName,sizeof(szMapNameSafe));
  szMapNameSafe[cch-1] = 0;

  m_strMapName = szMapNameSafe;
  float flMinElev = 1e30f;

  for(int x = 0;x < initState.cPointsUsed && x < TDGInitialState::NUM_MAP_POINTS; x++)
  {
    MAPPOINT pt;
    if(lstPoints.size() > 0)
    {
      const MAPPOINT& ptLast = lstPoints[lstPoints.size()-1];
      if(ptLast.flLat == initState.rgLat[x] && ptLast.flLon == initState.rgLon[x]) continue; // friends don't let friends use coincident map coords
    }
    pt.flLat = initState.rgLat[x];
    pt.flLon = initState.rgLon[x];
    pt.flDistance = initState.rgDistance[x];
    pt.flElev = initState.rgElevation[x];
    pt.flOrigDist = initState.rgOrigDist[x];
    DASSERT(pt.flOrigDist.v >= 0 && pt.flOrigDist.v <= 200000);
    lstPoints.push_back(pt);
    flMinElev = min(flMinElev,pt.flElev);
  }

  if( (IsTimedMode(GetLaps(),GetTimedLength()) || m_cLaps > 1) && // this is a multi-lap event
     IS_FLAG_SET(initState.mapCap,MAPCAP_LOOP)) // the map is appropriate to be looped
  {
    // do a road from the last point to the first
    const MAPPOINT& ptLast = lstPoints.back();
    MAPPOINT ptFirst;
    ptFirst.flLat = initState.rgLat[0];
    ptFirst.flLon = initState.rgLon[0];

    const float flDX = ptFirst.flLat - ptLast.flLat;
    const float flDY = ptFirst.flLon - ptLast.flLon;
    const float flAddedDistance = sqrt(flDX*flDX+flDY*flDY);
    ptFirst.flDistance = ptLast.flDistance + flAddedDistance;
    ptFirst.flElev = initState.rgElevation[0];
    ptFirst.flOrigDist.v = ptLast.flOrigDist.v + flAddedDistance;

    DASSERT(ptFirst.flOrigDist.v >= 0 && ptFirst.flOrigDist.v <= 200000);
    lstPoints.push_back(ptFirst);
  }


  const float flElevShift = max(0.0f,4 - flMinElev);
  for(unsigned int x = 0;x < lstPoints.size(); x++)
  {
    lstPoints[x].flElev += flElevShift;
  }

  m_lstPoints.clear();

  if(!m_pSpline)
  {
    m_pSpline = boost::shared_ptr<ArtSimpleSpline>(new ArtSimpleSpline());
  }
  else
  {
    m_pSpline->clear();
  }

  BuildSmoothMap(lstPoints, m_lstPoints, m_pSpline.get());


  InitSlopes();

  if(fBuildHeightmap)
  {
    vector<SRTMCELL> lstSRTM;
    for(int ixSRTM = 0; ixSRTM < initState.cSRTMPoints; ixSRTM++)
    {
      const SRTMCELL& srtm = initState.rgSRTM[ixSRTM];
      lstSRTM.push_back(srtm);
    }
    this->ComputeMapBounds();
    const MAPBOUNDS& mb = GetMapBounds();
    LATLON tl; // topleft
    LATLON br; // bottomright;

    float flBigDim = max(mb.flMaxLat - mb.flMinLat,mb.flMaxLon - mb.flMinLon);

    tl.flLat = mb.flCenterLat - flBigDim/2 - 500;
    tl.flLon = mb.flCenterLon - flBigDim/2 - 500; // +500/-500: make the heightmap larger than the actual map so we don't have "cliffs" of nothingness when the user rides near the bounds of the gpx file
    br.flLat = mb.flCenterLat + flBigDim/2 + 500;
    br.flLon= mb.flCenterLon + flBigDim/2 + 500;

    m_heightMap = boost::shared_ptr<IHeightMap>(new InterpHeightMap());
#ifdef _DEBUG
    m_heightMap->Init(*this,lstSRTM,64,64,false,tl,br);
#else
    m_heightMap->Init(*this,lstSRTM,256,256,false,tl,br);
#endif
  }
  ComputeMapBounds();

  
  { // for each scoring source, we need to put a checker in
    m_lstScoring.clear();
    for(int ixSource = 0;ixSource < initState.cScorePoints && ixSource < TDGInitialState::NUM_SCORE_POINTS; ixSource++)
    {
      const SPRINTCLIMBDATA_RAW& pt = initState.rgScorePoints[ixSource];

      const float flRealDistanceStart = this->GetDistanceOfOrigDist(pt.flOrigDistance - pt.flOrigLeadInDist);
      const float flRealDistanceEnd = this->GetDistanceOfOrigDist(pt.flOrigDistance);
      const float flEndDist = GetEndDistance().flDistPerLap;
      const float flRise = GetElevationAtDistance(PLAYERDIST(0,flRealDistanceEnd,flEndDist)) - GetElevationAtDistance(PLAYERDIST(0,flRealDistanceStart,flEndDist));;
      SprintClimbPointPtr spNewSource;
      switch(pt.eScoreType)
      {
      case SCORE_SPRINT:
        spNewSource = SprintClimbPointPtr(new SprintClimbImpl(pt, flRealDistanceEnd - flRealDistanceStart, flRealDistanceEnd, flRise, false));
        break;
      case SCORE_CLIMB:
        spNewSource = SprintClimbPointPtr(new SprintClimbImpl(pt, flRealDistanceEnd - flRealDistanceStart, flRealDistanceEnd, flRise, false));
        break;
      default:
        spNewSource.reset();
        break;
      }

      if(spNewSource)
      {
        this->m_lstScoring.push_back(spNewSource);
      }
    }

  }

  InitCornerRadii();

  return true;
}
void Map::OneMoreLap()
{
  m_cLaps++;
}
vector<MAPPOINT> BuildSwitchbacks(const vector<MAPPOINT>& lstSrc, int ixStart, int ixEnd)
{
  vector<MAPPOINT> lstRet;

  const MAPPOINT& ptStart = lstSrc[ixStart];
  const MAPPOINT& ptEnd = lstSrc[ixEnd-1];

  const float flRise = ptEnd.flElev - ptStart.flElev;
  const float flRun = ptEnd.flDistance - ptStart.flDistance;
  const float flSlope = flRise / flRun;

  // we need to get the slope down to 10% or thereabouts.  What distance expansion do we need?
  const float flGoalSlope = 0.1;
  const float flRatio = flSlope / flGoalSlope;
  const float flNeededDist = flRun * flRatio;
  
  if(flNeededDist < 40)
  {
    // no real purpose to build switchbacks here
    for(int x = ixStart;x < ixEnd; x++)
    {
      lstRet.push_back(lstSrc[x]);
    }
    return lstRet;
  }

  Vector2D vStart = V2D(ptStart.flLat,ptStart.flLon);
  Vector2D vEnd = V2D(ptEnd.flLat,ptEnd.flLon);
  Vector2D vDir = vEnd - vStart;
  Vector2D vPerpDir = vDir.RotateAboutOrigin(PI/2).Unit();
  

  int cSwitchesNeeded = (int)flRatio;
  const float flDistPerSwitchSimple = flNeededDist / cSwitchesNeeded;
  const float flDistPerSwitchMax = 100;
  float flDistPerSwitch = flDistPerSwitchSimple;
  if(flDistPerSwitchSimple > flDistPerSwitchMax)
  {
    // we don't want our switchbacks to be too big, or else they'll look really stupid
    flDistPerSwitch = flDistPerSwitchMax;
    // now we need to figure out how many switches we'll need at 50m each to achieve the needed gain
    const float flGainPerSwitch = flGoalSlope*flDistPerSwitch;
    cSwitchesNeeded = flRise / flGainPerSwitch;
  }

  bool fLeft = false;
  MAPPOINT ptLast = ptStart;
  Vector2D vLast = vStart;
  const float flPctStep = 1.0/(float)cSwitchesNeeded;

  lstRet.push_back(ptStart);
  for(int x = 0;x < cSwitchesNeeded;x++)
  {
    const float flPct = (((float)x)+flPctStep/2)/ (float)cSwitchesNeeded;
    Vector2D vAlong = vStart + vDir*flPct; // how far along the linear distance we want to go are we?  aka: if ptStart is (5,5) and ptEnd is (10,10), this'll be something like (6.67,6.67)
    Vector2D vSwitchEnd;
    if(fLeft) { vSwitchEnd = vAlong + vPerpDir*flDistPerSwitch/2; }
    else { vSwitchEnd = vAlong - vPerpDir*flDistPerSwitch/2; }

    const float flDistanceActuallyAdded = (vSwitchEnd - vLast).Length();

    MAPPOINT pt;
    pt.flLat = vSwitchEnd.m_v[0];
    pt.flLon = vSwitchEnd.m_v[1];
    pt.flElev = ptStart.flElev + flRise*flPct;
    pt.flDistance = ptLast.flDistance + flDistanceActuallyAdded;
    pt.flSlope = (pt.flElev - ptLast.flElev) / flDistanceActuallyAdded;
    pt.flOrigDist = (1-flPct)*ptStart.flOrigDist + flPct*ptEnd.flOrigDist;

    lstRet.push_back(pt);

    ptLast = pt;
    vLast = vSwitchEnd;

    fLeft = !fLeft;
  }
  
  return lstRet;
}

void InsertSwitchback(vector<MAPPOINT>& lstOut, const vector<MAPPOINT>& lstSwitch, int ixStart, int ixEnd)
{
  vector<MAPPOINT>::iterator iStart = lstOut.begin()+ixStart;
  vector<MAPPOINT>::iterator iEnd = lstOut.begin()+ixEnd-1;
  lstOut.erase(iStart,iEnd);

  iStart = lstOut.begin()+ixStart;
  lstOut.insert(iStart,lstSwitch.begin(),lstSwitch.end());
}

void Map::InitSlopes()
{
  { // making sure distances are proper
    float flCurDistance = 0;
    for(unsigned int x = 0; x < m_lstPoints.size(); x++)
    {
      if(x >= 1)
      {
        const float flDLat = m_lstPoints[x].flLat - m_lstPoints[x-1].flLat;
        const float flDLon = m_lstPoints[x].flLon - m_lstPoints[x-1].flLon;
        const float flDist = sqrt(flDLat*flDLat + flDLon*flDLon);
        flCurDistance += flDist;
      }
      m_lstPoints[x].flDistance = flCurDistance;
    }
  }
  

  for(unsigned int x = 0; x < m_lstPoints.size(); x++)
  {
    float flRise = 0;
    float flRun = 1;
    if(x > 0 && x < m_lstPoints.size() - 1)
    {
      flRun = m_lstPoints[x+1].flDistance - m_lstPoints[x-1].flDistance;
      flRise = m_lstPoints[x+1].flElev - m_lstPoints[x-1].flElev;
    }
    else if(x == 0)
    {
      flRun = m_lstPoints[x+1].flDistance - m_lstPoints[x].flDistance;
      flRise = m_lstPoints[x+1].flElev - m_lstPoints[x].flElev;
    }
    else if(x == m_lstPoints.size() - 1)
    {
      flRun = m_lstPoints[x].flDistance - m_lstPoints[x-1].flDistance;
      flRise = m_lstPoints[x].flElev - m_lstPoints[x-1].flElev;
    }
    m_lstPoints[x].flSlope = flRise / flRun;
  }

}
void Map::InitCornerRadii()
{
  const float flWeightSpan = 20;
  const float flLapLength = GetLapLength();
  
  for(unsigned int x = 0; x < m_lstPoints.size(); x++)
  {
    // now do corner radius calcs.  We average together and weight all the local angle changes
    const float pos = m_lstPoints[x].flDistance;
    
    float flX1,flY1,flX2,flY2;
    GetDirectionAtDistance(PLAYERDIST(0,pos-flWeightSpan,flLapLength),&flX1,&flY1);
    GetDirectionAtDistance(PLAYERDIST(0,pos+flWeightSpan,flLapLength),&flX2,&flY2);
    Vector2D vPrev;
    vPrev.m_v[0] = flX1;
    vPrev.m_v[1] = flY1;
    Vector2D vNext;
    vNext.m_v[0] = flX2;
    vNext.m_v[1] = flY2;
      
    const Vector2D vPerp = vPrev.RotateAboutOrigin(PI/2);
    const float flDP = vPerp.DP(vNext);
    const float flAngleBetween = vPrev.AngleBetween(vNext);
    const float flRadiansPerMeter = flAngleBetween / (flWeightSpan*2);
    const float flCircleCircumference = (PI)/flRadiansPerMeter;
    const float flCircleRadius = flCircleCircumference / (PI);
    if(flCircleRadius >= 0 && flCircleRadius < 10000)
    {
      m_lstPoints[x].flRadius = flDP < 0 ? -flCircleRadius : flCircleRadius;
    }
    else
    {
      m_lstPoints[x].flRadius = 10000;
    }

  }

  // precompute the speed limits for each mappoint.
  // since riders have to brake early, then we need to compute their braking curves coming into corners
  m_lstPoints[m_lstPoints.size()-1].flMaxSpeed = 1e30;
  for(int x = m_lstPoints.size()-2; x >= 0;x--)
  {
    // we know the radius of a corner.  Let's assume that the coef of friction for a bike tire is 0.8
    // CentripForce = m*v*v/R
    // v = sqrt(CentripForce*R / m) = sqrt(CentripAccel * R)
    // CentripForce = F[friction] = Coef*NormalForce = Coef*Mass*Gravity
    // CentripAccel = CentripForce / Mass = Coef*Gravity
    // v = sqrt(Coef*Gravity*R)
    const float flCornerSpeed = sqrt(0.8*9.81*abs(m_lstPoints[x].flRadius)); // option 1: they can go this fast due to cornering
    
    float flBrakeSpeed;
    { // figuring out braking.  imagine someone is doing this backwards on a rocket-powered bike, where the rockets add as much force as our brakes would subtract
      float flLastSpeed = m_lstPoints[x+1].flMaxSpeed;
      float flDist = m_lstPoints[x+1].flDistance - m_lstPoints[x].flDistance;
      float flAccel = 0.8*9.81;
      // kinematic equation: vf*vf = vi*vi + 2*a*d
      float flVFinalSquared = pow(flLastSpeed,2) + 2*flAccel*flDist;
      flBrakeSpeed = sqrt(flVFinalSquared);
    }
    m_lstPoints[x].flMaxSpeed = min(flBrakeSpeed,flCornerSpeed);
    m_lstPoints[x].flMaxSpeed = max(5.0f,m_lstPoints[x].flMaxSpeed); // make sure we don't get anything really weird
  }
}
void Map::BuildInitialState(TDGInitialState* initState, const vector<SRTMCELL>& lstSRTM) const
{
  initState->timedLengthSeconds = m_timedLengthSeconds;
  string strMapName = GetMapName();
  const int cch = min(sizeof(initState->szMapName)-1,strMapName.size()+1);
  strncpy(initState->szMapName,strMapName.c_str(),cch);

  const int cchInitState = sizeof(initState->szMapName);
  initState->szMapName[cchInitState-1]=0;
  initState->cLaps = m_cLaps;
  initState->mapCap = m_mapCap;

  const float flStart = 0;
  const float flEnd = GetLapLength();
  const float flSpan = flEnd - flStart;

  if(m_lstPoints.size() > TDGInitialState::NUM_MAP_POINTS)
  {
    initState->cPointsUsed = TDGInitialState::NUM_MAP_POINTS;
    // if our map has too many points, then just interpolate.  It sucks, but we have to.
    for(int x = 0;x < TDGInitialState::NUM_MAP_POINTS; x++)
    {
      const float flDist = ((x*flSpan)/TDGInitialState::NUM_MAP_POINTS) + flStart;
      const float flElev = GetElevationAtDistance(PLAYERDIST(0,flDist,GetLapLength()));
      const LATLON ll = GetLatLonAtDistance(PLAYERDIST(0,flDist,GetLapLength()));
      initState->rgLat[x] = ll.flLat;
      initState->rgLon[x] = ll.flLon;
      initState->rgDistance[x] = flDist;
      initState->rgElevation[x] = flElev;
      initState->rgOrigDist[x] = GetOrigDistAtDistance(flDist);
      DASSERT(initState->rgOrigDist[x].v >= 0 && initState->rgOrigDist[x].v <= 200000);
    }
  }
  else
  {
    // our map has fewer points than the TDGInitialState struct.  This means we can transmit it perfectly.
    initState->cPointsUsed = m_lstPoints.size();
    for(unsigned int x = 0;x < m_lstPoints.size(); x++)
    {
      initState->rgLat[x] = m_lstPoints[x].flLat;
      initState->rgLon[x] = m_lstPoints[x].flLon;
      initState->rgDistance[x] = m_lstPoints[x].flDistance;
      initState->rgElevation[x] = m_lstPoints[x].flElev;
      initState->rgOrigDist[x] = m_lstPoints[x].flOrigDist;
      DASSERT(initState->rgOrigDist[x].v >= 0 && initState->rgOrigDist[x].v <= 200000);
    }
  }

  // scoring source stuff
  initState->cScorePoints = m_lstScoring.size();
  for(unsigned int ixScore = 0; ixScore < m_lstScoring.size() && ixScore < TDGInitialState::NUM_SCORE_POINTS; ixScore++)
  {
    m_lstScoring[ixScore]->GetRaw(&initState->rgScorePoints[ixScore]);
  }

  // SRTM stuff
  if(lstSRTM.size() > TDGInitialState::NUM_SRTM_POINTS)
  {
    // we need to randomly select
    initState->cSRTMPoints = TDGInitialState::NUM_SRTM_POINTS;
    for(unsigned int x = 0;x < TDGInitialState::NUM_SRTM_POINTS; x++)
    {
      initState->rgSRTM[x] = lstSRTM[rand() % lstSRTM.size()];
    }
  }
  else
  {
    // we can just copy the points direct to the structure
    initState->cSRTMPoints = lstSRTM.size();
    for(unsigned int x = 0;x < lstSRTM.size(); x++)
    {
      initState->rgSRTM[x] = lstSRTM[x];
    }
  }
}

void FixOverlap(const Map& map, const vector<MAPPOINT> lstSrc, vector<MAPPOINT>& lstDst)
{
  lstDst.clear();

  const float flStartZipperDist = 150;
  const float flEscapeZipperDist = 225;

  int ixLast = 0;
  for(unsigned int x = 0;x < lstSrc.size(); x++)
  {
    int ixFirst = 0;
    while(lstSrc[ixLast].flDistance < lstSrc[x].flDistance - flStartZipperDist*3) ixLast++; // advances ixLast - ixFirst to ixLast should represent the road from the start until 45m from our point - we will always scan this amount of road looking for bits that are close to the "current point"
    
    if(ixLast > ixFirst)
    {
      const MAPPOINT& _pt = lstSrc[x];

      bool fTooClose = false;

      int ixTempLast = ixLast;
      if(map.IsInRangeToRoad(_pt.flLat,_pt.flLon,0,flStartZipperDist,&ixFirst,&ixTempLast,&fTooClose) && 
        abs(lstSrc[ixFirst].flElev - lstSrc[x].flElev) < 10) // we don't want to zipper up switchbacks
      {
        cout<<"point "<<x<<" at dist "<<lstSrc[x].flDistance<<" has entered the zipper"<<endl;
        // this guy is within zipper range.  so we want to find the closest point in [ixFirst,ixLast] to him and add it.
        // then we want to advance x until we escape from the zippered region.  At each point, we'll check if we've found a new matching point.  If we have, we'll add it to the output
        // we'll also check to see if we've escape the zipper distance from the other road.  If we have, then we'll break and go back to normal operation
        int ixLastClosest = -1;
        for(;x < lstSrc.size(); x++)
        {
          while(lstSrc[ixLast].flDistance < lstSrc[x].flDistance - flStartZipperDist*3) ixLast++; // advances ixLast - ixFirst to ixLast should represent the road from the start until 45m from our point - we will always scan this amount of road looking for bits that are close to the "current point"

          const MAPPOINT& pt = lstSrc[x];
          // first, let's see if pt has escaped...
          
          int ixTempFirst = 0;
          int ixTempLast = ixLast;
          {
            if(!map.IsInRangeToRoad(pt.flLat,pt.flLon,0,flEscapeZipperDist,&ixTempFirst,&ixTempLast,&fTooClose))
            {
              cout<<"point "<<x<<" at dist "<<pt.flDistance<<" has exited the zipper"<<endl;
              // this guy is out of the zipper range!  exit!
              break;
            }

            if(ixTempLast == -1)
            {
              ixTempLast = ixLast;
            }
          }


          int ixClosest = -1;
          float flClosest = 1e30;
          for(int j = ixTempFirst; j <= ixTempLast && j <= (int)lstSrc.size(); j++)
          {
            const float flDist = pt.DistTo(lstSrc[j]);
            if(flDist < flClosest)
            {
              flClosest = flDist;
              ixClosest = j;
            }
          }
          if(ixClosest != ixLastClosest && ixClosest >= 0 && ixClosest < (int)lstSrc.size())
          {
            // we've found a new point that is close to us.

            MAPPOINT ptNew = lstSrc[ixClosest];
            Vector2D vDir = ixClosest > 0 ? V2D(lstSrc[ixClosest].flLat - lstSrc[ixClosest-1].flLat,lstSrc[ixClosest].flLon - lstSrc[ixClosest-1].flLon) : 
                                            V2D(lstSrc[ixClosest+1].flLat - lstSrc[ixClosest].flLat,lstSrc[ixClosest+1].flLon - lstSrc[ixClosest].flLon);
            vDir = vDir.RotateAboutOrigin(-PI/2);
            vDir = vDir.Unit();
            ptNew.flLat += vDir.m_v[0] * ROAD_WIDTH*4;
            ptNew.flLon += vDir.m_v[1] * ROAD_WIDTH*4;
            ptNew.flOrigDist = pt.flOrigDist; // gotta keep our original distance through it all...

            DASSERT(!IsNaN(ptNew.flLat) && !IsNaN(ptNew.flLon));
            if(!IsNaN(ptNew.flLat) && !IsNaN(ptNew.flLon))
            {
              lstDst.push_back(ptNew);
            }
            ixLastClosest = ixClosest;
          }
        }
      }
      else
      {
        lstDst.push_back(lstSrc[x]);
      }
    }
    else
    {
      // we're not far enough to possibly have a zipper going on, so just add this to the out list
      lstDst.push_back(lstSrc[x]);
    }
  }
}

void Map::ComputeSwitchbacks()
{
  const float flMaxSlope = 0.15;

  { // put in fake switchbacks if needed.  if we see a bunch of consecutive points > 20% slope, then we want to build a series of switchbacks to keep the slope at/around 10%
    vector<MAPPOINT>::iterator iSwitchStart = m_lstPoints.end();
    bool fClimbing=false; // whether our current thing is going up or down
    MAPPOINT ptLast = m_lstPoints[0];
    for(unsigned int x = 1; x < m_lstPoints.size(); x++)
    {
      const MAPPOINT ptNow = m_lstPoints[x];
      float flSlope = 0;
      if(iSwitchStart != m_lstPoints.end())
      {
        const float flRise = ptNow.flElev - iSwitchStart->flElev;
        const float flRun = ptNow.flDistance - iSwitchStart->flDistance;
        flSlope = flRise / flRun;
      }
      else
      {
        const float flRise = ptNow.flElev - ptLast.flElev;
        const float flRun = ptNow.flDistance - ptLast.flDistance;
        flSlope = flRise / flRun;
      }
      //cout<<"slope @ "<<ptNow.flDistance<<" = "<<(flSlope*100)<<"%"<<endl;
      if(flSlope > flMaxSlope || flSlope < -flMaxSlope) // extreme slope detected
      {
        const bool fMatchedDir = (flSlope > flMaxSlope && fClimbing) || (flSlope < -flMaxSlope && !fClimbing);
        if(iSwitchStart == m_lstPoints.end())
        {
          // we haven't started a switchback sequence yet
          iSwitchStart = m_lstPoints.begin() + (x-1);
          fClimbing = flSlope > 0;
        }
        else
        {
          // we have started a switchback sequence.  if we're going in the same direction, then we want to just continue it.  else, we want to build a switchback series
          if(fMatchedDir)
          {
            // we're going in the same direction (up/down) as the current switchback series.  nothing to do
          }
          else
          {
            // we've changed direction.  build a switchback series
            const int iInsertStart = iSwitchStart-m_lstPoints.begin();
            vector<MAPPOINT> lstSwitchbacks = BuildSwitchbacks(m_lstPoints,iInsertStart,x);
            InsertSwitchback(m_lstPoints,lstSwitchbacks,iInsertStart,x);
            unsigned int cBefore = x;
            const int cAdded = lstSwitchbacks.size();
            const int cRemoved = x - 1 - iInsertStart;
            x = cBefore + cAdded - cRemoved; // we erased (x-1-iSwitchbackStart), and added lstSwitchbacks.size(), so we need to rejigger x
          }
        }
      }
      else
      {
        if(iSwitchStart != m_lstPoints.end())
        {
          unsigned int cBefore = x;
          const int iInsertStart = iSwitchStart-m_lstPoints.begin();
          vector<MAPPOINT> lstSwitchbacks = BuildSwitchbacks(m_lstPoints,iInsertStart,x);
          InsertSwitchback(m_lstPoints,lstSwitchbacks,iInsertStart,x);

          const int cAdded = lstSwitchbacks.size();
          const int cRemoved = x - 1 - iInsertStart;
          x = cBefore + cAdded - cRemoved; // we erased (x-1-iSwitchbackStart), and added lstSwitchbacks.size(), so we need to rejigger x
        }
        iSwitchStart = m_lstPoints.end();
      }

      ptLast = ptNow;
    }
  }

  
  { // making sure distances are proper after switchback insertion
    float flCurDistance = 0;
    for(unsigned int x = 0; x < m_lstPoints.size(); x++)
    {
      if(x >= 1)
      {
        const float flDLat = m_lstPoints[x].flLat - m_lstPoints[x-1].flLat;
        const float flDLon = m_lstPoints[x].flLon - m_lstPoints[x-1].flLon;
        float flDist = sqrt(flDLat*flDLat + flDLon*flDLon);
        if(flDist != flDist) // check for NaN values
        {
          flDist = 0;
        }
        flCurDistance += flDist;
        DASSERT(!IsNaN(flCurDistance) && !IsNaN(flDist));
      }
      m_lstPoints[x].flDistance = flCurDistance;
    }
  }
}

bool GPXPointSource::GetLatLonPoints(vector<MAPPOINT>& lstPoints, string& strMapName, int iStartPct, int iEndPct, int* piOwnerId, MAPCAP* pMapCap)
{
  lstPoints.clear();
  float flAccumDistance = 0;
  MAPPOINT ptLast = {0};


  FILE* fp =NULL;
  mxml_node_t* tree = NULL;
  ArtFopen(&fp,this->m_path.c_str(),"r");
  if(fp)
  {
    cout<<"opened "<<m_path<<endl;
    tree = mxmlLoadFile(NULL,fp,MXML_OPAQUE_CALLBACK);
    if(tree)
    {
      mxml_node_t* treeSegment = mxmlFindPath(tree,"gpx/trk/trkseg");
      bool fVeryFirst = true;

      float flLastAcceptedElev = 1e30f;
      while(treeSegment) // loop through all the trkpts
      {
        const char* pszNodeTitle;
        do 
        {
          pszNodeTitle = mxmlGetElement(treeSegment);
          if(!pszNodeTitle)
          {
            treeSegment = ::mxmlGetNextSibling(treeSegment);
          }
          else if(pszNodeTitle && !strcmp(pszNodeTitle,"trkseg"))
          {
            // oops, we got trkseg instead of trkpt
            treeSegment = ::mxmlGetFirstChild(treeSegment);
          }
          else if(!strcmp(pszNodeTitle,"trkpt"))
          {
            break;
          }
          else
          {
            // keep hunting I guess
            treeSegment = ::mxmlGetNextSibling(treeSegment);
          }
        } while(treeSegment);
        if(pszNodeTitle && strcmp(pszNodeTitle,"trkpt") == 0)
        {
          // loop through the children of this trkpt and extract lat/long/elev
          float flElev = 0;
          float flLat = 1e30f;
          float flLon = 1e30f;

          // found a trkpt!  let's iterate through its children
          const char* pszLat = ::mxmlElementGetAttr(treeSegment,"lat");
          const char* pszLon = ::mxmlElementGetAttr(treeSegment,"lon");
          if(pszLat) flLat = (float)atof(pszLat);
          if(pszLon) flLon = (float)atof(pszLon);

          mxml_node_t* currentTrkPt = ::mxmlGetFirstChild(treeSegment);
      
          while(currentTrkPt) // loop through childrenof trkpt
          {
            const char* nodeTitle = mxmlGetElement(currentTrkPt);
            if(nodeTitle != 0 && strcmp(nodeTitle,"ele") == 0)
            {
              // found the elevation node
              const char* psz = currentTrkPt && currentTrkPt->child ? currentTrkPt->child->value.opaque : NULL;
              if(psz != NULL && psz[0] != 0)
              {
                // elevation node had data for us.
                flElev = (float)atof(psz);
              }
            }
            currentTrkPt = mxmlGetNextSibling(currentTrkPt);
          }
          
          const float flSegmentLength = DistanceInMeters(ptLast.flLat,ptLast.flLon,flLat,flLon);
          if(flSegmentLength <= 0)
          {
            treeSegment = mxmlGetNextSibling(treeSegment);
            continue; // just skip it
          }

          if(flLastAcceptedElev >= 1e29f)
          {
            flLastAcceptedElev = flElev;
          }
          else if(abs(flElev) <= 0.0001)
          {
            const float flSlope = (flElev - flLastAcceptedElev) / flSegmentLength;
            if(abs(flSlope) > 1)
            {
              // too steep.  probably a messed-up point.
              treeSegment = mxmlGetNextSibling(treeSegment);
              continue; 
            }
          }

          if(fVeryFirst)
          {
            // first point.  no distance updates to make
          }
          else
          {
            flAccumDistance += flSegmentLength;
          }


          MAPPOINT pt;
          pt.flLat = pt.flRawLat = flLat;
          pt.flLon = pt.flRawLon = flLon;
          pt.flElev = flElev;
          pt.flDistance = pt.flOrigDist.v = flAccumDistance;

          if(lstPoints.size() > 0)
          {
            if(ptLast.flLat == pt.flLat && ptLast.flLon == pt.flLon) 
            {
              treeSegment = mxmlGetNextSibling(treeSegment);
              continue; // coincident points.  Screw off.
            }
          }
          lstPoints.push_back(pt);

          ptLast = pt;
          fVeryFirst = false;
        }
        treeSegment = mxmlGetNextSibling(treeSegment);
      }

      treeSegment = mxmlFindPath(tree,"gpx/trk/name");
      if(treeSegment && treeSegment->value.opaque[0])
      {
        const char* psz = treeSegment->value.opaque;
        strMapName = psz;
      }
      else
      {
        strMapName = m_path;
      }

      ::mxmlRelease(tree);
    }
    fclose(fp);
  }
  else
  {
    cout<<"Failed to open "<<m_path;
    return false;
  }

    
  if(iStartPct != 0 || iEndPct != 100)
  { // filter by flStartPercent and flEndPercent
    if(iEndPct <= iStartPct) return false;

    const float flStartPercent = (float)iStartPct/100.0f;
    const float flEndPercent = (float)iEndPct/100.0f;

    const float flEndDistance = lstPoints.back().flDistance;
    const float flStartDistance = lstPoints[0].flDistance;
    const float flOrigLength = flEndDistance - flStartDistance;
    const float flNewStartDistance = flStartDistance + flStartPercent*flOrigLength;
    const float flNewEndDistance = flStartDistance + flEndPercent*flOrigLength;
    vector<MAPPOINT> lstFiltered;
    for(unsigned int x = 0;x < lstPoints.size(); x++)
    {
      if(lstPoints[x].flDistance >= flNewStartDistance && lstPoints[x].flDistance <= flNewEndDistance)
      {
        lstFiltered.push_back(lstPoints[x]);
      }
    }

    if(lstFiltered.size() <= 1) return false;

    float flRunningDistance = 0;
    for(unsigned int x = 0;x < lstFiltered.size()-1; x++)
    {
      const float flGap = lstFiltered[x+1].flDistance - lstFiltered[x].flDistance;
      lstFiltered[x].flDistance = flRunningDistance;
      flRunningDistance += flGap;
    }
    lstFiltered.back().flDistance = flRunningDistance;

    lstPoints = lstFiltered;

    // also change the name
    char szNewName[500];
    _snprintf(szNewName,sizeof(szNewName),"[%d-%d%%]%s",iStartPct,iEndPct,strMapName.c_str());
    strMapName = szNewName;
  }
    

  return true;
}

void FillGaps(const vector<MAPPOINT>& lstIn, vector<MAPPOINT>& lstOut)
{
  MAPPOINT ptLastOutputted = lstIn.front();
  lstOut.push_back(ptLastOutputted);
  for(unsigned int x = 1;x < lstIn.size(); x++)
  {
    const MAPPOINT& ptNow = lstIn[x];
    const float flGap = ptNow.flDistance - ptLastOutputted.flDistance;

    if(flGap > 50)
    {
      // we need to fill the gap.  Let's aim for 15m gaps
      const int cSteps = flGap / 15.0f;
      const float flStep = 1.0f / (float)cSteps;
      for(float flPct = 0; flPct <= 1.0f; flPct += flStep)
      {
        MAPPOINT ptInter = MAPPOINT::Blend(ptLastOutputted,ptNow,1.0-flPct);
        lstOut.push_back(ptInter);
      }
      ptLastOutputted = lstOut.back();
    }
    else if(flGap < 1.0)
    {
      // this gap is way too small.  don't output anything
    }
    else
    {
      lstOut.push_back(ptNow);
      ptLastOutputted = lstOut.back();
    }
  }

}

void SmoothenElevations(vector<MAPPOINT>& lstPoints)
{
  // algorithm:
  // go until we find a different elevation
  // if that elevation is more than 1 point away, interpolate all points between
  // this was put in place to fix "Joe Blow"/Steve Huddleston's map that he submitted on Dec 19
  int ixStart = 0;
  for(unsigned int x = 1; x < lstPoints.size(); x++)
  {
    if(lstPoints[x].flElev != lstPoints[ixStart].flElev)
    {
      const MAPPOINT& ptStart = lstPoints[ixStart];
      const MAPPOINT& ptNow = lstPoints[x];
      // found a difference.  over how long?
      if(x - ixStart > 2)
      {
        cout<<"Smoothened from "<<ptStart.flDistance<<"m to "<<ptNow.flDistance<<"m"<<endl;
        // this was a long-enough streak of elevations.  Let's interpolate
        const float flElevStart = ptStart.flElev;
        const float flElevEnd = ptNow.flElev;
        const float flSpan = ptNow.flDistance - ptStart.flDistance;
        for(unsigned int y = ixStart; y < x; y++)
        {
          const float flNowDist = lstPoints[y].flDistance;
          const float flPct = (flNowDist - ptStart.flDistance) / (flSpan); // 0 when we're at ptStart, 1 when we're at ptNow
          const float flElevNow = flPct*flElevEnd + (1-flPct)*flElevStart;
          lstPoints[y].flElev = flElevNow;
        }
      }
      ixStart = x; // the new start is here
    }
  }
}

struct DELTATRACKER
{
  DELTATRACKER(float flDist,float flDelta) : flDist(flDist),flDelta(flDelta) {};
  const float flDist;
  const float flDelta;
};
void Map::BuildFakeSprintClimbs()
{
  m_lstScoring.clear();

  {
    // climbs: we'll go through the map at 50m intervals and put all qualifying climbs in
    MAPPOINT ptClimbStart = m_lstPoints.front();
    MAPPOINT ptLast = m_lstPoints.front();
    for(unsigned int x = 0;x < m_lstPoints.size(); x++)
    {
      const MAPPOINT& pt = m_lstPoints[x];

      const float flDistSinceLast = pt.flDistance - ptLast.flDistance;
      if(flDistSinceLast < 133) continue; // don't look too close together, or else bumps or short flats might throw off longer-term things

      if(pt.flElev > ptLast.flElev)
      {
        // the climb continues!
      }
      else
      {
        // the current climb has ended
        // if it is longer than 500m and with a grade over 3%, then let's put it in!
        const float flRise = ptLast.flElev - ptClimbStart.flElev;
        const float flRun = ptLast.flDistance - ptClimbStart.flDistance;
        if(flRun > 500.0 && (flRise / flRun) > 0.03f)
        {
          // long enough
          SPRINTCLIMBDATA_RAW raw;
          raw.eScoreType = SCORE_CLIMB;
          raw.flMaxPoints = 0;
          raw.flOrigDistance = ptLast.flOrigDist;
          raw.flOrigLeadInDist = ptLast.flOrigDist - ptClimbStart.flOrigDist;
          raw.fIsFinish = false;

          char szSpot[200];
          FormatDistance(szSpot,sizeof(szSpot),ptLast.flDistance,1,METRIC,true);

          char szHeight[200];
          FormatDistanceShort(szHeight,sizeof(szHeight),flRise,0,METRIC);

          snprintf(raw.szName,sizeof(raw.szName),"%s climb @ %s",szHeight,szSpot);
        
          m_lstScoring.push_back(SprintClimbPointPtr(new SprintClimbImpl(raw,ptLast.flDistance-ptClimbStart.flDistance, ptLast.flDistance, flRise ,false)));
        }

        ptClimbStart = pt;
      }
      ptLast = pt;
    }
  }


  { // let's add some sprints too
    int cSprintsToAdd = 0;
    const float flDistPerLap = GetEndDistance().flDistPerLap;
    if(flDistPerLap < 5000)
    {
      cSprintsToAdd = 1;
    }
    else if(flDistPerLap < 10000)
    {
      cSprintsToAdd = 2;
    }
    else if(flDistPerLap < 50000)
    {
      cSprintsToAdd = 3;
    }
    else
    {
      cSprintsToAdd = 4;
    }

    const float flLongEnough = 500.0;
    vector<std::pair<MAPPOINT,MAPPOINT> > lstPossibleSprintPoints; // the pair includes the start and end point of the sprint
    const MAPPOINT* pt500mAgo = &m_lstPoints.front();
    const MAPPOINT* ptLast = &m_lstPoints.front();

    queue<DELTATRACKER> qCurSprint; // a queue of all the distances and deltas that make up the current sprint
    double dCurSprintClimbDeltaHeight=0; // a sum of all the climbing and descending that happens in the current sprint
    for(unsigned int x = 0;x < m_lstPoints.size(); x++)
    {
      const MAPPOINT& ptNow = m_lstPoints[x];
      double dCurSprintClimbDeltaRun = 1;
      { // figuring out total up/down amounts
        const double dDeltaSinceLast = abs(ptNow.flElev - ptLast->flElev);
        dCurSprintClimbDeltaHeight += dDeltaSinceLast; // up or down, we want to the amount of elevation changes
        qCurSprint.push(DELTATRACKER(ptNow.flDistance,dDeltaSinceLast));

        while(ptNow.flDistance - qCurSprint.front().flDist > flLongEnough)
        {
          dCurSprintClimbDeltaHeight -= qCurSprint.front().flDelta;
          qCurSprint.pop();
        }

        dCurSprintClimbDeltaRun = ptNow.flDistance - qCurSprint.front().flDist;
      }
      ptLast = &ptNow;

      const float flRise = ptNow.flElev - pt500mAgo->flElev;
      float flRun = ptNow.flDistance - pt500mAgo->flDistance;
      if(flRun > flLongEnough)
      {
        // we've got a long-enough span here
        if(abs(flRise/flRun) <= 0.01 && abs(dCurSprintClimbDeltaHeight / dCurSprintClimbDeltaRun) < 0.02)
        {
          // this is flat enough to be a qualifying potential sprint point
          lstPossibleSprintPoints.push_back(pair<MAPPOINT,MAPPOINT>(*pt500mAgo,ptNow));
        }
      }

      if(flRun > flLongEnough)
      {
        while(flRun > flLongEnough)
        {
          pt500mAgo++;
          flRun = ptNow.flDistance - pt500mAgo->flDistance;
        }
        pt500mAgo--; // this should get us back to >500m separation
        DASSERT(ptNow.flDistance - pt500mAgo->flDistance >= 500.0);
        DASSERT(pt500mAgo >= &m_lstPoints.front());
      }
    }
    while(cSprintsToAdd > 0 && lstPossibleSprintPoints.size() > 0)
    {
      // let's go through the map, find a sprint-qualified spot, and put a sprint there
      // sprint-qualified means:
      // -500m of close-to-flat (less than +/- 2%)
      // not within 1km of another sprint point
      const int ixSpot = rand() % lstPossibleSprintPoints.size(); // this picks where we're putting it

      { // adding the sprint point
        const MAPPOINT& ptStart = lstPossibleSprintPoints[ixSpot].first;
        const MAPPOINT& ptEnd = lstPossibleSprintPoints[ixSpot].second;

        DASSERT(ptEnd.flDistance >= ptStart.flDistance);

        SPRINTCLIMBDATA_RAW raw;
        raw.eScoreType = SCORE_SPRINT;
        raw.flMaxPoints = 0;
        raw.flOrigDistance = ptEnd.flOrigDist;
        raw.flOrigLeadInDist = ptEnd.flOrigDist - ptStart.flOrigDist;
        raw.fIsFinish = false;
        
        char szSpot[200];
        FormatDistance(szSpot,sizeof(szSpot),ptEnd.flDistance,1,METRIC,true);

        const float flLength = ptEnd.flDistance - ptStart.flDistance;
        char szHeight[200];
        FormatDistanceShort(szHeight,sizeof(szHeight),flLength,0,METRIC);

        snprintf(raw.szName,sizeof(raw.szName),"%s sprint @ %s",szHeight,szSpot);
        
        m_lstScoring.push_back(SprintClimbPointPtr(new SprintClimbImpl(raw,ptEnd.flDistance-ptStart.flDistance, ptEnd.flDistance, ptEnd.flElev-ptStart.flElev ,false)));
        cSprintsToAdd--;

        { // removing candidates that are near the sprint point we just added
          for(int x = lstPossibleSprintPoints.size()-1; x >= 0; x--)
          {
            const MAPPOINT& ptTest1 = lstPossibleSprintPoints[x].second;
            const MAPPOINT& ptTest2 = lstPossibleSprintPoints[x].first;
            if(abs(ptTest1.flDistance-ptStart.flDistance) < 1000.0 || 
               abs(ptTest1.flDistance-ptEnd.flDistance) < 1000.0 ||
               abs(ptTest2.flDistance-ptStart.flDistance) < 1000.0 || 
               abs(ptTest2.flDistance-ptEnd.flDistance) < 1000.0)
            {
              lstPossibleSprintPoints.erase(lstPossibleSprintPoints.begin()+x);
            }
          }
        }
      }
    }
  }

  { // adding finish sprint
    if(!IsTimedMode(GetLaps(),GetTimedLength())) // no point having a finish sprint for a timed-mode ride
    { // and finally, one for the finish
      SPRINTCLIMBDATA_RAW finish;
      finish.eScoreType = SCORE_SPRINT;
      finish.flMaxPoints = GetLaps()*(100.0f/3.0f); // 33.3333...
      const float flFinishSpot = GetEndDistance().flDistPerLap - 5;
      DASSERT(flFinishSpot > 0);
      finish.flOrigDistance = this->GetOrigDistAtDistance(flFinishSpot);
      finish.fIsFinish = true;
      DASSERT(finish.flOrigDistance < m_lstPoints.back().flOrigDist);

      const float flFinishLeadInSpot = max(flFinishSpot*0.25f, flFinishSpot-500.0f);
      finish.flOrigLeadInDist = finish.flOrigDistance - GetOrigDistAtDistance(flFinishLeadInSpot);
      DASSERT(finish.flOrigLeadInDist.v > 0);

      strncpy(finish.szName, "Finish Sprint", sizeof(finish.szName));
      const float flRise = GetElevationAtDistance(PLAYERDIST(0,flFinishSpot,flFinishSpot)) - GetElevationAtDistance(PLAYERDIST(0,flFinishLeadInSpot,flFinishSpot));
      m_lstScoring.push_back(SprintClimbPointPtr(new SprintClimbImpl(finish, flFinishSpot - flFinishLeadInSpot, flFinishSpot, flRise, true)));
    }
  }
}

void ValidatePoints(const vector<MAPPOINT>& points)
{
  for(unsigned int x = 0;x < points.size(); x++) DASSERT(!::IsNaN(points[x].flDistance) && !::IsNaN(points[x].flElev) && !::IsNaN(points[x].flLat) &&!::IsNaN(points[x].flLon));
  for(unsigned int x = 0;x < points.size(); x++) DASSERT(points[x].flOrigDist.v >= 0 && points[x].flOrigDist.v <= 200000);
}

bool Map::ProcessLoadedMap(SRTMSource* pSRTMSource, int iScaleKm, const MAPCAP mapCap)
{
  ::ValidatePoints(m_lstPoints);
  m_mapCap = mapCap;

  m_pSpline.reset();
  bool fRet = false;

  vector<MAPPOINT> lstDeDuplicated;
  // let's remove all duplicate points
  for(int x = 1;x < m_lstPoints.size(); x++)
  {
    if(m_lstPoints[x].flLat == m_lstPoints[x-1].flLat && 
       m_lstPoints[x].flLon == m_lstPoints[x-1].flLon &&
       m_lstPoints[x].flElev == m_lstPoints[x-1].flElev)
    {
      // exact same point. skip
    }
    else
    {
      // _something_ changed
      lstDeDuplicated.push_back(m_lstPoints[x]);
    }
  }
  m_lstPoints = lstDeDuplicated;

  m_flNaturalDistance = m_lstPoints.back().flDistance;

  vector<SRTMCELL> lstSRTM; // gotta build this from the DB
  if(m_lstPoints.size() > 0)
  { // processing lat/longs in degrees into eastings and southings
    float flMinLat = 1e30f;
    float flMinLon = 1e30f;
    float flMaxLat = -1e30f;
    float flMaxLon = -1e30f;
    for(unsigned int x = 0; x < m_lstPoints.size(); x++)
    {
      flMinLat = min(flMinLat,m_lstPoints[x].flLat);
      flMinLon = min(flMinLon,m_lstPoints[x].flLon);
      flMaxLat = max(flMaxLat,m_lstPoints[x].flLat);
      flMaxLon = max(flMaxLon,m_lstPoints[x].flLon);
    }

    for(unsigned int x = 0; x < m_lstPoints.size(); x++)
    {
      MAPPOINT& pt = m_lstPoints[x];
      pt.flLat = DistanceInMeters(flMinLat,flMinLon,pt.flLat,flMinLon);
      pt.flLon = DistanceInMeters(flMinLat,flMinLon,flMinLat,pt.flLon);

      DASSERT(pt.flOrigDist.v >= 0 && pt.flOrigDist.v <= 200000);
    }
    ::ValidatePoints(m_lstPoints);

    if(pSRTMSource)
    {
      pSRTMSource->GetPoints(flMinLat,flMinLon,flMaxLat,flMaxLon, TDGInitialState::NUM_SRTM_POINTS, lstSRTM);
      if(lstSRTM.size() > 0)
      {
        cout<<"found "<<lstSRTM.size()<<" points"<<endl;
      }
      for(unsigned int x = 0; x < lstSRTM.size(); x++)
      {
        SRTMCELL& pt = lstSRTM[x];
        pt.flLat = DistanceInMeters(flMinLat,flMinLon,pt.flLat,flMinLon);
        pt.flLon = DistanceInMeters(flMinLat,flMinLon,flMinLat,pt.flLon);
      }
    }
    fRet = true;
  }
  if(iScaleKm <= 0)
  {
    iScaleKm = (GetLapLength())/1000.0f;
  }
  // scaling the map.  If they wanted iMaxKm and the map is n km long, then we should scale every down by n/iMaxKm

  if(iScaleKm > 0)
  {
    RescaleMap(iScaleKm*1000,lstSRTM); // we need to rescale before going to fixing overlap
  }
  ::ValidatePoints(m_lstPoints);
  
  for(unsigned int x = 1;x < m_lstPoints.size(); x++) DASSERT(m_lstPoints[x].flOrigDist.v >= 0 && m_lstPoints[x].flOrigDist.v <= 200000 && m_lstPoints[x].flOrigDist > m_lstPoints[x-1].flOrigDist);
  /*{ // making sure there aren't long segments that don't have points, and making sure there aren't multiple points super-close together
    vector<MAPPOINT> lstGapFilled;
    FillGaps(m_lstPoints,lstGapFilled);
    m_lstPoints = lstGapFilled;
  }*/

  if(!IS_FLAG_SET(mapCap,MAPCAP_NOZIPPER))
  { // FixOverlap attempts to make wobbly GPS traces into arrow-straight, evenly-separated roads
    vector<MAPPOINT> lstOut;
    FixOverlap(*this,m_lstPoints,lstOut);
    m_lstPoints = lstOut;
  }
  ::ValidatePoints(m_lstPoints);

  // re-evaluate distances after fixing overlaps
  float flSumDist = 0;
  m_lstPoints[0].flDistance = 0;
  for(unsigned int x = 1;x  < m_lstPoints.size(); x++)
  {
    DASSERT(m_lstPoints[x].flOrigDist.v >= 0 && m_lstPoints[x].flOrigDist.v <= 200000);
    const float flDX = m_lstPoints[x].flLat - m_lstPoints[x-1].flLat;
    const float flDY = m_lstPoints[x].flLon - m_lstPoints[x-1].flLon;
    const float flDist = sqrt(flDX*flDX + flDY*flDY);
    DASSERT(!IsNaN(flDist));

    flSumDist += flDist;
    DASSERT(!IsNaN(flSumDist));

    m_lstPoints[x].flDistance = flSumDist;
    DASSERT(!IsNaN(m_lstPoints[x].flDistance));

    
  }
  ValidatePoints(m_lstPoints);

  SmoothenElevations(m_lstPoints); // makes sure extended sections of the exact same elevation get turned into steady slopes, before we do crazy switchback stuff.
  
  ValidatePoints(m_lstPoints);
  InitSlopes();
  
  ValidatePoints(m_lstPoints);
  ComputeSwitchbacks();
  
  ValidatePoints(m_lstPoints);
  if(iScaleKm != 0)
  {
    const float flTargetDist = iScaleKm*1000;
    float flLow = flTargetDist*0.75;
    float flHigh = flTargetDist*1.25;

    Map mapRemembered = *this;
    vector<SRTMCELL> lstSRTMBackup = lstSRTM;
    while(true) // do a binary search expanding/contracting the requested distance until BuildFromInitState builds a map that is a distance we like
    {
      *this = mapRemembered;
      lstSRTM = lstSRTMBackup;
      const float flDistanceToTry = (flLow + flHigh)/2;
      RescaleMap(flDistanceToTry,lstSRTM); // we need to rescale before going to fixing overlap

      ValidatePoints(m_lstPoints);

      boost::shared_ptr<TDGInitialState> tempState = boost::shared_ptr<TDGInitialState>(new TDGInitialState());
      BuildInitialState(tempState.get(),lstSRTM);
      if(!BuildFromInitState(*tempState, false,m_flNaturalDistance)) return false;

      const float flResult = GetLapLength();
      cout<<"Requested "<<flDistanceToTry<<", got "<<flResult<<endl;
      if( abs(flResult - flTargetDist) < 0.001*flTargetDist || (flHigh - flLow) < 0.001*flTargetDist)
      {
        // close enough, or we're out of attempts
        break;
      }
      else if(flResult < flTargetDist)
      {
        flLow = flDistanceToTry;
      }
      else if(flResult > flTargetDist)
      {
        flHigh = flDistanceToTry;
      }
    }
  }
  // map is now perfectly scaled.  Let's figure out corner radiuses and maximum cornering speeds...
  InitCornerRadii();

  if(!IS_FLAG_SET(mapCap,MAPCAP_NOSPRINTCLIMB))
  {
    // let's put in some fake sprints and climbs to make sure that there actually are some.
    BuildFakeSprintClimbs();
  }

  // now that we know how long the map is, and how long each climb will be, let's figure out the weightings for each of the sprint/climb segments!
  {
    int rgCounts[SCORE_COUNT] = {0};
    float flTotalClimbing = 0;
    const float flPerCategory = 100.0f/3.0f;
    for(unsigned int ixSprintClimb = 0;ixSprintClimb < this->m_lstScoring.size(); ixSprintClimb++)
    {
      SPRINTCLIMBDATA_RAW raw;
      m_lstScoring[ixSprintClimb]->GetRaw(&raw);
      if(!m_lstScoring[ixSprintClimb]->IsFinishMode()) rgCounts[raw.eScoreType]++;
      if(raw.eScoreType == SCORE_CLIMB)
      {
        const ORIGDIST flStartOrigDist = raw.flOrigDistance - raw.flOrigLeadInDist;
        const float flStartDist = GetDistanceOfOrigDist(flStartOrigDist);
        const float flStartElev = this->GetElevationAtDistance(PLAYERDIST(0,flStartDist,GetLapLength()));

        const float flEndDist = GetDistanceOfOrigDist(raw.flOrigDistance);
        const float flEndElev = GetElevationAtDistance(PLAYERDIST(0,flEndDist,GetLapLength()));

        flTotalClimbing += max(0.0f,(flEndElev - flStartElev));
      }
    }

    float flSprintTotalWeight = 0;
    if(rgCounts[SCORE_SPRINT] > 0)
    {
      flSprintTotalWeight = flTotalClimbing > 0 ? flPerCategory : flPerCategory*2;
    }
    float flClimbTotalWeight = 0;
    if(flTotalClimbing > 0)
    {
      flClimbTotalWeight = rgCounts[SCORE_SPRINT] > 0 ? flPerCategory : flPerCategory*2; // if there isn't any sprints, then climb points increase
    }
    const float flFinishWeight = GetLaps()*(100.0f - (flSprintTotalWeight + flClimbTotalWeight));
    
    for(unsigned int ixSprintClimb = 0;ixSprintClimb < this->m_lstScoring.size(); ixSprintClimb++)
    {
      SprintClimbPointPtr pSprintClimb = m_lstScoring[ixSprintClimb];
      SPRINTCLIMBDATA_RAW raw;
      m_lstScoring[ixSprintClimb]->GetRaw(&raw);
      switch(raw.eScoreType)
      {
      case SCORE_CLIMB:
      {
        if(flTotalClimbing <= 0) break; // nothing worth it here
        const ORIGDIST flStartOrigDist = raw.flOrigDistance - raw.flOrigLeadInDist;
        const float flStartDist = GetDistanceOfOrigDist(flStartOrigDist);
        const float flStartElev = this->GetElevationAtDistance(PLAYERDIST(0,flStartDist,GetLapLength()));

        const float flEndDist = GetDistanceOfOrigDist(raw.flOrigDistance);
        const float flEndElev = GetElevationAtDistance(PLAYERDIST(0,flEndDist,GetLapLength()));

        const float flMyClimb = max(0.0f,(flEndElev - flStartElev));
        const float flMyClimbPct = flMyClimb / flTotalClimbing;
        pSprintClimb->SetMaxPoints(flClimbTotalWeight * flMyClimbPct);
        break;
      }
      case SCORE_SPRINT:
        if(pSprintClimb->IsFinishMode())
        {
          pSprintClimb->SetMaxPoints(flFinishWeight);
        }
        else
        {
          // sprints are equal-weighted
          pSprintClimb->SetMaxPoints(flSprintTotalWeight / rgCounts[SCORE_SPRINT]);
        }
        break;
      default:
        break;
      }
    }
  }

  ComputeMapBounds();

  { // one more rebuild of the initial state to pick up the changes to sprint/climb points
    boost::shared_ptr<TDGInitialState> tempState = boost::shared_ptr<TDGInitialState>(new TDGInitialState());
    BuildInitialState(tempState.get(),lstSRTM);
    this->BuildFromInitState(*tempState,false,m_flNaturalDistance);
    this->m_tdgInitState = tempState;
  }

  return fRet;
}

bool Map::LoadFromGPX(SRTMSource* pSRTMSource, const string& strGPX, int iOwnerId, int iMaxKm, int laps, int timedLengthSeconds, int iStartPercent, int iEndPercent, bool fPermitSprintClimb)
{
  m_lstPoints.clear();
  m_lstScoring.clear();
  this->m_strMapFile = strGPX;

  this->m_cLaps = laps;
  this->m_timedLengthSeconds = timedLengthSeconds;
  DASSERT(IsLappingMode(laps,timedLengthSeconds) ^ IsTimedMode(laps,timedLengthSeconds)); // either we're doing a timed race or we're doing a finite-lap race.  Can't do both

  DWORD tmStart = ArtGetTime();
  m_iOwnerId = iOwnerId;
  MAPCAP mapCap=0;

  GPXPointSource source(strGPX);
  if(!source.GetLatLonPoints(m_lstPoints,m_strMapName,iStartPercent,iEndPercent,NULL,&mapCap))
  {
    return false;
  }
  if(!fPermitSprintClimb)
  {
    mapCap |= MAPCAP_NOSPRINTCLIMB;
  }
  DWORD tmEnd = ArtGetTime();
  cout<<"took "<<(tmEnd - tmStart)<<" to load GPX"<<endl;
  
  return ProcessLoadedMap(pSRTMSource, iMaxKm, mapCap);
}

class DBPointSource : public LatLonSource
{
public:
  DBPointSource(StatsStore* pStatsStore, int iMapId) : m_pStatsStore(pStatsStore),m_iMapId(iMapId) {};
  virtual ~DBPointSource() {};
  
  virtual bool GetLatLonPoints(vector<MAPPOINT>& lstPoints, string& strMapName, int iStartPct, int iEndPct, int* piOwner, MAPCAP* pMapCap) ARTOVERRIDE
  {
    lstPoints.clear();

    string strMapFile;
    m_pStatsStore->GetMapData(m_iMapId,&strMapName,&strMapFile,piOwner,pMapCap);
    
    for(int x = 0;x < strMapName.length(); x++)
    {
      if(strMapName[x] > 127) 
        strMapName[x] = '_';
    }
    
    // also change the name
    if(iStartPct != 0 || iEndPct != 100)
    {
      char szNewName[500];
      _snprintf(szNewName,sizeof(szNewName),"[%d-%d%%]%s",iStartPct,iEndPct,strMapName.c_str());
      strMapName = szNewName;
    }

    return m_pStatsStore->GetMapRoute(m_iMapId,lstPoints,iStartPct,iEndPct);
  }
private:
  StatsStore* m_pStatsStore;
  const int m_iMapId;
};
bool Map::LoadFromDB(SRTMSource* pSRTMSource, StatsStore* pStatsStore, const int iMapId, int iMaxKm, int laps, int timedLengthSeconds, int iStartPercent, int iEndPercent, bool fPermitSprintClimb)
{
  m_lstPoints.clear();
  m_iMapId = iMapId;

  m_cLaps = laps;
  m_timedLengthSeconds = timedLengthSeconds;
  DASSERT(IsLappingMode(laps,timedLengthSeconds) ^ IsTimedMode(laps,timedLengthSeconds)); // either we're doing a timed race or we're doing a finite-lap race.  Can't do both

  MAPCAP mapCap=0;
  DBPointSource source(pStatsStore, iMapId);
  if(!source.GetLatLonPoints(m_lstPoints,m_strMapName,iStartPercent,iEndPercent, &m_iOwnerId,&mapCap))
  {
    return false;
  }

  if(!fPermitSprintClimb)
  {
    mapCap |= MAPCAP_NOSPRINTCLIMB;
  }
  return ProcessLoadedMap(pSRTMSource,iMaxKm, mapCap);
}

class SinePointSource : public LatLonSource
{
public:
  SinePointSource() {};
  virtual ~SinePointSource() {};
  
  virtual bool GetLatLonPoints(vector<MAPPOINT>& lstPoints, string& strMapName, int iStartPct, int iEndPct, int* piOwner, MAPCAP* pMapCap) ARTOVERRIDE
  {
    const int kPoints = 1000;
    for(int x = 0;x < kPoints; x++)
    {
      const float flPct = (float)x / (float)kPoints;
      const float flRadians = flPct*2*PI;
      MAPPOINT pt;
      pt.flElev = 1;
      pt.flLat = sin(flRadians)/10.0;
      pt.flLon = cos(flRadians)/10.0;
      pt.flDistance = x*20;
      pt.flOrigDist.v = x*20;
      pt.flRadius =1e30f;
      pt.flRawLat = pt.flLat;
      pt.flRawLon = pt.flLon;
      lstPoints.push_back(pt);
    }
    strMapName="Sine Map for testing";
    *piOwner = 9;
    *pMapCap = 0;
    return true;
  }
};
bool Map::LoadFromSine(int iMaxKm, int laps, int timedLengthSeconds)
{
  m_lstPoints.clear();
  m_iMapId = 5;

  m_cLaps = laps;
  m_timedLengthSeconds = timedLengthSeconds;
  DASSERT(IsLappingMode(laps,timedLengthSeconds) ^ IsTimedMode(laps,timedLengthSeconds)); // either we're doing a timed race or we're doing a finite-lap race.  Can't do both

  MAPCAP mapCap=0;
  SinePointSource source;
  if(!source.GetLatLonPoints(m_lstPoints,m_strMapName,0,100, &m_iOwnerId,&mapCap))
  {
    return false;
  }

  return ProcessLoadedMap(NULL,iMaxKm, mapCap);
}
float Map::GetNaturalDistance() const
{
  return m_flNaturalDistance;
}
void Map::RescaleMap(float flTarget, vector<SRTMCELL>& lstSRTM)
{
  if(m_lstPoints.size() > 0 && flTarget > 0)
  {
    const float flDist = GetLapLength();
    const float flRatio = flDist / (float)flTarget;
    for(unsigned int x = 0; x < m_lstPoints.size(); x++)
    {
      MAPPOINT& pt = m_lstPoints[x];
      pt.flDistance /= flRatio;
      pt.flElev /= flRatio;
      pt.flLat /= flRatio;
      pt.flLon /= flRatio;
      
      DASSERT(!IsNaN(flRatio));
      DASSERT(!IsNaN(pt.flDistance));
      DASSERT(!IsNaN(pt.flElev));
      DASSERT(!IsNaN(pt.flLat));
      DASSERT(!IsNaN(pt.flLon));
    }
    ComputeMapBounds();
  

    const MAPBOUNDS mb = m_mb;
    RECTF rcAll(mb.flMinLat,mb.flMinLon,mb.flMaxLat,mb.flMaxLon);
    boost::shared_ptr<DistanceDeterminer> dist(new DistanceDeterminer(rcAll,-1,-1));
    dist->Precompute(150,100000,*this);
    for(unsigned int x = 0; x < lstSRTM.size(); x++)
    {
      SRTMCELL& pt = lstSRTM[x];
      pt.flElev /= flRatio;
      pt.flLat /= flRatio;
      pt.flLon /= flRatio;

      if(!dist->IsInRangeToRoad(pt.flLat,pt.flLon))
      {
        // this guy is too close (or waaay too far) from the road to matter
        lstSRTM[x] = lstSRTM.back();
        lstSRTM.pop_back();
        x--;
      }
    }
  }
}

bool Map::IsValid() const
{
  for(unsigned int x = 0;x < m_lstPoints.size();x++)
  {
    const MAPPOINT& pt = m_lstPoints[x];
    if(pt.flDistance > 500000) 
      return false;
    if(pt.flDistance < 0) 
      return false;
    if(pt.flElev < -5000) 
      return false;
    if(pt.flElev > 50000) 
      return false;
  }
  return true;
}

DWORD Map::GetStateHash() const
{
  DWORD dwRet = 0;
  const int cHashOutputDwords=4;
  unsigned int rgMD5[cHashOutputDwords];
  memset(rgMD5,0,sizeof(rgMD5));
  md5((unsigned char*)&m_mb, sizeof(m_mb), rgMD5);
  for(int x = 0;x < cHashOutputDwords; x++)  {    dwRet ^= rgMD5[x];  }

  memset(rgMD5,0,sizeof(rgMD5));
  md5((unsigned char*)GetMapName().data(), GetMapName().size(), rgMD5);
  for(int x = 0;x < cHashOutputDwords; x++)  {    dwRet ^= rgMD5[x];  }

  if(m_tdgInitState)
  {
    memset(rgMD5,0,sizeof(rgMD5));
    md5((unsigned char*)m_tdgInitState->rgLat,sizeof(m_tdgInitState->rgLat[0])*m_tdgInitState->cPointsUsed,rgMD5);
    for(int x = 0;x < cHashOutputDwords; x++)  {    dwRet ^= rgMD5[x];  }
    
    memset(rgMD5,0,sizeof(rgMD5));
    md5((unsigned char*)m_tdgInitState->rgLon,sizeof(m_tdgInitState->rgLon[0])*m_tdgInitState->cPointsUsed,rgMD5);
    for(int x = 0;x < cHashOutputDwords; x++)  {    dwRet ^= rgMD5[x];  }
  }
  
  return dwRet;
}


void Map::ComputeMapBounds()
{
  MAPBOUNDS ret;
  ret.flMinLat = 1e30f;
  ret.flMaxLat = -1e30f;

  ret.flMinLon = 1e30f;
  ret.flMaxLon = -1e30f;

  ret.flMinElev = 1e30f;
  ret.flMaxElev = -1e30f;

  for(unsigned int x = 0; x < m_lstPoints.size(); x++)
  {
    const MAPPOINT& pt = m_lstPoints[x];
    ret.flMinLat = min(ret.flMinLat,pt.flLat);
    ret.flMaxLat = max(ret.flMaxLat,pt.flLat);

    ret.flMinLon = min(ret.flMinLon,pt.flLon);
    ret.flMaxLon = max(ret.flMaxLon,pt.flLon);

    ret.flMinElev = min(ret.flMinElev,pt.flElev);
    ret.flMaxElev = max(ret.flMaxElev,pt.flElev);
  }

  ret.flCenterLat = (ret.flMaxLat+ret.flMinLat)/2;
  ret.flCenterElev = (ret.flMaxElev+ret.flMinElev)/2;
  ret.flCenterLon = (ret.flMaxLon+ret.flMinLon)/2;
  m_mb = ret;
}

MAPBOUNDS Map::GetMapBounds() const
{
  return m_mb;
}
void Map::GetDirectionAtDistance(const PLAYERDIST& dist, float* pflXDir, float* pflYDir) const
{
  PLAYERDIST distToUse = dist;
  if(IsTimedMode(m_cLaps,this->m_timedLengthSeconds))
  {
    distToUse.flDistance = dist.flDistance;
    distToUse.iCurrentLap = 0;
  }
  
  if(distToUse <= GetStartDistance())
  {
    distToUse = GetStartDistance();
  }
  else if(distToUse >= GetEndDistance())
  {
    distToUse = GetEndDistance();
  }


  {
    distToUse.AddMeters(-0.5);
    const LATLON ll1 = GetLatLonAtDistance(distToUse);
    distToUse.AddMeters(1);
    const LATLON ll2 = GetLatLonAtDistance(distToUse);
    *pflXDir = ll2.flLat - ll1.flLat;
    *pflYDir = ll2.flLon - ll1.flLon;
  }
}

LATLON Map::GetLatLonAtDistance(const PLAYERDIST& dist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetPoints(dist.flDistance, &pt1,&pt2);

  const float flSpan = pt2.flDistance - pt1.flDistance;
  const float flPct = (dist.flDistance - pt1.flDistance) / flSpan;
    
  LATLON ll;
  if(flPct > 1.0)
  {
    ll.flLat = pt2.flLat;
    ll.flLon = pt2.flLon;
  }
  else if(flPct < 0) 
  {
    ll.flLat = pt1.flLat;
    ll.flLon = pt1.flLon;
  }
  else
  {
    if(m_pSpline)
    {
      const float flT = pt1.flSplineT*(1-flPct) + pt2.flSplineT*flPct;
      const NonOgreVector vPos = m_pSpline->interpolate(flT,NULL,NULL,NULL);
      DASSERT(flPct >= 0 && flPct <= 1.0);

      ll.flLat = vPos.x;
      ll.flLon = vPos.z;
    }
    else
    {
      ll.flLat = flPct*pt2.flLat + (1-flPct)*pt1.flLat;
      ll.flLon = flPct*pt2.flLon + (1-flPct)*pt1.flLon;
    }
  }
  return ll;
}
float Map::GetElevationAtDistance(const PLAYERDIST& dist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetPoints(dist.flDistance, &pt1,&pt2);

  const float flSpan = pt2.flDistance - pt1.flDistance;
  const float flPct = (dist.flDistance - pt1.flDistance) / flSpan;
  if(flPct > 1.0) return pt2.flElev;
  if(flPct < 0) return pt1.flElev;

  if(m_pSpline)
  {
    const float flT = pt1.flSplineT*(1-flPct) + pt2.flSplineT*flPct;
    const NonOgreVector vPos = m_pSpline->interpolate(flT,NULL,NULL,NULL);
    return vPos.y;
  }
  else
  {
    return flPct*pt2.flElev + (1-flPct)*pt1.flElev;
  }

}
ORIGDIST Map::GetOrigDistAtDistance(float flDist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetPoints(flDist, &pt1,&pt2);

  const float flSpan = pt2.flDistance - pt1.flDistance;
  const float flPct = (flDist - pt1.flDistance) / flSpan;
  if(flPct > 1.0) return pt2.flOrigDist;
  if(flPct < 0) return pt1.flOrigDist;
  DASSERT(flPct >= 0 && flPct <= 1.0);
  return (1-flPct)*pt1.flOrigDist + flPct*pt2.flOrigDist;
}
float Map::GetDistanceOfOrigDist(const ORIGDIST& dist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetOrigPoints(dist, &pt1,&pt2);

  const float flSpan = (pt2.flOrigDist - pt1.flOrigDist).v;
  const float flPct = (dist - pt1.flOrigDist).v / flSpan;
  if(flPct > 1.0) return pt2.flDistance;
  if(flPct < 0) return pt1.flDistance;
  DASSERT(flPct >= 0 && flPct <= 1.0);
  return (1-flPct)*pt1.flDistance + flPct*pt2.flDistance;
}
float Map::GetMaxSpeedAtDistance(const PLAYERDIST& dist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetPoints(dist.flDistance, &pt1,&pt2);

  const float flSpan = pt2.flDistance - pt1.flDistance;
  const float flPct = (dist.flDistance - pt1.flDistance) / flSpan;
  if(flPct > 1.0) return pt2.flMaxSpeed;
  if(flPct < 0) return pt1.flMaxSpeed;
  DASSERT(flPct >= 0 && flPct <= 1.0);
  return pt1.flMaxSpeed*(1-flPct) + pt2.flMaxSpeed*flPct;
}
float Map::GetRadiusAtDistance(const PLAYERDIST& dist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetPoints(dist.flDistance, &pt1,&pt2);

  const float flSpan = pt2.flDistance - pt1.flDistance;
  const float flPct = (dist.flDistance - pt1.flDistance) / flSpan;
  if(flPct > 1.0) return pt2.flRadius;
  if(flPct < 0) return pt1.flRadius;
  DASSERT(flPct >= 0 && flPct <= 1.0);
  return pt1.flRadius*(1-flPct) + pt2.flRadius*flPct;
}

int Map::GetLaps() const
{
  return m_cLaps;
}
float Map::GetSlopeAtDistance(const PLAYERDIST& dist) const
{
  MAPPOINT pt1;
  MAPPOINT pt2;
  GetPoints(dist.flDistance, &pt1,&pt2);

  const float flSpan = pt2.flDistance - pt1.flDistance;
  const float flPct = (dist.flDistance - pt1.flDistance) / flSpan;
  if(flPct > 1.0) return pt2.flSlope;
  if(flPct < 0) return pt1.flSlope;
  DASSERT(flPct >= 0 && flPct <= 1.0);
  return pt1.flSlope*(1-flPct) + pt2.flSlope*flPct;
}
float Map::GetClimbing() const
{
  float flSum = 0;
  for(unsigned int x = 0;x < m_lstPoints.size()-1; x++)
  {
    const MAPPOINT& pt1 = m_lstPoints[x];
    const MAPPOINT& pt2 = m_lstPoints[x+1];
    if(pt2.flElev > pt1.flElev)
    {
      flSum += (pt2.flElev - pt1.flElev);
    }
  }
  return flSum;
}
float Map::GetLapLength() const
{
  return this->m_lstPoints.back().flDistance - m_lstPoints.front().flDistance;
}

void Map::GetScoringSources(std::vector<SprintClimbPointPtr>& lstSources) const
{
  lstSources = m_lstScoring;
}
float Map::GetMaxGradient() const
{
  float flMax = -1e30f;
  for(unsigned int x = 0;x < m_lstPoints.size()-1; x++)
  {
    const MAPPOINT& pt1 = m_lstPoints[x];
    const float flAheadHeight = this->GetElevationAtDistance(PLAYERDIST(GetLaps()-1,pt1.flDistance+200,GetLapLength()));
    if(flAheadHeight > pt1.flElev)
    {
      float flGrad = (flAheadHeight - pt1.flElev) / (200);
      flMax = max(flGrad,flMax);
    }
  }
  return flMax;
}

template<class Lookup, class Pred> void GetPointsImpl(const vector<MAPPOINT>& lstPoints, const Lookup& dist, MAPPOINT* p1, MAPPOINT* p2, const Pred& p)
{
  if(p.TooEarly(dist,lstPoints))
  {
    *p1 = lstPoints[0];
    *p2 = lstPoints[1];
    return;
  }
  else if(p.TooLate(dist,lstPoints))
  {
    const int c = lstPoints.size();
    *p1 = lstPoints[c-2];
    *p2 = lstPoints[c-1];
    return;
  }
  else
  {
    vector<MAPPOINT>::const_iterator i = lower_bound(lstPoints.begin(),lstPoints.end(),dist, p); // this will find the item AFTER the distance we're looking for
    if(i == lstPoints.end())
    {
      DASSERT(FALSE); // shouldn't happen - we already checked for this case...
    }
    else
    {
      *p2 = *i;
      i--;
      *p1 = *i;
      return;
    }
  }
  DASSERT(FALSE);
  return;
}

struct GetPointsPred
{
  bool TooEarly(float dist, const vector<MAPPOINT>& lstPoints) const {return lstPoints.size() <= 0 || dist <= lstPoints.front().flDistance;}
  bool TooLate(float dist, const vector<MAPPOINT>& lstPoints) const {return lstPoints.size() <= 0 || dist >= lstPoints.back().flDistance;}
  bool operator () (const MAPPOINT& m1, const float& m2) const
  {
    return m1.flDistance < m2;
  }
};
void Map::GetPoints(const float flDistance, MAPPOINT* p1, MAPPOINT* p2) const
{
  GetPointsImpl(m_lstPoints, flDistance,p1,p2,GetPointsPred());
}
struct GetOrigPred
{
  bool TooEarly(const ORIGDIST& dist, const vector<MAPPOINT>& lstPoints) const {return lstPoints.size() <= 0 || dist <= lstPoints.front().flOrigDist;}
  bool TooLate(const ORIGDIST& dist, const vector<MAPPOINT>& lstPoints) const {return lstPoints.size() <= 0 || dist >= lstPoints.back().flOrigDist;}
  bool operator () (const MAPPOINT& m1, const ORIGDIST& m2) const
  {
    return m1.flOrigDist < m2;
  }
};
void Map::GetOrigPoints(const ORIGDIST& dist, MAPPOINT* p1, MAPPOINT* p2) const
{
  GetPointsImpl(m_lstPoints, dist,p1,p2,GetOrigPred());
}

#define d2r (3.14159 / 180.0)
float DistanceInMeters(float lat1,float long1, float lat2, float long2)
{
  //http://stackoverflow.com/questions/365826/calculate-distance-between-2-gps-coordinates
  double dlong = (long2 - long1) * d2r;
  double dlat = (lat2 - lat1) * d2r;
  double a = pow(sin(dlat/2.0), 2) + cos(lat1*d2r) * cos(lat2*d2r) * pow(sin(dlong/2.0), 2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  double d = 6367 * c;

  return (float)(d*1000.0f);
}

// find the point with the highest distance < flSkipTo
int Map::GetIndexOfDistance(int ixStart, int max, float flSkipTo) const
{
  int ixLow = ixStart;
  int ixHigh = max;
  while(true)
  {
    const int ixTest = (ixLow + ixHigh)/2;
    const float flDistAt = m_lstPoints[ixTest].flDistance;
    if(ixLow < ixHigh - 1)
    {
      if(flDistAt > flSkipTo)
      {
        // too far
        ixHigh = ixTest;
      }
      else if(flDistAt < flSkipTo)
      {
        // not far enough
        ixLow = ixTest;
      }
      else
      {
        // exact hit
        return ixTest;
      }
    }
    else // ixLow >= ixHigh-1
    {
      return ixLow;
    }
    
  }
}

int Map::m_cRangeCalls = 0;
int Map::m_cPointsChecked = 0;

// checks to see if the specced lat/lon is between rangeLow and rangeHigh from a road - used to place scenery not to close and not too far from the road
bool Map::IsInRangeToRoad(float lat, float lon, float rangeLow, float rangeHigh, int* pixFirst, int* pixLast, bool* pfTooClose) const
{
  bool scratch;
  if(!pfTooClose) pfTooClose = &scratch;

  m_cRangeCalls++;

  const MAPBOUNDS& mb = this->GetMapBounds();
  if(lat < mb.flMinLat) return false;
  if(lat > mb.flMaxLat) return false;
  if(lon < mb.flMinLon) return false;
  if(lon > mb.flMaxLon) return false;

  const int ixStart = pixFirst && *pixFirst >= 0 ? *pixFirst : 0;
  const int ixEnd = pixLast && *pixLast >= 0 ? *pixLast : m_lstPoints.size();

  if(pixFirst) *pixFirst = -1;
  if(pixLast) *pixLast = -1;

  const float rangeLowSq = rangeLow*rangeLow;
  const float rangeHighSq = rangeHigh*rangeHigh;
  bool fInRangeOnce = false;

  // check from the end for the first point on the road that might be in range to the point
  if(pixLast)
  {
    for(int x = ixEnd-1;x >= ixStart; x--)
    {
      const MAPPOINT& pt1 = m_lstPoints[x];
      Vector2D vPointToP1 = V2D(pt1.flLat - lat,pt1.flLon-lon);
      const float flLenSq = vPointToP1.LengthSq();
      if(flLenSq < rangeHighSq)
      {
        // found the closest!
        *pixLast = x+1;
        break;
      }
    }
  }
  
  // what is the average distance between points in this map?  we should avoid calling GetIndexOfDist if we're only skipping in this range
  const float flAvgDist = (float)(GetLapLength()) / (float)m_lstPoints.size();

  for(int x = ixStart;x < ixEnd-1; x++)
  {
    if(x >= (int)m_lstPoints.size() -1)
    {
      break; // no good will come of this
    }
    const MAPPOINT& pt1 = m_lstPoints[x];
    const MAPPOINT& pt2 = m_lstPoints[x+1];
    
    Vector2D vPointToP2 = V2D(pt2.flLat - lat,pt2.flLon-lon);
    Vector2D vPointToP1 = V2D(pt1.flLat - lat,pt1.flLon-lon);

    const float flLengthTo1Sq = V2D(pt1.flLat - lat,pt1.flLon-lon).LengthSq();
    const float flLengthTo2Sq = vPointToP2.LengthSq();
    if(flLengthTo1Sq < rangeLowSq) 
    {
      *pfTooClose = true;
      return false; // too close
    }
    if(flLengthTo2Sq < rangeLowSq) 
    {
      *pfTooClose = true;
      return false; // too close
    }

    
    if(flLengthTo1Sq > rangeHighSq && flLengthTo2Sq > rangeHighSq && vPointToP1.DP(vPointToP2) > 0)
    {
      // the point is super-far from our chunk of road.  this is actually good news, as it means we can skip at least flSkipDist worth of road before having to do any rechecking
      const float flSkipDist = min(sqrt(flLengthTo1Sq) - rangeHigh, sqrt(flLengthTo2Sq) - rangeHigh)*0.95;
      if(flSkipDist > flAvgDist*1)
      {
        const float flSkipTo = pt1.flDistance + flSkipDist;
        x = GetIndexOfDistance(x,ixEnd, flSkipTo);
      }
      continue;
    }
    Vector2D vLineSeg = V2D(pt2.flLat-pt1.flLat,pt2.flLon-pt1.flLon);
    m_cPointsChecked++;
    if(pixFirst && *pixFirst == -1) *pixFirst = x; // this is the first point that we actually couldn't skip over

    //Vector2D vPerp = vLineSeg.RotateAboutOrigin(PI/2);

    // now we kinda have right-angle triangle, formed by pt2, (lat,lon), and the intersect point
    // we know the length of vPointToP1, and we can find the angle formed by pt2->intersect and pt2->(lat,lon)
    // we can find the angle at the pt2 corner, which means we can then find d

    float flDenon = (vLineSeg.Length() * vPointToP2.Length());
    if(flDenon != 0)
    {
      const float flCosTheta = vLineSeg.DP(vPointToP2) / flDenon; // cos(theta) = a dot b / |a||b|
      float flAdjacentSq = flCosTheta*vPointToP2.Length();
      flAdjacentSq*=flAdjacentSq; // make it actually squared5
      const float flDistBetweenSq = vLineSeg.LengthSq();
      if(flAdjacentSq < -flDistBetweenSq*1.25 || flAdjacentSq > flDistBetweenSq*2.25)
      {
        continue; // intersect point is beyond the start and end of this line segment
      }
      const float flTheta = acos(flCosTheta);
      float flOppositeSq = sin(flTheta)*vPointToP2.Length(); // sin(theta) = opposite/hypotenuse --> hypotenuse*sin(theta) = opposite
      flOppositeSq*=flOppositeSq;
      if(flOppositeSq < rangeLowSq) 
      {
        *pfTooClose = true;
        return false; // too close
      }
      fInRangeOnce |= (flOppositeSq >= rangeLowSq && flOppositeSq <= rangeHighSq);

      if(flOppositeSq >= rangeLowSq && flOppositeSq <= rangeHighSq)
      { // optimization: figure out the minimum distance before we go out of range in either direction.  We can skip that much road
        const float flDistToTooFar = rangeHigh - sqrt(flOppositeSq);
        const float flDistToTooClose = sqrt(flOppositeSq) - rangeLow;
        const float flSkipDistance = min(flDistToTooClose,flDistToTooFar);
        if(flSkipDistance > flAvgDist*1)
        {
          x = GetIndexOfDistance(x,ixEnd, pt1.flDistance + flSkipDistance);
        }
      }
    }
    else if(vLineSeg.Length() == 0)
    {
      continue;
    }
    else if(vPointToP2.LengthSq())
    {
      // point is on p2, so distance is zero
      fInRangeOnce |= rangeLow == 0;
    }
    else
    {
      DASSERT(FALSE);
    }
  }

  return fInRangeOnce;
}


void Map::Test()
{
  Map map;

  for(int km = 3; km < 6; km+= 1)
  {
    cout<<"Running map test for "<<km<<"km"<<endl;
    // test 1: this is a <km> long map (within some tolerance).  The sine point source made it a circle, so therefore it should have a radius at all distance of <km>/<2pi>
    float flSum = 0;
    int cSamples = 0;
    const double TwoPi = 3.14159*2;
    const double dExpectedRadius = (double)(1000.0*km) / TwoPi;
    map.LoadFromSine(km,1,-1);

    const PLAYERDIST distStart = map.GetStartDistance().CalcAddMeters(500);
    const PLAYERDIST distEnd = map.GetEndDistance().CalcAddMeters(-500);
    for(PLAYERDIST dist = distStart; dist < distEnd; dist.AddMeters(500))
    {
      const float flRadius = map.GetRadiusAtDistance(dist);
      flSum += flRadius;
      cSamples++;
      EXPECT_T(abs(flRadius) >= dExpectedRadius*0.5 && abs(flRadius) <= dExpectedRadius*1.5); // let's hope the radius is within a reasonable distance of what we'd want
    }

    const float flActualExpectation = -dExpectedRadius; // since this is a left-turning circle, we are expecting negative radii
    const float flAvgRadius = flSum / (float)cSamples;
    const float flError = flActualExpectation - flAvgRadius;
    EXPECT_T(abs(flError) < 0.1*dExpectedRadius); // hopefully the error is within 10%
  }

}
void DumpMap(const Map& map, ostream& out)
{
  for(PLAYERDIST spot = map.GetStartDistance(); spot < map.GetEndDistance(); spot.AddMeters(50))
  {
    out<<"\t"<<map.GetElevationAtDistance(spot);
    out<<"\t"<<map.GetSlopeAtDistance(spot);
    out<<"\t"<<map.GetMaxSpeedAtDistance(spot);
    out<<"\t"<<map.GetRadiusAtDistance(spot);
    out<<"\t"<<map.GetOrigDistAtDistance(spot.flDistance).v<<endl;
  }
}