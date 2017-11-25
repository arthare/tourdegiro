
#include "stdafx.h"
#include "Tools.h"

GRAPHICS_QOS g_qos;

void EXPECT_T(bool f)
{
#ifdef _MSC_VER
  if(IsDebuggerPresent())
  {
    if(!f)
    {
      __asm int 3;
    }
  }
#endif
}
float rand(float flMin, float flMax)
{
  int iRand = rand() % 32768;
  float flRand = (float)iRand / 32768.0f;
  float flRet = (flRand * (flMax-flMin)) + flMin;
  return flRet;
}

void NextSpeedPref()
{
  g_qos.eSpeedPref = GetNextSpeedPref(g_qos.eSpeedPref);
}
SPEEDPREF GetSpeedPref()
{
  return g_qos.eSpeedPref;
}
SPEEDPREF GetNextSpeedPref(SPEEDPREF eCurrent)
{
  switch(eCurrent)
  {
  case METRIC:
    return IMPERIAL;
  case IMPERIAL:
    return METRIC;
  default:
    return METRIC;
  }
  return METRIC;
}

void FormatDistanceShort(char* psz, int cch, float flMeters, int decimals, SPEEDPREF eUnits)
{
  switch(eUnits)
  {
  case IMPERIAL:
  {
    const float flMiles = flMeters / 1609.34;
    snprintf(psz,cch,"%4.0fft",flMiles*5280);
    break;
  }
  default:
  case METRIC:
  {
    snprintf(psz,cch,"%4.0fm",flMeters);
    break;
  }
  }
}
// if you include pszUnits, then they won't be included in the actual string and will instead be output to pszUnits
void FormatSpeed(char* psz, int cch, float flMetersPerSecond, SPEEDPREF eUnits, char* pszUnits, int cchUnits)
{
  switch(eUnits)
  {
  case IMPERIAL:
  {
    const float flMPH = flMetersPerSecond * 2.23694;
    if(pszUnits)
    {
      snprintf(psz,cch,"%3.1f",flMPH);
      snprintf(pszUnits,cch,"mph");
    }
    else
    {
      snprintf(psz,cch,"%3.1fmph",flMPH);
    }
    break;
  }
  default:
  case METRIC:
  {
    const float flKmh = flMetersPerSecond * 3.6;
    if(pszUnits)
    {
      snprintf(psz,cch,"%3.1f",flKmh);
      snprintf(pszUnits,cch,"km/h");
    }
    else
    {
      snprintf(psz,cch,"%3.1fkm/h",flKmh);
    }
    break;
  }
  }
}
void FormatMass(char* psz, int cch, float flKg, SPEEDPREF eUnits, char* pszUnits, int cchUnits)
{
  float flLbs = flKg*2.2;
  switch(eUnits)
  {
  case IMPERIAL:
    snprintf(psz,cch,"%3.1f",flLbs);
    snprintf(pszUnits,cch,"lbs");
    break;
  case METRIC:
    snprintf(psz,cch,"%3.1f",flKg);
    snprintf(pszUnits,cch,"kg");
    break;
  }
}


////////////////////////////////////////////////////////////////////////////////////
GRAPHICS_QOS::GRAPHICS_QOS() : fRenderToTarget(true),flMaxFieldDist(3000),fShow3DScenery(true),fShowClouds(true),fShowFields(true),fShowHUD(true),fShowHDTerrain(true),fShowCities(true),eSpeedPref(IMPERIAL),fNoDownhillMode(false)
{

}

bool GRAPHICS_QOS::Load()
{
  std::ifstream in;
  Default();
  try
  {
    std::wstring strW(GetRoamingPath(L"tourdegiro/graphics.txt"));
    std::string strA(strW.begin(),strW.end());
    in.open(strA.c_str());
    if(!in.fail() && !in.eof())
    {
      int version = 0;
      in>>version;
      if(version <= FIRST)
      {
        LoadFirstVersion(in,version);
      }
      else if(version == AUG27)
      {
        LoadAug27(in);
      }
      else if(version == SEPT23)
      {
        LoadSept23(in);
      }
      else if(version == APRIL8)
      {
        LoadApril8(in);
      }
      else if(version == AUGUST21)
      {
        this->LoadAugust21_2014(in);
      }
      else if(version == OCT4_2014)
      {
        LoadOct4_2014(in);
      }
      else if(version == OCT4_2015)
      {
        LoadOct4_2015(in);
      }
      else if(version == NOV29_2015)
      {
        LoadNov29_2015(in);
      }

      if(version < AUGUST21)
      {
        // if we just upgraded to AUGUST21 or later, kill the clouds
        g_qos.fShowClouds = false;
      }
    }
    else
    {
      return false; // file didn't exist
    }
  }
  catch(std::exception& e)
  {
    return false;
  }
  return true;
}
void GRAPHICS_QOS::Save()
{
  std::wstring strW(GetRoamingPath(L"tourdegiro/graphics.txt"));
  std::string strA(strW.begin(),strW.end());
  std::ofstream out;
  out.open(strA.c_str());
  Save(out);
}
void GRAPHICS_QOS::Default()
{
  fShow3DScenery = true;
  fShowClouds = false; // for the aug21/2014 upgrade, we want the clouds (which now refer to skyX being on) to default to off
  fShowFields = true;
  fShowHUD = true;
  fShowHDTerrain = true;
  fShowCities = true;

  fRenderToTarget = true;
  flMaxFieldDist = 2250;
  eSpeedPref = IMPERIAL;
  fExtremeGraphics = false;

  fShowSpectators = false;
  fShowShadows = false;
  fShowWater = false;
  fHideAILabels = false;
  fSuppressKickrReset = false;
  fPowerlessLabels = false;

  iTrainingPlanId = -1;
}
void GRAPHICS_QOS::LoadFirstVersion(std::ifstream& in, int first)
{
  // the first thing in the old version was whether to use direct3d or not
  fDirect3D = first!=0;
  int RTT = 0;
  in>>RTT;
  fRenderToTarget = RTT != 0;
}
void GRAPHICS_QOS::LoadAug27(std::ifstream& in)
{
  // order goes:
  // direct3d, show3d, clouds, fields, hud, hdterrain, cities, RTT, maxfielddist
  in>>fDirect3D;
  in>>fShow3DScenery;
  in>>fShowClouds;
  in>>fShowFields;
  in>>fShowHUD;
  in>>fShowHDTerrain;
  in>>fShowCities;
  in>>fRenderToTarget;
  in>>flMaxFieldDist;
  in>>(int&)eSpeedPref;
}
void GRAPHICS_QOS::LoadSept23(std::ifstream& in)
{
  LoadAug27(in);
  
  char szLine[MAX_PATH*2];
  in.getline(szLine,sizeof(szLine)); // skips the final token of the last line
  in.getline(szLine,sizeof(szLine));
  szLine[MAX_PATH*2 - 1] = 0; // make sure we're terminated

  bool fSuccess = false;
  char* pszColon = 0;
  for(int x = 0;x < sizeof(szLine)-3;x++)
  {
    if(szLine[x] == ':' && szLine[x+1] == ':' && szLine[x+2] == ':')
    {
      pszColon = &szLine[x];
    }
  }
  if(pszColon)
  {
    // make sure we're properly null-terminated
    pszColon[0] = 0;
    pszColon[1] = 0;
    pszColon[2] = 0; 

    TCHAR szLineW[MAX_PATH];
    memcpy(szLineW,szLine,sizeof(szLine));
    if(ArtDirectoryExists(szLineW))
    {
      fSuccess = true;
      strSaveDest = szLineW;
    }
  }
  
  if(!fSuccess)
  {
    strSaveDest = ::GetRoamingPath(L"Tour de Giro Data");
    if(!ArtDirectoryExists(strSaveDest.c_str()))
    {
      ArtCreateDirectory(strSaveDest.c_str(),NULL);
    }
  }
}
void GRAPHICS_QOS::LoadApril8(std::ifstream& in)
{
  LoadSept23(in);
  in>>fExtremeGraphics;
}
void GRAPHICS_QOS::LoadAugust21_2014(std::ifstream& in)
{
  LoadApril8(in);

  in>>fShowSpectators;
  in>>fShowShadows;
  in>>fShowWater;
  
#ifndef _MSC_VER
  fRenderToTarget = true;
#endif
}
void GRAPHICS_QOS::LoadOct4_2014(std::ifstream& in)
{
  LoadAugust21_2014(in);
  in>>iTrainingPlanId;
}
void GRAPHICS_QOS::LoadOct4_2015(std::ifstream& in)
{
  LoadOct4_2014(in);
  in>>fHideAILabels;
}
void GRAPHICS_QOS::LoadNov29_2015(std::ifstream& in)
{
  LoadOct4_2015(in);
  in>>fSuppressKickrReset;
  in>>fPowerlessLabels;
}
void GRAPHICS_QOS::Save(std::ostream& out) const
{
  out<<NOV29_2015<<std::endl; // version indicator

  out<<fDirect3D<<std::endl;
  out<<fShow3DScenery<<std::endl;
  out<<fShowClouds<<std::endl;
  out<<fShowFields<<std::endl;
  out<<fShowHUD<<std::endl;
  out<<fShowHDTerrain<<std::endl;
  out<<fShowCities<<std::endl;
  out<<fRenderToTarget<<std::endl;
  out<<flMaxFieldDist<<std::endl;
  out<<eSpeedPref<<std::endl;

  if(ArtDirectoryExists(strSaveDest.c_str()))
  {
    out.write((const char*)strSaveDest.c_str(),(strSaveDest.length()+1)*sizeof(strSaveDest[0]));
    out<<":::"<<std::endl;
  }
  out<<fExtremeGraphics<<std::endl;
  out<<fShowSpectators<<std::endl;
  out<<fShowShadows<<std::endl;
  out<<fShowWater<<std::endl;
  out<<iTrainingPlanId<<std::endl;
  out<<fHideAILabels<<std::endl;
  out<<fSuppressKickrReset<<std::endl;
  out<<fPowerlessLabels<<std::endl;
  out<<std::endl;
}